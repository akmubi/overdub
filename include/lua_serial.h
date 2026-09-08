#ifndef LUA_SERIAL_H
#define LUA_SERIAL_H

#include "types.h"

typedef struct lua_State lua_State;

typedef struct lua_serial_blob_s lua_serial_blob_t;
struct lua_serial_blob_s {
  uint8_t *data;
  uint64_t len;
};

bool
lua_serial_encode(lua_State *state, int value_idx, lua_serial_blob_t *out_blob, char *error, size_t error_cap);
bool
lua_serial_decode(lua_State *state, lua_serial_blob_t blob, char *error, size_t error_cap);
void
lua_serial_blob_free(lua_serial_blob_t *blob);

#endif /* LUA_SERIAL_H */
