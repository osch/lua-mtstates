#ifndef MTSTATES_ERROR_H
#define MTSTATES_ERROR_H

#include "util.h"

extern const char* const MTSTATES_ERROR_CLASS_NAME;

int mtstates_ERROR_OBJECT_EXISTS(lua_State* L, const char* objectString);

int mtstates_ERROR_UNKNOWN_OBJECT_state_name(lua_State* L, const char* stateName, size_t nameLength);
int mtstates_ERROR_UNKNOWN_OBJECT_state_id(lua_State* L, lua_Integer id);

int mtstates_ERROR_OUT_OF_MEMORY(lua_State* L);
int mtstates_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes);

int mtstates_error_init_module(lua_State* L, int module, int errorMeta, int errorClass);


#endif /* MTSTATES_ERROR_H */
