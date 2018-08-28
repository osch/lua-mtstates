#ifndef MTSTATES_MAIN_H
#define MTSTATES_MAIN_H

#include "util.h"

extern Mutex*        mtstates_global_lock;
extern AtomicCounter mtstates_id_counter;

DLL_PUBLIC int luaopen_mtstates(lua_State* L);

#endif /* MTSTATES_MAIN_H */
