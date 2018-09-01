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

static Mutex         global_mutex;
static AtomicCounter initStage          = 0;
static bool          initialized        = false;
static int           stateCounter       = 0;

Mutex*        mtstates_global_lock = NULL;
AtomicCounter mtstates_id_counter  = 0;

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "Internal error: %s (%s:%d)", text, MTSTATES_MODULE_NAME, line);
}*/


static int handleClosingLuaState(lua_State* L)
{
    async_mutex_lock(mtstates_global_lock);
    stateCounter -= 1;
    if (stateCounter == 0) {

    }
    async_mutex_unlock(mtstates_global_lock);
    return 0;
}


static int Mtstates_type(lua_State* L)
{
    luaL_checkany(L, 1);
    int tp = lua_type(L, 1);
    if (tp == LUA_TUSERDATA) {
        if (lua_getmetatable(L, 1)) {
            if (lua_getfield(L, -1, "__name") == LUA_TSTRING) {
                lua_pushvalue(L, -1);
                if (lua_gettable(L, LUA_REGISTRYINDEX) == LUA_TTABLE) {
                    if (lua_rawequal(L, -3, -1)) {
                        lua_pop(L, 1);
                        return 1;
                    }
                }
            }
        }
    }
    lua_pushstring(L, lua_typename(L, tp));
    return 1;
}

static const luaL_Reg ModuleFunctions[] = 
{
    { "type",    Mtstates_type },      
    { NULL,      NULL          } /* sentinel */
};

DLL_PUBLIC int luaopen_mtstates(lua_State* L)
{
    luaL_checkversion(L); /* does nothing if compiled for Lua 5.1 */

    /* ---------------------------------------- */

    if (atomic_get(&initStage) != 2) {
        if (atomic_set_if_equal(&initStage, 0, 1)) {
            async_mutex_init(&global_mutex);
            mtstates_global_lock = &global_mutex;
            atomic_set(&initStage, 2);
        } 
        else {
            while (atomic_get(&initStage) != 2) {
                Mutex waitMutex;
                async_mutex_init(&waitMutex);
                async_mutex_lock(&waitMutex);
                async_mutex_wait_millis(&waitMutex, 1);
                async_mutex_destruct(&waitMutex);
            }
        }
    }
    /* ---------------------------------------- */

    async_mutex_lock(mtstates_global_lock);
    {
        if (!initialized) {
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
            stateCounter += 1;
            
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

    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    
    lua_pushliteral(L, MTSTATES_VERSION_STRING);
    lua_setfield(L, module, "_VERSION");
    
    {
    #if defined(MTSTATES_ASYNC_USE_WIN32)
        #define MTSTATES_INFO1 "WIN32"
    #elif defined(MTSTATES_ASYNC_USE_GNU)
        #define MTSTATES_INFO1 "GNU"
    #elif defined(MTSTATES_ASYNC_USE_STDATOMIC)
        #define MTSTATES_INFO1 "STD"
    #endif
    #if defined(MTSTATES_ASYNC_USE_WINTHREAD)
        #define MTSTATES_INFO2 "WIN32"
    #elif defined(MTSTATES_ASYNC_USE_PTHREAD)
        #define MTSTATES_INFO2 "PTHREAD"
    #elif defined(MTSTATES_ASYNC_USE_STDTHREAD)
        #define MTSTATES_INFO2 "STD"
    #endif
        lua_pushstring(L, "atomic:" MTSTATES_INFO1 ",thread:" MTSTATES_INFO2);
        lua_setfield(L, module, "_INFO");
    }
    
    lua_pushvalue(L, stateClass);
    lua_setfield (L, stateMeta, "__index");

    lua_pushvalue(L, errorModule);
    lua_setfield(L, module, "error");

    lua_checkstack(L, LUA_MINSTACK);
    
    mtstates_state_init_module   (L, module,      stateMeta,    stateClass);
    mtstates_error_init_module   (L, errorModule);
    
    lua_settop(L, module);
    return 1;
}
