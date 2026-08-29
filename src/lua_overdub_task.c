#include "lua_overdub_task.h"

#include "config.h"
#include "globals.h"
#include "log.h"
#include "lua_overdub.h"
#include "lua_overdub_worker.h"
#include "lua_serial.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define LUA_OVERDUB_TASK_HANDLE_META "overdub.task.Task"
#define LUA_OVERDUB_FUTURE_META      "overdub.task.Future"
#define LUA_OVERDUB_TASK_NAME_CAP    (64)
#define LUA_OVERDUB_WORKER_NAME_CAP  (512)

typedef uint8_t lua_overdub_task_state_t;
enum {
  LUA_OVERDUB_TASK_PENDING = 0,
  LUA_OVERDUB_TASK_RUNNING,
  LUA_OVERDUB_TASK_WAIT_FRAME,
  LUA_OVERDUB_TASK_WAIT_TIME,
  LUA_OVERDUB_TASK_DONE,
  LUA_OVERDUB_TASK_CANCELLED,
  LUA_OVERDUB_TASK_FAILED,
};

typedef struct lua_overdub_task_s lua_overdub_task_t;
struct lua_overdub_task_s {
  lua_State                *thread;
  uint64_t                  id;
  uint64_t                  ready_frame;
  uint64_t                  due_us;
  int                       thread_ref;
  int                       handle_ref;
  lua_overdub_task_state_t  state;
  bool                      active;
  bool                      cancel_requested;
  char                      name[LUA_OVERDUB_TASK_NAME_CAP];
};

typedef struct lua_overdub_task_handle_s lua_overdub_task_handle_t;
struct lua_overdub_task_handle_s {
  lua_overdub_task_context_t *context;
  uint64_t                    id;
  lua_overdub_task_state_t    terminal_state;
};

typedef uint8_t lua_overdub_future_state_t;
enum {
  LUA_OVERDUB_FUTURE_PENDING = 0,
  LUA_OVERDUB_FUTURE_SUCCESS,
  LUA_OVERDUB_FUTURE_ERROR,
  LUA_OVERDUB_FUTURE_CANCELLED,
};

typedef struct lua_overdub_future_handle_s lua_overdub_future_handle_t;
struct lua_overdub_future_handle_s {
  lua_overdub_task_context_t *context;
  uint64_t                    id;
  lua_overdub_future_state_t  state;
  int                         success_ref;
  int                         error_ref;
  int                         complete_ref;
  lua_serial_blob_t           result;
  char                        error[1024];
};

typedef struct lua_overdub_future_s lua_overdub_future_t;
struct lua_overdub_future_s {
  uint64_t id;
  int      handle_ref;
  bool     active;
};

struct lua_overdub_task_context_s {
  lua_runtime_t                 *runtime;
  lua_overdub_worker_context_t  *worker;
  str_t                          mod_id;
  lua_overdub_task_t             tasks[CONFIG_LUA_OVERDUB_MAX_TASKS];
  lua_overdub_future_t           futures[CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS];
  lua_overdub_task_t            *current;
  uint64_t                       current_resume_start_us;
  uint64_t                       next_id;
  int                            next_slot;
  int                            active_count;
  int                            future_count;
  bool                           dispatching;
};

static char g_lua_overdub_task_context_key;

static uint64_t
lua_overdub_task_next_frame(void)
{
  return globals.frame_counter == UINT64_MAX ? UINT64_MAX : globals.frame_counter + 1;
}

static lua_overdub_task_context_t *
lua_overdub_task_context_get(lua_State *state)
{
  lua_pushlightuserdata(state, &g_lua_overdub_task_context_key);
  lua_rawget(state, LUA_REGISTRYINDEX);
  lua_overdub_task_context_t *context = lua_touserdata(state, -1);
  lua_pop(state, 1);
  return context;
}

static lua_overdub_task_context_t *
lua_overdub_task_context_check(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_overdub_task_context_t *context = lua_overdub_task_context_get(state);
  if (!context || !context->runtime || !context->runtime->state) {
    luaL_error(state, "Overdub task context is unavailable");
  }
  return context;
}

static lua_overdub_task_t *
lua_overdub_task_find(lua_overdub_task_context_t *context, uint64_t id)
{
  if (!context || id == 0) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_TASKS; ++i) {
    lua_overdub_task_t *task = &context->tasks[i];
    if (task->active && task->id == id) {
      return task;
    }
  }
  return NULL;
}

static lua_overdub_task_t *
lua_overdub_task_from_thread(lua_overdub_task_context_t *context, lua_State *thread)
{
  if (context && context->current && context->current->thread == thread) {
    return context->current;
  }
  return NULL;
}

static void
lua_overdub_task_finish(lua_overdub_task_context_t *context, lua_overdub_task_t *task, lua_overdub_task_state_t state)
{
  if (!context || !task || !task->active) {
    return;
  }

  lua_State *main_state = context->runtime->state;
  if (task->handle_ref != LUA_NOREF && task->handle_ref != LUA_REFNIL) {
    lua_rawgeti(main_state, LUA_REGISTRYINDEX, task->handle_ref);
    lua_overdub_task_handle_t *handle = luaL_testudata(main_state, -1, LUA_OVERDUB_TASK_HANDLE_META);
    if (handle && handle->id == task->id) {
      handle->terminal_state = state;
    }

    lua_pop(main_state, 1);
    luaL_unref(main_state, LUA_REGISTRYINDEX, task->handle_ref);
  }

  if (task->thread_ref != LUA_NOREF && task->thread_ref != LUA_REFNIL) {
    luaL_unref(main_state, LUA_REGISTRYINDEX, task->thread_ref);
  }

  *task = (lua_overdub_task_t){
    .thread_ref = LUA_NOREF,
    .handle_ref = LUA_NOREF,
  };
  context->active_count = MAX_VAL(context->active_count - 1, 0);
}

static lua_overdub_task_handle_t *
lua_overdub_task_check_handle(lua_State *state)
{
  lua_overdub_task_handle_t  *handle  = luaL_checkudata(state, 1, LUA_OVERDUB_TASK_HANDLE_META);
  lua_overdub_task_context_t *context = lua_overdub_task_context_check(state);
  if (!handle->context || handle->context != context || handle->id == 0) {
    luaL_error(state, "Task handle is invalid");
  }
  return handle;
}

static int
lua_overdub_task_handle_cancel(lua_State *state)
{
  lua_overdub_task_handle_t *handle = lua_overdub_task_check_handle(state);
  lua_overdub_task_t        *task   = lua_overdub_task_find(handle->context, handle->id);
  if (!task || task->cancel_requested) {
    lua_pushboolean(state, false);
    return 1;
  }

  task->cancel_requested = true;
  if (task->state != LUA_OVERDUB_TASK_RUNNING) {
    lua_overdub_task_finish(handle->context, task, LUA_OVERDUB_TASK_CANCELLED);
  }
  lua_pushboolean(state, true);
  return 1;
}

static int
lua_overdub_task_handle_is_done(lua_State *state)
{
  lua_overdub_task_handle_t *handle = lua_overdub_task_check_handle(state);
  bool active = lua_overdub_task_find(handle->context, handle->id) != NULL;
  lua_pushboolean(state, !active && (handle->terminal_state == LUA_OVERDUB_TASK_DONE      ||
                                     handle->terminal_state == LUA_OVERDUB_TASK_CANCELLED ||
                                     handle->terminal_state == LUA_OVERDUB_TASK_FAILED));
  return 1;
}

static int
lua_overdub_task_handle_is_cancelled(lua_State *state)
{
  lua_overdub_task_handle_t *handle = lua_overdub_task_check_handle(state);
  lua_overdub_task_t        *task   = lua_overdub_task_find(handle->context, handle->id);
  lua_pushboolean(state, (task && task->cancel_requested) || (!task && handle->terminal_state == LUA_OVERDUB_TASK_CANCELLED));
  return 1;
}

static int
lua_overdub_task_spawn(lua_State *state)
{
  lua_overdub_task_context_t *context      = lua_overdub_task_context_check(state);
  int                         count        = lua_gettop(state);
  int                         function_idx = 1;
  const char                 *name_data    = NULL;
  size_t                      name_len     = 0;

  if (count == 1) {
    function_idx = 1;
  } else if (count == 2) {
    name_data    = luaL_checklstring(state, 1, &name_len);
    function_idx = 2;
  } else {
    return luaL_error(state, "expected Spawn(function) or Spawn(name, function)");
  }
  luaL_checktype(state, function_idx, LUA_TFUNCTION);

  lua_overdub_task_t *task = NULL;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_TASKS; ++i) {
    if (!context->tasks[i].active) {
      task = &context->tasks[i];
      break;
    }
  }

  if (!task) {
    return luaL_error(state, "Overdub task limit reached");
  }

  uint64_t id = context->next_id++;
  if (id == 0) {
    id = context->next_id++;
  }

  lua_State *thread     = lua_newthread(state);
  int        thread_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_pushvalue(state, function_idx);
  lua_xmove(state, thread, 1);

  lua_overdub_task_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_task_handle_t){
    .context        = context,
    .id             = id,
    .terminal_state = LUA_OVERDUB_TASK_PENDING,
  };
  luaL_setmetatable(state, LUA_OVERDUB_TASK_HANDLE_META);
  lua_pushvalue(state, -1);

  int handle_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  *task = (lua_overdub_task_t){
    .thread      = thread,
    .id          = id,
    .ready_frame = context->dispatching ? lua_overdub_task_next_frame() : globals.frame_counter,
    .thread_ref  = thread_ref,
    .handle_ref  = handle_ref,
    .state       = LUA_OVERDUB_TASK_PENDING,
    .active      = true,
  };

  if (name_data && name_len > 0) {
    size_t copy_len = MIN_VAL(name_len, sizeof(task->name) - 1);
    memcpy(task->name, name_data, copy_len);
    task->name[copy_len] = 0;
  } else {
    snprintf(task->name, sizeof(task->name), "task-%llu", (unsigned long long)id);
  }

  context->active_count += 1;
  return 1;
}

static lua_overdub_task_t *
lua_overdub_task_check_current(lua_State *state, lua_overdub_task_context_t **out_context)
{
  lua_overdub_task_context_t *context = lua_overdub_task_context_check(state);
  lua_overdub_task_t         *task    = lua_overdub_task_from_thread(context, state);
  if (!task || task->state != LUA_OVERDUB_TASK_RUNNING) {
    luaL_error(state, "this function can only be called from a task spawned by task.Spawn");
  }

  if (out_context) {
    *out_context = context;
  }
  return task;
}

static int
lua_overdub_task_yield_frame(lua_State *state)
{
  lua_overdub_task_t *task = lua_overdub_task_check_current(state, NULL);
  task->state       = LUA_OVERDUB_TASK_WAIT_FRAME;
  task->ready_frame = lua_overdub_task_next_frame();
  task->due_us      = 0;
  return lua_yield(state, 0);
}

static int
lua_overdub_task_sleep(lua_State *state)
{
  lua_overdub_task_t *task = lua_overdub_task_check_current(state, NULL);
  lua_Number seconds = luaL_checknumber(state, 1);
  if (!(seconds >= 0.0) || seconds > (lua_Number)(UINT64_MAX / 1000000ULL)) {
    return luaL_argerror(state, 1, "seconds must be finite and non-negative");
  }

  uint64_t now      = time_now_us();
  uint64_t delay_us = (uint64_t)(seconds * 1000000.0);
  task->state       = LUA_OVERDUB_TASK_WAIT_TIME;
  task->ready_frame = lua_overdub_task_next_frame();
  task->due_us      = delay_us > UINT64_MAX - now ? UINT64_MAX : now + delay_us;
  return lua_yield(state, 0);
}

static int
lua_overdub_task_should_yield(lua_State *state)
{
  lua_overdub_task_context_t *context = NULL;
  lua_overdub_task_check_current(state, &context);

  uint64_t elapsed = time_now_us() - context->current_resume_start_us;
  lua_pushboolean(state, elapsed >= CONFIG_LUA_OVERDUB_TASK_WORK_BUDGET_US);
  return 1;
}

static lua_overdub_future_t *
lua_overdub_future_find(lua_overdub_task_context_t *context, uint64_t id)
{
  if (!context || id == 0) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS; ++i) {
    if (context->futures[i].active && context->futures[i].id == id) {
      return &context->futures[i];
    }
  }
  return NULL;
}

static lua_overdub_future_handle_t *
lua_overdub_future_check(lua_State *state)
{
  lua_overdub_future_handle_t *handle  = luaL_checkudata(state, 1, LUA_OVERDUB_FUTURE_META);
  lua_overdub_task_context_t  *context = lua_overdub_task_context_check(state);
  if (!handle->context || handle->context != context || handle->id == 0) {
    luaL_error(state, "Future handle is invalid");
  }
  return handle;
}

static void
lua_overdub_future_unref(lua_State *state, int *ref)
{
  if (ref && *ref != LUA_NOREF && *ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_NOREF;
  }
}

static void
lua_overdub_future_clear_callbacks(lua_State *state, lua_overdub_future_handle_t *handle)
{
  lua_overdub_future_unref(state, &handle->success_ref);
  lua_overdub_future_unref(state, &handle->error_ref);
  lua_overdub_future_unref(state, &handle->complete_ref);
}

static void
lua_overdub_future_release_record(lua_overdub_task_context_t *context, lua_overdub_future_t *future)
{
  if (!context || !future || !future->active) {
    return;
  }

  if (future->handle_ref != LUA_NOREF && future->handle_ref != LUA_REFNIL) {
    luaL_unref(context->runtime->state, LUA_REGISTRYINDEX, future->handle_ref);
  }

  *future = (lua_overdub_future_t){.handle_ref = LUA_NOREF};
  context->future_count = MAX_VAL(context->future_count - 1, 0);
}

static int
lua_overdub_future_push_result(lua_State *state, lua_overdub_future_handle_t *handle)
{
  if (handle->state == LUA_OVERDUB_FUTURE_PENDING) {
    return luaL_error(state, "Future is still pending");
  }

  if (handle->state == LUA_OVERDUB_FUTURE_CANCELLED) {
    return luaL_error(state, "Future was cancelled");
  }

  if (handle->state == LUA_OVERDUB_FUTURE_ERROR) {
    return luaL_error(state, "%s", handle->error[0] ? handle->error : "worker job failed");
  }

  char error[256] = {0};
  if (!lua_serial_decode(state, handle->result, error, sizeof(error))) {
    return luaL_error(state, "could not decode worker result: %s", error);
  }
  return 1;
}

static int
lua_overdub_future_is_pending_lua(lua_State *state)
{
  lua_pushboolean(state, lua_overdub_future_check(state)->state == LUA_OVERDUB_FUTURE_PENDING);
  return 1;
}

static int
lua_overdub_future_is_done_lua(lua_State *state)
{
  lua_pushboolean(state, lua_overdub_future_check(state)->state != LUA_OVERDUB_FUTURE_PENDING);
  return 1;
}

static int
lua_overdub_future_is_cancelled_lua(lua_State *state)
{
  lua_pushboolean(state, lua_overdub_future_check(state)->state == LUA_OVERDUB_FUTURE_CANCELLED);
  return 1;
}

static int
lua_overdub_future_get_result_lua(lua_State *state)
{
  return lua_overdub_future_push_result(state, lua_overdub_future_check(state));
}

static int
lua_overdub_future_get_error_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  if (handle->state == LUA_OVERDUB_FUTURE_ERROR) {
    lua_pushstring(state, handle->error[0] ? handle->error : "worker job failed");
  } else if (handle->state == LUA_OVERDUB_FUTURE_CANCELLED) {
    lua_pushliteral(state, "cancelled");
  } else {
    lua_pushnil(state);
  }
  return 1;
}

static int
lua_overdub_future_set_callback(lua_State *state, int *ref_slot, lua_overdub_future_state_t matching_state, bool complete)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  if (handle->state == LUA_OVERDUB_FUTURE_PENDING) {
    lua_overdub_future_unref(state, ref_slot);
    lua_pushvalue(state, 2);
    *ref_slot = luaL_ref(state, LUA_REGISTRYINDEX);
  } else if (complete || handle->state == matching_state || (matching_state == LUA_OVERDUB_FUTURE_ERROR && handle->state == LUA_OVERDUB_FUTURE_CANCELLED)) {
    lua_pushvalue(state, 2);
    if (complete) {
      lua_pushvalue(state, 1);
    } else if (matching_state == LUA_OVERDUB_FUTURE_SUCCESS) {
      lua_overdub_future_push_result(state, handle);
    } else {
      lua_pushstring(state, handle->state == LUA_OVERDUB_FUTURE_CANCELLED ? "cancelled" : (handle->error[0] ? handle->error : "worker job failed"));
    }
    lua_call(state, 1, 0);
  }

  lua_pushvalue(state, 1);
  return 1;
}

static int
lua_overdub_future_on_success_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  return lua_overdub_future_set_callback(state, &handle->success_ref, LUA_OVERDUB_FUTURE_SUCCESS, false);
}

static int
lua_overdub_future_on_error_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  return lua_overdub_future_set_callback(state, &handle->error_ref, LUA_OVERDUB_FUTURE_ERROR, false);
}

static int
lua_overdub_future_on_complete_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  return lua_overdub_future_set_callback(state, &handle->complete_ref, LUA_OVERDUB_FUTURE_PENDING, true);
}

static int
lua_overdub_future_cancel_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  if (handle->state != LUA_OVERDUB_FUTURE_PENDING) {
    lua_pushboolean(state, false);
    return 1;
  }

  lua_overdub_future_t *future = lua_overdub_future_find(handle->context, handle->id);
  if (!future || !lua_overdub_worker_cancel(handle->context->worker, handle->id)) {
    lua_pushboolean(state, false);
    return 1;
  }

  handle->state = LUA_OVERDUB_FUTURE_CANCELLED;
  snprintf(handle->error, sizeof(handle->error), "cancelled");

  int error_ref    = handle->error_ref;
  int complete_ref = handle->complete_ref;

  handle->error_ref    = LUA_NOREF;
  handle->complete_ref = LUA_NOREF;
  lua_overdub_future_unref(state, &handle->success_ref);

  if (error_ref != LUA_NOREF && error_ref != LUA_REFNIL) {
    lua_pushliteral(state, "cancelled");
    lua_runtime_call_ref(handle->context->runtime, error_ref, 1, 0);
    luaL_unref(state, LUA_REGISTRYINDEX, error_ref);
  }

  if (complete_ref != LUA_NOREF && complete_ref != LUA_REFNIL) {
    lua_pushvalue(state, 1);
    lua_runtime_call_ref(handle->context->runtime, complete_ref, 1, 0);
    luaL_unref(state, LUA_REGISTRYINDEX, complete_ref);
  }

  lua_overdub_future_release_record(handle->context, future);
  lua_pushboolean(state, true);
  return 1;
}

static int lua_overdub_future_await_continue(lua_State *state, int status, lua_KContext continuation);

static int
lua_overdub_future_await_continue(lua_State *state, int status, lua_KContext continuation)
{
  (void)status;

  lua_overdub_future_handle_t *handle  = (lua_overdub_future_handle_t *)(uintptr_t)continuation;
  lua_overdub_task_context_t  *context = lua_overdub_task_context_check(state);

  if (!handle || handle->context != context) {
    return luaL_error(state, "Future handle is invalid");
  }

  if (handle->state == LUA_OVERDUB_FUTURE_PENDING) {
    lua_overdub_task_t *task = lua_overdub_task_check_current(state, NULL);
    task->state       = LUA_OVERDUB_TASK_WAIT_FRAME;
    task->ready_frame = lua_overdub_task_next_frame();
    task->due_us      = 0;
    return lua_yieldk(state, 0, continuation, lua_overdub_future_await_continue);
  }
  return lua_overdub_future_push_result(state, handle);
}

static int
lua_overdub_future_await_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = lua_overdub_future_check(state);
  lua_overdub_task_check_current(state, NULL);
  return lua_overdub_future_await_continue(state, LUA_OK, (lua_KContext)(uintptr_t)handle);
}

static int
lua_overdub_future_gc_lua(lua_State *state)
{
  lua_overdub_future_handle_t *handle = luaL_checkudata(state, 1, LUA_OVERDUB_FUTURE_META);
  lua_serial_blob_free(&handle->result);
  lua_overdub_future_clear_callbacks(state, handle);
  handle->context = NULL;
  return 0;
}

static int
lua_overdub_task_offload(lua_State *state)
{
  lua_overdub_task_context_t *context = lua_overdub_task_context_check(state);

  size_t module_len   = 0;
  size_t function_len = 0;

  const char *module_name   = luaL_checklstring(state, 1, &module_len);
  const char *function_name = luaL_checklstring(state, 2, &function_len);

  if (module_len == 0 || function_len == 0 || module_len >= LUA_OVERDUB_WORKER_NAME_CAP ||
      function_len >= LUA_OVERDUB_WORKER_NAME_CAP || memchr(module_name, 0, module_len) ||
      memchr(function_name, 0, function_len)) {
    return luaL_error(state, "worker module and function names must be non-empty, shorter than %d bytes, and contain no null bytes", LUA_OVERDUB_WORKER_NAME_CAP);
  }

  if (lua_gettop(state) != 3) {
    return luaL_error(state, "expected Offload(module_name, function_name, value)");
  }

  lua_overdub_future_t *future = NULL;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS; ++i) {
    if (!context->futures[i].active) {
      future = &context->futures[i];
      break;
    }
  }

  if (!future) {
    return luaL_error(state, "Overdub Future limit reached");
  }

  lua_serial_blob_t input = {0};
  char error[256] = {0};
  if (!lua_serial_encode(state, 3, &input, error, sizeof(error))) {
    return luaL_argerror(state, 3, error[0] ? error : "value cannot cross a worker boundary");
  }

  uint64_t id = 0;
  if (!lua_overdub_worker_submit(context->worker, str_make((void *)module_name, (uint64_t)module_len), str_make((void *)function_name, (uint64_t)function_len), &input, &id, error, sizeof(error))) {
    lua_serial_blob_free(&input);
    return luaL_error(state, "%s", error[0] ? error : "could not submit worker job");
  }

  lua_overdub_future_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_future_handle_t){
    .context      = context,
    .id           = id,
    .state        = LUA_OVERDUB_FUTURE_PENDING,
    .success_ref  = LUA_NOREF,
    .error_ref    = LUA_NOREF,
    .complete_ref = LUA_NOREF,
  };

  luaL_setmetatable(state, LUA_OVERDUB_FUTURE_META);
  lua_pushvalue(state, -1);
  int handle_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  *future = (lua_overdub_future_t){
    .id         = id,
    .handle_ref = handle_ref,
    .active     = true,
  };
  context->future_count += 1;
  return 1;
}

static int
luaopen_overdub_task(lua_State *state)
{
  lua_overdub_task_context_check(state);
  static const luaL_Reg functions[] = {
    {"Spawn",       lua_overdub_task_spawn       },
    {"YieldFrame",  lua_overdub_task_yield_frame },
    {"Sleep",       lua_overdub_task_sleep       },
    {"ShouldYield", lua_overdub_task_should_yield},
    {"Offload",     lua_overdub_task_offload     },
    {NULL,          NULL                         },
  };
  luaL_newlib(state, functions);
  return 1;
}

lua_overdub_task_context_t *
lua_overdub_task_context_create(arena_t *arena, lua_runtime_t *runtime, lua_overdub_worker_context_t *worker, str_t mod_id)
{
  if (!arena || !arena->backing || !runtime || !runtime->state || !worker) {
    return NULL;
  }

  lua_overdub_task_context_t *context = ARENA_PUSH_ZERO(arena, lua_overdub_task_context_t);
  if (!context) {
    return NULL;
  }

  context->runtime = runtime;
  context->worker  = worker;
  context->mod_id  = mod_id;
  context->next_id = 1;

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_TASKS; ++i) {
    context->tasks[i].thread_ref = LUA_NOREF;
    context->tasks[i].handle_ref = LUA_NOREF;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS; ++i) {
    context->futures[i].handle_ref = LUA_NOREF;
  }

  return context;
}

void
lua_overdub_task_context_destroy(lua_overdub_task_context_t *context)
{
  if (!context) {
    return;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_TASKS; ++i) {
    lua_overdub_task_t *task = &context->tasks[i];
    if (task->active) {
      lua_overdub_task_finish(context, task, LUA_OVERDUB_TASK_CANCELLED);
    }
  }

  lua_State *state = context->runtime ? context->runtime->state : NULL;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS; ++i) {
    lua_overdub_future_t *future = &context->futures[i];
    if (!future->active) {
      continue;
    }

    if (state && future->handle_ref != LUA_NOREF && future->handle_ref != LUA_REFNIL) {
      lua_rawgeti(state, LUA_REGISTRYINDEX, future->handle_ref);

      lua_overdub_future_handle_t *handle = luaL_testudata(state, -1, LUA_OVERDUB_FUTURE_META);
      if (handle && handle->id == future->id) {
        handle->state   = LUA_OVERDUB_FUTURE_CANCELLED;
        handle->context = NULL;
        snprintf(handle->error, sizeof(handle->error), "cancelled");
        lua_overdub_future_clear_callbacks(state, handle);
      }
      lua_pop(state, 1);
    }

    lua_overdub_worker_cancel(context->worker, future->id);
    lua_overdub_future_release_record(context, future);
  }
}

bool
lua_overdub_task_register(lua_State *state, lua_overdub_task_context_t *context)
{
  if (!state || !context) {
    return false;
  }

  lua_pushlightuserdata(state, &g_lua_overdub_task_context_key);
  lua_pushlightuserdata(state, context);
  lua_rawset(state, LUA_REGISTRYINDEX);

  if (luaL_newmetatable(state, LUA_OVERDUB_TASK_HANDLE_META)) {
    static const luaL_Reg methods[] = {
      {"Cancel",      lua_overdub_task_handle_cancel      },
      {"IsDone",      lua_overdub_task_handle_is_done     },
      {"IsCancelled", lua_overdub_task_handle_is_cancelled},
      {NULL,          NULL                                },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushliteral(state, "Overdub Task");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  if (luaL_newmetatable(state, LUA_OVERDUB_FUTURE_META)) {
    static const luaL_Reg methods[] = {
      {"IsPending",  lua_overdub_future_is_pending_lua  },
      {"IsDone",     lua_overdub_future_is_done_lua     },
      {"IsCancelled",lua_overdub_future_is_cancelled_lua},
      {"Cancel",     lua_overdub_future_cancel_lua      },
      {"OnSuccess",  lua_overdub_future_on_success_lua  },
      {"OnError",    lua_overdub_future_on_error_lua    },
      {"OnComplete", lua_overdub_future_on_complete_lua },
      {"Await",      lua_overdub_future_await_lua       },
      {"GetResult",  lua_overdub_future_get_result_lua  },
      {"GetError",   lua_overdub_future_get_error_lua   },
      {NULL,         NULL                               },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, lua_overdub_future_gc_lua);
    lua_setfield(state, -2, "__gc");
    lua_pushliteral(state, "Overdub Future");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);
  return lua_overdub_preload_module(state, "overdub.task", luaopen_overdub_task);
}

uint64_t
lua_overdub_task_dispatch(lua_overdub_task_context_t *context, uint64_t budget_us)
{
  if (!context || !context->runtime || !context->runtime->state || context->dispatching || budget_us == 0) {
    return 0;
  }

  uint64_t dispatch_start = time_now_us();
  context->dispatching = true;
  int scan_start = context->next_slot;
  for (int offset = 0; offset < CONFIG_LUA_OVERDUB_MAX_TASKS; ++offset) {
    if (time_now_us() - dispatch_start >= budget_us) {
      break;
    }

    int idx = (scan_start + offset) % CONFIG_LUA_OVERDUB_MAX_TASKS;
    lua_overdub_task_t *task = &context->tasks[idx];
    if (!task->active || task->cancel_requested || task->ready_frame > globals.frame_counter) {
      continue;
    }

    uint64_t now = time_now_us();
    if (task->state == LUA_OVERDUB_TASK_WAIT_TIME && task->due_us > now) {
      continue;
    }

    if (task->state != LUA_OVERDUB_TASK_PENDING && task->state != LUA_OVERDUB_TASK_WAIT_FRAME && task->state != LUA_OVERDUB_TASK_WAIT_TIME) {
      continue;
    }

    context->next_slot               = (idx + 1) % CONFIG_LUA_OVERDUB_MAX_TASKS;
    context->current                 = task;
    context->current_resume_start_us = now;
    task->state                      = LUA_OVERDUB_TASK_RUNNING;

    uint64_t previous_limit = context->runtime->execution_limit_us;
    context->runtime->execution_limit_us = CONFIG_LUA_OVERDUB_CALLBACK_LIMIT_US;

    int result_count = 0;
    int status       = lua_runtime_resume(context->runtime, task->thread, 0, &result_count);

    context->runtime->execution_limit_us = previous_limit;
    context->current                     = NULL;

    if ((status == LUA_OK || status == LUA_YIELD) && result_count > 0) {
      lua_pop(task->thread, result_count);
    }

    if (task->cancel_requested) {
      lua_overdub_task_finish(context, task, LUA_OVERDUB_TASK_CANCELLED);
    } else if (status == LUA_OK) {
      lua_overdub_task_finish(context, task, LUA_OVERDUB_TASK_DONE);
    } else if (status == LUA_YIELD) {
      if (task->state == LUA_OVERDUB_TASK_RUNNING) {
        task->state       = LUA_OVERDUB_TASK_WAIT_FRAME;
        task->ready_frame = lua_overdub_task_next_frame();
        task->due_us      = 0;
      }
    } else {
      LOG_ERROR("Overdub Lua mod '%.*s': task '%s' failed and was stopped", STR_ARG(context->mod_id), task->name);
      lua_overdub_task_finish(context, task, LUA_OVERDUB_TASK_FAILED);
    }
  }

  context->dispatching = false;
  return time_now_us() - dispatch_start;
}

static void
lua_overdub_future_call_ref(lua_overdub_task_context_t *context, bool *in_callback, int callback_ref, int arg_count)
{
  if (!context || !context->runtime || !in_callback || callback_ref == LUA_NOREF || callback_ref == LUA_REFNIL) {
    if (context && context->runtime && context->runtime->state && arg_count > 0) {
      lua_pop(context->runtime->state, arg_count);
    }
    return;
  }

  bool previous = *in_callback;
  *in_callback = true;
  lua_runtime_call_ref(context->runtime, callback_ref, arg_count, 0);
  *in_callback = previous;
  luaL_unref(context->runtime->state, LUA_REGISTRYINDEX, callback_ref);
}

uint64_t
lua_overdub_task_dispatch_worker_completions(lua_overdub_task_context_t *context, bool *in_callback, uint64_t budget_us)
{
  if (!context || !context->runtime || !context->runtime->state || !context->worker || !in_callback || *in_callback || budget_us == 0) {
    return 0;
  }

  lua_State *state = context->runtime->state;
  uint64_t start_us = time_now_us();
  while (time_now_us() - start_us < budget_us) {
    lua_overdub_worker_completion_t completion = {0};
    if (!lua_overdub_worker_poll(context->worker, &completion)) {
      break;
    }

    lua_overdub_future_t *future = lua_overdub_future_find(context, completion.id);
    if (!future || future->handle_ref == LUA_NOREF || future->handle_ref == LUA_REFNIL) {
      lua_overdub_worker_completion_destroy(&completion);
      continue;
    }

    int stack_base = lua_gettop(state);
    lua_rawgeti(state, LUA_REGISTRYINDEX, future->handle_ref);

    int                          handle_idx = lua_absindex(state, -1);
    lua_overdub_future_handle_t *handle     = luaL_testudata(state, handle_idx, LUA_OVERDUB_FUTURE_META);
    if (!handle || handle->id != completion.id || handle->state != LUA_OVERDUB_FUTURE_PENDING) {
      lua_settop(state, stack_base);
      lua_overdub_future_release_record(context, future);
      lua_overdub_worker_completion_destroy(&completion);
      continue;
    }

    if (completion.result == LUA_OVERDUB_WORKER_SUCCESS) {
      handle->state  = LUA_OVERDUB_FUTURE_SUCCESS;
      handle->result = completion.value;
      completion.value = (lua_serial_blob_t){0};
    } else if (completion.result == LUA_OVERDUB_WORKER_CANCELLED) {
      handle->state = LUA_OVERDUB_FUTURE_CANCELLED;
      snprintf(handle->error, sizeof(handle->error), "cancelled");
    } else {
      handle->state = LUA_OVERDUB_FUTURE_ERROR;
      snprintf(handle->error, sizeof(handle->error), "%s", completion.error[0] ? completion.error : "worker job failed");
    }

    int success_ref  = handle->success_ref;
    int error_ref    = handle->error_ref;
    int complete_ref = handle->complete_ref;

    handle->success_ref  = LUA_NOREF;
    handle->error_ref    = LUA_NOREF;
    handle->complete_ref = LUA_NOREF;

    if (handle->state == LUA_OVERDUB_FUTURE_SUCCESS && success_ref != LUA_NOREF && success_ref != LUA_REFNIL) {
      char decode_error[256] = {0};
      if (lua_serial_decode(state, handle->result, decode_error, sizeof(decode_error))) {
        lua_overdub_future_call_ref(context, in_callback, success_ref, 1);
      } else {
        luaL_unref(state, LUA_REGISTRYINDEX, success_ref);
        LOG_ERROR("Overdub Lua mod '%.*s': could not decode worker result: %s", STR_ARG(context->mod_id), decode_error);
      }
    } else {
      lua_overdub_future_unref(state, &success_ref);
    }

    if ((handle->state == LUA_OVERDUB_FUTURE_ERROR || handle->state == LUA_OVERDUB_FUTURE_CANCELLED) &&
        error_ref != LUA_NOREF && error_ref != LUA_REFNIL) {
      lua_pushstring(state, handle->state == LUA_OVERDUB_FUTURE_CANCELLED ? "cancelled" : (handle->error[0] ? handle->error : "worker job failed"));
      lua_overdub_future_call_ref(context, in_callback, error_ref, 1);
    } else {
      lua_overdub_future_unref(state, &error_ref);
    }

    if (complete_ref != LUA_NOREF && complete_ref != LUA_REFNIL) {
      lua_pushvalue(state, handle_idx);
      lua_overdub_future_call_ref(context, in_callback, complete_ref, 1);
    }

    lua_settop(state, stack_base);
    lua_overdub_future_release_record(context, future);
    lua_overdub_worker_completion_destroy(&completion);
  }
  return time_now_us() - start_us;
}

int
lua_overdub_task_active_count(lua_overdub_task_context_t *context)
{
  return context ? context->active_count : 0;
}

int
lua_overdub_task_future_count(lua_overdub_task_context_t *context)
{
  return context && context->worker ? lua_overdub_worker_outstanding_count(context->worker) : 0;
}
