#include "state.h"
#include "error.h"
#include "main.h"
#include "state_intern.h"

const char* const MTSTATES_STATE_CLASS_NAME = "mtstates.state";


static AtomicCounter state_counter     = 0;
static lua_Integer   state_buckets     = 0;
static int           bucket_usage      = 0;
static StateBucket*  state_bucket_list = NULL;

typedef struct StateUserData {
    MtState*         state;
} StateUserData;

inline static void toBuckets(MtState* s, lua_Integer n, StateBucket* list)
{
    StateBucket* bucket        = &(list[s->id % n]);
    MtState**    firstStatePtr = &bucket->firstState;
    if (*firstStatePtr) {
        (*firstStatePtr)->prevStatePtr = &s->nextState;
    }
    s->nextState    = *firstStatePtr;
    s->prevStatePtr =  firstStatePtr;
    *firstStatePtr  =  s;
    bucket->count += 1;
    if (bucket->count > bucket_usage) {
        bucket_usage = bucket->count;
    }
}

static void newBuckets(lua_Integer n, StateBucket* newList)
{
    bucket_usage = 0;
    if (state_bucket_list) {
        lua_Integer i;
        for (i = 0; i < state_buckets; ++i) {
            StateBucket* b = &(state_bucket_list[i]);
            MtState*     s = b->firstState;
            while (s != NULL) {
                MtState* s2 = s->nextState;
                toBuckets(s, n, newList);
                s = s2;
            }
        }
        free(state_bucket_list);
    }
    state_buckets     = n;
    state_bucket_list = newList;
}

static MtState* findStateWithName(const char* stateName, size_t stateNameLength, bool* unique)
{
    MtState* rslt = NULL;
    if (stateName && state_bucket_list) {
        lua_Integer i;
        for (i = 0; i < state_buckets; ++i) {
            StateBucket* b = &(state_bucket_list[i]);
            MtState*     s = b->firstState;
            while (s != NULL) {
                if (   s->stateName 
                    && stateNameLength == s->stateNameLength 
                    && memcmp(s->stateName, stateName, stateNameLength) == 0
                    && atomic_get(&s->initialized))
                {
                    if (unique) {
                        *unique = (rslt == NULL);
                    }
                    if (rslt) {
                        return rslt;
                    } else {
                        rslt = s;
                    }
                }
                s = s->nextState;
            }
        }
    }
    return rslt;
}

static MtState* findStateWithId(lua_Integer stateId)
{
    if (state_bucket_list) {
        MtState* s = state_bucket_list[stateId % state_buckets].firstState;
        while (s != NULL) {
            if (s->id == stateId && atomic_get(&s->initialized)) {
                return s;
            }
            s = s->nextState;
        }
    }
    return NULL;
}


static const char* toLuaString(lua_State* L, StateUserData* udata, MtState* s)
{
    if (s) {
        if (s->stateName) {
            mtstates_util_quote_lstring(L, s->stateName, s->stateNameLength);
            const char* rslt;
            if (udata) {
                rslt = lua_pushfstring(L, "%s: %p (name=%s,id=%d)", MTSTATES_STATE_CLASS_NAME, 
                                                                    udata,
                                                                    lua_tostring(L, -1),
                                                                    (int)s->id);
            } else {
                rslt = lua_pushfstring(L, "%s(name=%s,id=%d)", MTSTATES_STATE_CLASS_NAME, 
                                                               lua_tostring(L, -1),
                                                               (int)s->id);
            }
            lua_remove(L, -2);
            return rslt;
        } else {
            if (udata) {
                return lua_pushfstring(L, "%s: %p (id=%d)", MTSTATES_STATE_CLASS_NAME, udata, (int)s->id);
            } else {
                return lua_pushfstring(L, "%s(id=%d)", MTSTATES_STATE_CLASS_NAME, (int)s->id);
            }
        }
    } else {
        return lua_pushfstring(L, "%s: invalid", MTSTATES_STATE_CLASS_NAME);
    }
}

const char* mtstates_state_tostring(lua_State* L, MtState* s)
{
    return toLuaString(L, NULL, s);
}

static const char* udataToLuaString(lua_State* L, StateUserData* udata)
{
    if (udata) {
        return toLuaString(L, udata, udata->state);
    } else {
        return lua_pushfstring(L, "%s: invalid", MTSTATES_STATE_CLASS_NAME);
    }
}

static int errormsghandler2(lua_State* L2)
{
    const char* msg = lua_tostring(L2, 1);
    if (msg == NULL) {  /* is error object not a string? */
        if (   luaL_callmeta(L2, 1, "__tostring")  /* does it have a metamethod */
            && lua_type(L2, -1) == LUA_TSTRING)  /* that produces a string? */
        {
            return 1;  /* that is the message */
        } else {
            msg = lua_pushfstring(L2, "(error object is a %s value)",
                                      luaL_typename(L2, 1));
        }
    }
    luaL_traceback(L2, L2, msg, 1);  /* append a standard traceback */
    return 1;  /* return the traceback */
}

static int dumpWriter(lua_State* L, const void* p, size_t sz, void* ud)
{
    MemBuffer* b = (MemBuffer*) ud;
    if (mtstates_membuf_reserve(b, sz) == 0) {
        memcpy(b->bufferStart + b->bufferLength, p, sz);
        b->bufferLength += sz;
        return 0;
    } else {
        return 1;
    }
}

static int pushArg(lua_State* L2, lua_State* L, int arg, lua_State* errorL)
{
    int tp = lua_type(L, arg);
    switch (tp) {
        case LUA_TNIL: {
            lua_pushnil(L2);
            break;
        }
        case LUA_TNUMBER: {
            if (lua_isinteger(L, arg)) {
                lua_pushinteger(L2, lua_tointeger(L, arg));
            } else {
                lua_pushnumber(L2, lua_tonumber(L, arg));
            }
            break;
        }
        case LUA_TBOOLEAN: {
            lua_pushboolean(L2, lua_toboolean(L, arg));
            break;
        }
        case LUA_TSTRING: {
            size_t len;
            const char* str = lua_tolstring(L, arg, &len);
            lua_pushlstring(L2, str, len);
            break;
        }
        case LUA_TLIGHTUSERDATA: {
            lua_pushlightuserdata(L2, lua_touserdata(L, arg));
            break;
        }
        default: {
            lua_pushfstring(errorL, "type '%s' not supported", lua_typename(L, tp));
            return arg;
        }
    }
    return 0;
}

static int pushArgs(lua_State* L2, lua_State* L, int firstarg, int lastarg, lua_State* errorL)
{
    int n = lastarg - firstarg + 1;
    lua_checkstack(L2, n + LUA_MINSTACK);
    int arg;
    for (arg = firstarg; arg <= lastarg; ++arg) {
        int rc = pushArg(L2, L, arg, errorL);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static const luaL_Reg mtstates_stdlibs[] =
{
#if LUA_VERSION_NUM > 501
    { LUA_COLIBNAME,   luaopen_coroutine },
#endif
    { LUA_TABLIBNAME,  luaopen_table     },
    { LUA_IOLIBNAME,   luaopen_io        },
    { LUA_OSLIBNAME,   luaopen_os        },
    { LUA_STRLIBNAME,  luaopen_string    },
    { LUA_MATHLIBNAME, luaopen_math      },
#if LUA_VERSION_NUM == 502
    { LUA_BITLIBNAME,  luaopen_bit32     },
#endif
#if LUA_VERSION_NUM > 502
    { LUA_UTF8LIBNAME, luaopen_utf8      },
#endif
    { LUA_DBLIBNAME,   luaopen_debug     },

    { NULL, NULL}
};

static int Mtstates_newState2(lua_State* L);

static int Mtstates_newState(lua_State* L)
{
    NewStateVars vars; memset(&vars, 0, sizeof(NewStateVars));
    NewStateVars* this = &vars;

    int nargs = lua_gettop(L);

    lua_pushcfunction(L, Mtstates_newState2);
    lua_insert(L, 1);
    
    lua_pushlightuserdata(L, this);
    lua_insert(L, 2);

    mtstates_membuf_init(&this->stateFunctionBuffer, 0, 2);

    int rc = lua_pcall(L, nargs + 1, LUA_MULTRET, 0);

    if (this->globalLocked) {
        async_mutex_unlock(mtstates_global_lock);
    }
    if (this->stateLocked) {
        async_mutex_unlock(&this->state->stateMutex);
    }
    if (this->L2) {
        lua_close(this->L2);
    }
    mtstates_membuf_free(&this->stateFunctionBuffer);
       
    if (rc != LUA_OK) {
        if (this->errorArg) {
            return luaL_argerror(L, this->errorArg - 1, lua_tostring(L, -1));
        } else {
            return lua_error(L);
        }
    } else {
        return this->nrslts;
    }
}

static int Mtstates_newState2(lua_State* L)
{
    int arg = 1;
    
    NewStateVars* this = (NewStateVars*)lua_touserdata(L, arg++);

    int firstArg = arg;
    int lastArg  = lua_gettop(L);
    int nargs    = lastArg - firstArg + 1;

    const char* stateName       = NULL;
    size_t      stateNameLength = 0;
    if (arg <= lastArg && nargs >= 2) {
        int t = lua_type(L, arg);
        if (t == LUA_TSTRING) {
            stateName = lua_tolstring(L, arg++, &stateNameLength);
        } else if (t == LUA_TNIL) {
            arg++;
        }
    }
    
    this->openlibs = true;
    if (arg <= lastArg && lua_type(L, arg) == LUA_TBOOLEAN) {
        this->openlibs = lua_toboolean(L, arg++);
    }
    
    const char* stateCode       = NULL;
    size_t      stateCodeLength = 0;
    if (arg <= lastArg && lua_type(L, arg) == LUA_TSTRING) {
        stateCode = lua_tolstring(L, arg++, &stateCodeLength);
    }
    
    int stateFunction = 0;
    if (stateCode == NULL) {
        luaL_checktype(L, arg, LUA_TFUNCTION);
        if (lua_iscfunction(L, arg)) {
            /* passing c function might by harmful: in the target 
             * state registry entries might not be initialized
             * and not every c function implementation will correctly
             * handle this. The user can wrap the c function in a lua
             * function and within this lua function require the 
             * desired c module which will initialize everything 
             * in the target state's registry the usual way. */
            this->errorArg = arg;
            return luaL_error(L, "lua function expected");
        }
        stateFunction = arg++;
        const char* varname = lua_getupvalue(L, stateFunction, 1);
        if (varname) {
            if (strcmp(varname, "_ENV") == 0) {
                /* _ENV -> function usese global variables */
                lua_pop(L, 1); /* pop result from first lua_getupvalue */
                varname = lua_getupvalue(L, stateFunction, 2);
            }
            if (varname) {
                this->errorArg = stateFunction;
                return luaL_error(L, "state function uses upvalue '%s'", varname);
            }
        }
    }

    
    if (stateFunction) {
        mtstates_membuf_reserve(&this->stateFunctionBuffer, 4 * 1024);
        lua_pushvalue(L, stateFunction);
        int rc = lua_dump(L, &dumpWriter, &this->stateFunctionBuffer, false);
        if (rc != 0) {
            return mtstates_ERROR_OUT_OF_MEMORY(L);
        }
        lua_pop(L, 1);
    }
    
    StateUserData* udata = lua_newuserdata(L, sizeof(StateUserData));
    memset(udata, 0, sizeof(StateUserData));
    luaL_setmetatable(L, MTSTATES_STATE_CLASS_NAME);

    async_mutex_lock(mtstates_global_lock); this->globalLocked = true;

    /* ------------------------------------------------------------------------------------ */

    MtState* s = calloc(1, sizeof(MtState));
    if (s == NULL) {
        return mtstates_ERROR_OUT_OF_MEMORY(L);
    }
    this->state = s;
    async_mutex_init(&s->stateMutex);
    
    s->id        = atomic_inc(&mtstates_id_counter);
    s->used      = 1;
    udata->state = s; /* now udata is responsible for freeing s */
    
    if (stateName) {
        s->stateName = malloc(stateNameLength + 1);
        if (s->stateName == NULL) {
            return mtstates_ERROR_OUT_OF_MEMORY(L);
        }
        memcpy(s->stateName, stateName, stateNameLength + 1);
        s->stateNameLength = stateNameLength;
    }
    if (atomic_get(&state_counter) + 1 > state_buckets * 4 || bucket_usage > 30) {
        lua_Integer n = state_buckets ? (2 * state_buckets) : 64;
        StateBucket* newList = calloc(n, sizeof(StateBucket));
        if (newList) {
            newBuckets(n, newList);
        } else if (!state_buckets) {
            return mtstates_ERROR_OUT_OF_MEMORY(L);
        }
    }
    toBuckets(s, state_buckets, state_bucket_list);
    atomic_inc(&state_counter);
    
    async_mutex_lock(&s->stateMutex);         this->stateLocked  = true;
    async_mutex_unlock(mtstates_global_lock); this->globalLocked = false;

    /* ------------------------------------------------------------------------------------ */
    
    lua_State* L2 = luaL_newstate();
    int l2top = 0;
    if (L2 == NULL) {
        return mtstates_ERROR_OUT_OF_MEMORY(L);
    }
    this->L2 = L2;
    if (this->openlibs) {
        luaL_openlibs(L2);
    } else {
        luaL_requiref(L2, "_G", luaopen_base, true);
        lua_pop(L2, 1);
        luaL_requiref(L2, LUA_LOADLIBNAME, luaopen_package, true);
            lua_getfield(L2, -1, "preload");
                const luaL_Reg* r;
                for (r = mtstates_stdlibs; r->func; ++r) {
                    lua_pushcfunction(L2, r->func);
                    lua_setfield(L2, -2, r->name);
                }
        lua_pop(L2, 2);
    }
    
    lua_pushcfunction(L2, errormsghandler2);
    int msghandler = ++l2top;
    
    int rc;
    if (stateFunction != 0) {
        rc = luaL_loadbuffer(L2, this->stateFunctionBuffer.bufferStart, 
                                 this->stateFunctionBuffer.bufferLength, NULL);
    } else {   
        rc = luaL_loadbuffer(L2, stateCode, stateCodeLength, stateCode);
    }
    if (rc != LUA_OK) {
        size_t errorMessageLength;
        const char* errorMessage = luaL_tolstring(L2, -1, &errorMessageLength);
        lua_pushlstring(L, errorMessage, errorMessageLength);
        return lua_error(L);
    }
    int func = ++l2top;
    rc = pushArgs(L2, L, arg, lastArg, L);
    if (rc != 0) {
        this->errorArg = rc;
        return lua_error(L); /* error has been pushed by pushArgs */
    }
    rc = lua_pcall(L2, lastArg - arg + 1, LUA_MULTRET, msghandler);
    if (rc != LUA_OK) {
        size_t errorMessageLength;
        const char* errorString = luaL_tolstring(L2, -1, &errorMessageLength);
        lua_pushlstring(L, errorString, errorMessageLength);
        return mtstates_ERROR_INVOKING_STATE(L, NULL, lua_tostring(L, -1));
    }
    int firstrslt = func;
    int lastrslt  = lua_gettop(L2);
    int nrslts = lastrslt - firstrslt + 1;
    if (nrslts >= 2) {
        rc = pushArgs(L, L2, firstrslt + 1, lastrslt, L);
        if (rc != 0) {
            lua_pushfstring(L, "state function returns bad parameter #%d: %s", rc - firstrslt + 1, lua_tostring(L, -1));
            return mtstates_ERROR_STATE_RESULT(L, NULL, lua_tostring(L, -1));
        }
    }
    if (nrslts < 1 || lua_type(L2, firstrslt) != LUA_TFUNCTION) {
        const char* t = (nrslts < 1) ? "nothing" : lua_typename(L2, lua_type(L2, firstrslt));
        lua_pushfstring(L, "state setup returns %s but a function is required as first result parameter", t);
        return mtstates_ERROR_STATE_RESULT(L, NULL, lua_tostring(L, -1));
    }
    s->L2 = L2; this->L2 = NULL;
    lua_pushvalue(L2, firstrslt);
    s->callbackref = luaL_ref(L2, LUA_REGISTRYINDEX);
    
    /* ------------------------------------------------------------------------------------ */
    atomic_set(&s->initialized, true);
    
    this->nrslts = nrslts - 1 + 1;
    return this->nrslts;
}

static int Mtstates_state(lua_State* L)
{
    int arg = 1;

    bool hasArg = false;
    const char* stateName       = NULL;
    size_t      stateNameLength = 0;
    lua_Integer stateId         = 0;

    if (lua_gettop(L) >= arg) {
        if (lua_type(L, arg) == LUA_TSTRING) {
            stateName = lua_tolstring(L, arg++, &stateNameLength);
            hasArg = true;
        }
        else if (lua_type(L, arg) == LUA_TNUMBER && lua_isinteger(L, arg)) {
            stateId = lua_tointeger(L, arg++);
            hasArg = true;
        }
    }
    if (!hasArg) {
        return luaL_argerror(L, arg, "state name or id expected");
    }

    StateUserData* userData = lua_newuserdata(L, sizeof(StateUserData));
    memset(userData, 0, sizeof(StateUserData));
    luaL_setmetatable(L, MTSTATES_STATE_CLASS_NAME);

    /* Lock */
    
    async_mutex_lock(mtstates_global_lock);

    MtState* state;
    if (stateName != NULL) {
        bool unique;
        state = findStateWithName(stateName, stateNameLength, &unique);
        if (!state) {
            async_mutex_unlock(mtstates_global_lock);
            return mtstates_ERROR_UNKNOWN_OBJECT_state_name(L, stateName, stateNameLength);
        } else if (!unique) {
            async_mutex_unlock(mtstates_global_lock);
            return mtstates_ERROR_AMBIGUOUS_NAME_state_name(L, stateName, stateNameLength);
        }
    } else {
        state = findStateWithId(stateId);
        if (!state) {
            async_mutex_unlock(mtstates_global_lock);
            return mtstates_ERROR_UNKNOWN_OBJECT_state_id(L, stateId);
        }
    }

    userData->state = state;
    atomic_inc(&state->used);
    
    async_mutex_unlock(mtstates_global_lock);
    return 1;
}


static void MtState_free(MtState* s)
{
    bool wasInBucket = (s->prevStatePtr != NULL);
    
    if (wasInBucket) {
        *s->prevStatePtr = s->nextState;
    }
    if (s->nextState) {
        s->nextState->prevStatePtr = s->prevStatePtr;
    }
    
    if (s->L2) {
        lua_close(s->L2);
    }
    if (s->stateName) {
        free(s->stateName);
    }
    async_mutex_destruct(&s->stateMutex);
    free(s);
    
    if (wasInBucket) {
        int c = atomic_dec(&state_counter);
        if (c == 0) {
            if (state_bucket_list)  {
                free(state_bucket_list);
            }
            state_buckets     = 0;
            state_bucket_list = NULL;
            bucket_usage      = 0;
        }
        else if (c * 10 < state_buckets) {
            lua_Integer n = 2 * c;
            if (n > 64) {
                StateBucket* newList = calloc(n, sizeof(StateBucket));
                if (newList) {
                    newBuckets(n, newList);
                }
            }
        }
    }
}

static int MtState_release(lua_State* L)
{
    StateUserData* udata = luaL_checkudata(L, 1, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;

    if (s) {
        async_mutex_lock(mtstates_global_lock);

        if (atomic_dec(&s->used) == 0) {
            MtState_free(s);
        }
        udata->state = NULL;
        
        async_mutex_unlock(mtstates_global_lock);
    }
    return 0;
}

static void interruptHook1(lua_State* L2, lua_Debug* ar) 
{
  (void)ar;  /* unused arg. */
  lua_sethook(L2, NULL, 0, 0);  /* reset hook */
  mtstates_ERROR_INTERRUPTED(L2);
}

static void interruptHook2(lua_State* L2, lua_Debug* ar) 
{
  (void)ar;  /* unused arg. */
  mtstates_ERROR_INTERRUPTED(L2);
}

static int MtState_interrupt(lua_State* L)
{   
    int arg = 1;
    StateUserData* udata = luaL_checkudata(L, arg++, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;

    lua_Hook hook = interruptHook1;
    if (!lua_isnoneornil(L, arg)) {
        if (!lua_isboolean(L, arg)) {
            return luaL_argerror(L, arg, "boolean expected");
        }
        hook = lua_toboolean(L, arg) ? interruptHook2 : NULL;
    }
    if (hook) {
        lua_sethook(s->L2, hook, LUA_MASKCALL|LUA_MASKRET|LUA_MASKCOUNT, 1);
    } else {
        lua_sethook(s->L2, NULL, 0, 0);  /* reset hook */
    }
    return 0;
}

static int MtState_close(lua_State* L)
{
    StateUserData* udata = luaL_checkudata(L, 1, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;

    async_mutex_lock(&s->stateMutex);
    
    if (s->isBusy) {
        async_mutex_unlock(&s->stateMutex);
        return mtstates_ERROR_CONCURRENT_ACCESS(L, mtstates_state_tostring(L, s));
    }
    
    if (s->L2) {
        lua_close(s->L2);
        s->L2 = NULL;
    }
    async_mutex_unlock(&s->stateMutex);
    return 0;
}

static int MtState_call2(lua_State* L, bool isTimed)
{
    int lastarg = lua_gettop(L);
    int arg     = 1;
    StateUserData* udata = luaL_checkudata(L, arg++, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;

    lua_Number endTime;
    lua_Number waitSeconds;
    
    if (isTimed) {
        waitSeconds = luaL_checknumber(L, arg++);
        endTime     = mtstates_current_time_seconds() + waitSeconds;
    }

    if (!isTimed || waitSeconds > 0) {
        async_mutex_lock(&s->stateMutex);
    } else if (!async_mutex_trylock(&s->stateMutex)) {
        lua_pushboolean(L, false);
        return 1;
    }

    if (s->L2 == NULL) {
        async_mutex_unlock(&s->stateMutex);
        return mtstates_ERROR_OBJECT_CLOSED(L, mtstates_state_tostring(L, s));
    }
    while (s->isBusy) {
        if (isTimed) {
            lua_Number now = mtstates_current_time_seconds();
            if (now < endTime) {
                async_mutex_wait_millis(&s->stateMutex, (int)((endTime - now) * 1000 + 0.5));
            } else {
                async_mutex_unlock(&s->stateMutex);
                lua_pushboolean(L, false);
                return 1;
            }
        } else {
            async_mutex_wait(&s->stateMutex);
        }
    }
    s->isBusy = true;
    async_mutex_unlock(&s->stateMutex);
    
    lua_State* L2 = s->L2;
    int l2start = lua_gettop(L2);
    int l2top = l2start;
    lua_pushcfunction(L2, errormsghandler2);
    int msghandler = ++l2top;
    lua_rawgeti(L2, LUA_REGISTRYINDEX, s->callbackref);
    int func = ++l2top;
    int rc = pushArgs(L2, L, arg, lastarg, L);
    if (rc != 0) {
        lua_settop(L2, l2start);
     
        async_mutex_lock(&s->stateMutex);
        s->isBusy = false;
        async_mutex_notify(&s->stateMutex);
        async_mutex_unlock(&s->stateMutex);
     
        return luaL_argerror(L, rc, lua_tostring(L, -1));
    }
    rc = lua_pcall(L2, lastarg - arg + 1, LUA_MULTRET, msghandler);
    if (rc != LUA_OK) {
        int ltop = lua_gettop(L);
        size_t errorMessageLength;
        const char* errorString = luaL_tolstring(L2, -1, &errorMessageLength);
        lua_pushlstring(L, errorString, errorMessageLength); 
        int details = ++ltop;
        lua_settop(L2, l2start);

        async_mutex_lock(&s->stateMutex);
        s->isBusy = false;
        async_mutex_notify(&s->stateMutex);
        async_mutex_unlock(&s->stateMutex);

        const char* stateString = mtstates_state_tostring(L, s);
        return mtstates_ERROR_INVOKING_STATE(L, stateString, lua_tostring(L, details));
    }
    int firstrslt = func;
    int lastrslt  = lua_gettop(L2);
    int nrslts = lastrslt - firstrslt + 1;
    if (isTimed) {
        nrslts += 1;
        lua_pushboolean(L, true);
    }
    rc = pushArgs(L, L2, firstrslt, lastrslt, L);
    if (rc != 0) {
        lua_settop(L2, l2start);

        async_mutex_lock(&s->stateMutex);
        s->isBusy = false;
        async_mutex_notify(&s->stateMutex);
        async_mutex_unlock(&s->stateMutex);

        lua_pushfstring(L, "state function returns bad parameter #%d: %s", rc - firstrslt + 1, lua_tostring(L, -1));
        return mtstates_ERROR_STATE_RESULT(L, NULL, lua_tostring(L, -1));
    }
    lua_settop(L2, l2start);

    async_mutex_lock(&s->stateMutex);
    s->isBusy = false;
    async_mutex_notify(&s->stateMutex);
    async_mutex_unlock(&s->stateMutex);

    return nrslts;
}

static int MtState_call(lua_State* L)
{
    return MtState_call2(L, false);
}

static int MtState_tcall(lua_State* L)
{
    return MtState_call2(L, true);
}


static int MtState_toString(lua_State* L)
{
    StateUserData* udata = luaL_checkudata(L, 1, MTSTATES_STATE_CLASS_NAME);
    
    udataToLuaString(L, udata);
    return 1;
}


static int MtState_id(lua_State* L)
{
    int arg = 1;
    StateUserData* udata = luaL_checkudata(L, arg++, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;
    lua_pushinteger(L, s->id);
    return 1;
}

static int MtState_name(lua_State* L)
{
    int arg = 1;
    StateUserData* udata = luaL_checkudata(L, arg++, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;
    if (s->stateName) {
        lua_pushlstring(L, s->stateName, s->stateNameLength);
        return 1;
    } else {
        return 0;
    }
}


static const luaL_Reg StateMethods[] = 
{
    { "id",         MtState_id         },
    { "name",       MtState_name       },
    { "call",       MtState_call       },
    { "tcall",      MtState_tcall      },
    { "interrupt",  MtState_interrupt  },
    { "close",      MtState_close      },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg StateMetaMethods[] = 
{
    { "__tostring", MtState_toString },
    { "__gc",       MtState_release  },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newstate",  Mtstates_newState  },
    { "state",     Mtstates_state  },
    { NULL,        NULL } /* sentinel */
};

int mtstates_state_init_module(lua_State* L, int module, int stateMeta, int stateClass)
{
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);

        lua_pushvalue(L, stateMeta);
            luaL_setfuncs(L, StateMetaMethods, 0);
    
            lua_pushvalue(L, stateClass);
                luaL_setfuncs(L, StateMethods, 0);
    
    lua_pop(L, 3);

    lua_pushstring(L, MTSTATES_STATE_CLASS_NAME);
    lua_setfield(L, stateMeta, "__metatable");
    
    return 0;
}

