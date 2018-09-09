
typedef struct MtState {
    lua_Integer        id;
    AtomicCounter      used;
    AtomicCounter      initialized;
    char*              stateName;
    size_t             stateNameLength;
    Mutex              stateMutex;
    lua_State*         L2;
    int                callbackref;
    bool               isBusy;
    
    struct MtState**   prevStatePtr;
    struct MtState*    nextState;
    
} MtState;

typedef struct {
    int      count;
    MtState* firstState;
} StateBucket;

typedef struct 
{
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
    
    int errorArg;
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

    int errorArg;
    int nrslts;

} CallStateVars;


