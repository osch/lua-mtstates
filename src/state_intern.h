
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
