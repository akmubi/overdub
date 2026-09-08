#include "lua_serial.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define LUA_SERIAL_MAX_BYTES   (4 * MB)
#define LUA_SERIAL_MAX_DEPTH   (32)
#define LUA_SERIAL_MAX_ENTRIES (64 * 1024)

typedef uint8_t lua_serial_tag_t;
enum {
  LUA_SERIAL_NIL = 0,
  LUA_SERIAL_FALSE,
  LUA_SERIAL_TRUE,
  LUA_SERIAL_INTEGER,
  LUA_SERIAL_NUMBER,
  LUA_SERIAL_STRING,
  LUA_SERIAL_TABLE,
};

typedef struct lua_serial_writer_s lua_serial_writer_t;
struct lua_serial_writer_s {
  lua_serial_blob_t blob;
  uint64_t          cap;
  const void       *tables[LUA_SERIAL_MAX_DEPTH];
  int               table_count;
  char             *error;
  size_t            error_cap;
};

typedef struct lua_serial_reader_s lua_serial_reader_t;
struct lua_serial_reader_s {
  lua_serial_blob_t blob;
  uint64_t          offset;
  char             *error;
  size_t            error_cap;
};

static void
lua_serial_error(char *error, size_t error_cap, const char *fmt, ...)
{
  if (!error || error_cap == 0 || error[0]) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(error, error_cap, fmt, args);
  va_end(args);

  error[error_cap - 1] = 0;
}

void
lua_serial_blob_free(lua_serial_blob_t *blob)
{
  if (!blob) {
    return;
  }

  if (blob->data) {
    HeapFree(GetProcessHeap(), 0, blob->data);
  }
  *blob = (lua_serial_blob_t){0};
}

static bool
lua_serial_writer_reserve(lua_serial_writer_t *writer, uint64_t additional)
{
  if (!writer || additional > LUA_SERIAL_MAX_BYTES || writer->blob.len > LUA_SERIAL_MAX_BYTES - additional) {
    lua_serial_error(writer ? writer->error : NULL, writer ? writer->error_cap : 0,
                     "serialized value exceeds the %llu-byte limit", (unsigned long long)LUA_SERIAL_MAX_BYTES);
    return false;
  }

  uint64_t required = writer->blob.len + additional;
  if (required <= writer->cap) {
    return true;
  }

  uint64_t new_cap = writer->cap ? writer->cap : 256;
  while (new_cap < required) {
    new_cap = MIN_VAL(new_cap * 2, (uint64_t)LUA_SERIAL_MAX_BYTES);
    if (new_cap < required && new_cap == LUA_SERIAL_MAX_BYTES) {
      return false;
    }
  }

  void *data = writer->blob.data ? HeapReAlloc(GetProcessHeap(), 0, writer->blob.data, (SIZE_T)new_cap) : HeapAlloc(GetProcessHeap(), 0, (SIZE_T)new_cap);
  if (!data) {
    lua_serial_error(writer->error, writer->error_cap, "could not allocate serialized value storage");
    return false;
  }

  writer->blob.data = data;
  writer->cap       = new_cap;
  return true;
}

static bool
lua_serial_writer_write(lua_serial_writer_t *writer, const void *data, uint64_t size)
{
  if (!lua_serial_writer_reserve(writer, size)) {
    return false;
  }

  if (size > 0) {
    memcpy(writer->blob.data + writer->blob.len, data, (size_t)size);
  }

  writer->blob.len += size;
  return true;
}

static bool
lua_serial_writer_tag(lua_serial_writer_t *writer, lua_serial_tag_t tag)
{
  return lua_serial_writer_write(writer, &tag, sizeof(tag));
}

static bool lua_serial_encode_value(lua_State *state, int idx, lua_serial_writer_t *writer, int depth);

static bool
lua_serial_encode_table(lua_State *state, int idx, lua_serial_writer_t *writer, int depth)
{
  if (depth >= LUA_SERIAL_MAX_DEPTH) {
    lua_serial_error(writer->error, writer->error_cap, "table nesting exceeds the %d-level limit", LUA_SERIAL_MAX_DEPTH);
    return false;
  }

  if (lua_getmetatable(state, idx)) {
    lua_pop(state, 1);
    lua_serial_error(writer->error, writer->error_cap, "tables with metatables cannot cross a worker boundary");
    return false;
  }

  const void *identity = lua_topointer(state, idx);
  for (int i = 0; i < writer->table_count; ++i) {
    if (writer->tables[i] == identity) {
      lua_serial_error(writer->error, writer->error_cap, "cyclic tables cannot cross a worker boundary");
      return false;
    }
  }

  uint32_t count = 0;
  idx = lua_absindex(state, idx);
  lua_pushnil(state);
  while (lua_next(state, idx)) {
    lua_pop(state, 1);
    if (count == LUA_SERIAL_MAX_ENTRIES) {
      lua_pop(state, 1);
      lua_serial_error(writer->error, writer->error_cap, "table exceeds the %u-entry limit", LUA_SERIAL_MAX_ENTRIES);
      return false;
    }
    count += 1;
  }

  if (!lua_serial_writer_tag(writer, LUA_SERIAL_TABLE) || !lua_serial_writer_write(writer, &count, sizeof(count))) {
    return false;
  }

  writer->tables[writer->table_count++] = identity;
  bool encoded = true;
  lua_pushnil(state);
  while (encoded && lua_next(state, idx)) {
    int key_type = lua_type(state, -2);
    if (!((key_type == LUA_TNUMBER && lua_isinteger(state, -2)) || key_type == LUA_TSTRING)) {
      lua_serial_error(writer->error, writer->error_cap, "worker table keys must be strings or integers");
      encoded = false;
    } else {
      encoded = lua_serial_encode_value(state, -2, writer, depth + 1) && lua_serial_encode_value(state, -1, writer, depth + 1);
    }
    lua_pop(state, 1);
  }

  if (!encoded && lua_gettop(state) >= idx + 1) {
    lua_pop(state, 1);
  }

  writer->table_count -= 1;
  return encoded;
}

static bool
lua_serial_encode_value(lua_State *state, int idx, lua_serial_writer_t *writer, int depth)
{
  switch (lua_type(state, idx)) {
    case LUA_TNIL: {
      return lua_serial_writer_tag(writer, LUA_SERIAL_NIL);
    }

    case LUA_TBOOLEAN: {
      return lua_serial_writer_tag(writer, lua_toboolean(state, idx) ? LUA_SERIAL_TRUE : LUA_SERIAL_FALSE);
    }

    case LUA_TNUMBER: {
      if (lua_isinteger(state, idx)) {
        lua_Integer value = lua_tointeger(state, idx);
        return lua_serial_writer_tag(writer, LUA_SERIAL_INTEGER) && lua_serial_writer_write(writer, &value, sizeof(value));
      } else {
        lua_Number value = lua_tonumber(state, idx);
        return lua_serial_writer_tag(writer, LUA_SERIAL_NUMBER) && lua_serial_writer_write(writer, &value, sizeof(value));
      }
    }

    case LUA_TSTRING: {
      size_t      len  = 0;
      const char *data = lua_tolstring(state, idx, &len);
      if (len > UINT32_MAX) {
        lua_serial_error(writer->error, writer->error_cap, "string is too large for a worker value");
        return false;
      }

      uint32_t encoded_len = (uint32_t)len;
      return lua_serial_writer_tag(writer, LUA_SERIAL_STRING) && lua_serial_writer_write(writer, &encoded_len, sizeof(encoded_len)) && lua_serial_writer_write(writer, data, encoded_len);
    }

    case LUA_TTABLE: {
      return lua_serial_encode_table(state, idx, writer, depth);
    }

    default: {
      lua_serial_error(writer->error, writer->error_cap, "%s values cannot cross a worker boundary", lua_typename(state, lua_type(state, idx)));
      return false;
    }
  }
}

bool
lua_serial_encode(lua_State *state, int value_idx, lua_serial_blob_t *out_blob, char *error, size_t error_cap)
{
  if (error && error_cap > 0) {
    error[0] = 0;
  }

  if (!state || !out_blob) {
    lua_serial_error(error, error_cap, "invalid serializer arguments");
    return false;
  }

  *out_blob = (lua_serial_blob_t){0};
  lua_serial_writer_t writer = {
    .error     = error,
    .error_cap = error_cap,
  };

  if (!lua_serial_encode_value(state, value_idx, &writer, 0)) {
    lua_serial_blob_free(&writer.blob);
    return false;
  }

  *out_blob = writer.blob;
  return true;
}

static bool
lua_serial_reader_read(lua_serial_reader_t *reader, void *out, uint64_t size)
{
  if (!reader || size > reader->blob.len || reader->offset > reader->blob.len - size) {
    lua_serial_error(reader ? reader->error : NULL, reader ? reader->error_cap : 0, "serialized worker value is truncated");
    return false;
  }

  if (size > 0 && out) {
    mem_copy(out, reader->blob.data + reader->offset, size);
  }

  reader->offset += size;
  return true;
}

static bool
lua_serial_decode_value(lua_State *state, lua_serial_reader_t *reader, int depth)
{
  lua_serial_tag_t tag = 0;
  if (!lua_serial_reader_read(reader, &tag, sizeof(tag))) {
    return false;
  }

  switch (tag) {
    case LUA_SERIAL_NIL: {
      lua_pushnil(state);
      return true;
    }

    case LUA_SERIAL_FALSE: {
      lua_pushboolean(state, false);
      return true;
    }

    case LUA_SERIAL_TRUE: {
      lua_pushboolean(state, true);
      return true;
    }

    case LUA_SERIAL_INTEGER: {
      lua_Integer value = 0;
      if (!lua_serial_reader_read(reader, &value, sizeof(value))) {
        return false;
      }

      lua_pushinteger(state, value);
      return true;
    }

    case LUA_SERIAL_NUMBER: {
      lua_Number value = 0;
      if (!lua_serial_reader_read(reader, &value, sizeof(value))) {
        return false;
      }

      lua_pushnumber(state, value);
      return true;
    }

    case LUA_SERIAL_STRING: {
      uint32_t len = 0;
      if (!lua_serial_reader_read(reader, &len, sizeof(len)) || (uint64_t)len > reader->blob.len || reader->offset > reader->blob.len - (uint64_t)len) {
        lua_serial_error(reader->error, reader->error_cap, "serialized worker string is truncated");
        return false;
      }

      lua_pushlstring(state, (const char *)reader->blob.data + reader->offset, len);
      reader->offset += len;
      return true;
    }

    case LUA_SERIAL_TABLE: {
      if (depth >= LUA_SERIAL_MAX_DEPTH) {
        lua_serial_error(reader->error, reader->error_cap, "serialized worker table is nested too deeply");
        return false;
      }

      uint32_t count = 0;
      if (!lua_serial_reader_read(reader, &count, sizeof(count)) || count > LUA_SERIAL_MAX_ENTRIES) {
        lua_serial_error(reader->error, reader->error_cap, "serialized worker table has an invalid entry count");
        return false;
      }

      lua_createtable(state, 0, (int)MIN_VAL(count, (uint32_t)INT32_MAX));
      for (uint32_t i = 0; i < count; ++i) {
        if (!lua_serial_decode_value(state, reader, depth + 1) || !lua_serial_decode_value(state, reader, depth + 1)) {
          lua_pop(state, 1);
          return false;
        }

        if (!((lua_type(state, -2) == LUA_TNUMBER && lua_isinteger(state, -2)) || lua_type(state, -2) == LUA_TSTRING)) {
          lua_pop(state, 3);
          lua_serial_error(reader->error, reader->error_cap, "serialized worker table contains an invalid key");
          return false;
        }

        lua_rawset(state, -3);
      }
      return true;
    }

    default: {
      lua_serial_error(reader->error, reader->error_cap, "serialized worker value has an unknown type tag");
      return false;
    }
  }
}

bool
lua_serial_decode(lua_State *state, lua_serial_blob_t blob, char *error, size_t error_cap)
{
  if (error && error_cap > 0) {
    error[0] = 0;
  }

  if (!state || !blob.data || blob.len == 0) {
    lua_serial_error(error, error_cap, "serialized worker value is empty");
    return false;
  }

  int stack_base = lua_gettop(state);
  lua_serial_reader_t reader = {
    .blob      = blob,
    .error     = error,
    .error_cap = error_cap,
  };

  if (!lua_serial_decode_value(state, &reader, 0) || reader.offset != blob.len) {
    lua_settop(state, stack_base);
    if (reader.offset != blob.len) {
      lua_serial_error(error, error_cap, "serialized worker value contains trailing data");
    }
    return false;
  }
  return true;
}
