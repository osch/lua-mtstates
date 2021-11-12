#include "state.h"
#include "state_intern.h"
#include "notify_capi_impl.h"

/* ============================================================================================ */

static notify_notifier* notify_capi_toNotifier(lua_State* L, int index)
{
    void* state = lua_touserdata(L, index);
    if (state) {
        if (lua_getmetatable(L, index))
        {                                                      /* -> meta1 */
            if (luaL_getmetatable(L, MTSTATES_STATE_CLASS_NAME)
                != LUA_TNIL)                                   /* -> meta1, meta2 */
            {
                if (lua_rawequal(L, -1, -2)) {                 /* -> meta1, meta2 */
                    state = ((StateUserData*)state)->state;
                } else {
                    state = NULL;
                }
            }                                                  /* -> meta1, meta2 */
            lua_pop(L, 2);                                     /* -> */
        }                                                      /* -> */
    }
    return state;
}

/* ============================================================================================ */

static void notify_capi_retainNotifier(notify_notifier* n)
{
    MtState* state = (MtState*)n;
    atomic_inc(&state->used);
}

/* ============================================================================================ */

static void notify_capi_releaseNotifier(notify_notifier* n)
{
    MtState* state = (MtState*)n;
    if (atomic_dec(&state->used) <= 0) {
        mtstates_state_free(state);
    }
}

/* ============================================================================================ */

static int notify_capi_notify(notify_notifier* n, notifier_error_handler eh, void* ehdata)
{
    MtState* state = (MtState*)n;
    int rc = mtstates_state_call(NULL, false, 0, state, NULL, eh, ehdata);
    if (rc == 101) {
        return 1; // closed
    } else {
        return rc;
    }
}


/* ============================================================================================ */

const notify_capi mtstates_notify_capi_impl =
{
    NOTIFY_CAPI_VERSION_MAJOR,
    NOTIFY_CAPI_VERSION_MINOR,
    NOTIFY_CAPI_VERSION_PATCH,
    NULL, // next_capi
    
    notify_capi_toNotifier,

    notify_capi_retainNotifier,
    notify_capi_releaseNotifier,

    notify_capi_notify
};


