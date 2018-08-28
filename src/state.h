#ifndef MTSTATES_STATE_H
#define MTSTATES_STATE_H

#include "util.h"

extern const char* const MTSTATES_STATE_CLASS_NAME;

int mtstates_state_init_module(lua_State* L, int module, int stateMeta, int stateClass);

#endif /* MTSTATES_STATE_H */
