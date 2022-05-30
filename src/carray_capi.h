#ifndef CARRAY_CAPI_H
#define CARRAY_CAPI_H

#ifndef CARRAY_CAPI_HAVE_LONG_LONG
#  include <limits.h>
#  if defined(LLONG_MAX)
#    define CARRAY_CAPI_HAVE_LONG_LONG 1
#  else
#    define CARRAY_CAPI_HAVE_LONG_LONG 0
#  endif
#endif

#define CARRAY_CAPI_ID_STRING     "_capi_carray"
#define CARRAY_CAPI_VERSION_MAJOR  -2
#define CARRAY_CAPI_VERSION_MINOR   0
#define CARRAY_CAPI_VERSION_PATCH   0

typedef struct carray_capi     carray_capi;
typedef struct carray_info     carray_info;
typedef struct carray          carray;

typedef enum carray_type  carray_type;
typedef enum carray_attr  carray_attr;

#ifndef CARRAY_CAPI_IMPLEMENT_SET_CAPI
#  define CARRAY_CAPI_IMPLEMENT_SET_CAPI 0
#endif

#ifndef CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI
#  define CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI 0
#endif

#ifndef CARRAY_CAPI_IMPLEMENT_GET_CAPI
#  define CARRAY_CAPI_IMPLEMENT_GET_CAPI 0
#endif

enum carray_type
{
    CARRAY_UCHAR  =  1,
    CARRAY_SCHAR  =  2,
    
    CARRAY_SHORT  =  3,
    CARRAY_USHORT =  4,
    
    CARRAY_INT    =  5,
    CARRAY_UINT   =  6,
    
    CARRAY_LONG   =  7,
    CARRAY_ULONG  =  8,
    
    CARRAY_FLOAT  =  9,
    CARRAY_DOUBLE = 10,

#if CARRAY_CAPI_HAVE_LONG_LONG
    CARRAY_LLONG  = 11,
    CARRAY_ULLONG = 12,
#endif
};

enum carray_attr
{
    CARRAY_DEFAULT  = 0,
    CARRAY_READONLY = 1
};

struct carray_info
{
    carray_type type;
    carray_attr attr;
    size_t      elementSize;
    size_t      elementCount;
    size_t      elementCapacity;
};

/**
 *  Carray C API.
 */
struct carray_capi
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
     * Creates new carray object which manages the underlying data.
     * 
     * elementCount - number of elements
     * data         - if not NULL, contains after the call the pointer
     *                to the uninitialized array elements. the caller
     *                is responsible for initializing the elements.
     *                If NULL, the elements are initialized with zeros.
     *
     * Returns NULL pointer on parameter error. 
     * This function may also raise a Lua error.
     */
    carray* (*newCarray)(lua_State* L, carray_type t, carray_attr attr, size_t elementCount, void** data);
    
    /** 
     * Creates new carray object with a reference to underlying data
     * managed by the caller.
     * 
     * elementCount    - number of elements
     * dataRef         - pointer to the element content. The caller guerantees that the content
     *                   memory remains valid until the release callback is called.
     * releaseCallback - is called by the carray object if the reference to the underlying
     *                   data is not any longer needed. This callback may also be NULL
     *                   for the case of static data.
     *
     * Returns NULL pointer on parameter error.
     * This function may also raise a Lua error.
     */
    carray* (*newCarrayRef)(lua_State* L, carray_type t, carray_attr attr, void* dataRef, size_t elementCount,
                            void (*releaseCallback)(void* dataRef, size_t elementCount));
    
    /**
     * Returns a valid pointer if the Lua object at the given stack
     * index is a valid readable carray, otherwise returns NULL.
     *
     * info - contains information about the carray after the call
     *        May be NULL.
     *
     * The returned carray object is be valid as long as the Lua 
     * object at the given stack index remains valid.
     * To keep the carray object beyond this call, the function 
     * retainConstCarray() should be called (see below).
     */
    const carray* (*toReadableCarray)(lua_State* L, int index, carray_info* info);
    
    /**
     * Returns a valid pointer if the Lua object at the given stack
     * index is a valid writable carray, otherwise returns NULL.
     * 
     * info - contains information about the carray after the call
     *        May be NULL.
     *
     * The returned carray object is valid as long as the Lua 
     * object at the given stack index remains valid.
     * To keep the carray object beyond this call, the function 
     * retainCarray() should be called (see below).
     */
    carray* (*toWritableCarray)(lua_State* L, int index, carray_info* info);

    /**
     * Increase the reference counter of the carray object.
     *
     * This function must be called after the function toCarray()
     * as long as the Lua object on the given stack index is
     * valid (see above).
     */
    void (*retainCarray)(const carray* a);

    /**
     * Decrease the reference counter of the carray object and
     * destructs the carray object if no reference is left.
     */
    void (*releaseCarray)(const carray* a);
    
    /**
     * Get pointer to elements.
     * offset - index of the first element, 0 <= offset < elementCount
     * count  - number of elements, 0 <= offset + count <= elementCount
     * Returns the pointer to the element in the array at the given offset.
     * The caller may only read or write at most count elements at this pointer,
     * otherwise behaviour may be undefined.
     */
    void* (*getWritableElementPtr)(carray* a, size_t offset, size_t count);

    /**
     * Get pointer to elements.
     * offset - index of the first element, 0 <= offset < elementCount
     * count  - number of elements, 0 <= offset + count <= elementCount
     * Returns the pointer to the element in the array at the given offset.
     * The caller may only read at most count elements at this pointer,
     * otherwise behaviour may be undefined.
     */
    const void* (*getReadableElementPtr)(const carray* a, size_t offset, size_t count);
    
    /**
     * Resizes the array.
     *
     * newElementCount - new number of elements. If this is larger than the current 
     *                   element count, the new elements are uninitialized.
     *
     * shrinkCapacity  - flag, if true the capacity is set to the new size.
     *
     * Returns pointer to the first element in the array. The caller may read
     * or write newElementCount bytes at this pointer.
     * Returns NULL on failure or if newElementCount == 0.
     */
    void* (*resizeCarray)(carray* a, size_t newElementCount, int shrinkCapacity);
};

#if CARRAY_CAPI_IMPLEMENT_SET_CAPI
/**
 * Sets the Carray C API into the metatable at the given index.
 * 
 * index: index of the table that is be used as metatable for objects 
 *        that are associated to the given capi.
 */
static int carray_set_capi(lua_State* L, int index, const carray_capi* capi)
{
    lua_pushlstring(L, CARRAY_CAPI_ID_STRING, strlen(CARRAY_CAPI_ID_STRING));             /* -> key */
    void** udata = lua_newuserdata(L, sizeof(void*) + strlen(CARRAY_CAPI_ID_STRING) + 1); /* -> key, value */
    *udata = (void*)capi;
    strcpy((char*)(udata + 1), CARRAY_CAPI_ID_STRING);    /* -> key, value */
    lua_rawset(L, (index < 0) ? (index - 2) : index);     /* -> */
    return 0;
}
#endif /* CARRAY_CAPI_IMPLEMENT_SET_CAPI */

#if CARRAY_CAPI_IMPLEMENT_GET_CAPI || CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI
/**
 * Gives the associated Carray C API for the object at the given stack index.
 * Returns NULL, if the object at the given stack index does not have an 
 * associated Carray C API or only has a Carray C API with incompatible version
 * number. If errorReason is not NULL it receives the error reason in this case:
 * 1 for incompatible version nummber and 2 for no associated C API at all.
 */
static const carray_capi* carray_get_capi(lua_State* L, int index, int* errorReason)
{
    if (luaL_getmetafield(L, index, CARRAY_CAPI_ID_STRING) != LUA_TNIL)      /* -> _capi */
    {
        void** udata = lua_touserdata(L, -1);                                /* -> _capi */

        if (   udata
            && (lua_rawlen(L, -1) >= sizeof(void*) + strlen(CARRAY_CAPI_ID_STRING) + 1)
            && (memcmp((char*)(udata + 1), CARRAY_CAPI_ID_STRING, 
                       strlen(CARRAY_CAPI_ID_STRING) + 1) == 0))
        {
            const carray_capi* capi = *udata;                                /* -> _capi */
            while (capi) {
                if (   capi->version_major == CARRAY_CAPI_VERSION_MAJOR
                    && capi->version_minor >= CARRAY_CAPI_VERSION_MINOR)
                {                                                            /* -> _capi */
                    lua_pop(L, 1);                                           /* -> */
                    return capi;
                }
                capi = capi->next_capi;
            }
            if (errorReason) {
                *errorReason = 1;
            }
        } else {                                                             /* -> _capi */
            if (errorReason) {
                *errorReason = 2;
            }
        }
        lua_pop(L, 1);                                                       /* -> */
    } else {                                                                 /* -> */
        if (errorReason) {
            *errorReason = 2;
        }
    }
    return NULL;
}
#endif /* CARRAY_CAPI_IMPLEMENT_GET_CAPI || CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI */

#if CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI

static const carray_capi* carray_require_capi(lua_State* L)
{
    if (luaL_loadstring(L, "return require('carray')") != 0) {   /* -> chunk */
        lua_error(L);
    }
    lua_call(L, 0, 1); /* -> carray */
    int errorReason;
    const carray_capi* capi = carray_get_capi(L, -1, &errorReason);
    if (!capi) {
        if (errorReason == 1) {
            luaL_error(L, "carray capi version mismatch");
        } else {
            luaL_error(L, "carray capi not found");
        }
    }
    lua_pop(L, 1);
    return capi;
}

#endif /* CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI */

#endif /* CARRAY_CAPI_H */
