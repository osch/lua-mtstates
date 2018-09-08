
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
    bool openlibs;
    
    bool globalLocked;
    
    MtState* state;
    
    bool stateLocked;

    MemBuffer stateFunctionBuffer;

    lua_State* L2;

    int errorArg;

    int nrslts;

} NewStateVars;

