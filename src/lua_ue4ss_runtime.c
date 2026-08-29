#include "lua_ue4ss_runtime.h"

#include "config.h"
#include "file.h"
#include "globals.h"
#include "log.h"
#include "lua_runtime.h"
#include "lua_ue4ss.h"
#include "lua_unreal_prop.h"
#include "mod_manager.h"
#include "scratch.h"
#include "types.h"

#include <windows.h>

typedef struct lua_ue4ss_runtime_s lua_ue4ss_runtime_t;
struct lua_ue4ss_runtime_s {
  void                *worker;
  arena_t              perm;
  lua_runtime_t        lua;
  lua_unreal_context_t unreal;
  lua_ue4ss_context_t  api;
};

typedef struct lua_ue4ss_game_call_s lua_ue4ss_game_call_t;
struct lua_ue4ss_game_call_s {
  uobject_t    *receiver;
  ufunc_t      *function;
  void         *params;
  volatile LONG executed;
};

typedef struct lua_ue4ss_worker_s lua_ue4ss_worker_t;
struct lua_ue4ss_worker_s {
  mod_t                          *mod;
  HANDLE                          thread;
  HANDLE                          ready_event;
  HANDLE                          wake_event;
  HANDLE                          game_call_request_event;
  HANDLE                          game_call_complete_event;
  lua_ue4ss_game_call_t *volatile pending_game_call;
  CRITICAL_SECTION                state_lock;
  volatile LONG                   stop_requested;
  volatile LONG                   status;
  char                            error[CONFIG_MOD_MANAGER_ERROR_MSG_MAX_LEN];
  int                             error_len;
};

enum {
  LUA_UE4SS_WORKER_STARTING = 0,
  LUA_UE4SS_WORKER_ACTIVE,
  LUA_UE4SS_WORKER_FAILED,
  LUA_UE4SS_WORKER_STOPPED,
};

static CRITICAL_SECTION g_lua_ue4ss_notification_lock       = {0};
static volatile LONG    g_lua_ue4ss_notification_lock_ready = 0;

static lua_ue4ss_runtime_t *
lua_ue4ss_runtime_from_mod(mod_t *mod)
{
  return mod ? (lua_ue4ss_runtime_t *)mod->lua.instance : NULL;
}

static void
lua_ue4ss_error_set(err_msg_t *error, str_t message)
{
  error->len = (int)MIN_VAL(message.len, sizeof(error->msg));
  mem_copy(error->msg, message.data, error->len);
}

static LONG
lua_ue4ss_worker_status(lua_ue4ss_worker_t *worker)
{
  return worker ? InterlockedCompareExchange(&worker->status, 0, 0) : LUA_UE4SS_WORKER_STOPPED;
}

static void
lua_ue4ss_worker_set_error(lua_ue4ss_worker_t *worker, str_t error)
{
  if (!worker) {
    return;
  }

  worker->error_len = (int)MIN_VAL(error.len, (uint64_t)(sizeof(worker->error) - 1));
  if (worker->error_len > 0) {
    mem_copy(worker->error, error.data, (uint64_t)worker->error_len);
  }
  worker->error[worker->error_len] = 0;
}

static bool
lua_ue4ss_unreal_access_check(void *user)
{
  mod_t *mod = (mod_t *)user;
  return mod && mod->manifest.lua.runtime_kind == MOD_LUA_RUNTIME_UE4SS && globals.game_thread_id != 0 && globals.unreal.core_object != NULL;
}

static bool
lua_ue4ss_unreal_process_event(void *user, uobject_t *receiver, ufunc_t *function, void *params)
{
  mod_t *mod = (mod_t *)user;
  if (!mod || !receiver || !function || globals.game_thread_id == 0) {
    return false;
  }

  if (unreal_is_in_game_thread()) {
    unreal_process_event_observed(receiver, function, params);
    return true;
  }

  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  lua_ue4ss_worker_t  *worker  = runtime ? (lua_ue4ss_worker_t *)runtime->worker : NULL;
  if (!worker || !worker->game_call_request_event || !worker->game_call_complete_event ||
      InterlockedCompareExchange(&worker->stop_requested, 0, 0)) {
    return false;
  }

  lua_ue4ss_game_call_t call = {
    .receiver = receiver,
    .function = function,
    .params   = params,
  };

  ResetEvent(worker->game_call_complete_event);
  if (InterlockedCompareExchange(&worker->stop_requested, 0, 0)) {
    return false;
  }

  if (InterlockedCompareExchangePointer((PVOID volatile *)&worker->pending_game_call, &call, NULL) != NULL) {
    return false;
  }

  SetEvent(worker->game_call_request_event);

  DWORD wait_result = WaitForSingleObject(worker->game_call_complete_event, INFINITE);
  if (wait_result != WAIT_OBJECT_0) {
    InterlockedCompareExchangePointer((PVOID volatile *)&worker->pending_game_call, NULL, &call);
    return false;
  }
  return InterlockedCompareExchange(&call.executed, 0, 0) != 0;
}

static bool
lua_ue4ss_dispatch_pending_game_call(mod_t *mod)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  lua_ue4ss_worker_t  *worker  = runtime ? (lua_ue4ss_worker_t *)runtime->worker : NULL;
  if (!worker) {
    return false;
  }

  lua_ue4ss_game_call_t *call = (lua_ue4ss_game_call_t *)InterlockedExchangePointer((PVOID volatile *)&worker->pending_game_call, NULL);
  if (!call) {
    return false;
  }

  ResetEvent(worker->game_call_request_event);
  if (call->receiver && call->function && unreal_uobject_is_valid(call->receiver) && unreal_uobject_is_valid((uobject_t *)call->function)) {
    unreal_process_event_observed(call->receiver, call->function, call->params);
    InterlockedExchange(&call->executed, 1);
  }
  SetEvent(worker->game_call_complete_event);
  return true;
}

static void
lua_ue4ss_cleanup(mod_t *mod)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (!runtime) {
    mod->lua.active = false;
    return;
  }

  lua_ue4ss_context_deinit(&runtime->api);
  lua_runtime_deinit(&runtime->lua);
  arena_destroy(&runtime->perm);
  HeapFree(GetProcessHeap(), 0, runtime);

  mod->lua.instance = NULL;
  mod->lua.active = false;
}

static void
lua_ue4ss_wake_worker(void *user)
{
  lua_ue4ss_worker_t *worker = (lua_ue4ss_worker_t *)user;
  if (worker && worker->wake_event) {
    SetEvent(worker->wake_event);
  }
}

static bool
lua_ue4ss_worker_init_runtime(lua_ue4ss_worker_t *worker)
{
  mod_t               *mod     = worker->mod;
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (!runtime) {
    lua_ue4ss_worker_set_error(worker, STR_LIT("UE4SS runtime instance is unavailable"));
    return false;
  }

  if (!lua_runtime_init_with_execution_limit(&runtime->lua, CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US) || !lua_runtime_enable_mod_libs(&runtime->lua)) {
    lua_ue4ss_worker_set_error(worker, STR_LIT("failed to initialize Lua runtime"));
    return false;
  }
  runtime->lua.persistent_output = true;

  runtime->api = (lua_ue4ss_context_t){
    .game_dir     = mod->lua.game_dir,
    .root_mod_dir = mod->lua.root_mod_dir,
    .mod_dir      = mod->manifest.mod_dir,
    .mod_dir_name = mod->manifest.mod_dir_name,
    .mod_id       = mod->manifest.info.id,
    .entry_path   = mod->manifest.lua.path,
  };

  runtime->perm = arena_new_dynamic(CONFIG_LUA_UE4SS_ARENA_RESERVE_SIZE, CONFIG_LUA_UE4SS_ARENA_COMMIT_SIZE);
  if (!lua_ue4ss_context_init(&runtime->api, &runtime->perm, &runtime->lua)) {
    lua_ue4ss_worker_set_error(worker, STR_LIT("failed to allocate UE4SS runtime structures"));
    return false;
  }

  runtime->api.wake_async      = lua_ue4ss_wake_worker;
  runtime->api.wake_async_user = worker;
  runtime->api.async_events    = true;

  lua_unreal_context_init(&runtime->unreal);
  runtime->unreal.user          = mod;
  runtime->unreal.access_check  = lua_ue4ss_unreal_access_check;
  runtime->unreal.process_event = lua_ue4ss_unreal_process_event;

  if (!lua_unreal_register(runtime->lua.state, &runtime->unreal) || !lua_ue4ss_register(runtime->lua.state, &runtime->api)) {
    lua_ue4ss_worker_set_error(worker, STR_LIT("failed to register UE4SS Lua APIs"));
    return false;
  }

  runtime->lua.execution_limit_us = CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US;
  bool executed = lua_runtime_execute_file(&runtime->lua, mod->manifest.lua.path);
  runtime->lua.execution_limit_us = CONFIG_LUA_UE4SS_EXECUTION_LIMIT_US;
  if (!executed) {
    lua_ue4ss_worker_set_error(worker, STR_LIT("Lua entry script failed"));
    LOG_ERROR("Lua mod '%.*s' failed to start from '%.*s'", STR_ARG(mod->manifest.info.id), STR_ARG(mod->manifest.lua.path));
    return false;
  }
  return true;
}

static DWORD WINAPI
lua_ue4ss_worker_main(void *user)
{
  lua_ue4ss_worker_t *worker = (lua_ue4ss_worker_t *)user;
  EnterCriticalSection(&worker->state_lock);
  bool initialized = lua_ue4ss_worker_init_runtime(worker);
  LeaveCriticalSection(&worker->state_lock);

  if (!initialized) {
    InterlockedExchange(&worker->status, LUA_UE4SS_WORKER_FAILED);
    SetEvent(worker->ready_event);
    scratch_destroy();
    return 0;
  }

  InterlockedExchange(&worker->status, LUA_UE4SS_WORKER_ACTIVE);
  SetEvent(worker->ready_event);
  while (!InterlockedCompareExchange(&worker->stop_requested, 0, 0)) {
    lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(worker->mod);
    if (!runtime) {
      break;
    }

    EnterCriticalSection(&worker->state_lock);
    lua_ue4ss_dispatch_async(&runtime->api);
    uint32_t wait_ms = lua_ue4ss_async_wait_ms(&runtime->api);
    LeaveCriticalSection(&worker->state_lock);

    if (InterlockedCompareExchange(&worker->stop_requested, 0, 0)) {
      break;
    }
    WaitForSingleObject(worker->wake_event, wait_ms);
  }

  InterlockedExchange(&worker->status, LUA_UE4SS_WORKER_STOPPED);
  scratch_destroy();
  return 0;
}

static void
lua_ue4ss_worker_destroy(mod_t *mod)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  lua_ue4ss_worker_t  *worker  = runtime ? (lua_ue4ss_worker_t *)runtime->worker : NULL;
  if (!worker) {
    return;
  }

  InterlockedExchange(&worker->stop_requested, 1);
  if (worker->wake_event) {
    SetEvent(worker->wake_event);
  }

  if (worker->ready_event) {
    SetEvent(worker->ready_event);
  }

  InterlockedExchangePointer((PVOID volatile *)&worker->pending_game_call, NULL);

  if (worker->game_call_request_event) {
    SetEvent(worker->game_call_request_event);
  }

  if (worker->game_call_complete_event) {
    SetEvent(worker->game_call_complete_event);
  }

  if (worker->thread) {
    WaitForSingleObject(worker->thread, INFINITE);
    CloseHandle(worker->thread);
  }

  if (worker->wake_event) {
    CloseHandle(worker->wake_event);
  }

  if (worker->ready_event) {
    CloseHandle(worker->ready_event);
  }

  if (worker->game_call_request_event) {
    CloseHandle(worker->game_call_request_event);
  }

  if (worker->game_call_complete_event) {
    CloseHandle(worker->game_call_complete_event);
  }

  DeleteCriticalSection(&worker->state_lock);
  HeapFree(GetProcessHeap(), 0, worker);
  runtime->worker = NULL;
}

static void
lua_ue4ss_sync_worker_state(mod_t *mod)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  lua_ue4ss_worker_t  *worker  = runtime ? (lua_ue4ss_worker_t *)runtime->worker : NULL;
  if (!worker) {
    return;
  }

  LONG status = lua_ue4ss_worker_status(worker);
  if (status == LUA_UE4SS_WORKER_ACTIVE && !mod->lua.active) {
    mod->lua.active = true;
    LOG_INFO("Lua mod '%.*s' started on UE4SS compatibility worker thread", STR_ARG(mod->manifest.info.id));
  } else if (status == LUA_UE4SS_WORKER_FAILED) {
    str_t error = str_make(worker->error, (uint64_t)worker->error_len);
    lua_ue4ss_error_set(&mod->lua.err_msg, str_is_empty(error) ? STR_LIT("UE4SS worker failed to start") : error);
    lua_ue4ss_worker_destroy(mod);
    if (InterlockedCompareExchange(&g_lua_ue4ss_notification_lock_ready, 0, 0)) {
      EnterCriticalSection(&g_lua_ue4ss_notification_lock);
      lua_ue4ss_cleanup(mod);
      LeaveCriticalSection(&g_lua_ue4ss_notification_lock);
    } else {
      lua_ue4ss_cleanup(mod);
    }
  }
}

static bool
lua_ue4ss_state_try_enter(mod_t *mod)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  lua_ue4ss_worker_t  *worker  = runtime ? (lua_ue4ss_worker_t *)runtime->worker : NULL;
  return worker && TryEnterCriticalSection(&worker->state_lock) != 0;
}

static void
lua_ue4ss_state_leave(mod_t *mod)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  lua_ue4ss_worker_t  *worker  = runtime ? (lua_ue4ss_worker_t *)runtime->worker : NULL;
  if (worker) {
    LeaveCriticalSection(&worker->state_lock);
  }
}

void
lua_ue4ss_runtime_system_init(void)
{
  if (!InterlockedCompareExchange(&g_lua_ue4ss_notification_lock_ready, 0, 0)) {
    InitializeCriticalSection(&g_lua_ue4ss_notification_lock);
    InterlockedExchange(&g_lua_ue4ss_notification_lock_ready, 1);
  }
}

bool
lua_ue4ss_runtime_start(mod_t *mod)
{
  if (!mod || !mod->has_lua || mod->manifest.lua.runtime_kind != MOD_LUA_RUNTIME_UE4SS) {
    return false;
  }

  if (!unreal_is_in_game_thread()) {
    lua_ue4ss_error_set(&mod->lua.err_msg, STR_LIT("UE4SS Lua runtime can only be started from the game thread"));
    return false;
  }

  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (mod->lua.active || runtime) {
    return true;
  }

  mod->lua.start_attempted = true;
  lua_ue4ss_error_set(&mod->lua.err_msg, STR_NULL);
  if (!file_exists(mod->manifest.lua.path)) {
    lua_ue4ss_error_set(&mod->lua.err_msg, STR_LIT("Lua entry file does not exist"));
    return false;
  }

  runtime = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*runtime));
  if (!runtime) {
    lua_ue4ss_error_set(&mod->lua.err_msg, STR_LIT("failed to allocate UE4SS Lua runtime"));
    return false;
  }
  mod->lua.instance = runtime;

  lua_ue4ss_worker_t *worker = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*worker));
  if (!worker) {
    lua_ue4ss_cleanup(mod);
    lua_ue4ss_error_set(&mod->lua.err_msg, STR_LIT("failed to allocate UE4SS Lua worker"));
    return false;
  }

  worker->mod = mod;
  InitializeCriticalSection(&worker->state_lock);
  worker->ready_event              = CreateEventA(NULL, TRUE, FALSE, NULL);
  worker->wake_event               = CreateEventA(NULL, FALSE, FALSE, NULL);
  worker->game_call_request_event  = CreateEventA(NULL, TRUE, FALSE, NULL);
  worker->game_call_complete_event = CreateEventA(NULL, FALSE, FALSE, NULL);

  if (!worker->ready_event || !worker->wake_event || !worker->game_call_request_event || !worker->game_call_complete_event) {
    if (worker->ready_event) {
      CloseHandle(worker->ready_event);
    }

    if (worker->wake_event) {
      CloseHandle(worker->wake_event);
    }

    if (worker->game_call_request_event) {
      CloseHandle(worker->game_call_request_event);
    }

    if (worker->game_call_complete_event) {
      CloseHandle(worker->game_call_complete_event);
    }

    DeleteCriticalSection(&worker->state_lock);
    HeapFree(GetProcessHeap(), 0, worker);
    lua_ue4ss_cleanup(mod);
    lua_ue4ss_error_set(&mod->lua.err_msg, STR_LIT("failed to create UE4SS Lua worker events"));
    return false;
  }

  runtime->worker = worker;
  worker->thread = CreateThread(NULL, 0, lua_ue4ss_worker_main, worker, 0, NULL);
  if (!worker->thread) {
    lua_ue4ss_worker_destroy(mod);
    lua_ue4ss_cleanup(mod);
    lua_ue4ss_error_set(&mod->lua.err_msg, STR_LIT("failed to create UE4SS compatibility worker thread"));
    return false;
  }

  /* finish the entry script before gameplay resumes, while servicing any reflected calls made during initialization on the game thread */
  HANDLE startup_events[] = {worker->ready_event, worker->game_call_request_event};
  DWORD  wait_result      = WAIT_FAILED;
  while (1) {
    wait_result = WaitForMultipleObjects((DWORD)COUNTOF(startup_events), startup_events, FALSE, INFINITE);
    if (wait_result == WAIT_OBJECT_0) {
      break;
    }

    if (wait_result == WAIT_OBJECT_0 + 1) {
      lua_ue4ss_dispatch_pending_game_call(mod);
      continue;
    }
    break;
  }

  if (wait_result != WAIT_OBJECT_0) {
    lua_ue4ss_worker_set_error(worker, STR_LIT("failed while waiting for the UE4SS worker to initialize"));
    InterlockedExchange(&worker->status, LUA_UE4SS_WORKER_FAILED);
  }
  lua_ue4ss_sync_worker_state(mod);
  return mod->lua.active;
}

void
lua_ue4ss_runtime_stop(mod_t *mod)
{
  if (!mod || !mod->has_lua || mod->manifest.lua.runtime_kind != MOD_LUA_RUNTIME_UE4SS || !unreal_is_in_game_thread()) {
    return;
  }

  lua_ue4ss_worker_destroy(mod);
  if (InterlockedCompareExchange(&g_lua_ue4ss_notification_lock_ready, 0, 0)) {
    EnterCriticalSection(&g_lua_ue4ss_notification_lock);
    lua_ue4ss_cleanup(mod);
    LeaveCriticalSection(&g_lua_ue4ss_notification_lock);
  } else {
    lua_ue4ss_cleanup(mod);
  }
  mod->lua.start_attempted = false;
  LOG_INFO("Lua mod '%.*s' stopped", STR_ARG(mod->manifest.info.id));
}

bool
lua_ue4ss_runtime_is_initialized(mod_t *mod)
{
  return lua_ue4ss_runtime_from_mod(mod) != NULL;
}

void
lua_ue4ss_runtime_tick(mod_t *mod)
{
  if (!mod || mod->manifest.lua.runtime_kind != MOD_LUA_RUNTIME_UE4SS) {
    return;
  }

  lua_ue4ss_sync_worker_state(mod);
  if (mod->lua.active) {
    lua_ue4ss_dispatch_pending_game_call(mod);
  }

  if (mod->lua.active && lua_ue4ss_state_try_enter(mod)) {
    lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
    if (runtime) {
      lua_ue4ss_dispatch_game_thread(&runtime->api);
    }
    lua_ue4ss_state_leave(mod);
  }
}

void
lua_ue4ss_runtime_queue_input(mod_t *mod, input_event_t *event)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (runtime && mod->lua.active && mod->manifest.lua.runtime_kind == MOD_LUA_RUNTIME_UE4SS) {
    lua_ue4ss_queue_input(&runtime->api, event);
  }
}

bool
lua_ue4ss_runtime_process_event_pre(mod_t *mod, uobject_t *object, ufunc_t *function, void *params)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (runtime && mod->lua.active && mod->manifest.lua.runtime_kind == MOD_LUA_RUNTIME_UE4SS) {
    lua_ue4ss_dispatch_process_event_pre(&runtime->api, object, function, params);
  }
  return false;
}

void
lua_ue4ss_runtime_process_event_post(mod_t *mod, uobject_t *object, ufunc_t *function, void *params, bool consumed)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (runtime && mod->lua.active && mod->manifest.lua.runtime_kind == MOD_LUA_RUNTIME_UE4SS) {
    lua_ue4ss_dispatch_process_event_post(&runtime->api, object, function, params, consumed);
  }
}

bool
lua_ue4ss_runtime_queue_command(mod_t *mod, str_t name, str_t args)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  return runtime && mod->lua.active && mod->manifest.lua.runtime_kind == MOD_LUA_RUNTIME_UE4SS &&
         lua_ue4ss_queue_command(&runtime->api, name, args);
}

void
lua_ue4ss_runtime_notify_uobject_constructed(mod_manager_t *manager, uobject_t *object)
{
  if (!manager || !object || !InterlockedCompareExchange(&g_lua_ue4ss_notification_lock_ready, 0, 0)) {
    return;
  }

  EnterCriticalSection(&g_lua_ue4ss_notification_lock);
  if (manager->mods) {
    for (int i = 0; i < manager->mod_count; ++i) {
      mod_t               *mod     = &manager->mods[i];
      lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
      if (!mod->has_lua || mod->manifest.lua.runtime_kind != MOD_LUA_RUNTIME_UE4SS ||
          !runtime || !runtime->lua.inited || !runtime->api.pending_objects) {
        continue;
      }
      lua_ue4ss_notify_uobject_constructed(&runtime->api, object);
    }
  }
  LeaveCriticalSection(&g_lua_ue4ss_notification_lock);
}

int
lua_ue4ss_runtime_snapshot_commands(mod_t *mod, str_t *names, int capacity)
{
  lua_ue4ss_runtime_t *runtime = lua_ue4ss_runtime_from_mod(mod);
  if (!mod || !names || capacity <= 0 || !mod->lua.active ||
      mod->manifest.lua.runtime_kind != MOD_LUA_RUNTIME_UE4SS || !runtime || !lua_ue4ss_state_try_enter(mod)) {
    return 0;
  }

  int count = MIN_VAL(runtime->api.command_count, capacity);
  for (int i = 0; i < count; ++i) {
    names[i] = runtime->api.commands[i].name;
  }

  lua_ue4ss_state_leave(mod);
  return count;
}
