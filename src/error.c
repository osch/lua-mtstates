#include "error.h"

const char* const MTSTATES_ERROR_CLASS_NAME = "mtstates.error";

static const char* const MTSTATES_ERROR_CONCURRENT_ACCESS = "concurrent_access";
static const char* const MTSTATES_ERROR_OBJECT_EXISTS     = "object_exists";
static const char* const MTSTATES_ERROR_OBJECT_CLOSED     = "object_closed";
static const char* const MTSTATES_ERROR_UNKNOWN_OBJECT    = "unknown_object";
static const char* const MTSTATES_ERROR_INVOKING_STATE    = "invoking_state";
static const char* const MTSTATES_ERROR_INTERRUPTED       = "interrupted";
static const char* const MTSTATES_ERROR_STATE_RESULT      = "state_result";
static const char* const MTSTATES_ERROR_OUT_OF_MEMORY     = "out_of_memory";


typedef struct Error {
    const char* name;
    int         details;
    int         traceback;
} Error;


/* pushses Metatable onto stack */
static void pushErrorMetatable(lua_State* L) 
{
    if (luaL_newmetatable(L, MTSTATES_ERROR_CLASS_NAME))  {
        /* metatable is new and must be initialized */
        mtstates_error_init_meta(L);
    }
    /* else metatable does already exist and is pushed */
}


/* pops message from stack */
static void pushErrorClass(lua_State* L, const char* name)
{
    Error* e = lua_newuserdata(L, sizeof(Error));
    e->name       = name;
    e->details    = LUA_NOREF;
    e->traceback  = LUA_NOREF;
    pushErrorMetatable(L);
    lua_setmetatable(L, -2);
}

static void pushErrorMessage_traceback(lua_State* L, const char* name, int details, int level, const char* traceback)
{
    int top = lua_gettop(L);
    
    Error* e = lua_newuserdata(L, sizeof(Error)); ++top;
    e->name  = name;

    if (details != 0) {
        lua_pushvalue(L, details);
        e->details = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        e->details = LUA_NOREF;
    }

    luaL_traceback(L, L, traceback, level);
    e->traceback = luaL_ref(L, LUA_REGISTRYINDEX);
    pushErrorMetatable(L);
    lua_setmetatable(L, -2);
}

static void pushErrorMessageLevel(lua_State* L, const char* name, int details, int level)
{
    pushErrorMessage_traceback(L, name, details, level, NULL);
}

static void pushErrorMessage(lua_State* L, const char* name, int details)
{
    pushErrorMessageLevel(L, name, details, 0);
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

int mtstates_ERROR_OBJECT_EXISTS(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTSTATES_ERROR_OBJECT_EXISTS);
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

bool mtstates_is_ERROR_INTERRUPTED(Error* e)
{
    return e && e->name && (strcmp(e->name, MTSTATES_ERROR_INTERRUPTED) == 0);
}


int mtstates_ERROR_INTERRUPTED(lua_State* L)
{
    return throwError(L, MTSTATES_ERROR_INTERRUPTED);
}

int mtstates_ERROR_INTERRUPTED_traceback(lua_State* L, const char* stateString, const char* errorDetails, const char* traceback)
{
    int messageDetails = 0;
    if (stateString != NULL) {
        if (errorDetails != NULL) {
            lua_pushfstring(L, "%s: %s", stateString, errorDetails);
        } else {
            lua_pushfstring(L, "%s", stateString);
        }
        messageDetails = lua_gettop(L);
    } else if (errorDetails != NULL) {
        lua_pushstring(L, errorDetails);
        messageDetails = lua_gettop(L);
    }
    pushErrorMessage_traceback(L, MTSTATES_ERROR_INTERRUPTED, messageDetails, 0, traceback);
    if (messageDetails) {
        lua_remove(L, messageDetails);
    }
    return lua_error(L);
}


void mtstates_push_ERROR(lua_State* L, const char* errorDetails)
{
    lua_pushstring(L, errorDetails);
    int messageDetails = lua_gettop(L);
    pushErrorMessageLevel(L, NULL, messageDetails, 1);
    lua_remove(L, messageDetails);
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

int mtstates_ERROR_INVOKING_STATE_traceback(lua_State* L, const char* stateString, const char* errorDetails, const char* traceback)
{
    if (stateString != NULL) {
        lua_pushfstring(L, "%s: %s", stateString, errorDetails);
    } else {
        lua_pushstring(L, errorDetails);
    }
    int messageDetails = lua_gettop(L);
    pushErrorMessage_traceback(L, MTSTATES_ERROR_INVOKING_STATE, messageDetails, 0, traceback);
    lua_remove(L, messageDetails);
    return lua_error(L);
}

int mtstates_ERROR_STATE_RESULT(lua_State* L, const char* stateString, const char* errorDetails)
{
    if (stateString != NULL) {
        lua_pushfstring(L, "%s: %s", stateString, errorDetails);
    } else {
        lua_pushstring(L, errorDetails);
    }
    return throwErrorMessage(L, MTSTATES_ERROR_STATE_RESULT);
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


const char* mtstates_error_details(lua_State* L, Error* e)
{
    if (e->details != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->details);
        return lua_tostring(L, -1);
    } else {
        return NULL;
    }
}

const char* mtstates_error_name_and_details(lua_State* L, Error* e)
{
    if (e->details != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->details);
        if (e->name) {
            lua_pushfstring(L, "%s.%s: %s", MTSTATES_ERROR_CLASS_NAME, 
                                            e->name, 
                                            lua_tostring(L, -1));
        }
        return lua_tostring(L, -1);
    } else {
        lua_pushfstring(L, "%s.%s", MTSTATES_ERROR_CLASS_NAME, e->name);
        return lua_tostring(L, -1);
    }
}


const char* mtstates_error_traceback(lua_State* L, Error* e)
{
    if (e->traceback != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->traceback);
        return lua_tostring(L, -1);
    } else {
        return NULL;
    }
}


static int Error_name(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTSTATES_ERROR_CLASS_NAME);
    
    lua_pushfstring(L, "%s.%s", MTSTATES_ERROR_CLASS_NAME, e->name);
    return 1;
}

static int Error_details(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTSTATES_ERROR_CLASS_NAME);
    
    if (e->details != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->details);
        return 1;
    } else {
        return 0;
    }
}

static int Error_traceback(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTSTATES_ERROR_CLASS_NAME);
    
    if (e->traceback != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->traceback);
        return 1;
    } else {
        return 0;
    }
}

static int Error_message(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTSTATES_ERROR_CLASS_NAME);
    
    if (e->details != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->details);
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->traceback);
        lua_pushfstring(L, "%s.%s: %s\n%s", MTSTATES_ERROR_CLASS_NAME, 
                                            e->name, 
                                            lua_tostring(L, -2),
                                            lua_tostring(L, -1));
        lua_remove(L, -2);
        lua_remove(L, -2);
    }
    else if (e->traceback != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->traceback);
        lua_pushfstring(L, "%s.%s\n%s", MTSTATES_ERROR_CLASS_NAME, 
                                        e->name, 
                                        lua_tostring(L, -1));
        lua_remove(L, -2);
    }
    else {
        lua_pushfstring(L, "%s.%s", MTSTATES_ERROR_CLASS_NAME, 
                                    e->name);
    }
    return 1;
}

static int Error_equals(lua_State* L)
{
    if (lua_gettop(L) < 2 || (!lua_isuserdata(L, 1) && !lua_isuserdata(L, 2))) {
        return luaL_error(L, "bad arguments");
    }
    Error* e1 = luaL_testudata(L, 1, MTSTATES_ERROR_CLASS_NAME);
    Error* e2 = luaL_testudata(L, 2, MTSTATES_ERROR_CLASS_NAME);
    if (e1 == NULL && e2 == NULL) {
        return luaL_error(L, "bad arguments");
    }
    if (e1 != NULL && e2 != NULL) {
        lua_pushboolean(L, e1->name == e2->name);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

static int Error_release(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTSTATES_ERROR_CLASS_NAME);
    luaL_unref(L, LUA_REGISTRYINDEX, e->details);
    luaL_unref(L, LUA_REGISTRYINDEX, e->traceback);
    return 0;
}

static const luaL_Reg ErrorMethods[] = 
{
    { "name",      Error_name      },
    { "details",   Error_details   },
    { "traceback", Error_traceback },
    { "message",   Error_message   },

    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ErrorMetaMethods[] = 
{
    { "__tostring", Error_message  },
    { "__eq",       Error_equals   },
    { "__gc",       Error_release  },

    { NULL,       NULL } /* sentinel */
};

static void publishError(lua_State* L, int module, const char* errorName)
{
    pushErrorClass(L, errorName);
    lua_setfield(L, module, errorName);
}

void mtstates_error_init_meta(lua_State* L)
{
    luaL_newmetatable(L, MTSTATES_ERROR_CLASS_NAME);
        lua_pushstring(L, MTSTATES_ERROR_CLASS_NAME);
        lua_setfield(L, -2, "__metatable");
        luaL_setfuncs(L, ErrorMetaMethods, 0);
        
        lua_newtable(L);  /* errorClass */
            luaL_setfuncs(L, ErrorMethods, 0);
        lua_setfield (L, -2, "__index");
    lua_pop(L, 1);
}

int mtstates_error_init_module(lua_State* L, int errorModule)
{
    mtstates_error_init_meta(L);

    publishError(L, errorModule, MTSTATES_ERROR_CONCURRENT_ACCESS);    
    publishError(L, errorModule, MTSTATES_ERROR_OBJECT_EXISTS);
    publishError(L, errorModule, MTSTATES_ERROR_OBJECT_CLOSED);
    publishError(L, errorModule, MTSTATES_ERROR_UNKNOWN_OBJECT);
    publishError(L, errorModule, MTSTATES_ERROR_INTERRUPTED);
    publishError(L, errorModule, MTSTATES_ERROR_INVOKING_STATE);
    publishError(L, errorModule, MTSTATES_ERROR_STATE_RESULT);
    publishError(L, errorModule, MTSTATES_ERROR_OUT_OF_MEMORY);
    
    return 0;
}

