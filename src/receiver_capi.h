#ifndef RECEIVER_CAPI_H
#define RECEIVER_CAPI_H

#define RECEIVER_CAPI_ID_STRING     "_capi_receiver"
#define RECEIVER_CAPI_VERSION_MAJOR  1
#define RECEIVER_CAPI_VERSION_MINOR  0
#define RECEIVER_CAPI_VERSION_PATCH  0

#ifndef RECEIVER_CAPI_HAVE_LONG_LONG
#  include <limits.h>
#  if defined(LLONG_MAX)
#    define RECEIVER_CAPI_HAVE_LONG_LONG 1
#  else
#    define RECEIVER_CAPI_HAVE_LONG_LONG 0
#  endif
#endif

#ifndef RECEIVER_CAPI_IMPLEMENT_SET_CAPI
#  define RECEIVER_CAPI_IMPLEMENT_SET_CAPI 0
#endif

#ifndef RECEIVER_CAPI_IMPLEMENT_GET_CAPI
#  define RECEIVER_CAPI_IMPLEMENT_GET_CAPI 0
#endif

#ifdef __cplusplus

extern "C" {

struct receiver_object;
struct receiver_writer;
struct receiver_capi;

#else /* __cplusplus */

typedef struct receiver_object      receiver_object;
typedef struct receiver_writer      receiver_writer;
typedef struct receiver_capi        receiver_capi;
typedef enum   receiver_array_type  receiver_array_type;

#endif /* ! __cplusplus */

enum receiver_array_type
{
    RECEIVER_UCHAR  =  1,
    RECEIVER_SCHAR  =  2,
    
    RECEIVER_SHORT  =  3,
    RECEIVER_USHORT =  4,
    
    RECEIVER_INT    =  5,
    RECEIVER_UINT   =  6,
    
    RECEIVER_LONG   =  7,
    RECEIVER_ULONG  =  8,
    
    RECEIVER_FLOAT  =  9,
    RECEIVER_DOUBLE = 10,

#if RECEIVER_CAPI_HAVE_LONG_LONG
    RECEIVER_LLONG  = 11,
    RECEIVER_ULLONG = 12,
#endif
};


/**
 * Type for pointer to function that may be called if an error occurs.
 * ehdata: void pointer that is given in add/setMsgToReceiver method (see below)
 * msg:    detailed error message
 * msglen: length of error message
 */
typedef void (*receiver_error_handler)(void* ehdata, const char* msg, size_t msglen);


/**
 *  Receiver C API.
 */
struct receiver_capi
{
    int version_major;
    int version_minor;
    int version_patch;
    
    /**
     * May point to another (incompatible) version of this API implementation.
     * NULL if no such implementation exists.
     *
     * The usage of next_capi makes it possible to implement two or more
     * incompatible versions of the C API.
     *
     * An API is compatible to another API if both have the same major 
     * version number and if the minor version number of the first API is
     * greater or equal than the second one's.
     */
    void* next_capi;
    
    /**
     * Must return a valid pointer if the object at the given stack
     * index is a valid receiver object, otherwise must return NULL,
     */
    receiver_object* (*toReceiver)(lua_State* L, int index);

    /**
     * Increase the reference counter of the receiver object.
     * Must be thread safe.
     */
    void (*retainReceiver)(receiver_object* b);

    /**
     * Decrease the reference counter of the receiver object and
     * destructs the receiver object if no reference is left.
     * Must be thread safe.
     */
    void (*releaseReceiver)(receiver_object* b);

    /**
     * Creates new writer object.
     * Does not need to be thread safe.
     */
    receiver_writer* (*newWriter)(size_t initialCapacity, float growFactor);

    /**
     * Destructs writer object.
     * Does not need to be thread safe.
     */
    void (*freeWriter)(receiver_writer* w);

    /**
     * Must be thread safe.
     *
     * Send the writer's content as one message to the receiver. If successfull, the writer's 
     * content is cleared.
     *
     * clear:    not 0 if old messages should be discarded (exact meaning depends on receiver implementation)
     *           0, if new message should be added.
     * nonblock: not 0 if function should do nothing if receiver cannot receive immediately,
     *           0 if function should wait for receiver to becomde ready.
     * eh:       error handling function, may be NULL
     * ehdata:   additional data that is given to error handling function.
     *
     * returns:   0 - if message could be handled by receiver
     *            1 - if receiver is closed. The caller is expected to release the receiver.
     *                Subsequent calls will always result this return code again.
     *            2 - if receiver was aborted. Subsequent calls may function again. Exact
     *                context depends on implementation and use case. Normally the caller is
     *                expected to abort its operation.
     *            3 - if nonblock and receiver was not ready
     *            4 - if receiver could not handle message because queued messages together
     *                with the new message are exceeding the receiver's memory limit. This could
     *                be fixed by removing old messages from the receiver.
     *            5 - if receiver could not handle the message because the new message
     *                is larger than the receiver's memory limit. This cannot be fixed
     *                by removing old messages from the receiver.
     *            6 - if receiver has growable memory without limit, but cannot allocate more 
     *                memory because overall memory is exhausted.
     *
     * All other error codes are implementation specific.
     */
    int (*msgToReceiver)(receiver_object* b, receiver_writer* w, 
                         int clear, int nonblock,
                         receiver_error_handler eh, void* ehdata);

    /**
     * Does not need to be thread safe.
     */
    void (*clearWriter)(receiver_writer* w);

    /**
     * Does not need to be thread safe.
     */
    int  (*addBooleanToWriter)(receiver_writer* w, int b);

    /**
     * Does not need to be thread safe.
     */
    int  (*addIntegerToWriter)(receiver_writer* w, lua_Integer i);

    /**
     * Does not need to be thread safe.
     */
    int  (*addStringToWriter)(receiver_writer* w, const char* s, size_t len);

    /**
     * Adds each byte as an integer value between 0 and 255.
     * Does not need to be thread safe.
     */
    int  (*addBytesToWriter)(receiver_writer* w, const unsigned char* s, size_t len);
    
    /**
     * Adds an array of primitive numeric C data types as one value.
     * Does not need to be thread safe.
     * Returns pointer to the reserved memory for unintialized array 
     * elements. This pointer is valid until the next call to the writer.
     * The caller is responsible for filling in the element data before 
     * the next call to the writer.
     */
    void* (*addArrayToWriter)(receiver_writer* w, receiver_array_type t, 
                              size_t elementCount);
};


#if RECEIVER_CAPI_IMPLEMENT_SET_CAPI
/**
 * Sets the Receiver C API into the metatable at the given index.
 * 
 * index: index of the table that is be used as metatable for objects 
 *        that are associated to the given capi.
 */
static int receiver_set_capi(lua_State* L, int index, const receiver_capi* capi)
{
    lua_pushlstring(L, RECEIVER_CAPI_ID_STRING, strlen(RECEIVER_CAPI_ID_STRING));           /* -> key */
    void** udata = (void**) lua_newuserdata(L, sizeof(void*) + strlen(RECEIVER_CAPI_ID_STRING) + 1); /* -> key, value */
    *udata = (void*)capi;
    strcpy((char*)(udata + 1), RECEIVER_CAPI_ID_STRING);  /* -> key, value */
    lua_rawset(L, (index < 0) ? (index - 2) : index);     /* -> */
    return 0;
}
#endif /* RECEIVER_CAPI_IMPLEMENT_SET_CAPI */

#if RECEIVER_CAPI_IMPLEMENT_GET_CAPI
/**
 * Gives the associated Receiver C API for the object at the given stack index.
 * Returns NULL, if the object at the given stack index does not have an 
 * associated Receiver C API or only has a Receiver C API with incompatible version
 * number. If errorReason is not NULL it receives the error reason in this case:
 * 1 for incompatible version nummber and 2 for no associated C API at all.
 */
static const receiver_capi* receiver_get_capi(lua_State* L, int index, int* errorReason)
{
    if (luaL_getmetafield(L, index, RECEIVER_CAPI_ID_STRING) != LUA_TNIL)      /* -> _capi */
    {
        const void** udata = (const void**) lua_touserdata(L, -1);             /* -> _capi */

        if (   udata
            && (lua_rawlen(L, -1) >= sizeof(void*) + strlen(RECEIVER_CAPI_ID_STRING) + 1)
            && (memcmp((char*)(udata + 1), RECEIVER_CAPI_ID_STRING, 
                       strlen(RECEIVER_CAPI_ID_STRING) + 1) == 0))
        {
            const receiver_capi* capi = (const receiver_capi*) *udata;         /* -> _capi */
            while (capi) {
                if (   capi->version_major == RECEIVER_CAPI_VERSION_MAJOR
                    && capi->version_minor >= RECEIVER_CAPI_VERSION_MINOR)
                {                                                              /* -> _capi */
                    lua_pop(L, 1);                                             /* -> */
                    return capi;
                }
                capi = (const receiver_capi*) capi->next_capi;
            }
            if (errorReason) {
                *errorReason = 1;
            }
        } else {                                                               /* -> _capi */
            if (errorReason) {
                *errorReason = 2;
            }
        }                                                                      /* -> _capi */
        lua_pop(L, 1);                                                         /* -> */
    } else {                                                                   /* -> */
        if (errorReason) {
            *errorReason = 2;
        }
    }                                                                          /* -> */
    return NULL;
}
#endif /* RECEIVER_CAPI_IMPLEMENT_GET_CAPI */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RECEIVER_CAPI_H */
