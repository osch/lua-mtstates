#ifndef MTSTATES_ERROR_H
#define MTSTATES_ERROR_H

#include "util.h"

int mtstates_ERROR_CONCURRENT_ACCESS(lua_State* L, const char* objectString);
int mtstates_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString);

int mtstates_ERROR_UNKNOWN_OBJECT_state_name(lua_State* L, const char* stateName, size_t nameLength);
int mtstates_ERROR_UNKNOWN_OBJECT_state_id(lua_State* L, lua_Integer id);

int mtstates_ERROR_AMBIGUOUS_NAME_state_name(lua_State* L, const char* stateName, size_t nameLength);

int mtstates_ERROR_INTERRUPTED(lua_State* L);

int mtstates_ERROR_INVOKING_STATE(lua_State* L, const char* stateString, const char* errorDetails);

int mtstates_ERROR_STATE_RESULT(lua_State* L, const char* stateString, const char* errorDetails);

void mtstates_push_ERROR_STATE_RESULT(lua_State* L, const char* stateString, const char* errorDetails);

int mtstates_ERROR_OUT_OF_MEMORY(lua_State* L);
int mtstates_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes);

int mtstates_error_init_module(lua_State* L, int module);


#endif /* MTSTATES_ERROR_H */
