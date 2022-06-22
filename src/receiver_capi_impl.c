#include "receiver_capi_impl.h"
#include "state.h"
#include "state_intern.h"
#include "carray_capi.h"

static receiver_object* toReceiver(lua_State* L, int index)
{
    void* state = lua_touserdata(L, index);
    if (state) {
        if (lua_getmetatable(L, index))
        {                                                      /* -> meta1 */
            if (luaL_getmetatable(L, MTSTATES_STATE_CLASS_NAME)
                != LUA_TNIL)                                   /* -> meta1, meta2 */
            {
                if (lua_rawequal(L, -1, -2)) {                 /* -> meta1, meta2 */
                    state = ((StateUserData*)state)->state;
                } else {
                    state = NULL;
                }
            }                                                  /* -> meta1, meta2 */
            lua_pop(L, 2);                                     /* -> */
        }                                                      /* -> */
    }
    return state;
}

static void retainReceiver(receiver_object* receiver)
{
    MtState* state = (MtState*)receiver;
    atomic_inc(&state->used);
}


static void releaseReceiver(receiver_object* receiver)
{
    MtState* state = (MtState*)receiver;
    if (atomic_dec(&state->used) <= 0) {
        mtstates_state_free(state);
    }
}

static receiver_writer* newWriter(size_t initialCapacity, float growFactor)
{
    receiver_writer* writer = malloc(sizeof(receiver_writer));
    if (writer) {
        if (mtstates_membuf_init(&writer->mem, initialCapacity, growFactor)) {
            writer->nargs = 0;
        } else {
            free(writer);
            writer = NULL;
        }
    }
    return writer;
}
static void freeWriter(receiver_writer* writer)
{
    if (writer) {
        mtstates_membuf_free(&writer->mem);
        free(writer);
    }
}

static void clearWriter(receiver_writer* writer)
{
    writer->mem.bufferStart  = writer->mem.bufferData;
    writer->mem.bufferLength = 0;
    writer->nargs = 0;
}


static int addBooleanToWriter(receiver_writer* writer, int value)
{
    size_t args_size = 1 + 1;
    int rc = mtstates_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        char* dest = writer->mem.bufferStart + writer->mem.bufferLength;
        *dest++ = BUFFER_BOOLEAN;
        *dest   = value;
        writer->mem.bufferLength += args_size;
        writer->nargs += 1;
    }
    return rc;
}

static int addIntegerToWriter(receiver_writer* writer, lua_Integer value)
{
    size_t args_size;
    if (0 <= value && value <= 0xff) {
        args_size = 1 + 1;
    } else {
        args_size = 1 + sizeof(lua_Integer);
    }
    int rc = mtstates_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        char* dest = writer->mem.bufferStart + writer->mem.bufferLength;
        if (args_size == 2) {
            *dest++ = BUFFER_BYTE;
            *dest   = ((char)value);
        } else {
            *dest++ = BUFFER_INTEGER;
            memcpy(dest, &value, sizeof(lua_Integer));
        }
        writer->mem.bufferLength += args_size;
        writer->nargs += 1;
    }
    return rc;
}

static int addNumberToWriter(receiver_writer* writer, lua_Number value)
{
    size_t args_size = 1 + sizeof(lua_Number);
    int rc = mtstates_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        char* dest = writer->mem.bufferStart + writer->mem.bufferLength;
        *dest++ = BUFFER_NUMBER;
        memcpy(dest, &value, sizeof(lua_Number));
        writer->mem.bufferLength += args_size;
        writer->nargs += 1;
    }
    return rc;
}

static int addStringToWriter(receiver_writer* writer, const char* value, size_t len)
{
    size_t args_size;
    if (len <= 0xff) {
        args_size = 1 + 1 + len;
    } else {
        args_size = 1 + sizeof(size_t) + len;
    }
    int rc = mtstates_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        char* dest = writer->mem.bufferStart + writer->mem.bufferLength;
        if (len <= 0xff) {
            *dest++ = BUFFER_SMALLSTRING;
            *dest++ = ((char)len);
        } else {
            *dest++ = BUFFER_STRING;
            memcpy(dest, &len, sizeof(size_t));
            dest += sizeof(size_t);
        }
        memcpy(dest, value, len);
        writer->mem.bufferLength += args_size;
        writer->nargs += 1;
    }
    return rc;
}

static int addBytesToWriter(receiver_writer* writer, const unsigned char* value, size_t len)
{
    int rc = mtstates_membuf_reserve(&writer->mem, 2*len);
    if (rc == 0) {
        char* dest = writer->mem.bufferStart + writer->mem.bufferLength;
        size_t i;
        for (i = 0; i < len; ++i) {
            *dest++ = BUFFER_BYTE;
            *dest++ = ((char)(value[i]));
        }
        writer->mem.bufferLength += 2*len;
        writer->nargs += len;
    }
    return rc;
}

static void* addArrayToWriter(receiver_writer* writer, receiver_array_type type, 
                              size_t elementCount)
{
    carray_type carrayType = 0;
    size_t elementSize = 0;
    switch (type) {
        case RECEIVER_UCHAR: carrayType = CARRAY_UCHAR; elementSize = sizeof(unsigned char); break;
        case RECEIVER_SCHAR: carrayType = CARRAY_SCHAR; elementSize = sizeof(signed char); break;
        
        case RECEIVER_SHORT:  carrayType = CARRAY_SHORT;  elementSize = sizeof(short); break;
        case RECEIVER_USHORT: carrayType = CARRAY_USHORT; elementSize = sizeof(unsigned short); break;
        
        case RECEIVER_INT:    carrayType = CARRAY_INT;  elementSize = sizeof(int); break;
        case RECEIVER_UINT:   carrayType = CARRAY_UINT; elementSize = sizeof(unsigned int); break;
        
        case RECEIVER_LONG:   carrayType = CARRAY_LONG;  elementSize = sizeof(long); break;
        case RECEIVER_ULONG:  carrayType = CARRAY_ULONG; elementSize = sizeof(unsigned long); break;
        
        case RECEIVER_FLOAT:  carrayType = CARRAY_FLOAT;   elementSize = sizeof(float); break;
        case RECEIVER_DOUBLE: carrayType = CARRAY_DOUBLE;  elementSize = sizeof(double); break;
    
    #if RECEIVER_CAPI_HAVE_LONG_LONG
        case RECEIVER_LLONG:  carrayType = CARRAY_LLONG;   elementSize = sizeof(long long); break;
        case RECEIVER_ULLONG: carrayType = CARRAY_ULLONG;  elementSize = sizeof(unsigned long long); break;
    #endif
        default: return NULL;
    }
    size_t len = 3 + sizeof(size_t) + elementSize * elementCount;
    int rc = mtstates_membuf_reserve(&writer->mem, len);
    if (rc == 0) {
        unsigned char* dest = writer->mem.bufferStart + writer->mem.bufferLength;
        *dest++ = BUFFER_CARRAY;
        *dest++ = carrayType;
        *dest++ = (unsigned char)elementSize;
        memcpy(dest, &elementCount, sizeof(size_t));
        dest += sizeof(size_t);
        writer->mem.bufferLength += len;
        writer->nargs += 1;
        return dest;
    } else {
        return NULL;
    }
}

static int msgToReceiver(receiver_object* receiver, receiver_writer* writer, 
                         int clear, int nonblock,
                         receiver_error_handler eh, void* ehdata)
{
    MtState* state = (MtState*)receiver;
    int rc = mtstates_state_call(NULL, false, 0, state, writer, eh, ehdata);
    if (rc == 0) {
        clearWriter(writer);
    }
    if (rc == 101) {
        return 1; // closed
    } else {
        return rc;
    }
}

const receiver_capi mtstates_receiver_capi_impl =
{
    RECEIVER_CAPI_VERSION_MAJOR,
    RECEIVER_CAPI_VERSION_MINOR,
    RECEIVER_CAPI_VERSION_PATCH,
    
    NULL, /* next_capi */
    
    toReceiver,

    retainReceiver,
    releaseReceiver,

    newWriter,
    freeWriter,

    msgToReceiver,

    clearWriter,
    addBooleanToWriter,
    addIntegerToWriter,
    addNumberToWriter,
    addStringToWriter,
    addBytesToWriter,
    addArrayToWriter
};
