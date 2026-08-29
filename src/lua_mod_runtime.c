#include "lua_mod_runtime.h"

#include "lua_overdub_runtime.h"
#include "lua_ue4ss_runtime.h"
#include "mod_manager.h"

typedef struct lua_mod_runtime_ops_s lua_mod_runtime_ops_t;
struct lua_mod_runtime_ops_s {
  void (*system_init)               (void);
  bool (*start)                     (mod_t         *mod);
  void (*stop)                      (mod_t         *mod);
  bool (*is_initialized)            (mod_t         *mod);
  void (*tick)                      (mod_t         *mod,     float              delta_seconds);
  bool (*input)                     (mod_t         *mod,     input_event_t     *event);
  bool (*process_event_pre)         (mod_t         *mod,     uobject_t         *object, ufunc_t *function, void *params);
  void (*process_event_post)        (mod_t         *mod,     uobject_t         *object, ufunc_t *function, void *params, bool consumed);
  void (*post_load)                 (mod_t         *mod,     uobject_t         *object, bool     after);
  bool (*has_post_load_hooks)       (void);
  bool (*command)                   (mod_t         *mod,     str_t              name,   str_t    args);
  void (*notify_uobject_constructed)(mod_manager_t *manager, uobject_t         *object);
  void (*notify_uobject_deleted)    (mod_manager_t *manager, uobject_t         *object, int32_t  idx);
  int  (*snapshot_commands)         (mod_t         *mod,     str_t             *names,  int      capacity);
  void (*draw_panel)                (mod_t         *mod,     struct nk_context *ctx);
  bool (*draw_config)               (mod_t         *mod,     struct nk_context *ctx);
  bool (*has_draw_config)           (mod_t         *mod);
};

static void
lua_mod_runtime_error_set(err_msg_t *error, str_t message)
{
  error->len = (int)MIN_VAL(message.len, sizeof(error->msg));
  mem_copy(error->msg, message.data, error->len);
}

static void
lua_mod_ue4ss_tick(mod_t *mod, float delta_seconds)
{
  (void)delta_seconds;
  lua_ue4ss_runtime_tick(mod);
}

static bool
lua_mod_ue4ss_input(mod_t *mod, input_event_t *event)
{
  lua_ue4ss_runtime_queue_input(mod, event);
  return false;
}

static const lua_mod_runtime_ops_t g_lua_ue4ss_runtime_ops = {
  .system_init                = lua_ue4ss_runtime_system_init,
  .start                      = lua_ue4ss_runtime_start,
  .stop                       = lua_ue4ss_runtime_stop,
  .is_initialized             = lua_ue4ss_runtime_is_initialized,
  .tick                       = lua_mod_ue4ss_tick,
  .input                      = lua_mod_ue4ss_input,
  .process_event_pre          = lua_ue4ss_runtime_process_event_pre,
  .process_event_post         = lua_ue4ss_runtime_process_event_post,
  .command                    = lua_ue4ss_runtime_queue_command,
  .notify_uobject_constructed = lua_ue4ss_runtime_notify_uobject_constructed,
  .snapshot_commands          = lua_ue4ss_runtime_snapshot_commands,
};

static const lua_mod_runtime_ops_t g_lua_overdub_runtime_ops = {
  .system_init                = lua_overdub_runtime_system_init,
  .start                      = lua_overdub_runtime_start,
  .stop                       = lua_overdub_runtime_stop,
  .is_initialized             = lua_overdub_runtime_is_initialized,
  .tick                       = lua_overdub_runtime_tick,
  .input                      = lua_overdub_runtime_input,
  .process_event_pre          = lua_overdub_runtime_process_event_pre,
  .process_event_post         = lua_overdub_runtime_process_event_post,
  .post_load                  = lua_overdub_runtime_post_load,
  .has_post_load_hooks        = lua_overdub_runtime_has_post_load_hooks,
  .command                    = lua_overdub_runtime_command,
  .notify_uobject_constructed = lua_overdub_runtime_notify_uobject_constructed,
  .notify_uobject_deleted     = lua_overdub_runtime_notify_uobject_deleted,
  .snapshot_commands          = lua_overdub_runtime_snapshot_commands,
  .draw_panel                 = lua_overdub_runtime_draw_panel,
  .draw_config                = lua_overdub_runtime_draw_config,
  .has_draw_config            = lua_overdub_runtime_has_draw_config,
};

static const lua_mod_runtime_ops_t *const g_lua_mod_runtime_ops[MOD_LUA_RUNTIME_MAX] = {
  [MOD_LUA_RUNTIME_OVERDUB] = &g_lua_overdub_runtime_ops,
  [MOD_LUA_RUNTIME_UE4SS]   = &g_lua_ue4ss_runtime_ops,
};

static const lua_mod_runtime_ops_t *
lua_mod_runtime_ops_from_kind(mod_lua_runtime_kind_t kind)
{
  if (kind >= MOD_LUA_RUNTIME_MAX) {
    return NULL;
  }
  return g_lua_mod_runtime_ops[kind];
}

static const lua_mod_runtime_ops_t *
lua_mod_runtime_ops_from_mod(mod_t *mod)
{
  return mod ? lua_mod_runtime_ops_from_kind(mod->manifest.lua.runtime_kind) : NULL;
}

void
lua_mod_runtime_system_init(void)
{
  for (int i = 0; i < MOD_LUA_RUNTIME_MAX; ++i) {
    const lua_mod_runtime_ops_t *ops = g_lua_mod_runtime_ops[i];
    if (ops && ops->system_init) {
      ops->system_init();
    }
  }
}

bool
lua_mod_runtime_start(mod_t *mod)
{
  if (!mod || !mod->has_lua) {
    return mod != NULL;
  }

  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  if (ops && ops->start) {
    return ops->start(mod);
  }

  mod->lua.start_attempted = true;
  lua_mod_runtime_error_set(&mod->lua.err_msg, STR_LIT("invalid Lua runtime in mod.ini"));
  return false;
}

void
lua_mod_runtime_stop(mod_t *mod)
{
  if (!mod || !mod->has_lua) {
    return;
  }

  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  if (ops && ops->stop) {
    ops->stop(mod);
    return;
  }

  mod->lua.active          = false;
  mod->lua.start_attempted = false;
}

bool
lua_mod_runtime_is_initialized(mod_t *mod)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->is_initialized && ops->is_initialized(mod);
}

void
lua_mod_runtime_tick(mod_t *mod, float delta_seconds)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  if (ops && ops->tick) {
    ops->tick(mod, delta_seconds);
  }
}

bool
lua_mod_runtime_input(mod_t *mod, input_event_t *event)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->input && ops->input(mod, event);
}

bool
lua_mod_runtime_process_event_pre(mod_t *mod, uobject_t *object, ufunc_t *function, void *params)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->process_event_pre && ops->process_event_pre(mod, object, function, params);
}

void
lua_mod_runtime_process_event_post(mod_t *mod, uobject_t *object, ufunc_t *function, void *params, bool consumed)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  if (ops && ops->process_event_post) {
    ops->process_event_post(mod, object, function, params, consumed);
  }
}

void
lua_mod_runtime_post_load(mod_t *mod, uobject_t *object, bool after)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  if (ops && ops->post_load) {
    ops->post_load(mod, object, after);
  }
}

bool
lua_mod_runtime_has_post_load_hooks(void)
{
  bool found = false;
  for (int i = 0; i < MOD_LUA_RUNTIME_MAX && !found; ++i) {
    const lua_mod_runtime_ops_t *ops = g_lua_mod_runtime_ops[i];
    found = ops && ops->has_post_load_hooks && ops->has_post_load_hooks();
  }

  return found;
}

bool
lua_mod_runtime_command(mod_t *mod, str_t name, str_t args)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->command && ops->command(mod, name, args);
}

void
lua_mod_runtime_notify_uobject_constructed(mod_manager_t *manager, uobject_t *object)
{
  for (int i = 0; i < MOD_LUA_RUNTIME_MAX; ++i) {
    const lua_mod_runtime_ops_t *ops = g_lua_mod_runtime_ops[i];
    if (ops && ops->notify_uobject_constructed) {
      ops->notify_uobject_constructed(manager, object);
    }
  }
}

void
lua_mod_runtime_notify_uobject_deleted(mod_manager_t *manager, uobject_t *object, int32_t idx)
{
  for (int i = 0; i < MOD_LUA_RUNTIME_MAX; ++i) {
    const lua_mod_runtime_ops_t *ops = g_lua_mod_runtime_ops[i];
    if (ops && ops->notify_uobject_deleted) {
      ops->notify_uobject_deleted(manager, object, idx);
    }
  }
}

int
lua_mod_runtime_snapshot_commands(mod_t *mod, str_t *names, int capacity)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->snapshot_commands ? ops->snapshot_commands(mod, names, capacity) : 0;
}

void
lua_mod_runtime_draw_panel(mod_t *mod, struct nk_context *ctx)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  if (ops && ops->draw_panel) {
    ops->draw_panel(mod, ctx);
  }
}

bool
lua_mod_runtime_draw_config(mod_t *mod, struct nk_context *ctx)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->draw_config && ops->draw_config(mod, ctx);
}

bool
lua_mod_runtime_has_draw_config(mod_t *mod)
{
  const lua_mod_runtime_ops_t *ops = lua_mod_runtime_ops_from_mod(mod);
  return ops && ops->has_draw_config && ops->has_draw_config(mod);
}
