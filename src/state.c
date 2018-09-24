#include "state.h"
#include "error.h"
#include "main.h"
#include "state_intern.h"

static const char* const MTSTATES_STATE_CLASS_NAME = "mtstates.state";


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
    if (!lua_checkstack(L2, n + LUA_MINSTACK)) {
        return -1;
    }
    int arg;
    for (arg = firstarg; arg <= lastarg; ++arg) {
        int rc = pushArg(L2, L, arg, errorL);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

static void setupStateMeta(lua_State* L);

static int pushStateMeta(lua_State* L)
{
    if (luaL_newmetatable(L, MTSTATES_STATE_CLASS_NAME)) {
        setupStateMeta(L);
    }
    return 1;
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


static int Mtstates_newState1(lua_State* L, bool isNewState);
static int Mtstates_newState(lua_State* L)
{
    return Mtstates_newState1(L, true);
}

static int Mtstates_newState2(lua_State* L);
static int Mtstates_newState1(lua_State* L, bool isNewState)
{
    NewStateVars vars; memset(&vars, 0, sizeof(NewStateVars));
    NewStateVars* this = &vars;
    this->isNewState = isNewState;

    int nargs = lua_gettop(L);

    lua_pushcfunction(L, Mtstates_newState2);
    lua_insert(L, 1);
    
    lua_pushlightuserdata(L, this);
    lua_insert(L, 2);

    mtstates_membuf_init(&this->tmp, 0, 2);

    int rc = lua_pcall(L, nargs + 1, LUA_MULTRET, 0);
    {
        if (this->globalLocked) {
            async_mutex_unlock(mtstates_global_lock);
        }
        if (this->stateLocked) {
            async_mutex_unlock(&this->state->stateMutex);
        }
        if (this->L2) {
            lua_close(this->L2);
        }
        mtstates_membuf_free(&this->tmp);
    }
    luaL_checkstack(L, LUA_MINSTACK, NULL);
    
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

static int Mtstates_newState3(lua_State* L);

static int Mtstates_newState2(lua_State* L)
{
    NewStateVars* this = (NewStateVars*)lua_touserdata(L, 1);
    
    const int firstArg = 2;
    const int lastArg  = lua_gettop(L);
    const int nargs    = lastArg - firstArg + 1;
    
    if (!this->isNewState)
    {
        int arg = firstArg;
        
        bool hasArg = false;
        const char* stateName       = NULL;
        size_t      stateNameLength = 0;
        lua_Integer stateId         = 0;
    
        if (arg <= lastArg) {
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
            this->errorArg = arg;
            return luaL_error(L, "state name or id expected");
        }
    
        /* Lock */
        
        async_mutex_lock(mtstates_global_lock); this->globalLocked = true;
    
        MtState* state = NULL;
        if (stateName != NULL) {
            bool unique;
            state = findStateWithName(stateName, stateNameLength, &unique);
            if (!state && arg > lastArg) {
                return mtstates_ERROR_UNKNOWN_OBJECT_state_name(L, stateName, stateNameLength);
            } else if (state && !unique) {
                return mtstates_ERROR_AMBIGUOUS_NAME_state_name(L, stateName, stateNameLength);
            }
        } else {
            state = findStateWithId(stateId);
            if (!state) {
                return mtstates_ERROR_UNKNOWN_OBJECT_state_id(L, stateId);
            }
        }
        if (state) {
            StateUserData* userData = lua_newuserdata(L, sizeof(StateUserData));
            memset(userData, 0, sizeof(StateUserData));
            pushStateMeta(L);        /* -> udata, meta */
            lua_setmetatable(L, -2); /* -> udata */
        
            userData->state = state;
            atomic_inc(&state->used);

            this->nrslts = 1;
            return this->nrslts;        
        }
        // else we have a stateName and more args -> create new state
    }
    
    int arg = firstArg;
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
    
    if (arg <= lastArg && lua_type(L, arg) == LUA_TSTRING) {
        this->stateFunction = arg++;
        this->stateCode = lua_tolstring(L, this->stateFunction, &this->stateCodeLength);
    }
    
    if (this->stateCode == NULL) {
        if (lua_type(L, arg) != LUA_TFUNCTION || lua_iscfunction(L, arg)) {
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
        this->stateFunction = arg++;
        const char* varname = lua_getupvalue(L, this->stateFunction, 1);
        if (varname) {
            if (strcmp(varname, "_ENV") == 0) {
                /* _ENV -> function usese global variables */
                lua_pop(L, 1); /* pop result from first lua_getupvalue */
                varname = lua_getupvalue(L, this->stateFunction, 2);
            }
            if (varname) {
                this->errorArg = this->stateFunction;
                return luaL_error(L, "state function uses upvalue '%s'", varname);
            }
        }

        mtstates_membuf_reserve(&this->tmp, 4 * 1024);
        lua_pushvalue(L, this->stateFunction);
        int rc = lua_dump(L, &dumpWriter, &this->tmp, false);
        if (rc != 0) {
            return mtstates_ERROR_OUT_OF_MEMORY(L);
        }
        lua_pop(L, 1);
        this->stateCode       = this->tmp.bufferStart;
        this->stateCodeLength = this->tmp.bufferLength;
    }

    
    StateUserData* udata = lua_newuserdata(L, sizeof(StateUserData));
    memset(udata, 0, sizeof(StateUserData));
    pushStateMeta(L);        /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */

    if (!this->globalLocked) {
        async_mutex_lock(mtstates_global_lock); 
        this->globalLocked = true;
    }

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

    this->L        = L;
    this->firstArg = arg;
    this->lastArg  = lastArg;

    /* ------------------------------------------------------------------------------------ */
    
    lua_State* L2 = luaL_newstate();
    if (L2 == NULL) {
        return mtstates_ERROR_OUT_OF_MEMORY(L);
    }
    this->L2 = L2;
    {
        lua_pushcfunction(L2, errormsghandler2);
        lua_pushcfunction(L2, Mtstates_newState3);
        lua_pushlightuserdata(L2, this);
    
        int rc = lua_pcall(L2, 1, 0, -3);
    
        if (rc != LUA_OK) {
            if (this->isLError) {
                lua_pushstring(L, lua_tostring(L2, -1));
                return lua_error(L);
            } else {
                return mtstates_ERROR_INVOKING_STATE(L, NULL, lua_tostring(L2, -1));
            }
        } else {
            lua_pop(L2, 1); /* pop msghandler */
            return this->nrslts;
        }
    }
}

static int Mtstates_newState3(lua_State* L2)
{   
    int arg = 1;
    NewStateVars* this = (NewStateVars*)lua_touserdata(L2, arg++);

    /* own state id */
    lua_pushlightuserdata(L2, (void*)&state_counter); /* -> key */
    lua_pushinteger(L2, this->state->id);             /* -> key, value */
    lua_rawset(L2, LUA_REGISTRYINDEX);                /* -> */
    
    lua_State* L = this->L;

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
    
    int rc = luaL_loadbuffer(L2, this->stateCode, this->stateCodeLength, this->stateCode);

    if (rc != LUA_OK) {
        this->errorArg = this->stateFunction;
        this->isLError = true;
        return lua_error(L2); /* error has bee pushed by loadbuffer */
    }
    int func = lua_gettop(L2);
    rc = pushArgs(L2, L, this->firstArg, this->lastArg, L2);
    if (rc != 0) {
        if (rc > 0) {
            this->errorArg = rc;
            this->isLError = true;
            return lua_error(L2); /* error has been pushed by pushArgs */
        } else {
            this->isLError = true;
            return mtstates_ERROR_OUT_OF_MEMORY(L2);
        }
    }
    int nargs = this->lastArg - this->firstArg + 1;
    lua_call(L2, nargs, LUA_MULTRET);
    luaL_checkstack(L2, LUA_MINSTACK, NULL);
    
    int firstrslt = func;
    int lastrslt  = lua_gettop(L2);
    int nrslts = lastrslt - firstrslt + 1;
    if (nrslts >= 2) {
        rc = pushArgs(L, L2, firstrslt + 1, lastrslt, L2);
        if (rc != 0) {
            if (rc > 0) {
                lua_pushfstring(L2, "state setup function returned bad parameter #%d: %s", rc - firstrslt + 1, lua_tostring(L2, -1));
                mtstates_push_ERROR_STATE_RESULT(L2, NULL, lua_tostring(L2, -1));
                this->isLError = true;
                return lua_error(L2);
            } else {
                this->isLError = true;
                return mtstates_ERROR_OUT_OF_MEMORY(L2);
            }
        }
    }
    if (nrslts < 1 || lua_type(L2, firstrslt) != LUA_TFUNCTION) {
        const char* t = (nrslts < 1) ? "nothing" : lua_typename(L2, lua_type(L2, firstrslt));
        lua_pushfstring(L2, "state setup function returned %s but a function is required as first result parameter", t);
        mtstates_push_ERROR_STATE_RESULT(L2, NULL, lua_tostring(L2, -1));
        this->isLError = true;
        return lua_error(L2);
    }
    lua_pushvalue(L2, firstrslt);
    this->state->callbackref = luaL_ref(L2, LUA_REGISTRYINDEX);
    this->state->L2 = L2; this->L2 = NULL;
    
    /* ------------------------------------------------------------------------------------ */
    atomic_set(&this->state->initialized, true);
    
    this->nrslts = nrslts - 1 + 1;
    return 0;
}


static int Mtstates_newState1(lua_State* L, bool isNewState);
static int Mtstates_state(lua_State* L)
{
    return Mtstates_newState1(L, false);
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


static int MtState_call2(lua_State* L, bool isTimed);
static int MtState_call3(lua_State* L);
static int MtState_call4(lua_State* L2);

static int MtState_call(lua_State* L)
{
    return MtState_call2(L, false);
}

static int MtState_tcall(lua_State* L)
{
    return MtState_call2(L, true);
}

static int MtState_call2(lua_State* L, bool isTimed)
{
    int lastArg = lua_gettop(L);
    int arg     = 1;
    int nargs   = lastArg;
    
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
    ThreadId myThreadId = async_current_threadid();
    if (s->isBusy) {
        if (s->calledByThread == myThreadId) {
            async_mutex_unlock(&s->stateMutex);
            return mtstates_ERROR_CYCLE_DETECTED(L);
        }
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
    s->calledByThread = myThreadId;
    async_mutex_unlock(&s->stateMutex);
    
    /* ------------------------------------------------------------------- */
    
    CallStateVars vars; memset(&vars, 0, sizeof(CallStateVars));
    CallStateVars* this = &vars;
    
    this->L        = L;
    this->isTimed  = isTimed;
    this->state    = s;
    this->firstArg = arg;
    this->lastArg  = lastArg;

    lua_pushcfunction(L, MtState_call3);
    lua_insert(L, 1);
    
    lua_pushlightuserdata(L, this);
    lua_replace(L, 2);

    int l2start = lua_gettop(s->L2);

    int rc = lua_pcall(L, nargs, LUA_MULTRET, 0);
    luaL_checkstack(L, LUA_MINSTACK, NULL);

    /* ------------------------------------------------------------------- */

    lua_settop(s->L2, l2start);

    async_mutex_lock(&s->stateMutex);
    s->isBusy = false;
    async_mutex_notify(&s->stateMutex);
    async_mutex_unlock(&s->stateMutex);

    /* ------------------------------------------------------------------- */

    if (rc != LUA_OK) {
        if (this->errorArg) {
            return luaL_argerror(L, this->errorArg, lua_tostring(L, -1));
        } else {
            return lua_error(L);
        }
    } else {
        return this->nrslts;
    }
}
    
static int MtState_call3(lua_State* L)
{
    CallStateVars* this = (CallStateVars*)lua_touserdata(L, 1);

    MtState*   s  = this->state;
    lua_State* L2 = s->L2;

    lua_pushcfunction(L2, errormsghandler2);
    lua_pushcfunction(L2, MtState_call4);
    lua_pushlightuserdata(L2, this);
 
    int rc = lua_pcall(L2, 1, 0, -3);

    if (rc != LUA_OK) {
        if (this->isLError) {
            lua_pushstring(L, lua_tostring(L2, -1));
            return lua_error(L);
        } else {
            const char* stateString = mtstates_state_tostring(L, s);
            return mtstates_ERROR_INVOKING_STATE(L, stateString, lua_tostring(L2, -1));
        }
    } else {
        return this->nrslts;
    }
}


static int MtState_call4(lua_State* L2)
{
    CallStateVars* this = (CallStateVars*)lua_touserdata(L2, 1);

    MtState*   s = this->state;
    lua_State* L = this->L;

    lua_rawgeti(L2, LUA_REGISTRYINDEX, s->callbackref);
    int func = lua_gettop(L2);
    int rc = pushArgs(L2, L, this->firstArg, this->lastArg, L2);
    if (rc != 0) {
        if (rc > 0) {
            this->errorArg = rc;
            this->isLError = true;
            return lua_error(L2); /* error has been pushed by pushArgs */
        } else {
            this->isLError = true;
            return mtstates_ERROR_OUT_OF_MEMORY(L2);
        }
    }
    int nargs = this->lastArg - this->firstArg + 1;
    lua_call(L2, nargs, LUA_MULTRET);
    luaL_checkstack(L2, LUA_MINSTACK, NULL);
    int firstrslt = func;
    int lastrslt  = lua_gettop(L2);
    int nrslts = lastrslt - firstrslt + 1;
    if (this->isTimed) {
        nrslts += 1;
        lua_pushboolean(L, true);
    }
    rc = pushArgs(L, L2, firstrslt, lastrslt, L2);
    if (rc != 0) {
        if (rc > 0) {
            lua_pushfstring(L2, "state callback function returned bad parameter #%d: %s", rc - firstrslt + 1, lua_tostring(L2, -1));
            mtstates_push_ERROR_STATE_RESULT(L2, NULL, lua_tostring(L2, -1));
            this->isLError = true;
            return lua_error(L2);
        } else {
            this->isLError = true;
            return mtstates_ERROR_OUT_OF_MEMORY(L2);
        }
    }
    this->nrslts = nrslts;
    return 0;
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

static int Mtstates_id(lua_State* L2)
{
    lua_pushlightuserdata(L2, (void*)&state_counter); /* -> key */
    lua_rawget(L2, LUA_REGISTRYINDEX);                /* -> value */
    return 1;
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
    { "state",     Mtstates_state     },
    { "id",        Mtstates_id        },
    { NULL,        NULL } /* sentinel */
};

static void setupStateMeta(lua_State* L)
{
    lua_pushstring(L, MTSTATES_STATE_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, StateMetaMethods, 0);
    
    lua_newtable(L);  /* StateClass */
        luaL_setfuncs(L, StateMethods, 0);
    lua_setfield (L, -2, "__index");
}


int mtstates_state_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, MTSTATES_STATE_CLASS_NAME)) {
        setupStateMeta(L);
    }
    lua_pop(L, 1);

    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}

