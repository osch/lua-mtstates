#include "state.h"
#include "error.h"
#include "main.h"

const char* const MTSTATES_STATE_CLASS_NAME = "mtstates.state";

typedef struct MtState {
    lua_Integer        id;
    AtomicCounter      used;
    AtomicCounter      initialized;
    char*              stateName;
    size_t             stateNameLength;
    Lock               stateLock;
    lua_State*         L2;
    int                callbackref;
    
    struct MtState**   prevStatePtr;
    struct MtState*    nextState;
    
} MtState;

typedef struct {
    int      count;
    MtState* firstState;
} StateBucket;

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

static int errormsghandler(lua_State* L)
{
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL) {
        if (luaL_testudata(L, 1, MTSTATES_ERROR_CLASS_NAME)) {
            return 1;
        }
        else if (   luaL_callmeta(L, 1, "__tostring")  /* does it have a metamethod */
                 && lua_type(L, -1) == LUA_TSTRING)  /* that produces a string? */
        {
            msg = lua_tostring(L, -1);  /* that is the message */
        } else {
            msg = lua_pushfstring(L, "(error object is a %s value)",
                                     luaL_typename(L, 1));
        }
    }
    mtstates_push_ERROR(L, msg);
    return 1;
}

static int dumpWriter(lua_State* L, const void* p, size_t sz, void* ud)
{
    MemBuffer* b = (MemBuffer*) ud;
    mtstates_membuf_reserve(b, sz);
    memcpy(b->bufferStart + b->bufferLength, p, sz);
    b->bufferLength += sz;
    return 0;
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

static int Mtstates_newState(lua_State* L)
{
    int nargs = lua_gettop(L);
    int arg   = 1;
    
    const char* stateName       = NULL;
    size_t      stateNameLength = 0;
    if (arg <= nargs && nargs >= 2) {
        int t = lua_type(L, arg);
        if (t == LUA_TSTRING) {
            stateName = lua_tolstring(L, arg++, &stateNameLength);
        } else if (t == LUA_TNIL) {
            arg++;
        }
    }
    
    bool openlibs = true;
    if (arg <= nargs && lua_type(L, arg) == LUA_TBOOLEAN) {
        openlibs = lua_toboolean(L, arg++);
    }
    
    size_t      stateCodeLength;
    const char* stateCode = NULL;
    if (arg <= nargs && lua_type(L, arg) == LUA_TSTRING) {
        stateCode = lua_tolstring(L, arg, &stateCodeLength);
    }
    
    int stateFunction = 0;
    if (stateCode == NULL) {
        luaL_checktype(L, arg, LUA_TFUNCTION);
        stateFunction = arg++;
    }
    
    StateUserData* udata = lua_newuserdata(L, sizeof(StateUserData));
    memset(udata, 0, sizeof(StateUserData));
    luaL_setmetatable(L, MTSTATES_STATE_CLASS_NAME);

    async_mutex_lock(mtstates_global_lock);

    /* ------------------------------------------------------------------------------------ */

    MtState* s = calloc(1, sizeof(MtState));
    if (s == NULL) {
        async_mutex_unlock(mtstates_global_lock);
        return mtstates_ERROR_OUT_OF_MEMORY(L);
    }
    async_lock_init(&s->stateLock);
    
    s->id        = atomic_inc(&mtstates_id_counter);
    s->used      = 1;
    udata->state = s;
    
    if (stateName) {
        s->stateName = malloc(stateNameLength + 1);
        if (s->stateName == NULL) {
            async_mutex_unlock(mtstates_global_lock);
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
            async_mutex_unlock(mtstates_global_lock);
            return mtstates_ERROR_OUT_OF_MEMORY(L);
        }
    }
    toBuckets(s, state_buckets, state_bucket_list);
    
    async_lock_acquire(&s->stateLock);

    async_mutex_unlock(mtstates_global_lock);

    /* ------------------------------------------------------------------------------------ */
    
    lua_State* L2 = luaL_newstate();
    int l2top = 0;
    if (L2 == NULL) {
        return mtstates_ERROR_OUT_OF_MEMORY(L);
    }
    if (openlibs) {
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
    mtstates_error_init_meta(L2);
    
    lua_pushcfunction(L2, errormsghandler);
    int msghandler = ++l2top;
    
    int rc;
    if (stateFunction != 0) {
        if (lua_iscfunction(L, stateFunction)) {
            lua_pushcfunction(L2, lua_tocfunction(L, stateFunction));
            rc = LUA_OK;
        } else {
            const char* varname = lua_getupvalue(L, stateFunction, 1);
            if (varname) {
                if (strcmp(varname, "_ENV") == 0) {
                    /* _ENV -> function usese global variables */
                    lua_pop(L, 1); /* pop result from first lua_getupvalue */
                    varname = lua_getupvalue(L, stateFunction, 2);
                }
                if (varname) {
                    lua_pushfstring(L, "state function uses upvalue '%s'", varname);
                    async_lock_release(&s->stateLock);
                    return luaL_argerror(L, stateFunction, lua_tostring(L, -1));
                }
            }
            MemBuffer tmp;
            mtstates_membuf_init(&tmp, 4 * 1024, 2);
            lua_pushvalue(L, stateFunction);
            lua_dump(L, &dumpWriter, &tmp, false);
            lua_pop(L, 1);
            rc = luaL_loadbuffer(L2, tmp.bufferStart, tmp.bufferLength, NULL);
            mtstates_membuf_free(&tmp);
        }
    } else {   
        rc = luaL_loadbuffer(L2, stateCode, stateCodeLength, stateCode);
    }
    if (rc != LUA_OK) {
        size_t errorMessageLength;
        const char* errorMessage = luaL_tolstring(L2, -1, &errorMessageLength);
        lua_pushlstring(L, errorMessage, errorMessageLength);
        lua_close(L2);
        async_lock_release(&s->stateLock);
        return lua_error(L);
    }
    int func = ++l2top;
    rc = pushArgs(L2, L, arg, nargs, L);
    if (rc != 0) {
        lua_close(L2);
        async_lock_release(&s->stateLock);
        return luaL_argerror(L, rc, lua_tostring(L, -1));
    }
    rc = lua_pcall(L2, nargs - arg + 1, LUA_MULTRET, msghandler);
    if (rc != LUA_OK) {
        int ltop = lua_gettop(L);
        Error* e = luaL_testudata(L2, -1, MTSTATES_ERROR_CLASS_NAME);
        int details      = 0;
        int traceback    = 0;
        if (e) {
            lua_pushstring(L, mtstates_error_details(L2, e));   details   = ++ltop;
            lua_pushstring(L, mtstates_error_traceback(L2, e)); traceback = ++ltop;
        } else {
            size_t errorMessageLength;
            const char* errorString = luaL_tolstring(L2, -1, &errorMessageLength);
            lua_pushlstring(L, errorString, errorMessageLength); details = ++ltop;
        }
        lua_close(L2);
        async_lock_release(&s->stateLock);
        if (e) {
            return mtstates_ERROR_INVOKING_STATE_traceback(L, NULL,
                                                              lua_tostring(L, details),
                                                              lua_tostring(L, traceback));
        } else {
            return mtstates_ERROR_INVOKING_STATE(L, NULL, lua_tostring(L, details));
        }
    }
    int firstrslt = func;
    int lastrslt  = lua_gettop(L2);
    int nrslts = lastrslt - firstrslt + 1;
    if (nrslts >= 2) {
        rc = pushArgs(L, L2, firstrslt + 1, lastrslt, L);
        if (rc != 0) {
            lua_close(L2);
            async_lock_release(&s->stateLock);
            lua_pushfstring(L, "state function returns bad parameter #%d: %s", rc - firstrslt + 1, lua_tostring(L, -1));
            return mtstates_ERROR_STATE_RESULT(L, NULL, lua_tostring(L, -1));
        }
    }
    if (nrslts < 1 || lua_type(L2, firstrslt) != LUA_TFUNCTION) {
        const char* t = (nrslts < 1) ? "nothing" : lua_typename(L2, lua_type(L2, firstrslt));
        lua_pushfstring(L, "state setup returns %s but a function is required as first result parameter", t);
        lua_close(L2);
        async_lock_release(&s->stateLock);
        return mtstates_ERROR_STATE_RESULT(L, NULL, lua_tostring(L, -1));
    }
    s->L2 = L2;
    lua_pushvalue(L2, firstrslt);
    s->callbackref = luaL_ref(L2, LUA_REGISTRYINDEX);
    
    /* ------------------------------------------------------------------------------------ */
    atomic_set(&s->initialized, true);
    atomic_inc(&state_counter);
    async_lock_release(&s->stateLock);
    
    return nrslts - 1 + 1;
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
    if (s->prevStatePtr) {
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
    async_lock_destruct(&s->stateLock);
    free(s);
    
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

    if (!async_lock_tryacquire(&s->stateLock)) {
        return mtstates_ERROR_CONCURRENT_ACCESS(L, mtstates_state_tostring(L, s));
    }
    if (s->L2) {
        lua_close(s->L2);
        s->L2 = NULL;
    }
    async_lock_release(&s->stateLock);
    return 0;
}

static int MtState_call(lua_State* L)
{
    int lastarg = lua_gettop(L);
    int arg     = 1;
    StateUserData* udata = luaL_checkudata(L, arg++, MTSTATES_STATE_CLASS_NAME);
    MtState*       s     = udata->state;

    if (!async_lock_tryacquire(&s->stateLock)) {
        return mtstates_ERROR_CONCURRENT_ACCESS(L, mtstates_state_tostring(L, s));
    }
    if (s->L2 == NULL) {
        async_lock_release(&s->stateLock);
        return mtstates_ERROR_OBJECT_CLOSED(L, mtstates_state_tostring(L, s));
    }
    lua_State* L2 = s->L2;
    int l2start = lua_gettop(L2);
    int l2top = l2start;
    lua_pushcfunction(L2, errormsghandler);
    int msghandler = ++l2top;
    lua_rawgeti(L2, LUA_REGISTRYINDEX, s->callbackref);
    int func = ++l2top;
    int rc = pushArgs(L2, L, arg, lastarg, L);
    if (rc != 0) {
        lua_settop(L2, l2start);
        async_lock_release(&s->stateLock);
        return luaL_argerror(L, rc, lua_tostring(L, -1));
    }
    rc = lua_pcall(L2, lastarg - arg + 1, LUA_MULTRET, msghandler);
    if (rc != LUA_OK) {
        int ltop = lua_gettop(L);
        Error* e = luaL_testudata(L2, -1, MTSTATES_ERROR_CLASS_NAME);
        int details      = 0;
        int traceback    = 0;
        if (e) {
            lua_pushstring(L, mtstates_error_details(L2, e));   details   = ++ltop;
            lua_pushstring(L, mtstates_error_traceback(L2, e)); traceback = ++ltop;
        } else {
            size_t errorMessageLength;
            const char* errorString = luaL_tolstring(L2, -1, &errorMessageLength);
            lua_pushlstring(L, errorString, errorMessageLength); details = ++ltop;
        }
        lua_settop(L2, l2start);
        async_lock_release(&s->stateLock);
        const char* stateString = mtstates_state_tostring(L, s);
        if (e) {
            if (mtstates_is_ERROR_INTERRUPTED(e)) {
                return mtstates_ERROR_INTERRUPTED_traceback(L, stateString,
                                                               lua_tostring(L, details),
                                                               lua_tostring(L, traceback));
            } else {
                return mtstates_ERROR_INVOKING_STATE_traceback(L, stateString,
                                                                  lua_tostring(L, details),
                                                                  lua_tostring(L, traceback));
            }
        } else {
            return mtstates_ERROR_INVOKING_STATE(L, stateString, lua_tostring(L, details));
        }
    }
    int firstrslt = func;
    int lastrslt  = lua_gettop(L2);
    int nrslts = lastrslt - firstrslt + 1;
    rc = pushArgs(L, L2, firstrslt, lastrslt, L);
    if (rc != 0) {
        lua_settop(L2, l2start);
        async_lock_release(&s->stateLock);
        lua_pushfstring(L, "state function returns bad parameter #%d: %s", rc - firstrslt + 1, lua_tostring(L, -1));
        return mtstates_ERROR_STATE_RESULT(L, NULL, lua_tostring(L, -1));
    }
    lua_settop(L2, l2start);
    async_lock_release(&s->stateLock);
    return nrslts;
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

