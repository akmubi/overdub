#ifndef LUA_OVERDUB_INPUT_H
#define LUA_OVERDUB_INPUT_H

#include "input.h"
#include "types.h"

typedef struct lua_State lua_State;

bool
lua_overdub_input_register(lua_State *state);
bool
lua_overdub_input_push_event(lua_State *state, const input_event_t *event);

#endif /* LUA_OVERDUB_INPUT_H */
