#include "lua_overdub_worker.h"

#include "config.h"
#include "log.h"
#include "lua_overdub.h"
#include "lua_runtime.h"
#include "scratch.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef struct lua_overdub_worker_job_s lua_overdub_worker_job_t;
#define LUA_OVERDUB_WORKER_NAME_CAP (512)
struct lua_overdub_worker_job_s {
  uint64_t          id;
  char              module_name[LUA_OVERDUB_WORKER_NAME_CAP];
  char              function_name[LUA_OVERDUB_WORKER_NAME_CAP];
  lua_serial_blob_t input;
};

struct lua_overdub_worker_context_s {
  str_t                           mod_id;
  str_t                           entry_path;
  str_t                           mod_dir;
  uint32_t                        owner_thread_id;
  HANDLE                          thread;
  HANDLE                          wake_event;
  CRITICAL_SECTION                lock;
  lua_overdub_worker_job_t        jobs[CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS];
  lua_overdub_worker_completion_t completions[CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS];
  int                             job_count;
  int                             completion_count;
  int                             outstanding_count;
  uint64_t                        next_id;
  uint64_t                        running_id;
  bool                            running_cancelled;
  volatile LONG                   stop_requested;
};

static void
lua_overdub_worker_error(char *error, size_t error_cap, const char *fmt, ...)
{
  if (!error || error_cap == 0) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(error, error_cap, fmt, args);
  va_end(args);

  error[error_cap - 1] = 0;
}

static bool
lua_overdub_worker_name_valid(str_t name)
{
  return !str_is_empty(name) && name.len < LUA_OVERDUB_WORKER_NAME_CAP && !memchr(name.data, 0, (size_t)name.len);
}

static void
lua_overdub_worker_job_destroy(lua_overdub_worker_job_t *job)
{
  if (!job) {
    return;
  }

  lua_serial_blob_free(&job->input);
  *job = (lua_overdub_worker_job_t){0};
}

void
lua_overdub_worker_completion_destroy(lua_overdub_worker_completion_t *completion)
{
  if (!completion) {
    return;
  }

  lua_serial_blob_free(&completion->value);
  *completion = (lua_overdub_worker_completion_t){0};
}

static void
lua_overdub_worker_completion_error(lua_overdub_worker_completion_t *completion, const char *message)
{
  completion->result = LUA_OVERDUB_WORKER_ERROR;
  snprintf(completion->error, sizeof(completion->error), "%s", message ? message : "worker job failed");
}

static bool
lua_overdub_worker_execute(lua_runtime_t *runtime, lua_overdub_worker_job_t *job, lua_overdub_worker_completion_t *completion)
{
  lua_State *state = runtime ? runtime->state : NULL;
  if (!state) {
    lua_overdub_worker_completion_error(completion, "worker Lua state is unavailable");
    return false;
  }

  lua_settop(state, 0);
  lua_getglobal(state, "require");
  lua_pushstring(state, job->module_name);

  char call_error[1024] = {0};
  if (!lua_runtime_pcall(runtime, 1, 1, call_error, sizeof(call_error))) {
    lua_overdub_worker_completion_error(completion, call_error);
    lua_settop(state, 0);
    return false;
  }

  if (!lua_istable(state, -1)) {
    lua_overdub_worker_completion_error(completion, "worker module must return a table");
    lua_settop(state, 0);
    return false;
  }

  lua_getfield(state, -1, job->function_name);
  if (!lua_isfunction(state, -1)) {
    lua_overdub_worker_completion_error(completion, "named worker function was not found");
    lua_settop(state, 0);
    return false;
  }
  lua_remove(state, -2);

  char codec_error[256] = {0};
  if (!lua_serial_decode(state, job->input, codec_error, sizeof(codec_error))) {
    lua_overdub_worker_completion_error(completion, codec_error);
    lua_settop(state, 0);
    return false;
  }

  if (!lua_runtime_pcall(runtime, 1, 1, call_error, sizeof(call_error))) {
    lua_overdub_worker_completion_error(completion, call_error);
    lua_settop(state, 0);
    return false;
  }

  if (!lua_serial_encode(state, -1, &completion->value, codec_error, sizeof(codec_error))) {
    lua_overdub_worker_completion_error(completion, codec_error);
    lua_settop(state, 0);
    return false;
  }

  completion->result = LUA_OVERDUB_WORKER_SUCCESS;
  lua_settop(state, 0);
  return true;
}

static unsigned __stdcall
lua_overdub_worker_main(void *user)
{
  lua_overdub_worker_context_t *context = user;
  lua_runtime_t                 runtime = {0};

  bool runtime_ready = lua_runtime_init_with_execution_limit(&runtime, CONFIG_LUA_OVERDUB_WORKER_LIMIT_US) &&
                       lua_runtime_enable_mod_libs(&runtime) &&
                       lua_overdub_configure_package_paths(runtime.state, context->entry_path, context->mod_dir);

  while (!InterlockedCompareExchange(&context->stop_requested, 0, 0)) {
    lua_overdub_worker_job_t job = {0};
    EnterCriticalSection(&context->lock);
    if (context->job_count > 0) {
      job = context->jobs[0];
      context->job_count -= 1;
      if (context->job_count > 0) {
        mem_move(context->jobs, context->jobs + 1, sizeof(*context->jobs) * (uint64_t)context->job_count);
      }

      context->jobs[context->job_count] = (lua_overdub_worker_job_t){0};
      context->running_id        = job.id;
      context->running_cancelled = false;
    }
    LeaveCriticalSection(&context->lock);

    if (!job.id) {
      WaitForSingleObject(context->wake_event, INFINITE);
      continue;
    }

    lua_overdub_worker_completion_t completion = {.id = job.id};
    if (runtime_ready) {
      lua_overdub_worker_execute(&runtime, &job, &completion);
    } else {
      lua_overdub_worker_completion_error(&completion, "worker Lua state failed to initialize");
    }

    EnterCriticalSection(&context->lock);
    if (context->running_cancelled) {
      lua_overdub_worker_completion_destroy(&completion);
      completion.id     = job.id;
      completion.result = LUA_OVERDUB_WORKER_CANCELLED;
    }

    context->running_id        = 0;
    context->running_cancelled = false;

    if (!InterlockedCompareExchange(&context->stop_requested, 0, 0) && context->completion_count < CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS) {
      context->completions[context->completion_count++] = completion;
      completion = (lua_overdub_worker_completion_t){0};
    }
    LeaveCriticalSection(&context->lock);

    lua_overdub_worker_completion_destroy(&completion);
    lua_overdub_worker_job_destroy(&job);
  }

  lua_runtime_deinit(&runtime);
  scratch_destroy();
  return 0;
}

lua_overdub_worker_context_t *
lua_overdub_worker_context_create(arena_t *arena, str_t mod_id, str_t entry_path, str_t mod_dir, uint32_t owner_thread_id)
{
  if (!arena || !arena->backing || str_is_empty(mod_id) || str_is_empty(entry_path) || str_is_empty(mod_dir) || owner_thread_id == 0) {
    return NULL;
  }

  lua_overdub_worker_context_t *context = ARENA_PUSH_ZERO(arena, lua_overdub_worker_context_t);
  if (!context) {
    return NULL;
  }

  context->mod_id          = str_push_copy(arena, mod_id);
  context->entry_path      = str_push_copy(arena, entry_path);
  context->mod_dir         = str_push_copy(arena, mod_dir);
  context->owner_thread_id = owner_thread_id;
  context->next_id         = 1;

  InitializeCriticalSection(&context->lock);
  context->wake_event = CreateEventA(NULL, FALSE, FALSE, NULL);
  if (str_is_empty(context->mod_id) || str_is_empty(context->entry_path) || str_is_empty(context->mod_dir) || !context->wake_event) {
    lua_overdub_worker_context_destroy(context);
    return NULL;
  }

  return context;
}

void
lua_overdub_worker_context_destroy(lua_overdub_worker_context_t *context)
{
  if (!context) {
    return;
  }

  InterlockedExchange(&context->stop_requested, 1);
  if (context->wake_event) {
    SetEvent(context->wake_event);
  }

  if (context->thread) {
    WaitForSingleObject(context->thread, INFINITE);
    CloseHandle(context->thread);
  }

  for (int i = 0; i < context->job_count; ++i) {
    lua_overdub_worker_job_destroy(&context->jobs[i]);
  }

  for (int i = 0; i < context->completion_count; ++i) {
    lua_overdub_worker_completion_destroy(&context->completions[i]);
  }

  if (context->wake_event) {
    CloseHandle(context->wake_event);
  }

  DeleteCriticalSection(&context->lock);
}

bool
lua_overdub_worker_submit(lua_overdub_worker_context_t *context, str_t module_name, str_t function_name,
                          lua_serial_blob_t *input, uint64_t *out_id, char *error, size_t error_cap)
{
  if (error && error_cap > 0) {
    error[0] = 0;
  }

  bool valid_context = context && context->owner_thread_id == thread_current_id();
  bool valid_input   = input && input->data && input->len > 0;
  if (!valid_context || !valid_input || !lua_overdub_worker_name_valid(module_name) || !lua_overdub_worker_name_valid(function_name)) {
    lua_overdub_worker_error(error, error_cap, "invalid worker job");
    return false;
  }

  if (!context->thread) {
    context->thread = (HANDLE)_beginthreadex(NULL, 0, lua_overdub_worker_main, context, 0, NULL);
    if (!context->thread) {
      lua_overdub_worker_error(error, error_cap, "could not start the worker thread");
      return false;
    }
  }

  uint64_t id       = 0;
  bool     accepted = false;
  EnterCriticalSection(&context->lock);
  bool stopping = InterlockedCompareExchange(&context->stop_requested, 0, 0) != 0;
  bool full     = context->outstanding_count >= CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS || context->job_count >= CONFIG_LUA_OVERDUB_MAX_WORKER_JOBS;
  if (!stopping && !full) {
    id = context->next_id++;
    if (id == 0) {
      id = context->next_id++;
    }

    lua_overdub_worker_job_t *job = &context->jobs[context->job_count++];
    job->id                       = id;
    job->input                    = *input;
    mem_copy(job->module_name, module_name.data, module_name.len);
    mem_copy(job->function_name, function_name.data, function_name.len);
    job->module_name[module_name.len]     = 0;
    job->function_name[function_name.len] = 0;

    *input = (lua_serial_blob_t){0};
    context->outstanding_count += 1;
    accepted = true;
  }
  LeaveCriticalSection(&context->lock);

  if (!accepted) {
    lua_overdub_worker_error(error, error_cap, "worker job limit reached or worker is stopping");
    return false;
  }

  if (out_id) {
    *out_id = id;
  }

  SetEvent(context->wake_event);
  return true;
}

bool
lua_overdub_worker_cancel(lua_overdub_worker_context_t *context, uint64_t id)
{
  if (!context || id == 0 || context->owner_thread_id != thread_current_id()) {
    return false;
  }

  bool                     cancelled = false;
  int                      job_idx   = -1;
  lua_overdub_worker_job_t job       = {0};

  EnterCriticalSection(&context->lock);
  for (int i = 0; i < context->job_count && job_idx < 0; ++i) {
    if (context->jobs[i].id == id) {
      job_idx = i;
    }
  }

  if (job_idx >= 0) {
    job = context->jobs[job_idx];
    context->job_count -= 1;
    if (job_idx < context->job_count) {
      mem_move(&context->jobs[job_idx], &context->jobs[job_idx + 1], sizeof(*context->jobs) * (uint64_t)(context->job_count - job_idx));
    }

    context->jobs[context->job_count] = (lua_overdub_worker_job_t){0};
    context->outstanding_count = MAX_VAL(context->outstanding_count - 1, 0);
    cancelled = true;
  }

  if (!cancelled && context->running_id == id) {
    context->running_cancelled = true;
    cancelled = true;
  }

  LeaveCriticalSection(&context->lock);
  lua_overdub_worker_job_destroy(&job);
  return cancelled;
}

bool
lua_overdub_worker_poll(lua_overdub_worker_context_t *context, lua_overdub_worker_completion_t *out_completion)
{
  if (!context || !out_completion || context->owner_thread_id != thread_current_id()) {
    return false;
  }

  bool found = false;
  EnterCriticalSection(&context->lock);
  if (context->completion_count > 0) {
    *out_completion = context->completions[0];

    context->completion_count -= 1;
    if (context->completion_count > 0) {
      mem_move(context->completions, context->completions + 1, sizeof(*context->completions) * (uint64_t)context->completion_count);
    }

    context->completions[context->completion_count] = (lua_overdub_worker_completion_t){0};
    context->outstanding_count = MAX_VAL(context->outstanding_count - 1, 0);
    found = true;
  }

  LeaveCriticalSection(&context->lock);
  return found;
}

int
lua_overdub_worker_outstanding_count(lua_overdub_worker_context_t *context)
{
  if (!context) {
    return 0;
  }

  EnterCriticalSection(&context->lock);
  int count = context->outstanding_count;
  LeaveCriticalSection(&context->lock);
  return count;
}
