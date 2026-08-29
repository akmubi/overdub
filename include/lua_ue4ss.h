#ifndef LUA_UE4SS_H
#define LUA_UE4SS_H

#include "arena.h"
#include "input.h"
#include "lua_runtime.h"
#include "lua_unreal_prop.h"
#include "str.h"

typedef struct lua_ue4ss_command_s lua_ue4ss_command_t;
struct lua_ue4ss_command_s {
  str_t name;
  int   callback_ref;
};

typedef struct lua_ue4ss_keybind_s lua_ue4ss_keybind_t;
struct lua_ue4ss_keybind_s {
  keybind_t bind;
  int       callback_ref;
};

typedef struct lua_ue4ss_hook_s lua_ue4ss_hook_t;
struct lua_ue4ss_hook_s {
  str_t              target;
  fweak_object_ptr_t function;
  int64_t            pre_id;
  int64_t            post_id;
  int                pre_callback_ref;
  int                post_callback_ref;
  bool               active;
  bool               pre_slow_warned;
  bool               post_slow_warned;
};

typedef struct lua_ue4ss_notification_s lua_ue4ss_notification_t;
struct lua_ue4ss_notification_s {
  str_t              class_path;
  fweak_object_ptr_t target_class;
  int                callback_ref;
  bool               active;
};

typedef struct lua_ue4ss_pending_object_s lua_ue4ss_pending_object_t;
struct lua_ue4ss_pending_object_s {
  fweak_object_ptr_t object;
};

typedef struct lua_ue4ss_pending_hook_s lua_ue4ss_pending_hook_t;
struct lua_ue4ss_pending_hook_s {
  fweak_object_ptr_t  object;
  fweak_object_ptr_t  function;
  ufunc_t            *function_raw;
  void               *params;
  uint32_t            initialized_count;
  bool                post;
};

typedef struct lua_ue4ss_pending_command_s lua_ue4ss_pending_command_t;
struct lua_ue4ss_pending_command_s {
  char    *data;
  uint64_t name_len;
  uint64_t args_len;
};

typedef struct lua_ue4ss_scheduled_job_s lua_ue4ss_scheduled_job_t;
struct lua_ue4ss_scheduled_job_s {
  uint64_t due_us;
  uint64_t interval_us;
  uint64_t ready_tick;
  uint64_t interval_ticks;
  int64_t  handle;
  uint64_t last_error_hash;
  int      callback_ref;
  uint32_t consecutive_errors;
  bool     loop;
  bool     frame_based;
  bool     game_thread;
  bool     active;
  bool     slow_warned;
};

typedef struct lua_ue4ss_context_s lua_ue4ss_context_t;
struct lua_ue4ss_context_s {
  str_t                        game_dir;
  str_t                        root_mod_dir;
  str_t                        mod_dir;
  str_t                        mod_dir_name;
  str_t                        mod_id;
  str_t                        entry_path;
  arena_t                     *arena;
  lua_runtime_t               *runtime;
  lua_ue4ss_command_t         *commands;
  int                          command_count;
  lua_ue4ss_keybind_t         *keybinds;
  int                          keybind_count;
  lua_ue4ss_hook_t            *hooks;
  int                          hook_count;
  int64_t                      next_hook_id;
  lua_ue4ss_notification_t    *notifications;
  int                          notification_count;
  lua_ue4ss_pending_object_t  *pending_objects;
  int                          pending_object_count;
  lua_ue4ss_pending_hook_t    *pending_hooks;
  int                          pending_hook_count;
  input_event_t               *pending_inputs;
  int                          pending_input_count;
  lua_ue4ss_pending_command_t *pending_commands;
  int                          pending_command_count;
  volatile int32_t             pending_lock;
  bool                         pending_overflow;
  bool                         pending_hook_overflow;
  bool                         pending_input_overflow;
  bool                         pending_command_overflow;
  bool                         async_events;
  lua_ue4ss_scheduled_job_t   *scheduled_jobs;
  int                          scheduled_job_count;
  int64_t                      next_action_handle;
  uint64_t                     scheduler_tick;
  uint64_t                     async_scheduler_tick;
  void                       (*wake_async)(void *user);
  void                        *wake_async_user;
};

bool
lua_ue4ss_context_init(lua_ue4ss_context_t *context, arena_t *arena, lua_runtime_t *runtime);
void
lua_ue4ss_context_deinit(lua_ue4ss_context_t *context);

bool
lua_ue4ss_register(lua_State *state, lua_ue4ss_context_t *context);
void
lua_ue4ss_notify_uobject_constructed(lua_ue4ss_context_t *context, uobject_t *object);

void
lua_ue4ss_dispatch_game_thread(lua_ue4ss_context_t *context);
void
lua_ue4ss_dispatch_async(lua_ue4ss_context_t *context);
bool
lua_ue4ss_dispatch_command(lua_ue4ss_context_t *context, str_t name, str_t args, bool *handled);
bool
lua_ue4ss_dispatch_process_event_pre(lua_ue4ss_context_t *context, uobject_t *obj, ufunc_t *func, void *params);
void
lua_ue4ss_dispatch_process_event_post(lua_ue4ss_context_t *context, uobject_t *obj, ufunc_t *func, void *params, bool consumed);

uint32_t
lua_ue4ss_async_wait_ms(lua_ue4ss_context_t *context);

bool
lua_ue4ss_queue_input(lua_ue4ss_context_t *context, input_event_t *ev);
bool
lua_ue4ss_queue_command(lua_ue4ss_context_t *context, str_t name, str_t args);

#endif /* LUA_UE4SS_H */
