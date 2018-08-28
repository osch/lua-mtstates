#include "main.h"
#include "state.h"
#include "error.h"

#ifndef MTSTATES_VERSION
    #error MTSTATES_VERSION is not defined
#endif 

#define MTSTATES_STRINGIFY(x) #x
#define MTSTATES_TOSTRING(x) MTSTATES_STRINGIFY(x)
#define MTSTATES_VERSION_STRING MTSTATES_TOSTRING(MTSTATES_VERSION)

const char* const MTSTATES_MODULE_NAME = "mtstates";

static AtomicPtr atomic_lock_holder = 0;
static bool      initialized        = false;
static int       initCounter        = 0;

Mutex*        mtstates_global_lock = NULL;
AtomicCounter mtstates_id_counter  = 0;

static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "Internal error: %s (%s:%d)", text, MTSTATES_MODULE_NAME, line);
}



static const luaL_Reg ModuleFunctions[] = 
{
    { NULL,            NULL } /* sentinel */
};

static int handleClosingLuaState(lua_State* L)
{
    async_mutex_lock(mtstates_global_lock);
    initCounter -= 1;
    if (initCounter == 0) {

    }
    async_mutex_unlock(mtstates_global_lock);
    return 0;
}


DLL_PUBLIC int luaopen_mtstates(lua_State* L)
{
#if LUA_VERSION_NUM >= 502
    int luaVersion = (int)*lua_version(L);
    if (luaVersion != LUA_VERSION_NUM) {
        return luaL_error(L, "lua version mismatch: mtstates was compiled for %d, but current version is %d", LUA_VERSION_NUM, luaVersion);
    }
#endif

    /* ---------------------------------------- */

    if (atomic_get_ptr(&atomic_lock_holder) == NULL) {
        Mutex* newLock = malloc(sizeof(Mutex));
        if (!newLock) {
            return internalError(L, "initialize lock failed", __LINE__);
        }
        async_mutex_init(newLock);

        if (!atomic_set_ptr_if_equal(&atomic_lock_holder, NULL, newLock)) {
            async_mutex_destruct(newLock);
            free(newLock);
        }
    }
    /* ---------------------------------------- */

    async_mutex_lock(atomic_get_ptr(&atomic_lock_holder));
    {
        if (!initialized) {
            mtstates_global_lock = atomic_get_ptr(&atomic_lock_holder);
            /* create initial id that could not accidently be mistaken with "normal" integers */
            const char* ptr = MTSTATES_MODULE_NAME;
            AtomicCounter c = 0;
            if (sizeof(AtomicCounter) - 1 >= 1) {
                int i;
                for (i = 0; i < 2 * sizeof(char*); ++i) {
                    c ^= ((int)(((char*)&ptr)[(i + 1) % sizeof(char*)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
                lua_Number t = mtstates_current_time_seconds();
                for (i = 0; i < 2 * sizeof(lua_Number); ++i) {
                    c ^= ((int)(((char*)&t)[(i + 1) % sizeof(lua_Number)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
            }
            mtstates_id_counter = c;
            initialized = true;
        }


        /* check if initialization has been done for this lua state */
        lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
            lua_rawget(L, LUA_REGISTRYINDEX); 
            bool alreadyInitializedForThisLua = !lua_isnil(L, -1);
        lua_pop(L, 1);
        
        if (!alreadyInitializedForThisLua) 
        {
            initCounter += 1;
            
            lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
                lua_newuserdata(L, 1); /* sentinel for closing lua state */
                    lua_newtable(L); /* metatable for sentinel */
                        lua_pushstring(L, "__gc");
                            lua_pushcfunction(L, handleClosingLuaState);
                        lua_rawset(L, -3); /* metatable.__gc = handleClosingLuaState */
                    lua_setmetatable(L, -2); /* sets metatable for sentinal table */
            lua_rawset(L, LUA_REGISTRYINDEX); /* sets sentinel as value for unique void* in registry */
        }
    }
    async_mutex_unlock(mtstates_global_lock);

    /* ---------------------------------------- */
    
    int n = lua_gettop(L);
    
    int module      = ++n; lua_newtable(L);
    int errorModule = ++n; lua_newtable(L);

    int stateMeta = ++n; luaL_newmetatable(L, MTSTATES_STATE_CLASS_NAME);
    int stateClass= ++n; lua_newtable(L);

    int errorMeta = ++n; luaL_newmetatable(L, MTSTATES_ERROR_CLASS_NAME);
    int errorClass= ++n; lua_newtable(L);


    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    
    lua_pushliteral(L, MTSTATES_VERSION_STRING);
    lua_setfield(L, module, "_VERSION");
    
    lua_pushvalue(L, stateClass);
    lua_setfield (L, stateMeta, "__index");

    lua_pushvalue(L, errorModule);
    lua_setfield(L, module, "error");

    lua_pushvalue(L, errorClass);
    lua_setfield (L, errorMeta, "__index");


    lua_checkstack(L, LUA_MINSTACK);
    
    mtstates_state_init_module   (L, module,      stateMeta,    stateClass);
    mtstates_error_init_module   (L, errorModule, errorMeta,    errorClass);
    
    lua_settop(L, module);
    return 1;
}
