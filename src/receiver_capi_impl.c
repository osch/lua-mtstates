#include "receiver_capi_impl.h"
#include "state.h"
#include "state_intern.h"

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

    clearWriter,
    addBooleanToWriter,
    addIntegerToWriter,
    addStringToWriter,

    msgToReceiver,
};
