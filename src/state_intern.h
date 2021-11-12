#ifndef MTSTATES_STATE_INTERN
#define MTSTATES_STATE_INTERN

typedef struct receiver_writer receiver_writer;

typedef struct MtState {
    lua_Integer        id;
    AtomicCounter      used;
    AtomicCounter      owned;
    AtomicCounter      initialized;
    AtomicCounter      closed;
    char*              stateName;
    size_t             stateNameLength;
    Mutex              stateMutex;
    lua_State*         L2;
    int                callbackref;

    bool               isBusy;
    ThreadId           calledByThread;
    
    struct MtState**   prevStatePtr;
    struct MtState*    nextState;
    
} MtState;

typedef struct {
    int      count;
    MtState* firstState;
} StateBucket;

typedef enum {
    NEW_STATE,
    FIND_STATE,
    SINGLETON
} NewStateMode;

typedef struct 
{
    NewStateMode newStateMode;
    
    bool globalLocked;
    bool stateLocked;
    
    lua_State* L;

    int         stateFunction;
    const char* stateCode;
    size_t      stateCodeLength;
    MemBuffer   tmp;
    
    int firstArg;
    int lastArg;

    MtState* state;

    bool openlibs;
    lua_State* L2;
    
    bool isLError;
    
    int   errorArg;
    char* errorArgMsg;

    int nrslts;

} NewStateVars;

typedef struct
{
    lua_State* L;
    
    bool isTimed;
    
    MtState* state;
    
    int firstArg;
    int lastArg;

    bool isLError;

    int   errorArg;
    char* errorArgMsg;
    
    int nrslts;

} CallStateVars;

typedef struct StateUserData {
    MtState*         state;
    bool             isOwner;
} StateUserData;


static void setErrorArgMsg(char** errorArgMsg, lua_State* L2)
{
    size_t len;
    const char* lmsg = lua_tolstring(L2, -1, &len);
    char* msg = malloc(len + 1);
    if (msg) {
        memcpy(msg, lmsg, len + 1);
        *errorArgMsg = msg; /* error message without stack trace */
    }
}

void mtstates_state_free(MtState* state);

typedef void (*mtstates_capi_error_handler)(void* ehdata, const char* msg, size_t msglen);

int mtstates_state_call(lua_State* L, bool isTimed, int arg, 
                        MtState* s, receiver_writer* writer,
                        mtstates_capi_error_handler eh, void* ehdata);

#endif /* MTSTATES_STATE_INTERN */
