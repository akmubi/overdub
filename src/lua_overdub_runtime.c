#include "lua_overdub_runtime.h"

#include "config.h"
#include "file.h"
#include "globals.h"
#include "log.h"
#include "lua_overdub.h"
#include "lua_overdub_input.h"
#include "lua_overdub_task.h"
#include "lua_overdub_ui.h"
#include "lua_overdub_unreal.h"
#include "lua_overdub_worker.h"
#include "lua_runtime.h"
#include "mod_manager.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <windows.h>

typedef struct lua_overdub_runtime_s lua_overdub_runtime_t;
struct lua_overdub_runtime_s {
  lua_runtime_t                 lua;
  lua_overdub_context_t         api;
  lua_overdub_task_context_t   *tasks;
  lua_overdub_ui_context_t     *ui;
  lua_overdub_unreal_context_t *unreal;
  lua_overdub_worker_context_t *worker;
  uint32_t                      owner_thread_id;
  int                           startup_ref;
  int                           tick_ref;
  int                           input_ref;
  int                           draw_panel_ref;
  int                           draw_config_ref;
  int                           shutdown_ref;
  input_event_t                 pending_inputs[CONFIG_LUA_OVERDUB_MAX_PENDING_INPUTS];
  int                           pending_input_count;
  bool                          startup_completed;
  bool                          in_callback;
};

typedef struct lua_overdub_frame_budget_s lua_overdub_frame_budget_t;
struct lua_overdub_frame_budget_s {
  uint64_t frame;
  uint64_t lua_us;
  uint64_t scheduler_us;
};

static lua_overdub_frame_budget_t g_lua_overdub_frame_budget = {
  .frame = UINT64_MAX,
};

static void
lua_overdub_frame_budget_sync(void)
{
  if (g_lua_overdub_frame_budget.frame != globals.frame_counter) {
    g_lua_overdub_frame_budget = (lua_overdub_frame_budget_t){
      .frame = globals.frame_counter,
    };
  }
}

static void
lua_overdub_frame_budget_add_lua(uint64_t elapsed_us)
{
  lua_overdub_frame_budget_sync();
  g_lua_overdub_frame_budget.lua_us += elapsed_us;
}

static lua_overdub_runtime_t *
lua_overdub_runtime_from_mod(mod_t *mod)
{
  return mod ? (lua_overdub_runtime_t *)mod->lua.instance : NULL;
}

static bool
lua_overdub_ref_valid(int ref)
{
  return ref != LUA_NOREF && ref != LUA_REFNIL;
}

static void
lua_overdub_error_set(err_msg_t *error, str_t message)
{
  if (!error) {
    return;
  }

  error->len = (int)MIN_VAL(message.len, sizeof(error->msg));
  if (error->len > 0) {
    mem_copy(error->msg, message.data, (uint64_t)error->len);
  }
}

static bool
lua_overdub_on_owner_thread(lua_overdub_runtime_t *runtime)
{
  return runtime && runtime->owner_thread_id != 0 && runtime->owner_thread_id == thread_current_id();
}

static void
lua_overdub_unref(lua_overdub_runtime_t *runtime, int *ref)
{
  if (!runtime || !runtime->lua.state || !ref || !lua_overdub_ref_valid(*ref)) {
    return;
  }

  luaL_unref(runtime->lua.state, LUA_REGISTRYINDEX, *ref);
  *ref = LUA_NOREF;
}

static void
lua_overdub_cleanup(mod_t *mod)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime) {
    if (mod) {
      mod->lua.active = false;
    }
    return;
  }

  lua_overdub_unref(runtime, &runtime->startup_ref);
  lua_overdub_unref(runtime, &runtime->tick_ref);
  lua_overdub_unref(runtime, &runtime->input_ref);
  lua_overdub_unref(runtime, &runtime->draw_panel_ref);
  lua_overdub_unref(runtime, &runtime->draw_config_ref);
  lua_overdub_unref(runtime, &runtime->shutdown_ref);

  lua_overdub_ui_context_destroy(runtime->ui);
  runtime->ui = NULL;

  lua_overdub_task_context_destroy(runtime->tasks);
  runtime->tasks = NULL;

  lua_overdub_worker_context_destroy(runtime->worker);
  runtime->worker = NULL;

  lua_overdub_context_cleanup(&runtime->api);
  lua_overdub_unreal_context_deactivate(runtime->unreal);
  lua_runtime_deinit(&runtime->lua);
  lua_overdub_unreal_context_destroy(runtime->unreal);
  runtime->unreal = NULL;

  mod->lua.instance = NULL;
  mod->lua.active   = false;
  arena_destroy(&mod->lua.perm);
}

static bool
lua_overdub_lifecycle_ref(lua_State *state, int table_idx, const char *name, int *out_ref)
{
  table_idx = lua_absindex(state, table_idx);
  lua_getfield(state, table_idx, name);
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);
    *out_ref = LUA_NOREF;
    return true;
  }

  if (!lua_isfunction(state, -1)) {
    LOG_ERROR("Overdub lifecycle field '%s' must be a function or nil", name);
    lua_pop(state, 1);
    return false;
  }

  *out_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  return true;
}

static bool
lua_overdub_load_lifecycle(mod_t *mod, lua_overdub_runtime_t *runtime)
{
  if (!lua_runtime_execute_file_results(&runtime->lua, mod->manifest.lua.path, 1)) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("Lua entry script failed"));
    return false;
  }

  lua_State *state = runtime->lua.state;
  if (!lua_istable(state, -1)) {
    LOG_ERROR("Overdub Lua mod '%.*s' entry script must return a lifecycle table", STR_ARG(mod->manifest.info.id));
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("Lua entry script must return a lifecycle table"));
    lua_settop(state, 0);
    return false;
  }

  struct {
    const char *name;
    int        *ref;
  } callbacks[] = {
    {"Startup",    &runtime->startup_ref    },
    {"Tick",       &runtime->tick_ref       },
    {"Input",      &runtime->input_ref      },
    {"DrawPanel",  &runtime->draw_panel_ref },
    {"DrawConfig", &runtime->draw_config_ref},
    {"Shutdown",   &runtime->shutdown_ref   },
  };

  bool valid = true;
  for (int i = 0; i < COUNTOF(callbacks); ++i) {
    if (!lua_overdub_lifecycle_ref(state, -1, callbacks[i].name, callbacks[i].ref)) {
      valid = false;
      break;
    }
  }

  lua_settop(state, 0);
  if (!valid) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("Lua entry script contains an invalid lifecycle callback"));
  }

  return valid;
}

static bool
lua_overdub_call(lua_overdub_runtime_t *runtime, int callback_ref, int arg_count, int result_count, const char *name)
{
  if (!runtime || callback_ref == LUA_NOREF || callback_ref == LUA_REFNIL) {
    return true;
  }

  if (!lua_overdub_on_owner_thread(runtime)) {
    LOG_ERROR("Overdub lifecycle callback '%s' was invoked outside its owning game thread", name);
    if (runtime->lua.state) {
      lua_pop(runtime->lua.state, arg_count);
    }
    return false;
  }

  if (runtime->in_callback) {
    LOG_WARN("Overdub lifecycle callback '%s' was not invoked recursively", name);
    lua_pop(runtime->lua.state, arg_count);
    return false;
  }

  runtime->in_callback = true;
  bool called = lua_runtime_call_ref(&runtime->lua, callback_ref, arg_count, result_count);
  runtime->in_callback = false;
  return called;
}

static bool
lua_overdub_dispatch_input(lua_overdub_runtime_t *runtime, mod_t *mod, const input_event_t *event)
{
  if (!runtime || !event || !lua_overdub_ref_valid(runtime->input_ref)) {
    return false;
  }

  if (!lua_overdub_input_push_event(runtime->lua.state, event)) {
    return false;
  }

  uint64_t start_us = time_now_us();
  bool     called   = lua_overdub_call(runtime, runtime->input_ref, 1, 1, "Input");
  lua_overdub_frame_budget_add_lua(time_now_us() - start_us);
  if (!called) {
    LOG_ERROR("Overdub Lua mod '%.*s': disabling failed Input callback", STR_ARG(mod->manifest.info.id));
    lua_overdub_unref(runtime, &runtime->input_ref);
    return false;
  }

  bool consumed = lua_toboolean(runtime->lua.state, -1);
  lua_pop(runtime->lua.state, 1);
  return consumed;
}

static void
lua_overdub_dispatch_pending_inputs(lua_overdub_runtime_t *runtime, mod_t *mod)
{
  if (!runtime || runtime->pending_input_count <= 0) {
    return;
  }

  input_event_t pending[CONFIG_LUA_OVERDUB_MAX_PENDING_INPUTS];
  int           count = runtime->pending_input_count;
  mem_copy(pending, runtime->pending_inputs, sizeof(*pending) * (uint64_t)count);

  runtime->pending_input_count = 0;
  for (int i = 0; i < count; ++i) {
    if (!lua_overdub_ref_valid(runtime->input_ref)) {
      break;
    }

    lua_overdub_dispatch_input(runtime, mod, &pending[i]);
  }
}

void
lua_overdub_runtime_system_init(void)
{
  g_lua_overdub_frame_budget.frame = UINT64_MAX;
}

bool
lua_overdub_runtime_start(mod_t *mod)
{
  if (!mod || !mod->has_lua || mod->manifest.lua.runtime_kind != MOD_LUA_RUNTIME_OVERDUB) {
    return false;
  }

  if (!unreal_is_in_game_thread()) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("Overdub Lua runtime can only be started from the game thread"));
    return false;
  }

  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (mod->lua.active || runtime) {
    return true;
  }

  mod->lua.start_attempted = true;
  lua_overdub_error_set(&mod->lua.err_msg, STR_NULL);
  if (!file_exists(mod->manifest.lua.path)) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("Lua entry file does not exist"));
    return false;
  }

  mod->lua.perm = arena_new_dynamic(CONFIG_LUA_OVERDUB_ARENA_RESERVE_SIZE, CONFIG_LUA_OVERDUB_ARENA_COMMIT_SIZE);
  runtime       = ARENA_PUSH_ZERO(&mod->lua.perm, lua_overdub_runtime_t);
  if (!runtime) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("failed to allocate Overdub Lua runtime"));
    arena_destroy(&mod->lua.perm);
    return false;
  }

  runtime->owner_thread_id = globals.game_thread_id;
  runtime->startup_ref     = LUA_NOREF;
  runtime->tick_ref        = LUA_NOREF;
  runtime->input_ref       = LUA_NOREF;
  runtime->draw_panel_ref  = LUA_NOREF;
  runtime->draw_config_ref = LUA_NOREF;
  runtime->shutdown_ref    = LUA_NOREF;
  runtime->api = (lua_overdub_context_t){
    .arena           = &mod->lua.perm,
    .mod_id          = mod->manifest.info.id,
    .mod_name        = mod->manifest.info.name,
    .game_dir        = mod->lua.game_dir,
    .root_dir        = mod->lua.root_mod_dir,
    .mod_dir         = mod->manifest.mod_dir,
    .entry_path      = mod->manifest.lua.path,
    .owner_thread_id = globals.game_thread_id,
  };
  mod->lua.instance = runtime;

  bool lua_ready = lua_runtime_init_with_execution_limit(&runtime->lua, CONFIG_LUA_OVERDUB_STARTUP_LIMIT_US);
  if (!lua_ready || !lua_runtime_enable_mod_libs(&runtime->lua)) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("failed to initialize Overdub Lua runtime"));
    lua_overdub_cleanup(mod);
    return false;
  }

  runtime->lua.persistent_output = true;
  str_t    mod_id          = mod->manifest.info.id;
  str_t    entry_path      = mod->manifest.lua.path;
  str_t    mod_dir         = mod->manifest.mod_dir;
  uint32_t owner_thread_id = globals.game_thread_id;

  runtime->worker = lua_overdub_worker_context_create(&mod->lua.perm, mod_id, entry_path, mod_dir, owner_thread_id);
  runtime->tasks  = lua_overdub_task_context_create(&mod->lua.perm, &runtime->lua, runtime->worker, mod_id);
  runtime->ui     = lua_overdub_ui_context_create(&mod->lua.perm, &runtime->lua, mod_id, owner_thread_id);
  runtime->unreal = lua_overdub_unreal_context_create(&mod->lua.perm, &runtime->lua, &runtime->in_callback, mod_id, owner_thread_id);

  bool contexts_ready = runtime->worker && runtime->tasks && runtime->ui && runtime->unreal;
  bool packages_ready = contexts_ready && lua_overdub_register(runtime->lua.state, &runtime->api);
  packages_ready      = packages_ready && lua_overdub_input_register(runtime->lua.state);
  packages_ready      = packages_ready && lua_overdub_task_register(runtime->lua.state, runtime->tasks);
  packages_ready      = packages_ready && lua_overdub_ui_register(runtime->lua.state, runtime->ui);
  packages_ready      = packages_ready && lua_overdub_unreal_register(runtime->lua.state, runtime->unreal);
  if (!packages_ready) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("failed to register Overdub Lua packages"));
    lua_overdub_cleanup(mod);
    return false;
  }

  if (!lua_overdub_load_lifecycle(mod, runtime)) {
    lua_overdub_cleanup(mod);
    return false;
  }

  if (!lua_overdub_call(runtime, runtime->startup_ref, 0, 0, "Startup")) {
    lua_overdub_error_set(&mod->lua.err_msg, STR_LIT("Lua Startup callback failed"));
    lua_overdub_cleanup(mod);
    return false;
  }

  runtime->startup_completed      = true;
  runtime->lua.execution_limit_us = CONFIG_LUA_OVERDUB_CALLBACK_LIMIT_US;
  mod->lua.active                 = true;
  LOG_INFO("Lua mod '%.*s' started on the game thread with the native Overdub runtime", STR_ARG(mod->manifest.info.id));
  return true;
}

void
lua_overdub_runtime_stop(mod_t *mod)
{
  if (!mod || !mod->has_lua) {
    return;
  }

  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (runtime && lua_overdub_on_owner_thread(runtime) && runtime->startup_completed) {
    lua_overdub_call(runtime, runtime->shutdown_ref, 0, 0, "Shutdown");
  } else if (runtime && !lua_overdub_on_owner_thread(runtime)) {
    LOG_ERROR("Overdub Lua mod '%.*s' cannot be stopped outside its owning game thread", STR_ARG(mod->manifest.info.id));
    return;
  }

  lua_overdub_cleanup(mod);
  mod->lua.start_attempted = false;
}

bool
lua_overdub_runtime_is_initialized(mod_t *mod)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  return runtime && runtime->lua.inited;
}

void
lua_overdub_runtime_tick(mod_t *mod, float delta_seconds)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active) {
    return;
  }

  if (!lua_overdub_on_owner_thread(runtime)) {
    LOG_ERROR("Overdub Lua mod '%.*s' Tick was dispatched outside its owning game thread", STR_ARG(mod->manifest.info.id));
    return;
  }

  if (lua_overdub_ref_valid(runtime->tick_ref)) {
    lua_pushnumber(runtime->lua.state, (lua_Number)delta_seconds);
    uint64_t callback_start_us = time_now_us();
    if (!lua_overdub_call(runtime, runtime->tick_ref, 1, 0, "Tick")) {
      LOG_ERROR("Overdub Lua mod '%.*s': disabling failed Tick callback", STR_ARG(mod->manifest.info.id));
      lua_overdub_unref(runtime, &runtime->tick_ref);
    }
    lua_overdub_frame_budget_add_lua(time_now_us() - callback_start_us);
  }

  lua_overdub_dispatch_pending_inputs(runtime, mod);

  lua_overdub_frame_budget_sync();
  if (lua_overdub_unreal_pending_count(runtime->unreal) > 0 && g_lua_overdub_frame_budget.lua_us < CONFIG_LUA_OVERDUB_FRAME_SOFT_BUDGET_US) {
    uint64_t dispatch_budget = CONFIG_LUA_OVERDUB_FRAME_SOFT_BUDGET_US - g_lua_overdub_frame_budget.lua_us;
    uint64_t elapsed_us      = lua_overdub_unreal_dispatch_pending(runtime->unreal, &runtime->lua, &runtime->in_callback, dispatch_budget);
    g_lua_overdub_frame_budget.lua_us += elapsed_us;
  }

  if (lua_overdub_task_future_count(runtime->tasks) > 0 && g_lua_overdub_frame_budget.lua_us < CONFIG_LUA_OVERDUB_FRAME_SOFT_BUDGET_US) {
    uint64_t dispatch_budget = CONFIG_LUA_OVERDUB_FRAME_SOFT_BUDGET_US - g_lua_overdub_frame_budget.lua_us;
    uint64_t elapsed_us      = lua_overdub_task_dispatch_worker_completions(runtime->tasks, &runtime->in_callback, dispatch_budget);
    g_lua_overdub_frame_budget.lua_us += elapsed_us;
  }

  bool has_tasks        = lua_overdub_task_active_count(runtime->tasks) > 0;
  bool has_lua_budget   = g_lua_overdub_frame_budget.lua_us < CONFIG_LUA_OVERDUB_FRAME_SOFT_BUDGET_US;
  bool has_sched_budget = g_lua_overdub_frame_budget.scheduler_us < CONFIG_LUA_OVERDUB_SCHEDULER_CEILING_US;
  if (has_tasks && has_lua_budget && has_sched_budget) {
    uint64_t aggregate_remaining = CONFIG_LUA_OVERDUB_FRAME_SOFT_BUDGET_US - g_lua_overdub_frame_budget.lua_us;
    uint64_t scheduler_remaining = CONFIG_LUA_OVERDUB_SCHEDULER_CEILING_US - g_lua_overdub_frame_budget.scheduler_us;
    uint64_t dispatch_budget     = MIN_VAL(aggregate_remaining, scheduler_remaining);
    runtime->in_callback = true;
    uint64_t elapsed_us          = lua_overdub_task_dispatch(runtime->tasks, dispatch_budget);
    runtime->in_callback = false;
    g_lua_overdub_frame_budget.lua_us       += elapsed_us;
    g_lua_overdub_frame_budget.scheduler_us += elapsed_us;
  }

  struct nk_context *nk = globals.ui_manager.ctx;
  if (nk) {
    uint64_t elapsed_us = lua_overdub_ui_draw_windows(runtime->ui, nk, &runtime->in_callback, globals.ui_manager.vw, globals.ui_manager.vh);
    lua_overdub_frame_budget_add_lua(elapsed_us);
  }
}

bool
lua_overdub_runtime_input(mod_t *mod, input_event_t *event)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !event || !lua_overdub_on_owner_thread(runtime)) {
    return false;
  }

  if (!lua_overdub_ref_valid(runtime->input_ref)) {
    return false;
  }

  if (runtime->in_callback) {
    if (runtime->pending_input_count < CONFIG_LUA_OVERDUB_MAX_PENDING_INPUTS) {
      runtime->pending_inputs[runtime->pending_input_count++] = *event;
    } else {
      LOG_WARN_LIMITED(1, "Overdub Lua mod '%.*s': nested input queue is full; dropping events", STR_ARG(mod->manifest.info.id));
    }
    return false;
  }
  return lua_overdub_dispatch_input(runtime, mod, event);
}

bool
lua_overdub_runtime_process_event_pre(mod_t *mod, uobject_t *object, ufunc_t *function, void *params)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !lua_overdub_on_owner_thread(runtime)) {
    return false;
  }

  uint64_t start_us = time_now_us();
  bool     consumed = lua_overdub_unreal_process_event_pre(runtime->unreal, &runtime->lua, &runtime->in_callback, object, function, params);
  lua_overdub_frame_budget_add_lua(time_now_us() - start_us);
  return consumed;
}

void
lua_overdub_runtime_process_event_post(mod_t *mod, uobject_t *object, ufunc_t *function, void *params, bool consumed)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !lua_overdub_on_owner_thread(runtime)) {
    return;
  }

  uint64_t start_us = time_now_us();
  lua_overdub_unreal_process_event_post(runtime->unreal, &runtime->lua, &runtime->in_callback, object, function, params, consumed);
  lua_overdub_frame_budget_add_lua(time_now_us() - start_us);
}

void
lua_overdub_runtime_post_load(mod_t *mod, uobject_t *object, bool after)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !lua_overdub_on_owner_thread(runtime)) {
    return;
  }

  uint64_t start_us = time_now_us();
  lua_overdub_unreal_post_load(runtime->unreal, &runtime->lua, &runtime->in_callback, object, after);
  lua_overdub_frame_budget_add_lua(time_now_us() - start_us);
}

bool
lua_overdub_runtime_has_post_load_hooks(void)
{
  return lua_overdub_unreal_has_post_load_hooks();
}

bool
lua_overdub_runtime_command(mod_t *mod, str_t name, str_t args)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !lua_overdub_on_owner_thread(runtime)) {
    return false;
  }

  uint64_t command_id   = 0;
  int      callback_ref = LUA_NOREF;
  if (!lua_overdub_command_find(&runtime->api, name, &command_id, &callback_ref)) {
    return false;
  }

  const char *arg_data = "";
  if (args.data) {
    arg_data = (const char *)args.data;
  }

  lua_pushlstring(runtime->lua.state, arg_data, (size_t)args.len);
  uint64_t start_us = time_now_us();
  bool     called   = lua_overdub_call(runtime, callback_ref, 1, 0, "console command");
  lua_overdub_frame_budget_add_lua(time_now_us() - start_us);

  if (!called) {
    LOG_ERROR("Overdub Lua mod '%.*s': removing failed console command '%.*s'", STR_ARG(mod->manifest.info.id), STR_ARG(name));
    lua_overdub_command_remove(&runtime->api, command_id);
  }

  return true;
}

void
lua_overdub_runtime_notify_uobject_constructed(mod_manager_t *manager, uobject_t *object)
{
  (void)manager;
  lua_overdub_unreal_notify_constructed(object);
}

void
lua_overdub_runtime_notify_uobject_deleted(mod_manager_t *manager, uobject_t *object, int32_t idx)
{
  (void)manager;
  lua_overdub_unreal_notify_deleted(object, idx);
}

int
lua_overdub_runtime_snapshot_commands(mod_t *mod, str_t *names, int capacity)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !lua_overdub_on_owner_thread(runtime)) {
    return 0;
  }

  return lua_overdub_command_snapshot(&runtime->api, names, capacity);
}

static bool
lua_overdub_runtime_draw_lifecycle(mod_t *mod, struct nk_context *nk, int *ref, const char *name)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (!runtime || !mod->lua.active || !lua_overdub_ref_valid(*ref)) {
    return false;
  }

  if (!lua_overdub_on_owner_thread(runtime)) {
    LOG_ERROR("Overdub Lua mod '%.*s' %s was dispatched outside its owning game thread", STR_ARG(mod->manifest.info.id), name);
    return true;
  }

  if (!lua_overdub_ui_begin_callback(runtime->ui, nk, globals.ui_manager.vw, globals.ui_manager.vh)) {
    LOG_ERROR("Overdub Lua mod '%.*s' could not begin %s", STR_ARG(mod->manifest.info.id), name);
    return true;
  }

  uint64_t start_us = time_now_us();
  bool     called   = lua_overdub_call(runtime, *ref, 1, 0, name);
  lua_overdub_ui_end_callback(runtime->ui);
  lua_overdub_frame_budget_add_lua(time_now_us() - start_us);

  if (!called) {
    LOG_ERROR("Overdub Lua mod '%.*s': disabling failed %s callback", STR_ARG(mod->manifest.info.id), name);
    lua_overdub_unref(runtime, ref);
  }

  return true;
}

void
lua_overdub_runtime_draw_panel(mod_t *mod, struct nk_context *ctx)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  if (runtime) {
    lua_overdub_runtime_draw_lifecycle(mod, ctx, &runtime->draw_panel_ref, "DrawPanel");
  }
}

bool
lua_overdub_runtime_draw_config(mod_t *mod, struct nk_context *ctx)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  return runtime && lua_overdub_runtime_draw_lifecycle(mod, ctx, &runtime->draw_config_ref, "DrawConfig");
}

bool
lua_overdub_runtime_has_draw_config(mod_t *mod)
{
  lua_overdub_runtime_t *runtime = lua_overdub_runtime_from_mod(mod);
  return runtime && mod->lua.active && lua_overdub_ref_valid(runtime->draw_config_ref);
}
