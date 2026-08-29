#ifndef LUA_OVERDUB_WORKER_H
#define LUA_OVERDUB_WORKER_H

#include "lua_serial.h"
#include "str.h"
#include "types.h"

typedef struct lua_overdub_worker_context_s lua_overdub_worker_context_t;

typedef uint8_t lua_overdub_worker_result_t;
enum {
  LUA_OVERDUB_WORKER_SUCCESS = 0,
  LUA_OVERDUB_WORKER_ERROR,
  LUA_OVERDUB_WORKER_CANCELLED,
};

typedef struct lua_overdub_worker_completion_s lua_overdub_worker_completion_t;
struct lua_overdub_worker_completion_s {
  uint64_t                    id;
  lua_overdub_worker_result_t result;
  lua_serial_blob_t           value;
  char                        error[1024];
};

lua_overdub_worker_context_t *
lua_overdub_worker_context_create(arena_t *arena, str_t mod_id, str_t entry_path, str_t mod_dir, uint32_t owner_thread_id);
void
lua_overdub_worker_context_destroy(lua_overdub_worker_context_t *context);
bool
lua_overdub_worker_submit(lua_overdub_worker_context_t *context, str_t module_name, str_t function_name,
                          lua_serial_blob_t *input, uint64_t *out_id, char *error, size_t error_cap);
bool
lua_overdub_worker_cancel(lua_overdub_worker_context_t *context, uint64_t id);
bool
lua_overdub_worker_poll(lua_overdub_worker_context_t *context, lua_overdub_worker_completion_t *out_completion);
void
lua_overdub_worker_completion_destroy(lua_overdub_worker_completion_t *completion);
int
lua_overdub_worker_outstanding_count(lua_overdub_worker_context_t *context);

#endif /* LUA_OVERDUB_WORKER_H */
