#include "error.h"

static const char* const MTSTATES_ERROR_CLASS_NAME = "mtstates.error";

static const char* const MTSTATES_ERROR_AMBIGUOUS_NAME    = "ambiguous_name";
static const char* const MTSTATES_ERROR_CONCURRENT_ACCESS = "concurrent_access";
static const char* const MTSTATES_ERROR_OBJECT_CLOSED     = "object_closed";
static const char* const MTSTATES_ERROR_UNKNOWN_OBJECT    = "unknown_object";
static const char* const MTSTATES_ERROR_INVOKING_STATE    = "invoking_state";
static const char* const MTSTATES_ERROR_INTERRUPTED       = "interrupted";
static const char* const MTSTATES_ERROR_STATE_RESULT      = "state_result";
static const char* const MTSTATES_ERROR_OUT_OF_MEMORY     = "out_of_memory";


static void pushErrorMessage(lua_State* L, const char* name, int details)
{
    if (details != 0) {
        lua_pushfstring(L, "%s.%s: %s", MTSTATES_ERROR_CLASS_NAME, 
                                        name, 
                                        lua_tostring(L, details));
    } else {
        lua_pushfstring(L, "%s.%s", MTSTATES_ERROR_CLASS_NAME, 
                                    name);
    }
}

/* error message details must be on top of stack */
static int throwErrorMessage(lua_State* L, const char* errorName)
{
    int messageDetails = lua_gettop(L);
    pushErrorMessage(L, errorName, messageDetails);
    lua_remove(L, messageDetails);
    return lua_error(L);
}

static int throwError(lua_State* L, const char* errorName)
{
    pushErrorMessage(L, errorName, 0);
    return lua_error(L);
}

int mtstates_ERROR_CONCURRENT_ACCESS(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTSTATES_ERROR_CONCURRENT_ACCESS);
}

int mtstates_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTSTATES_ERROR_OBJECT_CLOSED);
}

int mtstates_ERROR_UNKNOWN_OBJECT_state_name(lua_State* L, const char* stateName, size_t nameLength)
{
    mtstates_util_quote_lstring(L, stateName, nameLength);
    lua_pushfstring(L, "state name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, MTSTATES_ERROR_UNKNOWN_OBJECT);
}
int mtstates_ERROR_UNKNOWN_OBJECT_state_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, "state id %d", (int)id);
    return throwErrorMessage(L, MTSTATES_ERROR_UNKNOWN_OBJECT);
}
int mtstates_ERROR_AMBIGUOUS_NAME_state_name(lua_State* L, const char* stateName, size_t nameLength)
{
    mtstates_util_quote_lstring(L, stateName, nameLength);
    lua_pushfstring(L, "state name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, MTSTATES_ERROR_AMBIGUOUS_NAME);
}

int mtstates_ERROR_INTERRUPTED(lua_State* L)
{
    return throwError(L, MTSTATES_ERROR_INTERRUPTED);
}


int mtstates_ERROR_INVOKING_STATE(lua_State* L, const char* stateString, const char* errorDetails)
{
    if (stateString != NULL) {
        lua_pushfstring(L, "%s: %s", stateString, errorDetails);
    } else {
        lua_pushstring(L, errorDetails);
    }
    return throwErrorMessage(L, MTSTATES_ERROR_INVOKING_STATE);
}

void mtstates_push_ERROR_STATE_RESULT(lua_State* L, const char* stateString, const char* errorDetails)
{
    if (stateString != NULL) {
        lua_pushfstring(L, "%s: %s", stateString, errorDetails);
    } else {
        lua_pushstring(L, errorDetails);
    }
    int messageDetails = lua_gettop(L);
    pushErrorMessage(L, MTSTATES_ERROR_STATE_RESULT, messageDetails);
    lua_remove(L, messageDetails);
}


int mtstates_ERROR_STATE_RESULT(lua_State* L, const char* stateString, const char* errorDetails)
{
    mtstates_push_ERROR_STATE_RESULT(L, stateString, errorDetails);
    return lua_error(L);
}

int mtstates_ERROR_OUT_OF_MEMORY(lua_State* L)
{
    return throwError(L, MTSTATES_ERROR_OUT_OF_MEMORY);
}

int mtstates_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, "failed to allocate %I bytes", (lua_Integer)bytes);
#else
    lua_pushfstring(L, "failed to allocate %f bytes", (lua_Number)bytes);
#endif
    return throwErrorMessage(L, MTSTATES_ERROR_OUT_OF_MEMORY);
}


static void publishError(lua_State* L, int module, const char* errorName)
{
    lua_pushfstring(L, "%s.%s", MTSTATES_ERROR_CLASS_NAME, errorName);
    lua_setfield(L, module, errorName);
}

int mtstates_error_init_module(lua_State* L, int errorModule)
{
    publishError(L, errorModule, MTSTATES_ERROR_AMBIGUOUS_NAME);
    publishError(L, errorModule, MTSTATES_ERROR_CONCURRENT_ACCESS);    
    publishError(L, errorModule, MTSTATES_ERROR_OBJECT_CLOSED);
    publishError(L, errorModule, MTSTATES_ERROR_UNKNOWN_OBJECT);
    publishError(L, errorModule, MTSTATES_ERROR_INTERRUPTED);
    publishError(L, errorModule, MTSTATES_ERROR_INVOKING_STATE);
    publishError(L, errorModule, MTSTATES_ERROR_STATE_RESULT);
    publishError(L, errorModule, MTSTATES_ERROR_OUT_OF_MEMORY);
    
    return 0;
}

