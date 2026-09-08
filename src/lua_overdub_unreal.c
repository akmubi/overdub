#include "lua_overdub_unreal.h"

#include "config.h"
#include "globals.h"
#include "log.h"
#include "lua_overdub.h"
#include "lua_unreal_prop.h"
#include "lua_runtime.h"
#include "scratch.h"
#include "unreal.h"
#include "unreal_prop.h"
#include "unreal_reflect.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <limits.h>
#include <string.h>
#include <windows.h>

#define LUA_OVERDUB_HOOK_HANDLE_META     "overdub.unreal.HookHandle"
#define LUA_OVERDUB_DELEGATE_HANDLE_META "overdub.unreal.DelegateHandle"
#define LUA_OVERDUB_CALL_META            "overdub.unreal.Call"
#define LUA_OVERDUB_NOTIFY_HANDLE_META   "overdub.unreal.NotificationHandle"

typedef struct lua_overdub_reflected_class_s lua_overdub_reflected_class_t;
typedef struct lua_overdub_reflected_func_s  lua_overdub_reflected_func_t;
typedef struct lua_overdub_pending_func_s    lua_overdub_pending_func_t;

struct lua_overdub_reflected_func_s {
  struct lua_overdub_unreal_context_s *context;
  int                                  callback_ref;
  bool                                 active;
  bool                                 reentry_reported;
};

struct lua_overdub_reflected_class_s {
  lua_overdub_reflected_class_t *next;
  lua_overdub_reflected_func_t  *funcs;
  str_t                          name;
  uint32_t                       num_funcs;
};

struct lua_overdub_pending_func_s {
  fweak_object_ptr_t            object;
  fweak_object_ptr_t            function;
  ufunc_t                      *function_raw;
  lua_overdub_reflected_func_t *impl;
  void                         *params;
  uint32_t                      initialized_count;
};

static void MOD_CALL
lua_overdub_unreal_reflected_func_call(unreal_func_call_t *call);
static bool
lua_overdub_unreal_queue_reflected_func(lua_overdub_reflected_func_t *func, unreal_func_call_t *call);
static int
lua_overdub_unreal_bind_delegate_lua(lua_State *state);

typedef struct lua_overdub_root_claim_s lua_overdub_root_claim_t;
struct lua_overdub_root_claim_s {
  fweak_object_ptr_t        object;
  lua_overdub_root_claim_t *next;
};

typedef struct lua_overdub_global_root_s lua_overdub_global_root_t;
struct lua_overdub_global_root_s {
  fweak_object_ptr_t         object;
  uint32_t                   claim_count;
  bool                       externally_rooted;
  lua_overdub_global_root_t *next;
};

typedef uint8_t lua_overdub_hook_kind_t;
enum {
  LUA_OVERDUB_HOOK_FUNCTION,
  LUA_OVERDUB_HOOK_POST_LOAD,
};

typedef struct lua_overdub_hook_s lua_overdub_hook_t;
struct lua_overdub_hook_s {
  fweak_object_ptr_t      target;
  uint64_t                id;
  int                     before_ref;
  int                     after_ref;
  lua_overdub_hook_kind_t kind;
  bool                    active;
};

typedef struct lua_overdub_pending_hook_s lua_overdub_pending_hook_t;
struct lua_overdub_pending_hook_s {
  fweak_object_ptr_t object;
  fweak_object_ptr_t function;
  ufunc_t           *function_raw;
  void              *params;
  uint32_t           initialized_count;
  bool               after;
  bool               consumed;
};

typedef struct lua_overdub_hook_handle_s lua_overdub_hook_handle_t;
struct lua_overdub_hook_handle_s {
  lua_overdub_unreal_context_t *context;
  uint64_t                      id;
  int                           slot;
};

typedef struct lua_overdub_delegate_binding_s lua_overdub_delegate_binding_t;
struct lua_overdub_delegate_binding_s {
  fweak_object_ptr_t source;
  fscript_delegate_t delegate;
  fprop_t           *property;
  uint64_t           id;
  bool               active;
};

typedef struct lua_overdub_delegate_handle_s lua_overdub_delegate_handle_t;
struct lua_overdub_delegate_handle_s {
  lua_overdub_unreal_context_t *context;
  uint64_t                      id;
  int                           slot;
};

typedef struct lua_overdub_call_s lua_overdub_call_t;
struct lua_overdub_call_s {
  lua_overdub_unreal_context_t *context;
  fweak_object_ptr_t            object;
  fweak_object_ptr_t            function;
  ufunc_t                      *function_raw;
  void                         *params;
  bool                          valid;
  bool                          before;
  bool                          deferred;
  bool                          consumed;
  bool                          skip_requested;
};

typedef struct lua_overdub_notification_s lua_overdub_notification_t;
typedef uint8_t lua_overdub_notification_kind_t;
enum {
  LUA_OVERDUB_NOTIFICATION_CREATED,
  LUA_OVERDUB_NOTIFICATION_DELETED,
};

struct lua_overdub_notification_s {
  fweak_object_ptr_t              target;
  uobject_t                      *target_object;
  uint64_t                        id;
  int                             callback_ref;
  lua_overdub_notification_kind_t kind;
  bool                            active;
  bool                            delete_queued;
};

typedef struct lua_overdub_pending_object_s lua_overdub_pending_object_t;
struct lua_overdub_pending_object_s {
  fweak_object_ptr_t object;
  int                next_notification;
};

typedef struct lua_overdub_notification_handle_s lua_overdub_notification_handle_t;
struct lua_overdub_notification_handle_s {
  lua_overdub_unreal_context_t *context;
  uint64_t                      id;
  int                           slot;
};

struct lua_overdub_unreal_context_s {
  arena_t                       *arena;
  arena_t                        param_arena;
  lua_runtime_t                 *runtime;
  bool                          *in_callback;
  lua_unreal_context_t           codec;
  str_t                          mod_id;
  uint32_t                       owner_thread_id;
  lua_overdub_reflected_class_t *reflected_classes;
  lua_overdub_root_claim_t      *root_claims;
  lua_overdub_root_claim_t      *root_claim_free;
  lua_overdub_hook_t             hooks[CONFIG_LUA_OVERDUB_MAX_HOOKS];
  lua_overdub_delegate_binding_t delegate_bindings[CONFIG_LUA_OVERDUB_MAX_DELEGATE_BINDINGS];
  lua_overdub_pending_func_t     pending_funcs[CONFIG_LUA_OVERDUB_MAX_PENDING_FUNC_CALLS];
  lua_overdub_pending_hook_t     pending_hooks[CONFIG_LUA_OVERDUB_MAX_PENDING_HOOKS];
  lua_overdub_notification_t     notifications[CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS];
  lua_overdub_pending_object_t   pending_objects[CONFIG_LUA_OVERDUB_MAX_PENDING_OBJECTS];
  lua_overdub_unreal_context_t  *notification_next;
  CRITICAL_SECTION               notification_lock;
  uint64_t                       next_hook_id;
  uint64_t                       next_delegate_id;
  uint64_t                       next_notification_id;
  int                            pending_func_count;
  int                            pending_hook_count;
  int                            pending_object_head;
  int                            pending_object_count;
  int                            pending_delete_count;
  int                            notification_count;
  bool                           pending_func_overflow;
  bool                           pending_hook_overflow;
  bool                           pending_object_overflow;
  bool                           notifications_attached;
  bool                           reflected_active;
};

static char                          g_lua_overdub_unreal_context_key;
static lua_overdub_global_root_t    *g_lua_overdub_global_roots;
static lua_overdub_global_root_t    *g_lua_overdub_global_root_free;
static int                           g_lua_overdub_post_load_hook_count;
static SRWLOCK                       g_lua_overdub_notification_lock = SRWLOCK_INIT;
static volatile LONG                 g_lua_overdub_notification_context_count;
static lua_overdub_unreal_context_t *g_lua_overdub_notification_first;

static void
lua_overdub_unreal_notifications_detach(lua_overdub_unreal_context_t *context)
{
  if (!context || !context->notifications_attached) {
    return;
  }

  AcquireSRWLockExclusive(&g_lua_overdub_notification_lock);
  lua_overdub_unreal_context_t **at = &g_lua_overdub_notification_first;
  while (*at && *at != context) {
    at = &(*at)->notification_next;
  }

  if (*at == context) {
    *at = context->notification_next;
    InterlockedDecrement(&g_lua_overdub_notification_context_count);
  }

  context->notification_next      = NULL;
  context->notifications_attached = false;
  ReleaseSRWLockExclusive(&g_lua_overdub_notification_lock);
}

static bool
lua_overdub_weak_equal(fweak_object_ptr_t left, fweak_object_ptr_t right)
{
  return left.object_idx == right.object_idx && left.object_serial == right.object_serial;
}

static bool
lua_overdub_unreal_access_check(void *user)
{
  lua_overdub_unreal_context_t *context = user;
  return context && context->owner_thread_id != 0 && context->owner_thread_id == thread_current_id();
}

static uobject_t *
lua_overdub_unreal_find_object(void *user, str_t path)
{
  (void)user;
  return unreal_uobject_find_by_path_name(NULL, path);
}

static bool
lua_overdub_unreal_process_event(void *user, uobject_t *receiver, ufunc_t *function, void *params)
{
  if (!lua_overdub_unreal_access_check(user) || !receiver || !function) {
    return false;
  }
  unreal_process_event_observed(receiver, function, params);
  return true;
}

static lua_overdub_unreal_context_t *
lua_overdub_unreal_context_get(lua_State *state)
{
  lua_pushlightuserdata(state, &g_lua_overdub_unreal_context_key);
  lua_rawget(state, LUA_REGISTRYINDEX);
  lua_overdub_unreal_context_t *context = lua_touserdata(state, -1);
  lua_pop(state, 1);
  return context;
}

static lua_overdub_unreal_context_t *
lua_overdub_unreal_context_check(lua_State *state)
{
  lua_overdub_context_check(state);
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_get(state);
  if (!lua_overdub_unreal_access_check(context)) {
    luaL_error(state, "Unreal access is only available on the owning game thread");
  }
  return context;
}

static uobject_t *
lua_overdub_unreal_object_check(lua_State *state, int idx)
{
  lua_overdub_unreal_context_check(state);
  uobject_t *object = lua_unreal_object_get(state, idx);
  if (!object) {
    luaL_argerror(state, idx, "expected a live UObject");
  }
  return object;
}

static uclass_t *
lua_overdub_unreal_class_check(lua_State *state, int idx)
{
  uobject_t *object = lua_overdub_unreal_object_check(state, idx);
  if (!globals.unreal.core_class || !unreal_uobject_is_a(object, globals.unreal.core_class)) {
    luaL_argerror(state, idx, "expected a live UClass");
  }
  return (uclass_t *)object;
}

static ustruct_t *
lua_overdub_unreal_struct_check(lua_State *state, int idx)
{
  uobject_t *object = lua_overdub_unreal_object_check(state, idx);
  bool       is_struct = (globals.unreal.core_class && unreal_uobject_is_a(object, globals.unreal.core_class)) ||
                         (globals.unreal.core_scriptstruct && unreal_uobject_is_a(object, globals.unreal.core_scriptstruct)) ||
                         (globals.unreal.core_func && unreal_uobject_is_a(object, globals.unreal.core_func));
  if (!is_struct) {
    luaL_argerror(state, idx, "expected a live UStruct, UClass, UScriptStruct, or UFunction");
  }
  return (ustruct_t *)object;
}

static ufunc_t *
lua_overdub_unreal_function_check(lua_State *state, int idx)
{
  uobject_t *object = lua_overdub_unreal_object_check(state, idx);
  if (!globals.unreal.core_func || !unreal_uobject_is_a(object, globals.unreal.core_func)) {
    luaL_argerror(state, idx, "expected a live UFunction");
  }
  return (ufunc_t *)object;
}

static uenum_t *
lua_overdub_unreal_enum_check(lua_State *state, int idx)
{
  uobject_t *object = lua_overdub_unreal_object_check(state, idx);
  if (!globals.unreal.core_enum || !unreal_uobject_is_a(object, globals.unreal.core_enum)) {
    luaL_argerror(state, idx, "expected a live UEnum");
  }

  uenum_t *uenum = (uenum_t *)object;
  if (uenum->names.num < 0 || uenum->names.max < uenum->names.num || (uenum->names.num > 0 && !uenum->names.data)) {
    luaL_error(state, "the UEnum name table is invalid");
  }
  return uenum;
}

static uint32_t
lua_overdub_unreal_u32_check(lua_State *state, int idx, const char *message)
{
  lua_Integer value = luaL_checkinteger(state, idx);
  if (value < 0 || (lua_Unsigned)value > UINT32_MAX) {
    luaL_argerror(state, idx, message);
  }
  return (uint32_t)value;
}

static uint64_t
lua_overdub_unreal_next_hook_id(lua_overdub_unreal_context_t *context)
{
  uint64_t id = context->next_hook_id++;
  if (id == 0) {
    id = context->next_hook_id++;
  }
  return id;
}

static uint64_t
lua_overdub_unreal_next_id(uint64_t *next_id)
{
  uint64_t id = (*next_id)++;
  if (id == 0) {
    id = (*next_id)++;
  }
  return id;
}

static fweak_object_ptr_t
lua_overdub_unreal_invalid_weak(void)
{
  fweak_object_ptr_t weak = {.object_idx = -1};
  return weak;
}

static uclass_t *
lua_overdub_unreal_class_arg(lua_State *state, int idx)
{
  if (lua_type(state, idx) != LUA_TSTRING) {
    return lua_overdub_unreal_class_check(state, idx);
  }

  size_t      path_len  = 0;
  const char *path_data = lua_tolstring(state, idx, &path_len);
  if (path_len == 0 || memchr(path_data, 0, path_len)) {
    luaL_argerror(state, idx, "UClass path cannot be empty or contain null bytes");
  }

  str_t      path   = str_make((void *)path_data, (uint64_t)path_len);
  uobject_t *object = unreal_uobject_find_by_path_name(globals.unreal.core_class, path);
  if (!object) {
    luaL_argerror(state, idx, "UClass was not found");
  }
  return (uclass_t *)object;
}

static bool
lua_overdub_unreal_notification_matches(lua_overdub_notification_t *notification, uobject_t *object)
{
  if (!notification || !notification->active || !object) {
    return false;
  }

  if (notification->kind != LUA_OVERDUB_NOTIFICATION_CREATED) {
    return false;
  }

  uclass_t *target = (uclass_t *)unreal_fweak_object_resolve(notification->target);
  return target && unreal_uobject_is_a(object, target);
}

static bool
lua_overdub_unreal_delete_notification_matches(lua_overdub_notification_t *notification, uobject_t *object, int32_t idx)
{
  if (!notification || !notification->active || notification->kind != LUA_OVERDUB_NOTIFICATION_DELETED) {
    return false;
  }

  bool same_object = notification->target_object == object;
  bool same_index  = notification->target.object_idx == idx;
  return !notification->delete_queued && same_object && same_index;
}

static lua_overdub_notification_t *
lua_overdub_unreal_notification_from_handle(lua_overdub_notification_handle_t *handle)
{
  if (!handle || !handle->context || handle->slot < 0 || handle->slot >= CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS) {
    return NULL;
  }

  lua_overdub_notification_t *notification = &handle->context->notifications[handle->slot];
  if (!notification->active || notification->id != handle->id) {
    return NULL;
  }
  return notification;
}

static lua_overdub_notification_handle_t *
lua_overdub_unreal_notification_handle_check(lua_State *state)
{
  lua_overdub_notification_handle_t *handle = luaL_checkudata(state, 1, LUA_OVERDUB_NOTIFY_HANDLE_META);
  lua_overdub_unreal_context_t      *context = lua_overdub_unreal_context_check(state);
  if (!handle->context || handle->context != context || handle->id == 0) {
    luaL_error(state, "Unreal notification handle is invalid");
  }
  return handle;
}

static int
lua_overdub_unreal_notification_remove_lua(lua_State *state)
{
  lua_overdub_notification_handle_t *handle  = lua_overdub_unreal_notification_handle_check(state);
  lua_overdub_unreal_context_t      *context = handle->context;
  lua_overdub_notification_t        *notification;
  int                                callback_ref = LUA_NOREF;
  bool                               detach       = false;

  EnterCriticalSection(&context->notification_lock);
  notification = lua_overdub_unreal_notification_from_handle(handle);
  if (notification) {
    if (notification->delete_queued) {
      context->pending_delete_count -= 1;
    }

    callback_ref                 = notification->callback_ref;
    notification->callback_ref   = LUA_NOREF;
    notification->active         = false;
    notification->delete_queued  = false;
    context->notification_count -= 1;
    detach                       = context->notification_count == 0;
  }
  LeaveCriticalSection(&context->notification_lock);

  if (detach) {
    lua_overdub_unreal_notifications_detach(context);
  }

  if (callback_ref != LUA_NOREF && callback_ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
  }

  lua_pushboolean(state, notification != NULL);
  return 1;
}

static int
lua_overdub_unreal_notification_is_active_lua(lua_State *state)
{
  lua_overdub_notification_handle_t *handle  = lua_overdub_unreal_notification_handle_check(state);
  lua_overdub_unreal_context_t      *context = handle->context;

  EnterCriticalSection(&context->notification_lock);
  bool active = lua_overdub_unreal_notification_from_handle(handle) != NULL;
  LeaveCriticalSection(&context->notification_lock);

  lua_pushboolean(state, active);
  return 1;
}

static int
lua_overdub_unreal_notification_handle_gc_lua(lua_State *state)
{
  lua_overdub_notification_handle_t *handle = luaL_checkudata(state, 1, LUA_OVERDUB_NOTIFY_HANDLE_META);
  handle->context = NULL;
  handle->id      = 0;
  handle->slot    = -1;
  return 0;
}

static void
lua_overdub_unreal_hook_deactivate(lua_State *state, lua_overdub_hook_t *hook)
{
  if (!hook || !hook->active) {
    return;
  }

  if (state && hook->before_ref != LUA_NOREF && hook->before_ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, hook->before_ref);
  }

  if (state && hook->after_ref != LUA_NOREF && hook->after_ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, hook->after_ref);
  }

  if (hook->kind == LUA_OVERDUB_HOOK_POST_LOAD) {
    ASSERT(g_lua_overdub_post_load_hook_count > 0);
    g_lua_overdub_post_load_hook_count -= 1;
  }

  hook->before_ref = LUA_NOREF;
  hook->after_ref  = LUA_NOREF;
  hook->active     = false;
}

static lua_overdub_hook_t *
lua_overdub_unreal_hook_from_handle(lua_overdub_hook_handle_t *handle)
{
  if (!handle || !handle->context || handle->slot < 0 || handle->slot >= CONFIG_LUA_OVERDUB_MAX_HOOKS) {
    return NULL;
  }

  lua_overdub_hook_t *hook = &handle->context->hooks[handle->slot];
  return hook->active && hook->id == handle->id ? hook : NULL;
}

static lua_overdub_hook_handle_t *
lua_overdub_unreal_hook_handle_check(lua_State *state)
{
  lua_overdub_hook_handle_t    *handle  = luaL_checkudata(state, 1, LUA_OVERDUB_HOOK_HANDLE_META);
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  if (!handle->context || handle->context != context || handle->id == 0) {
    luaL_error(state, "Hook handle is invalid");
  }
  return handle;
}

static int
lua_overdub_unreal_hook_handle_remove_lua(lua_State *state)
{
  lua_overdub_hook_handle_t *handle = lua_overdub_unreal_hook_handle_check(state);
  lua_overdub_hook_t        *hook   = lua_overdub_unreal_hook_from_handle(handle);
  if (!hook) {
    lua_pushboolean(state, false);
    return 1;
  }

  lua_overdub_unreal_hook_deactivate(state, hook);
  lua_pushboolean(state, true);
  return 1;
}

static int
lua_overdub_unreal_hook_handle_is_active_lua(lua_State *state)
{
  lua_overdub_hook_handle_t *handle = lua_overdub_unreal_hook_handle_check(state);
  lua_pushboolean(state, lua_overdub_unreal_hook_from_handle(handle) != NULL);
  return 1;
}

static lua_overdub_delegate_binding_t *
lua_overdub_unreal_delegate_from_handle(lua_overdub_delegate_handle_t *handle)
{
  if (!handle || !handle->context || handle->slot < 0 || handle->slot >= CONFIG_LUA_OVERDUB_MAX_DELEGATE_BINDINGS) {
    return NULL;
  }

  lua_overdub_delegate_binding_t *binding = &handle->context->delegate_bindings[handle->slot];
  if (!binding->active || binding->id != handle->id) {
    return NULL;
  }
  return binding;
}

static lua_overdub_delegate_handle_t *
lua_overdub_unreal_delegate_handle_check(lua_State *state)
{
  lua_overdub_delegate_handle_t *handle  = luaL_checkudata(state, 1, LUA_OVERDUB_DELEGATE_HANDLE_META);
  lua_overdub_unreal_context_t  *context = lua_overdub_unreal_context_check(state);
  if (!handle->context || handle->context != context || handle->id == 0) {
    luaL_error(state, "Unreal delegate handle is invalid");
  }
  return handle;
}

static bool
lua_overdub_unreal_delegate_is_installed(lua_overdub_delegate_binding_t *binding)
{
  if (!binding || !binding->active || !unreal_fweak_object_resolve(binding->delegate.object)) {
    return false;
  }

  uobject_t *source = unreal_fweak_object_resolve(binding->source);
  return source && unreal_fprop_delegate_contains(source, binding->property, &binding->delegate);
}

static bool
lua_overdub_unreal_delegate_deactivate(lua_overdub_delegate_binding_t *binding)
{
  bool removed = false;
  if (binding && binding->active) {
    uobject_t *source = unreal_fweak_object_resolve(binding->source);
    if (source) {
      removed = unreal_fprop_delegate_unbind(source, binding->property, &binding->delegate);
    }

    binding->active = false;
  }
  return removed;
}

static int
lua_overdub_unreal_delegate_handle_remove_lua(lua_State *state)
{
  lua_overdub_delegate_handle_t  *handle  = lua_overdub_unreal_delegate_handle_check(state);
  lua_overdub_delegate_binding_t *binding = lua_overdub_unreal_delegate_from_handle(handle);
  lua_pushboolean(state, lua_overdub_unreal_delegate_deactivate(binding));
  return 1;
}

static int
lua_overdub_unreal_delegate_handle_is_active_lua(lua_State *state)
{
  lua_overdub_delegate_handle_t  *handle  = lua_overdub_unreal_delegate_handle_check(state);
  lua_overdub_delegate_binding_t *binding = lua_overdub_unreal_delegate_from_handle(handle);
  bool                            active  = lua_overdub_unreal_delegate_is_installed(binding);
  if (binding && !active) {
    lua_overdub_unreal_delegate_deactivate(binding);
  }

  lua_pushboolean(state, active);
  return 1;
}

static ufunc_t *
lua_overdub_unreal_delegate_function_arg(lua_State *state, uobject_t *target, int idx)
{
  ufunc_t *function = NULL;
  if (lua_type(state, idx) == LUA_TSTRING) {
    size_t      name_len  = 0;
    const char *name_data = lua_tolstring(state, idx, &name_len);
    if (name_len == 0 || memchr(name_data, 0, name_len)) {
      luaL_argerror(state, idx, "function name cannot be empty or contain null bytes");
    }

    function = unreal_ustruct_find_func((ustruct_t *)target->cls, str_make((void *)name_data, (uint64_t)name_len));
    if (!function) {
      luaL_argerror(state, idx, "function was not found on the target object");
    }
  } else {
    function = lua_overdub_unreal_function_check(state, idx);
  }
  return function;
}

static int
lua_overdub_unreal_bind_delegate_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  uobject_t                    *source  = lua_overdub_unreal_object_check(state, 1);
  uobject_t                    *target  = lua_overdub_unreal_object_check(state, 3);

  size_t      property_len  = 0;
  const char *property_data = luaL_checklstring(state, 2, &property_len);
  if (property_len == 0 || memchr(property_data, 0, property_len)) {
    return luaL_argerror(state, 2, "delegate property name cannot be empty or contain null bytes");
  }

  str_t    property_name = str_make((void *)property_data, (uint64_t)property_len);
  fprop_t *property      = unreal_ustruct_find_prop((ustruct_t *)source->cls, property_name);
  ufunc_t *signature     = unreal_fprop_delegate_signature(property);
  if (!property || !signature) {
    return luaL_argerror(state, 2, "expected a single-cast, inline multicast, or sparse multicast delegate property");
  }

  ufunc_t *function = lua_overdub_unreal_delegate_function_arg(state, target, 4);
  if (function->func_flags & FUNC_FLAG_STATIC) {
    return luaL_argerror(state, 4, "a delegate cannot be bound to a static function");
  }

  if (!unreal_ufunction_signature_compatible(signature, function)) {
    return luaL_argerror(state, 4, "function signature is incompatible with the delegate property");
  }

  int slot = -1;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_DELEGATE_BINDINGS && slot < 0; ++i) {
    if (context->delegate_bindings[i].active && !lua_overdub_unreal_delegate_is_installed(&context->delegate_bindings[i])) {
      lua_overdub_unreal_delegate_deactivate(&context->delegate_bindings[i]);
    }

    if (!context->delegate_bindings[i].active) {
      slot = i;
    }
  }

  if (slot < 0) {
    return luaL_error(state, "Overdub Unreal delegate-binding limit reached");
  }

  fweak_object_ptr_t source_weak = lua_overdub_unreal_invalid_weak();
  fscript_delegate_t delegate    = {0};
  bool ready = unreal_fweak_object_from_object(source, &source_weak) && unreal_fscript_delegate_make(&delegate, target, function);
  if (!ready || !unreal_fprop_delegate_bind(source, property, &delegate)) {
    return luaL_error(state, "delegate property is already bound or rejected the binding");
  }

  uint64_t id = lua_overdub_unreal_next_id(&context->next_delegate_id);
  context->delegate_bindings[slot] = (lua_overdub_delegate_binding_t){
    .source   = source_weak,
    .delegate = delegate,
    .property = property,
    .id       = id,
    .active   = true,
  };

  lua_overdub_delegate_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_delegate_handle_t){
    .context = context,
    .id      = id,
    .slot    = slot,
  };
  luaL_setmetatable(state, LUA_OVERDUB_DELEGATE_HANDLE_META);
  return 1;
}

static bool
lua_overdub_unreal_call_is_live(lua_overdub_call_t *call)
{
  return call && call->valid && call->context && call->function_raw &&
         unreal_fweak_object_resolve(call->function) == (uobject_t *)call->function_raw &&
         unreal_fweak_object_resolve(call->object) != NULL;
}

static lua_overdub_call_t *
lua_overdub_unreal_call_check(lua_State *state)
{
  lua_overdub_call_t           *call    = luaL_checkudata(state, 1, LUA_OVERDUB_CALL_META);
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  if (!call->context || call->context != context || !lua_overdub_unreal_call_is_live(call)) {
    luaL_error(state, "UnrealCall is no longer valid");
  }
  return call;
}

static fprop_t *
lua_overdub_unreal_call_find_parameter(lua_State *state, lua_overdub_call_t *call, int name_idx)
{
  size_t      name_len  = 0;
  const char *name_data = luaL_checklstring(state, name_idx, &name_len);
  if (name_len == 0 || memchr(name_data, 0, name_len)) {
    luaL_argerror(state, name_idx, "parameter name cannot be empty or contain null bytes");
  }

  str_t name = str_make((void *)name_data, (uint64_t)name_len);
  for (ffield_t *field = call->function_raw->child_props; field; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if ((prop->prop_flags & CPF_PARM) && unreal_fname_match_text(prop->name, name, false, true)) {
      uint64_t size = unreal_fprop_complete_size(prop);
      bool fits = call->params && prop->offset_internal >= 0 && size > 0 &&
                  (uint64_t)prop->offset_internal <= call->function_raw->params_size &&
                  size <= (uint64_t)call->function_raw->params_size - (uint64_t)prop->offset_internal;
      if (!fits) {
        luaL_error(state, "parameter '%.*s' has an invalid reflected layout", (int)name.len, name.data);
      }
      return prop;
    }
  }

  luaL_error(state, "UFunction has no parameter named '%.*s'", (int)name.len, name.data);
  return NULL;
}

static fprop_t *
lua_overdub_unreal_call_find_return(lua_State *state, lua_overdub_call_t *call)
{
  for (ffield_t *field = call->function_raw->child_props; field; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (prop->prop_flags & CPF_RETURN_PARM) {
      uint64_t size = unreal_fprop_complete_size(prop);
      bool fits = call->params && prop->offset_internal >= 0 && size > 0 &&
                  (uint64_t)prop->offset_internal <= call->function_raw->params_size &&
                  size <= (uint64_t)call->function_raw->params_size - (uint64_t)prop->offset_internal;
      if (!fits) {
        luaL_error(state, "the UFunction return property has an invalid reflected layout");
      }
      return prop;
    }
  }

  luaL_error(state, "the UFunction has no return value");
  return NULL;
}

static int
lua_overdub_unreal_call_is_valid_lua(lua_State *state)
{
  lua_overdub_call_t *call = luaL_checkudata(state, 1, LUA_OVERDUB_CALL_META);
  lua_overdub_unreal_context_check(state);
  lua_pushboolean(state, lua_overdub_unreal_call_is_live(call));
  return 1;
}

static int
lua_overdub_unreal_call_get_object_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  return lua_unreal_object_push(state, unreal_fweak_object_resolve(call->object));
}

static int
lua_overdub_unreal_call_get_function_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  return lua_unreal_object_push(state, unreal_fweak_object_resolve(call->function));
}

static int
lua_overdub_unreal_call_get_phase_lua(lua_State *state)
{
  lua_pushstring(state, lua_overdub_unreal_call_check(state)->before ? "Before" : "After");
  return 1;
}

static int
lua_overdub_unreal_call_is_deferred_lua(lua_State *state)
{
  lua_pushboolean(state, lua_overdub_unreal_call_check(state)->deferred);
  return 1;
}

static int
lua_overdub_unreal_call_is_skipped_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  lua_pushboolean(state, call->consumed || call->skip_requested);
  return 1;
}

static int
lua_overdub_unreal_call_get_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  fprop_t            *prop = lua_overdub_unreal_call_find_parameter(state, call, 2);
  void               *value = unreal_fprop_value_in_container(prop, call->params, 0);
  uobject_t          *owner = unreal_fweak_object_resolve(call->object);
  if (!value || !lua_unreal_prop_push(state, prop, value, owner)) {
    return luaL_error(state, "parameter has an unsupported reflected type");
  }
  return 1;
}

static int
lua_overdub_unreal_call_set_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  if (call->deferred) {
    return luaL_error(state, "a deferred nested UnrealCall is read-only");
  }

  fprop_t   *prop  = lua_overdub_unreal_call_find_parameter(state, call, 2);
  void      *value = unreal_fprop_value_in_container(prop, call->params, 0);
  uobject_t *owner = unreal_fweak_object_resolve(call->object);
  if (!value || !lua_unreal_prop_write(state, 3, prop, value, owner)) {
    return luaL_error(state, "could not marshal the parameter value");
  }
  return 0;
}

static int
lua_overdub_unreal_call_get_return_lua(lua_State *state)
{
  lua_overdub_call_t *call  = lua_overdub_unreal_call_check(state);
  fprop_t            *prop  = lua_overdub_unreal_call_find_return(state, call);
  void               *value = unreal_fprop_value_in_container(prop, call->params, 0);
  uobject_t          *owner = unreal_fweak_object_resolve(call->object);
  if (!value || !lua_unreal_prop_push(state, prop, value, owner)) {
    return luaL_error(state, "return value has an unsupported reflected type");
  }
  return 1;
}

static int
lua_overdub_unreal_call_set_return_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  if (call->deferred) {
    return luaL_error(state, "a deferred nested UnrealCall is read-only");
  }

  fprop_t   *prop  = lua_overdub_unreal_call_find_return(state, call);
  void      *value = unreal_fprop_value_in_container(prop, call->params, 0);
  uobject_t *owner = unreal_fweak_object_resolve(call->object);
  if (!value || !lua_unreal_prop_write(state, 2, prop, value, owner)) {
    return luaL_error(state, "could not marshal the return value");
  }
  return 0;
}

static int
lua_overdub_unreal_call_skip_lua(lua_State *state)
{
  lua_overdub_call_t *call = lua_overdub_unreal_call_check(state);
  if (!call->before) {
    return luaL_error(state, "Skip is only valid in a Before hook");
  }

  if (call->deferred) {
    return luaL_error(state, "a deferred nested UnrealCall cannot skip an invocation that has already run");
  }

  bool changed = !call->skip_requested;
  call->skip_requested = true;
  lua_pushboolean(state, changed);
  return 1;
}

static lua_overdub_root_claim_t *
lua_overdub_unreal_mod_claim_find(lua_overdub_unreal_context_t *context, fweak_object_ptr_t weak)
{
  for (lua_overdub_root_claim_t *claim = context ? context->root_claims : NULL; claim; claim = claim->next) {
    if (lua_overdub_weak_equal(claim->object, weak)) {
      return claim;
    }
  }
  return NULL;
}

static lua_overdub_global_root_t *
lua_overdub_unreal_global_root_find(fweak_object_ptr_t weak)
{
  for (lua_overdub_global_root_t *root = g_lua_overdub_global_roots; root; root = root->next) {
    if (lua_overdub_weak_equal(root->object, weak)) {
      return root;
    }
  }
  return NULL;
}

static lua_overdub_root_claim_t *
lua_overdub_unreal_mod_claim_alloc(lua_overdub_unreal_context_t *context)
{
  lua_overdub_root_claim_t *claim = context->root_claim_free;
  if (claim) {
    context->root_claim_free = claim->next;
    *claim = (lua_overdub_root_claim_t){0};
  } else {
    claim = ARENA_PUSH_ZERO(context->arena, lua_overdub_root_claim_t);
  }

  return claim;
}

static void
lua_overdub_unreal_mod_claim_free(lua_overdub_unreal_context_t *context, lua_overdub_root_claim_t *claim)
{
  if (context && claim) {
    *claim                   = (lua_overdub_root_claim_t){0};
    claim->next              = context->root_claim_free;
    context->root_claim_free = claim;
  }
}

static lua_overdub_global_root_t *
lua_overdub_unreal_global_root_alloc(void)
{
  lua_overdub_global_root_t *root = g_lua_overdub_global_root_free;
  if (root) {
    g_lua_overdub_global_root_free = root->next;
    *root = (lua_overdub_global_root_t){0};
  } else {
    root = ARENA_PUSH_ZERO(&globals.perm, lua_overdub_global_root_t);
  }

  return root;
}

static void
lua_overdub_unreal_global_root_free(lua_overdub_global_root_t *root)
{
  if (root) {
    *root                           = (lua_overdub_global_root_t){0};
    root->next                      = g_lua_overdub_global_root_free;
    g_lua_overdub_global_root_free = root;
  }
}

static bool
lua_overdub_unreal_claim_root(lua_overdub_unreal_context_t *context, uobject_t *object)
{
  fweak_object_ptr_t weak = {0};
  if (!context || !unreal_fweak_object_from_object(object, &weak)) {
    return false;
  }

  if (lua_overdub_unreal_mod_claim_find(context, weak)) {
    return false;
  }

  lua_overdub_root_claim_t *claim = lua_overdub_unreal_mod_claim_alloc(context);
  if (!claim) {
    return false;
  }

  lua_overdub_global_root_t *root = lua_overdub_unreal_global_root_find(weak);
  if (!root) {
    root = lua_overdub_unreal_global_root_alloc();
    if (!root) {
      lua_overdub_unreal_mod_claim_free(context, claim);
      return false;
    }

    root->object            = weak;
    root->externally_rooted = unreal_uobject_is_rooted(object);
    root->next              = g_lua_overdub_global_roots;
    g_lua_overdub_global_roots = root;
    if (!root->externally_rooted && !unreal_uobject_add_to_root(object)) {
      g_lua_overdub_global_roots = root->next;
      lua_overdub_unreal_global_root_free(root);
      lua_overdub_unreal_mod_claim_free(context, claim);
      return false;
    }
  }

  root->claim_count   += 1;
  claim->object        = weak;
  claim->next          = context->root_claims;
  context->root_claims = claim;
  return true;
}

static void
lua_overdub_unreal_global_root_release(fweak_object_ptr_t weak)
{
  lua_overdub_global_root_t **at = &g_lua_overdub_global_roots;
  while (*at && !lua_overdub_weak_equal((*at)->object, weak)) {
    at = &(*at)->next;
  }

  lua_overdub_global_root_t *root = *at;
  if (!root) {
    return;
  }

  if (root->claim_count > 0) {
    root->claim_count -= 1;
  }

  if (root->claim_count != 0) {
    return;
  }

  uobject_t *object = unreal_fweak_object_resolve(root->object);
  if (object && !root->externally_rooted) {
    unreal_uobject_remove_from_root(object);
  }

  *at = root->next;
  lua_overdub_unreal_global_root_free(root);
}

static bool
lua_overdub_unreal_release_root(lua_overdub_unreal_context_t *context, uobject_t *object)
{
  fweak_object_ptr_t weak = {0};
  if (!context || !unreal_fweak_object_from_object(object, &weak)) {
    return false;
  }

  lua_overdub_root_claim_t **at = &context->root_claims;
  while (*at && !lua_overdub_weak_equal((*at)->object, weak)) {
    at = &(*at)->next;
  }

  lua_overdub_root_claim_t *claim = *at;
  if (!claim) {
    return false;
  }

  *at = claim->next;
  lua_overdub_unreal_global_root_release(claim->object);
  lua_overdub_unreal_mod_claim_free(context, claim);
  return true;
}

static int
lua_overdub_unreal_find_object_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  size_t      path_len  = 0;
  const char *path_data = luaL_checklstring(state, 1, &path_len);
  if (path_len == 0 || memchr(path_data, 0, path_len)) {
    return luaL_argerror(state, 1, "object path cannot be empty or contain null bytes");
  }

  uclass_t *cls = NULL;
  if (!lua_isnoneornil(state, 2)) {
    cls = lua_overdub_unreal_class_check(state, 2);
  }

  uobject_t *object = unreal_uobject_find_by_path_name(cls, str_make((void *)path_data, (uint64_t)path_len));
  return lua_unreal_object_push(state, object);
}

static int
lua_overdub_unreal_get_object_count_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  lua_pushinteger(state, unreal_uobject_array_count());
  return 1;
}

static int
lua_overdub_unreal_get_object_by_index_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  lua_Integer idx = luaL_checkinteger(state, 1);
  if (idx < 0 || idx > INT_MAX) {
    return luaL_argerror(state, 1, "object index is out of range");
  }

  return lua_unreal_object_push(state, unreal_uobject_array_get_obj((int)idx));
}

static int
lua_overdub_unreal_object_iterator_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  lua_Integer idx   = lua_tointeger(state, lua_upvalueindex(1));
  lua_Integer count = lua_tointeger(state, lua_upvalueindex(2));
  if (idx >= count) {
    return 0;
  }

  lua_pushinteger(state, idx + 1);
  lua_replace(state, lua_upvalueindex(1));
  lua_pushinteger(state, idx);
  lua_unreal_object_push(state, unreal_uobject_array_get_obj((int)idx));
  return 2;
}

static int
lua_overdub_unreal_objects_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  lua_pushinteger(state, 0);
  lua_pushinteger(state, unreal_uobject_array_count());
  lua_pushcclosure(state, lua_overdub_unreal_object_iterator_lua, 2);
  return 1;
}

static int
lua_overdub_unreal_load_object_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  size_t      path_len  = 0;
  const char *path_data = luaL_checklstring(state, 1, &path_len);
  if (path_len == 0 || memchr(path_data, 0, path_len)) {
    return luaL_argerror(state, 1, "object path cannot be empty or contain null bytes");
  }

  uclass_t *cls = globals.unreal.core_object;
  if (!lua_isnoneornil(state, 2)) {
    cls = lua_overdub_unreal_class_check(state, 2);
  }

  uobject_t *outer = NULL;
  if (!lua_isnoneornil(state, 3)) {
    outer = lua_overdub_unreal_object_check(state, 3);
  }

  if (!cls) {
    return luaL_error(state, "the UObject class is unavailable");
  }

  str_t path = str_make((void *)path_data, (uint64_t)path_len);
  return lua_unreal_object_push(state, unreal_static_load_object(cls, outer, path, STR_NULL, 0, NULL, true, NULL));
}

static int
lua_overdub_unreal_get_current_world_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  return lua_unreal_object_push(state, (uobject_t *)unreal_get_current_world());
}

static int
lua_overdub_unreal_get_transient_package_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  return lua_unreal_object_push(state, globals.unreal.transient_package);
}

static int
lua_overdub_unreal_construct_object_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  uclass_t  *cls   = lua_overdub_unreal_class_check(state, 1);
  uobject_t *outer = globals.unreal.transient_package;
  if (!lua_isnoneornil(state, 2)) {
    outer = lua_overdub_unreal_object_check(state, 2);
  }

  if (!outer) {
    return luaL_error(state, "the transient package is unavailable; provide an Outer explicitly");
  }

  if (globals.unreal.actor && unreal_uclass_is_child_of(cls, globals.unreal.actor)) {
    return luaL_argerror(state, 1, "actor classes must be created with SpawnActor");
  }

  if (cls->class_flags & CLASS_ABSTRACT) {
    return luaL_argerror(state, 1, "cannot construct an instance of an abstract UClass");
  }

  if (cls->class_within && !unreal_uobject_is_a(outer, cls->class_within)) {
    return luaL_argerror(state, 2, "Outer does not satisfy the UClass ClassWithin requirement");
  }

  fname_t name = {0};
  if (!lua_isnoneornil(state, 3)) {
    size_t      name_len  = 0;
    const char *name_data = luaL_checklstring(state, 3, &name_len);
    if (memchr(name_data, 0, name_len)) {
      return luaL_argerror(state, 3, "object name cannot contain null bytes");
    }

    if (name_len > 0) {
      name = unreal_fname_from_str(str_make((void *)name_data, (uint64_t)name_len), FNAME_FIND_OR_ADD);
    }
  }

  eobj_flags_t flags = RF_NO_FLAGS;
  if (!lua_isnoneornil(state, 4)) {
    flags = (eobj_flags_t)lua_overdub_unreal_u32_check(state, 4, "object flags must fit in an unsigned 32-bit integer");
  }

  const eobj_flags_t engine_managed_flags = RF_CLASS_DEFAULT_OBJECT | RF_NEED_INITIALIZATION       | RF_NEED_LOAD |
                                            RF_NEED_POST_LOAD       | RF_NEED_POST_LOAD_SUBOBJECTS |
                                            RF_BEGIN_DESTROYED      | RF_FINISH_DESTROYED;
  if (flags & engine_managed_flags) {
    return luaL_argerror(state, 4, "object flags contain engine-managed construction, loading, or destruction state");
  }

  uobject_t *tmpl = NULL;
  if (!lua_isnoneornil(state, 5)) {
    tmpl = lua_overdub_unreal_object_check(state, 5);
    if (!unreal_uobject_is_a(tmpl, cls)) {
      return luaL_argerror(state, 5, "Template must be an instance of the constructed UClass");
    }
  }

  fstatic_construct_obj_params_t params = {
    .cls                                 = cls,
    .outer                               = outer,
    .name                                = name,
    .set_flags                           = flags,
    .copy_transients_from_class_defaults = lua_toboolean(state, 6) != 0,
    .tmpl                                = tmpl,
  };
  return lua_unreal_object_push(state, unreal_static_construct_object(&params));
}

static int
lua_overdub_unreal_spawn_actor_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  uclass_t *cls = lua_overdub_unreal_class_check(state, 1);
  if (!globals.unreal.actor || !unreal_uclass_is_child_of(cls, globals.unreal.actor)) {
    return luaL_argerror(state, 1, "expected an AActor-derived UClass");
  }

  if (cls->class_flags & CLASS_ABSTRACT) {
    return luaL_argerror(state, 1, "cannot spawn an instance of an abstract UClass");
  }

  uobject_t *world_context = (uobject_t *)unreal_get_current_world();
  if (!lua_isnoneornil(state, 2)) {
    world_context = lua_overdub_unreal_object_check(state, 2);
  }

  if (!world_context) {
    return luaL_error(state, "the current world is unavailable; provide a WorldContextObject explicitly");
  }

  return lua_unreal_object_push(state, unreal_spawn_actor(world_context, cls));
}

static bool
lua_overdub_unreal_prop_kind_from_name(str_t name, unreal_prop_kind_t *out)
{
  static const struct {
    const char         *name;
    unreal_prop_kind_t  kind;
  } names[] = {
    {"Bool",               UNREAL_PROP_KIND_BOOL       },
    {"BoolProperty",       UNREAL_PROP_KIND_BOOL       },
    {"Byte",               UNREAL_PROP_KIND_BYTE       },
    {"ByteProperty",       UNREAL_PROP_KIND_BYTE       },
    {"Int8",               UNREAL_PROP_KIND_INT8       },
    {"Int8Property",       UNREAL_PROP_KIND_INT8       },
    {"Int16",              UNREAL_PROP_KIND_INT16      },
    {"Int16Property",      UNREAL_PROP_KIND_INT16      },
    {"Int",                UNREAL_PROP_KIND_INT32      },
    {"Int32",              UNREAL_PROP_KIND_INT32      },
    {"IntProperty",        UNREAL_PROP_KIND_INT32      },
    {"Int32Property",      UNREAL_PROP_KIND_INT32      },
    {"Int64",              UNREAL_PROP_KIND_INT64      },
    {"Int64Property",      UNREAL_PROP_KIND_INT64      },
    {"UInt16",             UNREAL_PROP_KIND_UINT16     },
    {"UInt16Property",     UNREAL_PROP_KIND_UINT16     },
    {"UInt",               UNREAL_PROP_KIND_UINT32     },
    {"UInt32",             UNREAL_PROP_KIND_UINT32     },
    {"UIntProperty",       UNREAL_PROP_KIND_UINT32     },
    {"UInt32Property",     UNREAL_PROP_KIND_UINT32     },
    {"UInt64",             UNREAL_PROP_KIND_UINT64     },
    {"UInt64Property",     UNREAL_PROP_KIND_UINT64     },
    {"Float",              UNREAL_PROP_KIND_FLOAT      },
    {"FloatProperty",      UNREAL_PROP_KIND_FLOAT      },
    {"Double",             UNREAL_PROP_KIND_DOUBLE     },
    {"DoubleProperty",     UNREAL_PROP_KIND_DOUBLE     },
    {"Name",               UNREAL_PROP_KIND_NAME       },
    {"NameProperty",       UNREAL_PROP_KIND_NAME       },
    {"String",             UNREAL_PROP_KIND_STRING     },
    {"StrProperty",        UNREAL_PROP_KIND_STRING     },
    {"Text",               UNREAL_PROP_KIND_TEXT       },
    {"TextProperty",       UNREAL_PROP_KIND_TEXT       },
    {"Object",             UNREAL_PROP_KIND_OBJECT     },
    {"ObjectProperty",     UNREAL_PROP_KIND_OBJECT     },
    {"ObjectPtrProperty",  UNREAL_PROP_KIND_OBJECT     },
    {"Class",              UNREAL_PROP_KIND_CLASS      },
    {"ClassProperty",      UNREAL_PROP_KIND_CLASS      },
    {"SoftObject",         UNREAL_PROP_KIND_SOFT_OBJECT},
    {"SoftObjectProperty", UNREAL_PROP_KIND_SOFT_OBJECT},
    {"SoftClass",          UNREAL_PROP_KIND_SOFT_CLASS },
    {"SoftClassProperty",  UNREAL_PROP_KIND_SOFT_CLASS },
    {"WeakObject",         UNREAL_PROP_KIND_WEAK_OBJECT},
    {"WeakObjectProperty", UNREAL_PROP_KIND_WEAK_OBJECT},
    {"LazyObject",         UNREAL_PROP_KIND_LAZY_OBJECT},
    {"LazyObjectProperty", UNREAL_PROP_KIND_LAZY_OBJECT},
    {"Interface",          UNREAL_PROP_KIND_INTERFACE  },
    {"InterfaceProperty",  UNREAL_PROP_KIND_INTERFACE  },
    {"Struct",             UNREAL_PROP_KIND_STRUCT     },
    {"StructProperty",     UNREAL_PROP_KIND_STRUCT     },
    {"Array",              UNREAL_PROP_KIND_ARRAY      },
    {"ArrayProperty",      UNREAL_PROP_KIND_ARRAY      },
    {"Set",                UNREAL_PROP_KIND_SET        },
    {"SetProperty",        UNREAL_PROP_KIND_SET        },
    {"Map",                UNREAL_PROP_KIND_MAP        },
    {"MapProperty",        UNREAL_PROP_KIND_MAP        },
    {"Enum",               UNREAL_PROP_KIND_ENUM       },
    {"EnumProperty",       UNREAL_PROP_KIND_ENUM       },
  };

  bool found = false;
  for (size_t i = 0; i < COUNTOF(names) && !found; ++i) {
    str_t candidate = str_from_cstr(names[i].name);
    if (str_equal(name, candidate, 0)) {
      *out  = names[i].kind;
      found = true;
    }
  }
  return found;
}

static bool
lua_overdub_unreal_prop_kind_uses_class(unreal_prop_kind_t kind)
{
  switch (kind) {
    case UNREAL_PROP_KIND_OBJECT:
    case UNREAL_PROP_KIND_CLASS:
    case UNREAL_PROP_KIND_SOFT_OBJECT:
    case UNREAL_PROP_KIND_SOFT_CLASS:
    case UNREAL_PROP_KIND_WEAK_OBJECT:
    case UNREAL_PROP_KIND_LAZY_OBJECT:
    case UNREAL_PROP_KIND_INTERFACE: return true;
    default:                         return false;
  }
}

static str_t
lua_overdub_unreal_required_string_field(lua_State *state, int table_idx, const char *field)
{
  table_idx = lua_absindex(state, table_idx);
  lua_getfield(state, table_idx, field);

  size_t      len  = 0;
  const char *data = lua_tolstring(state, -1, &len);
  if (!data || len == 0 || memchr(data, 0, len)) {
    luaL_error(state, "%s must be a non-empty string without null bytes", field);
  }

  str_t result = str_make((void *)data, (uint64_t)len);
  lua_pop(state, 1);
  return result;
}

static uint64_t
lua_overdub_unreal_optional_flags(lua_State *state, int table_idx)
{
  table_idx = lua_absindex(state, table_idx);
  lua_getfield(state, table_idx, "Flags");

  uint64_t flags = 0;
  if (!lua_isnil(state, -1)) {
    if (!lua_isinteger(state, -1)) {
      luaL_error(state, "Flags must be an integer");
    }
    flags = (uint64_t)(lua_Unsigned)lua_tointeger(state, -1);
  }

  lua_pop(state, 1);
  return flags;
}

static unreal_prop_type_t
lua_overdub_unreal_prop_type_parse(lua_State *state, lua_overdub_unreal_context_t *context, int idx, int depth)
{
  if (depth >= 16) {
    luaL_error(state, "property type nesting is too deep");
  }

  idx = lua_absindex(state, idx);
  int         table_idx = 0;
  const char *type_data = NULL;
  size_t      type_len  = 0;
  if (lua_type(state, idx) == LUA_TSTRING) {
    type_data = lua_tolstring(state, idx, &type_len);
  } else if (lua_istable(state, idx)) {
    table_idx = idx;
    lua_getfield(state, table_idx, "Type");
    type_data = lua_tolstring(state, -1, &type_len);
    lua_pop(state, 1);
  } else {
    luaL_error(state, "a property type must be a string or table");
  }

  unreal_prop_type_t type = {0};
  str_t              name = str_make((void *)type_data, (uint64_t)type_len);
  if (!type_data || type_len == 0 || memchr(type_data, 0, type_len) || !lua_overdub_unreal_prop_kind_from_name(name, &type.type)) {
    luaL_error(state, "unknown or missing reflected property Type");
  }

  if (lua_overdub_unreal_prop_kind_uses_class(type.type)) {
    type.cls = globals.unreal.core_object;
    if (table_idx) {
      lua_getfield(state, table_idx, "Class");
      if (!lua_isnil(state, -1)) {
        type.cls = lua_overdub_unreal_class_check(state, -1);
      }
      lua_pop(state, 1);
    }

    if (type.type == UNREAL_PROP_KIND_INTERFACE && (!type.cls || !(type.cls->class_flags & CLASS_INTERFACE))) {
      luaL_error(state, "an Interface property requires an interface UClass in Class");
    }
  }

  if (type.type == UNREAL_PROP_KIND_STRUCT) {
    if (!table_idx) {
      luaL_error(state, "a Struct property requires a type table with Struct");
    }

    lua_getfield(state, table_idx, "Struct");
    uobject_t *object = lua_overdub_unreal_object_check(state, -1);
    if (!globals.unreal.core_scriptstruct || !unreal_uobject_is_a(object, globals.unreal.core_scriptstruct)) {
      luaL_error(state, "Struct must be a live UScriptStruct");
    }
    type.struct_type = (uscript_struct_t *)object;
    lua_pop(state, 1);
  }

  if (type.type == UNREAL_PROP_KIND_ENUM) {
    if (!table_idx) {
      luaL_error(state, "an Enum property requires a type table with Enum");
    }

    lua_getfield(state, table_idx, "Enum");
    type.enum_type = lua_overdub_unreal_enum_check(state, -1);
    lua_pop(state, 1);

    unreal_prop_type_t *underlying = ARENA_PUSH_ZERO(context->arena, unreal_prop_type_t);
    if (!underlying) {
      luaL_error(state, "could not allocate the enum underlying type");
    }

    lua_getfield(state, table_idx, "Underlying");
    if (lua_isnil(state, -1)) {
      underlying->type = UNREAL_PROP_KIND_BYTE;
    } else {
      *underlying = lua_overdub_unreal_prop_type_parse(state, context, -1, depth + 1);
    }
    lua_pop(state, 1);
    type.inner = underlying;
  }

  bool has_inner = type.type == UNREAL_PROP_KIND_ARRAY || type.type == UNREAL_PROP_KIND_SET;
  if (has_inner) {
    if (!table_idx) {
      luaL_error(state, "an Array or Set property requires a type table with Inner");
    }

    unreal_prop_type_t *inner = ARENA_PUSH_ZERO(context->arena, unreal_prop_type_t);
    if (!inner) {
      luaL_error(state, "could not allocate the container element type");
    }

    lua_getfield(state, table_idx, "Inner");
    *inner = lua_overdub_unreal_prop_type_parse(state, context, -1, depth + 1);
    lua_pop(state, 1);
    type.inner = inner;
  }

  if (type.type == UNREAL_PROP_KIND_MAP) {
    if (!table_idx) {
      luaL_error(state, "a Map property requires a type table with Key and Value");
    }

    unreal_prop_type_t *key = ARENA_PUSH_ZERO(context->arena, unreal_prop_type_t);
    unreal_prop_type_t *val = ARENA_PUSH_ZERO(context->arena, unreal_prop_type_t);
    if (!key || !val) {
      luaL_error(state, "could not allocate the map key/value types");
    }

    lua_getfield(state, table_idx, "Key");
    *key = lua_overdub_unreal_prop_type_parse(state, context, -1, depth + 1);
    lua_pop(state, 1);

    lua_getfield(state, table_idx, "Value");
    *val = lua_overdub_unreal_prop_type_parse(state, context, -1, depth + 1);
    lua_pop(state, 1);

    type.inner = key;
    type.value = val;
  }
  return type;
}

static void
lua_overdub_unreal_prop_def_parse(lua_State *state, lua_overdub_unreal_context_t *context, int idx, unreal_prop_def_t *def, bool parameter)
{
  idx = lua_absindex(state, idx);
  if (!lua_istable(state, idx)) {
    luaL_error(state, "each reflected property or parameter declaration must be a table");
  }

  unreal_prop_type_t type = lua_overdub_unreal_prop_type_parse(state, context, idx, 0);
  def->name        = lua_overdub_unreal_required_string_field(state, idx, "Name");
  def->type        = type.type;
  def->flags       = (eprop_flags_t)lua_overdub_unreal_optional_flags(state, idx);
  def->cls         = type.cls;
  def->struct_type = type.struct_type;
  def->enum_type   = type.enum_type;
  if (type.inner) {
    def->inner = *type.inner;
  }

  if (type.value) {
    def->value = *type.value;
  }

  if (parameter) {
    lua_getfield(state, idx, "Const");
    if (lua_toboolean(state, -1)) {
      def->flags |= CPF_CONST_PARM;
    }
    lua_pop(state, 1);
  }
}

static uint32_t
lua_overdub_unreal_list_count(lua_State *state, int idx, const char *name, uint32_t max_count)
{
  lua_Integer count = luaL_len(state, idx);
  if (count < 0 || (lua_Unsigned)count > max_count) {
    luaL_error(state, "%s has too many entries", name);
  }
  return (uint32_t)count;
}

static unreal_prop_def_t *
lua_overdub_unreal_props_parse(lua_State *state, lua_overdub_unreal_context_t *context, int class_idx, uint32_t *out_count)
{
  class_idx = lua_absindex(state, class_idx);
  lua_getfield(state, class_idx, "Properties");
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);
    *out_count = 0;
    return NULL;
  }

  if (!lua_istable(state, -1)) {
    luaL_error(state, "Properties must be an ordered array");
  }

  int      list_idx = lua_gettop(state);
  uint32_t count    = lua_overdub_unreal_list_count(state, list_idx, "Properties", UINT16_MAX);
  unreal_prop_def_t *props = count > 0 ? ARENA_PUSH_ARRAY_ZERO(context->arena, unreal_prop_def_t, count) : NULL;
  if (count > 0 && !props) {
    luaL_error(state, "could not allocate reflected property declarations");
  }

  for (uint32_t i = 0; i < count; ++i) {
    lua_rawgeti(state, list_idx, (lua_Integer)i + 1);
    lua_overdub_unreal_prop_def_parse(state, context, -1, &props[i], false);
    lua_pop(state, 1);
  }

  lua_pop(state, 1);
  *out_count = count;
  return props;
}

static unreal_prop_def_t *
lua_overdub_unreal_func_params_parse(lua_State *state, lua_overdub_unreal_context_t *context, int func_idx, uint32_t *out_count)
{
  func_idx = lua_absindex(state, func_idx);
  lua_getfield(state, func_idx, "Parameters");
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);
    *out_count = 0;
    return NULL;
  }

  if (!lua_istable(state, -1)) {
    luaL_error(state, "Function Parameters must be an ordered array");
  }

  int      list_idx = lua_gettop(state);
  uint32_t count    = lua_overdub_unreal_list_count(state, list_idx, "Function Parameters", UINT8_MAX - 1);
  unreal_prop_def_t *params = count > 0 ? ARENA_PUSH_ARRAY_ZERO(context->arena, unreal_prop_def_t, count) : NULL;
  if (count > 0 && !params) {
    luaL_error(state, "could not allocate reflected function parameters");
  }

  for (uint32_t i = 0; i < count; ++i) {
    lua_rawgeti(state, list_idx, (lua_Integer)i + 1);
    lua_overdub_unreal_prop_def_parse(state, context, -1, &params[i], true);
    lua_pop(state, 1);
  }

  lua_pop(state, 1);
  *out_count = count;
  return params;
}

static unreal_func_def_t *
lua_overdub_unreal_funcs_parse(lua_State *state, lua_overdub_unreal_context_t *context, int class_idx,
                               lua_overdub_reflected_func_t **out_records, uint32_t *out_count)
{
  class_idx = lua_absindex(state, class_idx);
  lua_getfield(state, class_idx, "Functions");
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);
    *out_records = NULL;
    *out_count   = 0;
    return NULL;
  }

  if (!lua_istable(state, -1)) {
    luaL_error(state, "Functions must be an ordered array");
  }

  int      list_idx = lua_gettop(state);
  uint32_t count    = lua_overdub_unreal_list_count(state, list_idx, "Functions", UINT16_MAX);

  unreal_func_def_t            *funcs   = count > 0 ? ARENA_PUSH_ARRAY_ZERO(context->arena, unreal_func_def_t, count)            : NULL;
  lua_overdub_reflected_func_t *records = count > 0 ? ARENA_PUSH_ARRAY_ZERO(context->arena, lua_overdub_reflected_func_t, count) : NULL;

  if (count > 0 && (!funcs || !records)) {
    luaL_error(state, "could not allocate reflected function declarations");
  }

  for (uint32_t i = 0; i < count; ++i) {
    lua_rawgeti(state, list_idx, (lua_Integer)i + 1);
    if (!lua_istable(state, -1)) {
      luaL_error(state, "each reflected function declaration must be a table");
    }

    int func_idx = lua_gettop(state);

    funcs[i].name   = lua_overdub_unreal_required_string_field(state, func_idx, "Name");
    funcs[i].flags  = (efunc_flags_t)lua_overdub_unreal_optional_flags(state, func_idx);
    funcs[i].params = lua_overdub_unreal_func_params_parse(state, context, func_idx, &funcs[i].num_params);
    funcs[i].impl   = lua_overdub_unreal_reflected_func_call;
    funcs[i].user   = &records[i];

    records[i].context      = context;
    records[i].callback_ref = LUA_NOREF;

    lua_getfield(state, func_idx, "Return");
    if (!lua_isnil(state, -1)) {
      funcs[i].ret = lua_overdub_unreal_prop_type_parse(state, context, -1, 0);
    }
    lua_pop(state, 1);

    lua_getfield(state, func_idx, "Implementation");
    if (!lua_isfunction(state, -1)) {
      luaL_error(state, "Function Implementation must be a Lua function");
    }
    lua_pop(state, 1);
    lua_pop(state, 1);
  }

  for (uint32_t i = 0; i < count; ++i) {
    lua_rawgeti(state, list_idx, (lua_Integer)i + 1);
    lua_getfield(state, -1, "Implementation");
    records[i].callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    lua_pop(state, 1);
  }

  lua_pop(state, 1);
  *out_records = records;
  *out_count   = count;
  return funcs;
}

static fprop_t *
lua_overdub_unreal_func_return_prop(ufunc_t *function)
{
  fprop_t *result = NULL;
  for (ffield_t *field = function ? function->child_props : NULL; field && !result; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (unreal_fprop_get_param_role(prop) == UNREAL_FUNC_PARAM_RETURN) {
      result = prop;
    }
  }
  return result;
}

static void
lua_overdub_unreal_reflected_func_disable(lua_overdub_reflected_func_t *func)
{
  lua_overdub_unreal_context_t *context = func ? func->context : NULL;
  lua_State                    *state   = context && context->runtime ? context->runtime->state : NULL;
  if (state && func->callback_ref != LUA_NOREF && func->callback_ref != LUA_REFNIL) {
    luaL_unref(state, LUA_REGISTRYINDEX, func->callback_ref);
  }

  if (func) {
    func->callback_ref = LUA_NOREF;
    func->active       = false;
  }
}

static bool
lua_overdub_unreal_reflected_func_can_defer(ufunc_t *function)
{
  if (!function) {
    return false;
  }

  bool can_defer = true;
  for (ffield_t *field = function->child_props; field && can_defer; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if ((prop->prop_flags & CPF_PARM) && unreal_fprop_get_param_role(prop) != UNREAL_FUNC_PARAM_INPUT) {
      can_defer = false;
    }
  }
  return can_defer;
}

static bool
lua_overdub_unreal_reflected_func_ready(lua_overdub_reflected_func_t *func, unreal_func_call_t *call)
{
  if (!func || !call) {
    return false;
  }

  lua_overdub_unreal_context_t *context = func->context;
  if (!context) {
    return false;
  }

  lua_runtime_t *runtime = context->runtime;
  return call && call->object && call->function && func && func->active && runtime && runtime->state && context->in_callback && lua_overdub_unreal_access_check(context);
}

static void
lua_overdub_unreal_reflected_func_invoke(lua_overdub_reflected_func_t *func, unreal_func_call_t *call)
{
  lua_overdub_unreal_context_t *context     = func->context;
  lua_runtime_t                *runtime     = context->runtime;
  bool                         *in_callback = context->in_callback;
  lua_State                    *state       = runtime->state;

  int  stack_base = lua_gettop(state);
  int  arg_count  = lua_unreal_object_push(state, call->object);
  bool converted  = arg_count == 1;
  for (ffield_t *field = call->function->child_props; field && converted; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (unreal_fprop_get_param_role(prop) == UNREAL_FUNC_PARAM_INPUT) {
      void *value = unreal_fprop_value_in_container(prop, call->params, 0);
      converted   = value && lua_unreal_prop_push(state, prop, value, call->object) == 1;
      arg_count  += converted;
    }
  }

  fprop_t *ret_prop = lua_overdub_unreal_func_return_prop(call->function);
  bool     called   = false;
  if (converted) {
    *in_callback = true;
    called       = lua_runtime_call_ref(runtime, func->callback_ref, arg_count, ret_prop ? 1 : 0);
    *in_callback = false;
  }

  bool returned = called;
  if (called && ret_prop) {
    returned = call->return_value && lua_unreal_prop_write(state, -1, ret_prop, call->return_value, call->object);
  }

  lua_settop(state, stack_base);
  if (!converted || !called || !returned) {
    lua_overdub_unreal_reflected_func_disable(func);
    LOG_ERROR("Overdub Lua mod '%.*s': disabling failed Lua-backed UFunction implementation", STR_ARG(context->mod_id));
  }
}

static void MOD_CALL
lua_overdub_unreal_reflected_func_call(unreal_func_call_t *call)
{
  lua_overdub_reflected_func_t *func = call ? call->user : NULL;
  if (!lua_overdub_unreal_reflected_func_ready(func, call)) {
    return;
  }

  lua_overdub_unreal_context_t *context = func->context;
  if (*context->in_callback) {
    if (lua_overdub_unreal_reflected_func_can_defer(call->function)) {
      lua_overdub_unreal_queue_reflected_func(func, call);
    } else if (!func->reentry_reported) {
      LOG_ERROR("Overdub Lua mod '%.*s' cannot defer a recursive Lua-backed UFunction with return or output parameters", STR_ARG(context->mod_id));
      func->reentry_reported = true;
    }
    return;
  }

  lua_overdub_unreal_reflected_func_invoke(func, call);
}

static lua_overdub_reflected_class_t *
lua_overdub_unreal_reflected_class_find(lua_overdub_unreal_context_t *context, str_t name)
{
  lua_overdub_reflected_class_t *found = NULL;
  for (lua_overdub_reflected_class_t *entry = context->reflected_classes; entry && !found; entry = entry->next) {
    if (str_equal(entry->name, name, STR_CMP_FLAG_IGNORE_CASE)) {
      found = entry;
    }
  }
  return found;
}

static void
lua_overdub_unreal_exclude_native_time(lua_runtime_t *runtime, uint64_t start_us)
{
  if (runtime && runtime->execution_deadline_us > 0) {
    uint64_t elapsed_us = time_now_us() - start_us;
    if (runtime->execution_deadline_us <= UINT64_MAX - elapsed_us) {
      runtime->execution_deadline_us += elapsed_us;
    } else {
      runtime->execution_deadline_us = UINT64_MAX;
    }
  }
}

static bool
lua_overdub_unreal_apply_lua_defaults(lua_State *state, int class_idx, uclass_t *cls)
{
  bool applied = cls && cls->cdo;
  class_idx    = lua_absindex(state, class_idx);
  lua_getfield(state, class_idx, "Properties");
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);
    return applied;
  }

  int      list_idx = lua_gettop(state);
  uint32_t count    = (uint32_t)luaL_len(state, list_idx);
  for (uint32_t i = 0; i < count && applied; ++i) {
    lua_rawgeti(state, list_idx, (lua_Integer)i + 1);
    int   prop_idx = lua_gettop(state);
    str_t name     = lua_overdub_unreal_required_string_field(state, prop_idx, "Name");

    lua_getfield(state, prop_idx, "Default");
    if (!lua_isnil(state, -1)) {
      fprop_t *prop  = unreal_ustruct_find_prop((ustruct_t *)cls, name);
      void    *value = prop ? unreal_fprop_value_in_container(prop, cls->cdo, 0) : NULL;
      applied        = value && lua_unreal_prop_write(state, -1, prop, value, cls->cdo);
    }
    lua_pop(state, 1);
    lua_pop(state, 1);
  }

  lua_pop(state, 1);
  return applied;
}

static int
lua_overdub_unreal_define_class_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context   = lua_overdub_unreal_context_check(state);
  uint64_t                      start_us  = time_now_us();
  size_t                        name_len  = 0;
  const char                   *name_data = luaL_checklstring(state, 1, &name_len);
  if (name_len == 0 || memchr(name_data, 0, name_len)) {
    return luaL_argerror(state, 1, "class name cannot be empty or contain null bytes");
  }

  luaL_checktype(state, 2, LUA_TTABLE);
  str_t name = str_make((void *)name_data, (uint64_t)name_len);
  if (lua_overdub_unreal_reflected_class_find(context, name)) {
    return luaL_error(state, "Unreal class '%.*s' was already defined by this Lua runtime", (int)name.len, name.data);
  }

  lua_overdub_reflected_class_t *entry = ARENA_PUSH_ZERO(context->arena, lua_overdub_reflected_class_t);
  str_t                           name_copy = str_push_copy(context->arena, name);
  if (!entry || !name_copy.data) {
    return luaL_error(state, "could not retain Unreal class '%.*s'", (int)name.len, name.data);
  }

  lua_getfield(state, 2, "Parent");
  uclass_t *parent = lua_overdub_unreal_class_check(state, -1);
  lua_pop(state, 1);

  uint32_t           num_props = 0;
  unreal_prop_def_t *props     = lua_overdub_unreal_props_parse(state, context, 2, &num_props);

  uint32_t                      num_funcs = 0;
  lua_overdub_reflected_func_t *records   = NULL;
  unreal_func_def_t            *funcs     = lua_overdub_unreal_funcs_parse(state, context, 2, &records, &num_funcs);

  unreal_class_def_t def = {
    .struct_size = sizeof(def),
    .parent      = parent,
    .props       = props,
    .num_props   = num_props,
    .funcs       = funcs,
    .num_funcs   = num_funcs,
  };

  unreal_reflect_owner_t owner            = (unreal_reflect_owner_t)(uintptr_t)context;
  uclass_t              *cls              = unreal_reflect_define_class_owned(owner, context->mod_id, name, &def);
  bool                   defaults_applied = false;
  if (cls) {
    entry->name                 = name_copy;
    entry->funcs                = records;
    entry->num_funcs            = num_funcs;
    entry->next                 = context->reflected_classes;
    context->reflected_classes = entry;

    for (uint32_t i = 0; i < num_funcs; ++i) {
      records[i].active = true;
    }

    context->reflected_active = true;
    defaults_applied          = lua_overdub_unreal_apply_lua_defaults(state, 2, cls);
  }
  lua_overdub_unreal_exclude_native_time(context->runtime, start_us);

  if (!cls) {
    for (uint32_t i = 0; i < num_funcs; ++i) {
      lua_overdub_unreal_reflected_func_disable(&records[i]);
    }
    return luaL_error(state, "could not define Unreal class '%.*s'", (int)name.len, name.data);
  }

  if (!defaults_applied) {
    return luaL_error(state, "could not apply Lua defaults to Unreal class '%.*s'", (int)name.len, name.data);
  }
  return lua_unreal_object_push(state, (uobject_t *)cls);
}

static int
lua_overdub_unreal_notification_add_lua(lua_State *state, lua_overdub_unreal_context_t *context, fweak_object_ptr_t target,
                                        uobject_t *target_object, lua_overdub_notification_kind_t kind)
{
  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  int callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  int      slot = -1;
  uint64_t id   = 0;

  AcquireSRWLockExclusive(&g_lua_overdub_notification_lock);
  EnterCriticalSection(&context->notification_lock);
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS && slot < 0; ++i) {
    if (!context->notifications[i].active) {
      slot = i;
    }
  }

  if (slot >= 0) {
    id = lua_overdub_unreal_next_id(&context->next_notification_id);
    context->notifications[slot] = (lua_overdub_notification_t){
      .target        = target,
      .target_object = target_object,
      .id            = id,
      .callback_ref  = callback_ref,
      .kind          = kind,
      .active        = true,
    };
    context->notification_count += 1;

    if (!context->notifications_attached) {
      context->notification_next       = g_lua_overdub_notification_first;
      context->notifications_attached  = true;
      g_lua_overdub_notification_first = context;
      InterlockedIncrement(&g_lua_overdub_notification_context_count);
    }
  }
  LeaveCriticalSection(&context->notification_lock);
  ReleaseSRWLockExclusive(&g_lua_overdub_notification_lock);

  if (slot < 0) {
    luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);
    return luaL_error(state, "Overdub Unreal notification limit reached");
  }

  lua_overdub_notification_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_notification_handle_t){
    .context = context,
    .id      = id,
    .slot    = slot,
  };
  luaL_setmetatable(state, LUA_OVERDUB_NOTIFY_HANDLE_META);
  return 1;
}

static int
lua_overdub_unreal_notify_on_new_object_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  uclass_t                     *target  = lua_overdub_unreal_class_arg(state, 1);
  fweak_object_ptr_t            weak    = lua_overdub_unreal_invalid_weak();
  if (!unreal_fweak_object_from_object((uobject_t *)target, &weak)) {
    return luaL_argerror(state, 1, "expected a live UClass");
  }

  return lua_overdub_unreal_notification_add_lua(state, context, weak, NULL, LUA_OVERDUB_NOTIFICATION_CREATED);
}

static int
lua_overdub_unreal_notify_on_delete_object_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  uobject_t                    *target  = lua_overdub_unreal_object_check(state, 1);
  fweak_object_ptr_t            weak    = lua_overdub_unreal_invalid_weak();
  if (!unreal_fweak_object_from_object(target, &weak)) {
    return luaL_argerror(state, 1, "expected a live UObject");
  }

  return lua_overdub_unreal_notification_add_lua(state, context, weak, target, LUA_OVERDUB_NOTIFICATION_DELETED);
}

static int
lua_overdub_unreal_hook_add_lua(lua_State *state, lua_overdub_unreal_context_t *context, uobject_t *target, lua_overdub_hook_kind_t kind)
{
  luaL_checktype(state, 2, LUA_TTABLE);
  lua_getfield(state, 2, "Before");
  lua_getfield(state, 2, "After");

  bool has_before = lua_type(state, -2) == LUA_TFUNCTION;
  bool has_after  = lua_type(state, -1) == LUA_TFUNCTION;
  if (!lua_isnoneornil(state, -2) && !has_before) {
    return luaL_argerror(state, 2, "Before must be a function or nil");
  }

  if (!lua_isnoneornil(state, -1) && !has_after) {
    return luaL_argerror(state, 2, "After must be a function or nil");
  }

  if (!has_before && !has_after) {
    return luaL_argerror(state, 2, "expected a Before or After callback");
  }

  int slot = -1;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_HOOKS; ++i) {
    if (!context->hooks[i].active) {
      slot = i;
      break;
    }
  }

  if (slot < 0) {
    return luaL_error(state, "Overdub Unreal hook limit reached");
  }

  fweak_object_ptr_t target_weak = lua_overdub_unreal_invalid_weak();
  if (!unreal_fweak_object_from_object(target, &target_weak)) {
    return luaL_argerror(state, 1, "expected a live hook target");
  }

  int before_ref = LUA_NOREF;
  int after_ref  = LUA_NOREF;
  if (has_before) {
    lua_pushvalue(state, -2);
    before_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  }

  if (has_after) {
    lua_pushvalue(state, -1);
    after_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  }
  lua_pop(state, 2);

  uint64_t id = lua_overdub_unreal_next_hook_id(context);
  context->hooks[slot] = (lua_overdub_hook_t){
    .target     = target_weak,
    .id         = id,
    .before_ref = before_ref,
    .after_ref  = after_ref,
    .kind       = kind,
    .active     = true,
  };

  if (kind == LUA_OVERDUB_HOOK_POST_LOAD) {
    g_lua_overdub_post_load_hook_count += 1;
  }

  lua_overdub_hook_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_hook_handle_t){
    .context = context,
    .id      = id,
    .slot    = slot,
  };
  luaL_setmetatable(state, LUA_OVERDUB_HOOK_HANDLE_META);
  return 1;
}

static int
lua_overdub_unreal_hook_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context  = lua_overdub_unreal_context_check(state);
  ufunc_t                      *function = NULL;
  if (lua_type(state, 1) == LUA_TSTRING) {
    size_t      path_len  = 0;
    const char *path_data = lua_tolstring(state, 1, &path_len);
    if (path_len == 0 || memchr(path_data, 0, path_len)) {
      return luaL_argerror(state, 1, "UFunction path cannot be empty or contain null bytes");
    }

    uobject_t *object = unreal_uobject_find_by_path_name(globals.unreal.core_func, str_make((void *)path_data, (uint64_t)path_len));
    if (!object) {
      return luaL_error(state, "UFunction '%.*s' was not found", (int)path_len, path_data);
    }

    function = (ufunc_t *)object;
  } else {
    function = lua_overdub_unreal_function_check(state, 1);
  }

  return lua_overdub_unreal_hook_add_lua(state, context, (uobject_t *)function, LUA_OVERDUB_HOOK_FUNCTION);
}

static int
lua_overdub_unreal_hook_post_load_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  uclass_t                     *target  = lua_overdub_unreal_class_arg(state, 1);
  return lua_overdub_unreal_hook_add_lua(state, context, (uobject_t *)target, LUA_OVERDUB_HOOK_POST_LOAD);
}

static int
lua_overdub_unreal_struct_get_size_lua(lua_State *state)
{
  lua_pushinteger(state, lua_overdub_unreal_struct_check(state, 1)->props_size);
  return 1;
}

static int
lua_overdub_unreal_struct_get_min_alignment_lua(lua_State *state)
{
  lua_pushinteger(state, lua_overdub_unreal_struct_check(state, 1)->min_alignment);
  return 1;
}

static int
lua_overdub_unreal_class_get_flags_lua(lua_State *state)
{
  lua_pushinteger(state, (lua_Integer)lua_overdub_unreal_class_check(state, 1)->class_flags);
  return 1;
}

static int
lua_overdub_unreal_class_has_any_flags_lua(lua_State *state)
{
  uclass_t      *cls   = lua_overdub_unreal_class_check(state, 1);
  eclass_flags_t flags = lua_overdub_unreal_u32_check(state, 2, "class flags must fit in an unsigned 32-bit integer");
  lua_pushboolean(state, (cls->class_flags & flags) != 0);
  return 1;
}

static int
lua_overdub_unreal_class_has_all_flags_lua(lua_State *state)
{
  uclass_t      *cls   = lua_overdub_unreal_class_check(state, 1);
  eclass_flags_t flags = lua_overdub_unreal_u32_check(state, 2, "class flags must fit in an unsigned 32-bit integer");
  lua_pushboolean(state, (cls->class_flags & flags) == flags);
  return 1;
}

static int
lua_overdub_unreal_class_get_within_lua(lua_State *state)
{
  return lua_unreal_object_push(state, (uobject_t *)lua_overdub_unreal_class_check(state, 1)->class_within);
}

static int
lua_overdub_unreal_class_get_generated_by_lua(lua_State *state)
{
  return lua_unreal_object_push(state, lua_overdub_unreal_class_check(state, 1)->class_generated_by);
}

static int
lua_overdub_unreal_class_get_interfaces_lua(lua_State *state)
{
  uclass_t *cls = lua_overdub_unreal_class_check(state, 1);
  tarray_fimplemented_interface_t *interfaces = &cls->interfaces;
  if (interfaces->num < 0 || interfaces->max < interfaces->num || interfaces->num > 4096 || (interfaces->num > 0 && !interfaces->data)) {
    return luaL_error(state, "the UClass interface table is invalid");
  }

  lua_createtable(state, interfaces->num, 0);
  for (int32_t i = 0; i < interfaces->num; ++i) {
    fimplemented_interface_t *interface = &interfaces->data[i];
    lua_createtable(state, 0, 3);
    lua_unreal_object_push(state, (uobject_t *)interface->cls);
    lua_setfield(state, -2, "Class");
    lua_pushinteger(state, interface->pointer_offset);
    lua_setfield(state, -2, "PointerOffset");
    lua_pushboolean(state, interface->implemented_by_k2);
    lua_setfield(state, -2, "ImplementedByK2");
    lua_rawseti(state, -2, i + 1);
  }
  return 1;
}

static int
lua_overdub_unreal_function_get_parameter_count_lua(lua_State *state)
{
  lua_pushinteger(state, lua_overdub_unreal_function_check(state, 1)->num_params);
  return 1;
}

static int
lua_overdub_unreal_function_get_parameter_size_lua(lua_State *state)
{
  lua_pushinteger(state, lua_overdub_unreal_function_check(state, 1)->params_size);
  return 1;
}

static int
lua_overdub_unreal_function_get_return_offset_lua(lua_State *state)
{
  uint16_t offset = lua_overdub_unreal_function_check(state, 1)->return_val_offset;
  if (offset == UINT16_MAX) {
    lua_pushnil(state);
  } else {
    lua_pushinteger(state, offset);
  }
  return 1;
}

static int
lua_overdub_unreal_enum_num_lua(lua_State *state)
{
  lua_pushinteger(state, lua_overdub_unreal_enum_check(state, 1)->names.num);
  return 1;
}

static bool
lua_overdub_unreal_method_push(lua_State *state, uobject_t *object, str_t name)
{
  if (!state || !object) {
    return false;
  }

  lua_CFunction function = NULL;
  if (str_equal(name, STR_LIT("BindDelegate"), 0)) {
    function = lua_overdub_unreal_bind_delegate_lua;
  }

  if (globals.unreal.core_class && unreal_uobject_is_a(object, globals.unreal.core_class)) {
    if (str_equal(name, STR_LIT("GetClassFlags"), 0)) {
      function = lua_overdub_unreal_class_get_flags_lua;
    } else if (str_equal(name, STR_LIT("HasAnyClassFlags"), 0)) {
      function = lua_overdub_unreal_class_has_any_flags_lua;
    } else if (str_equal(name, STR_LIT("HasAllClassFlags"), 0)) {
      function = lua_overdub_unreal_class_has_all_flags_lua;
    } else if (str_equal(name, STR_LIT("GetWithinClass"), 0)) {
      function = lua_overdub_unreal_class_get_within_lua;
    } else if (str_equal(name, STR_LIT("GetGeneratedBy"), 0)) {
      function = lua_overdub_unreal_class_get_generated_by_lua;
    } else if (str_equal(name, STR_LIT("GetInterfaces"), 0)) {
      function = lua_overdub_unreal_class_get_interfaces_lua;
    } else if (str_equal(name, STR_LIT("ConstructObject"), 0)) {
      function = lua_overdub_unreal_construct_object_lua;
    } else if (str_equal(name, STR_LIT("SpawnActor"), 0)) {
      function = lua_overdub_unreal_spawn_actor_lua;
    }
  }

  if (!function && globals.unreal.core_func && unreal_uobject_is_a(object, globals.unreal.core_func)) {
    if (str_equal(name, STR_LIT("GetParameterCount"), 0)) {
      function = lua_overdub_unreal_function_get_parameter_count_lua;
    } else if (str_equal(name, STR_LIT("GetParameterSize"), 0)) {
      function = lua_overdub_unreal_function_get_parameter_size_lua;
    } else if (str_equal(name, STR_LIT("GetReturnValueOffset"), 0)) {
      function = lua_overdub_unreal_function_get_return_offset_lua;
    }
  }

  if (!function && globals.unreal.core_enum && unreal_uobject_is_a(object, globals.unreal.core_enum) &&
      str_equal(name, STR_LIT("NumEnums"), 0)) {
    function = lua_overdub_unreal_enum_num_lua;
  }

  bool is_struct = (globals.unreal.core_class && unreal_uobject_is_a(object, globals.unreal.core_class)) ||
                   (globals.unreal.core_scriptstruct && unreal_uobject_is_a(object, globals.unreal.core_scriptstruct)) ||
                   (globals.unreal.core_func && unreal_uobject_is_a(object, globals.unreal.core_func));
  if (!function && is_struct) {
    if (str_equal(name, STR_LIT("GetStructureSize"), 0)) {
      function = lua_overdub_unreal_struct_get_size_lua;
    } else if (str_equal(name, STR_LIT("GetMinAlignment"), 0)) {
      function = lua_overdub_unreal_struct_get_min_alignment_lua;
    }
  }

  if (!function) {
    return false;
  }

  lua_pushcfunction(state, function);
  return true;
}

static int
lua_overdub_unreal_object_index_lua(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  uobject_t *object = lua_unreal_object_get(state, 1);
  if (object && lua_type(state, 2) == LUA_TSTRING) {
    size_t      name_len  = 0;
    const char *name_data = lua_tolstring(state, 2, &name_len);
    str_t       name      = str_make((void *)name_data, (uint64_t)name_len);
    fprop_t    *property  = object->cls ? unreal_ustruct_find_prop((ustruct_t *)object->cls, name) : NULL;
    if (!property && lua_overdub_unreal_method_push(state, object, name)) {
      return 1;
    }
  }

  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 1);
  lua_pushvalue(state, 2);
  lua_call(state, 2, 1);
  return 1;
}

static void
lua_overdub_unreal_class_flags_push(lua_State *state)
{
  static const struct {
    const char    *name;
    eclass_flags_t value;
  } flags[] = {
    {"CLASS_None",                       CLASS_NONE                        },
    {"CLASS_Abstract",                   CLASS_ABSTRACT                    },
    {"CLASS_DefaultConfig",              CLASS_DEFAULT_CONFIG              },
    {"CLASS_Config",                     CLASS_CONFIG                      },
    {"CLASS_Transient",                  CLASS_TRANSIENT                   },
    {"CLASS_Parsed",                     CLASS_PARSED                      },
    {"CLASS_MatchedSerializers",         CLASS_MATCHED_SERIALIZERS         },
    {"CLASS_ProjectUserConfig",          CLASS_PROJECT_USER_CONFIG         },
    {"CLASS_Native",                     CLASS_NATIVE                      },
    {"CLASS_NoExport",                   CLASS_NO_EXPORT                   },
    {"CLASS_NotPlaceable",               CLASS_NOT_PLACEABLE               },
    {"CLASS_PerObjectConfig",            CLASS_PER_OBJECT_CONFIG           },
    {"CLASS_ReplicationDataIsSetUp",     CLASS_REPLICATION_DATA_IS_SET_UP  },
    {"CLASS_EditInlineNew",              CLASS_EDIT_INLINE_NEW             },
    {"CLASS_CollapseCategories",         CLASS_COLLAPSE_CATEGORIES         },
    {"CLASS_Interface",                  CLASS_INTERFACE                   },
    {"CLASS_CustomConstructor",          CLASS_CUSTOM_CONSTRUCTOR          },
    {"CLASS_Const",                      CLASS_CONST                       },
    {"CLASS_LayoutChanging",             CLASS_LAYOUT_CHANGING             },
    {"CLASS_CompiledFromBlueprint",      CLASS_COMPILED_FROM_BLUEPRINT     },
    {"CLASS_MinimalAPI",                 CLASS_MINIMAL_API                 },
    {"CLASS_RequiredAPI",                CLASS_REQUIRED_API                },
    {"CLASS_DefaultToInstanced",         CLASS_DEFAULT_TO_INSTANCED        },
    {"CLASS_TokenStreamAssembled",       CLASS_TOKEN_STREAM_ASSEMBLED      },
    {"CLASS_HasInstancedReference",      CLASS_HAS_INSTANCED_REFERENCE     },
    {"CLASS_Hidden",                     CLASS_HIDDEN                      },
    {"CLASS_Deprecated",                 CLASS_DEPRECATED                  },
    {"CLASS_HideDropDown",               CLASS_HIDE_DROP_DOWN              },
    {"CLASS_GlobalUserConfig",           CLASS_GLOBAL_USER_CONFIG          },
    {"CLASS_Intrinsic",                  CLASS_INTRINSIC                   },
    {"CLASS_Constructed",                CLASS_CONSTRUCTED                 },
    {"CLASS_ConfigDoNotCheckDefaults",   CLASS_CONFIG_DO_NOT_CHECK_DEFAULTS},
    {"CLASS_NewerVersionExists",         CLASS_NEWER_VERSION_EXISTS        },
  };

  lua_createtable(state, 0, (int)(sizeof(flags) / sizeof(flags[0])));
  for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
    lua_pushinteger(state, (lua_Integer)flags[i].value);
    lua_setfield(state, -2, flags[i].name);
  }
}

static void
lua_overdub_unreal_property_flags_push(lua_State *state)
{
  static const struct {
    const char    *name;
    eprop_flags_t  value;
  } flags[] = {
    {"CPF_None",                      CPF_NONE                       },
    {"CPF_Edit",                      CPF_EDIT                       },
    {"CPF_ConstParm",                 CPF_CONST_PARM                 },
    {"CPF_BlueprintVisible",          CPF_BLUEPRINT_VISIBLE          },
    {"CPF_BlueprintReadOnly",         CPF_BLUEPRINT_READ_ONLY        },
    {"CPF_EditFixedSize",             CPF_EDIT_FIXED_SIZE            },
    {"CPF_Transient",                 CPF_TRANSIENT                  },
    {"CPF_Config",                    CPF_CONFIG                     },
    {"CPF_DisableEditOnTemplate",     CPF_DISABLE_EDIT_ON_TEMPLATE   },
    {"CPF_DisableEditOnInstance",     CPF_DISABLE_EDIT_ON_INSTANCE   },
    {"CPF_EditConst",                 CPF_EDIT_CONST                 },
    {"CPF_GlobalConfig",              CPF_GLOBAL_CONFIG              },
    {"CPF_InstancedReference",        CPF_INSTANCED_REFERENCE        },
    {"CPF_DuplicateTransient",        CPF_DUPLICATE_TRANSIENT        },
    {"CPF_SaveGame",                  CPF_SAVE_GAME                  },
    {"CPF_NoClear",                   CPF_NO_CLEAR                   },
    {"CPF_Deprecated",                CPF_DEPRECATED                 },
    {"CPF_Interp",                    CPF_INTERP                     },
    {"CPF_NonTransactional",          CPF_NON_TRANSACTIONAL          },
    {"CPF_EditorOnly",                CPF_EDITOR_ONLY                },
    {"CPF_AssetRegistrySearchable",   CPF_ASSET_REGISTRY_SEARCHABLE  },
    {"CPF_SimpleDisplay",             CPF_SIMPLE_DISPLAY             },
    {"CPF_AdvancedDisplay",           CPF_ADVANCED_DISPLAY           },
    {"CPF_Protected",                 CPF_PROTECTED                  },
    {"CPF_ExposeOnSpawn",             CPF_EXPOSE_ON_SPAWN            },
    {"CPF_TextExportTransient",       CPF_TEXT_EXPORT_TRANSIENT      },
    {"CPF_NonPIEDuplicateTransient",  CPF_NON_PIE_DUPLICATE_TRANSIENT},
    {"CPF_SkipSerialization",         CPF_SKIP_SERIALIZATION         },
  };

  lua_createtable(state, 0, (int)COUNTOF(flags));
  for (size_t i = 0; i < COUNTOF(flags); ++i) {
    lua_pushinteger(state, (lua_Integer)flags[i].value);
    lua_setfield(state, -2, flags[i].name);
  }
}

static void
lua_overdub_unreal_function_flags_push(lua_State *state)
{
  static const struct {
    const char   *name;
    efunc_flags_t value;
  } flags[] = {
    {"FUNC_None",              FUNC_FLAG_NONE              },
    {"FUNC_Final",             FUNC_FLAG_FINAL             },
    {"FUNC_Public",            FUNC_FLAG_PUBLIC            },
    {"FUNC_Private",           FUNC_FLAG_PRIVATE           },
    {"FUNC_Protected",         FUNC_FLAG_PROTECTED         },
    {"FUNC_BlueprintCallable", FUNC_FLAG_BLUEPRINT_CALLABLE},
    {"FUNC_BlueprintPure",     FUNC_FLAG_BLUEPRINT_PURE    },
    {"FUNC_Const",             FUNC_FLAG_CONST             },
  };

  lua_createtable(state, 0, (int)COUNTOF(flags));
  for (size_t i = 0; i < COUNTOF(flags); ++i) {
    lua_pushinteger(state, (lua_Integer)flags[i].value);
    lua_setfield(state, -2, flags[i].name);
  }
}

static int
lua_overdub_unreal_object_get_name_lua(lua_State *state)
{
  uobject_t  *object = lua_overdub_unreal_object_check(state, 1);
  tmp_arena_t tmp    = scratch_begin(NULL);
  str_t       name   = unreal_uobject_push_name(object, tmp.arena);
  lua_pushlstring(state, (const char *)name.data, (size_t)name.len);
  scratch_end(tmp);
  return 1;
}

static int
lua_overdub_unreal_object_get_internal_index_lua(lua_State *state)
{
  lua_pushinteger(state, lua_overdub_unreal_object_check(state, 1)->internal_idx);
  return 1;
}

static int
lua_overdub_unreal_object_get_path_name_lua(lua_State *state)
{
  uobject_t  *object = lua_overdub_unreal_object_check(state, 1);
  tmp_arena_t tmp    = scratch_begin(NULL);
  str_t       path   = unreal_uobject_push_ue_path_name(object, tmp.arena);
  lua_pushlstring(state, (const char *)path.data, (size_t)path.len);
  scratch_end(tmp);
  return 1;
}

static int
lua_overdub_unreal_object_get_flags_lua(lua_State *state)
{
  lua_pushinteger(state, (lua_Integer)lua_overdub_unreal_object_check(state, 1)->obj_flags);
  return 1;
}

static int
lua_overdub_unreal_object_get_internal_flags_lua(lua_State *state)
{
  lua_pushinteger(state, (lua_Integer)unreal_uobject_get_internal_flags(lua_overdub_unreal_object_check(state, 1)));
  return 1;
}

static int
lua_overdub_unreal_object_is_rooted_lua(lua_State *state)
{
  lua_pushboolean(state, unreal_uobject_is_rooted(lua_overdub_unreal_object_check(state, 1)));
  return 1;
}

static int
lua_overdub_unreal_object_add_to_root_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  lua_pushboolean(state, lua_overdub_unreal_claim_root(context, lua_overdub_unreal_object_check(state, 1)));
  return 1;
}

static int
lua_overdub_unreal_object_remove_from_root_lua(lua_State *state)
{
  lua_overdub_unreal_context_t *context = lua_overdub_unreal_context_check(state);
  lua_pushboolean(state, lua_overdub_unreal_release_root(context, lua_overdub_unreal_object_check(state, 1)));
  return 1;
}

static void
lua_overdub_unreal_pending_params_destroy(ufunc_t *function, void *params, uint32_t initialized_count)
{
  if (params && function) {
    uint32_t destroyed = 0;
    for (ffield_t *field = function->child_props; field && destroyed < initialized_count; field = field->next) {
      fprop_t *prop = (fprop_t *)field;
      if (prop->prop_flags & CPF_PARM) {
        unreal_fprop_destroy_in_container(prop, params);
        destroyed += 1;
      }
    }
  }
}

static bool
lua_overdub_unreal_pending_params_snapshot(lua_overdub_unreal_context_t *context, ufunc_t *function, void *params, void **out_params, uint32_t *out_initialized_count)
{
  if (!context || !function || !out_params || !out_initialized_count) {
    return false;
  }

  *out_params            = NULL;
  *out_initialized_count = 0;
  if (function->params_size == 0) {
    return true;
  }

  if (!params) {
    return false;
  }

  uint64_t alignment = MAX_VAL((uint64_t)function->min_alignment, 1ULL);
  void    *copy      = arena_push_zero_aligned(&context->param_arena, function->params_size, alignment);
  if (!copy) {
    return false;
  }

  bool copied = true;
  for (ffield_t *field = function->child_props; field && copied; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (prop->prop_flags & CPF_PARM) {
      uint64_t size = unreal_fprop_complete_size(prop);
      bool fits = prop->offset_internal >= 0 && size > 0 && (uint64_t)prop->offset_internal <= function->params_size &&
                  size <= (uint64_t)function->params_size - (uint64_t)prop->offset_internal;

      void *dst = NULL;
      void *src = NULL;
      if (fits) {
        dst = unreal_fprop_value_in_container(prop, copy, 0);
        src = unreal_fprop_value_in_container(prop, params, 0);
      }

      copied = dst && src && unreal_fprop_initialize_value(prop, dst);
      if (copied) {
        *out_initialized_count += 1;
        copied = unreal_fprop_copy_complete_value(prop, dst, src);
      }
    }
  }

  if (!copied) {
    lua_overdub_unreal_pending_params_destroy(function, copy, *out_initialized_count);
    *out_initialized_count = 0;
    copy = NULL;
  }

  *out_params = copy;
  return copied;
}

static void
lua_overdub_unreal_param_arena_reset_if_empty(lua_overdub_unreal_context_t *context)
{
  if (context && context->pending_func_count == 0 && context->pending_hook_count == 0) {
    arena_reset(&context->param_arena);
  }
}

static void
lua_overdub_unreal_pending_hook_destroy(lua_overdub_pending_hook_t *pending)
{
  if (!pending) {
    return;
  }

  lua_overdub_unreal_pending_params_destroy(pending->function_raw, pending->params, pending->initialized_count);
  *pending = (lua_overdub_pending_hook_t){0};
}

static bool
lua_overdub_unreal_pending_hook_snapshot(lua_overdub_unreal_context_t *context, lua_overdub_pending_hook_t *pending, uobject_t *object,
                                         ufunc_t *function, void *params, bool after, bool consumed)
{
  if (!context || !pending || !object || !function) {
    return false;
  }

  bool object_saved   = unreal_fweak_object_from_object(object, &pending->object);
  bool function_saved = unreal_fweak_object_from_object((uobject_t *)function, &pending->function);
  if (!object_saved || !function_saved) {
    return false;
  }

  pending->function_raw = function;
  pending->after        = after;
  pending->consumed     = consumed;
  return lua_overdub_unreal_pending_params_snapshot(context, function, params, &pending->params, &pending->initialized_count);
}

static void
lua_overdub_unreal_pending_func_destroy(lua_overdub_pending_func_t *pending)
{
  if (!pending) {
    return;
  }

  lua_overdub_unreal_pending_params_destroy(pending->function_raw, pending->params, pending->initialized_count);
  *pending = (lua_overdub_pending_func_t){0};
}

static bool
lua_overdub_unreal_queue_reflected_func(lua_overdub_reflected_func_t *func, unreal_func_call_t *call)
{
  if (!func || !call) {
    return false;
  }

  lua_overdub_unreal_context_t *context = func->context;
  if (!context || !call->object || !call->function) {
    return false;
  }

  if (context->pending_func_count >= CONFIG_LUA_OVERDUB_MAX_PENDING_FUNC_CALLS) {
    context->pending_func_overflow = true;
    return false;
  }

  lua_overdub_pending_func_t pending = {
    .function_raw = call->function,
    .impl         = func,
  };
  bool saved = unreal_fweak_object_from_object(call->object, &pending.object) && unreal_fweak_object_from_object((uobject_t *)call->function, &pending.function);
  if (saved) {
    saved = lua_overdub_unreal_pending_params_snapshot(context, call->function, call->params, &pending.params, &pending.initialized_count);
  }

  if (!saved) {
    lua_overdub_unreal_pending_func_destroy(&pending);
    lua_overdub_unreal_param_arena_reset_if_empty(context);
    LOG_ERROR("Overdub Lua mod '%.*s' could not snapshot a nested Lua-backed UFunction call", STR_ARG(context->mod_id));
    return false;
  }

  context->pending_funcs[context->pending_func_count++] = pending;
  return true;
}

static bool
lua_overdub_unreal_hook_matches(lua_overdub_hook_t *hook, ufunc_t *function, bool after)
{
  int callback_ref = after ? hook->after_ref : hook->before_ref;
  return hook->active && hook->kind == LUA_OVERDUB_HOOK_FUNCTION && callback_ref != LUA_NOREF && callback_ref != LUA_REFNIL &&
         unreal_fweak_object_resolve(hook->target) == (uobject_t *)function;
}

static bool
lua_overdub_unreal_has_matching_hook(lua_overdub_unreal_context_t *context, ufunc_t *function, bool after)
{
  if (!context || !function) {
    return false;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_HOOKS; ++i) {
    if (lua_overdub_unreal_hook_matches(&context->hooks[i], function, after)) {
      return true;
    }
  }
  return false;
}

static bool
lua_overdub_unreal_queue_hook(lua_overdub_unreal_context_t *context, uobject_t *object,
                              ufunc_t *function, void *params, bool after, bool consumed)
{
  if (!lua_overdub_unreal_has_matching_hook(context, function, after)) {
    return false;
  }

  if (context->pending_hook_count >= CONFIG_LUA_OVERDUB_MAX_PENDING_HOOKS) {
    context->pending_hook_overflow = true;
    return false;
  }

  lua_overdub_pending_hook_t pending = {0};
  if (!lua_overdub_unreal_pending_hook_snapshot(context, &pending, object, function, params, after, consumed)) {
    lua_overdub_unreal_param_arena_reset_if_empty(context);
    LOG_ERROR("Overdub Lua mod '%.*s' could not snapshot nested UFunction hook parameters", STR_ARG(context->mod_id));
    return false;
  }

  context->pending_hooks[context->pending_hook_count++] = pending;
  return true;
}

static bool
lua_overdub_unreal_hook_call(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                             bool *in_callback, lua_overdub_hook_t *hook, uobject_t *object,
                             ufunc_t *function, void *params, bool after, bool consumed, bool deferred)
{
  int *callback_ref_slot = after ? &hook->after_ref : &hook->before_ref;
  int  callback_ref      = *callback_ref_slot;
  if (!runtime || !runtime->state || !in_callback ||
      !lua_overdub_unreal_hook_matches(hook, function, after)) {
    return false;
  }

  fweak_object_ptr_t object_weak   = {.object_idx = -1};
  fweak_object_ptr_t function_weak = {.object_idx = -1};
  if (!unreal_fweak_object_from_object(object, &object_weak) || !unreal_fweak_object_from_object((uobject_t *)function, &function_weak)) {
    return false;
  }

  lua_State          *state      = runtime->state;
  int                 stack_base = lua_gettop(state);
  lua_overdub_call_t *call       = lua_newuserdatauv(state, sizeof(*call), 0);
  *call = (lua_overdub_call_t){
    .context      = context,
    .object       = object_weak,
    .function     = function_weak,
    .function_raw = function,
    .params       = params,
    .valid        = true,
    .before       = !after,
    .deferred     = deferred,
    .consumed     = consumed,
  };
  luaL_setmetatable(state, LUA_OVERDUB_CALL_META);

  *in_callback = true;
  bool called = lua_runtime_call_ref(runtime, callback_ref, 1, 0);
  *in_callback = false;

  bool skip_requested = call->skip_requested;
  call->valid = false;
  lua_settop(state, stack_base);

  if (!called && hook->active && *callback_ref_slot == callback_ref) {
    luaL_unref(state, LUA_REGISTRYINDEX, callback_ref);

    *callback_ref_slot = LUA_NOREF;
    if (hook->before_ref == LUA_NOREF && hook->after_ref == LUA_NOREF) {
      lua_overdub_unreal_hook_deactivate(NULL, hook);
    }

    LOG_ERROR("Overdub Lua mod '%.*s': disabling failed %s hook callback", STR_ARG(context->mod_id), after ? "After" : "Before");
  }
  return called && skip_requested;
}

static bool
lua_overdub_unreal_dispatch_hook(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                                 bool *in_callback, uobject_t *object, ufunc_t *function,
                                 void *params, bool after, bool consumed, bool deferred)
{
  bool skip_requested = false;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_HOOKS; ++i) {
    lua_overdub_hook_t *hook = &context->hooks[i];
    if (lua_overdub_unreal_hook_matches(hook, function, after) &&
        lua_overdub_unreal_hook_call(context, runtime, in_callback, hook, object, function, params, after, consumed, deferred)) {
      skip_requested = true;
    }
  }
  return skip_requested;
}

bool
lua_overdub_unreal_process_event_pre(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                                     bool *in_callback, uobject_t *object, ufunc_t *function, void *params)
{
  if (!context || !runtime || !in_callback || !object || !function ||
      !lua_overdub_unreal_access_check(context)) {
    return false;
  }

  if (*in_callback) {
    lua_overdub_unreal_queue_hook(context, object, function, params, false, false);
    return false;
  }
  return lua_overdub_unreal_dispatch_hook(context, runtime, in_callback, object, function,
                                          params, false, false, false);
}

void
lua_overdub_unreal_process_event_post(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime,
                                      bool *in_callback, uobject_t *object, ufunc_t *function,
                                      void *params, bool consumed)
{
  if (!context || !runtime || !in_callback || !object || !function ||
      !lua_overdub_unreal_access_check(context)) {
    return;
  }

  if (*in_callback) {
    lua_overdub_unreal_queue_hook(context, object, function, params, true, consumed);
    return;
  }
  lua_overdub_unreal_dispatch_hook(context, runtime, in_callback, object, function, params, true, consumed, false);
}

static bool
lua_overdub_unreal_post_load_matches(lua_overdub_hook_t *hook, uobject_t *object, bool after)
{
  if (!hook || !hook->active || hook->kind != LUA_OVERDUB_HOOK_POST_LOAD || !object) {
    return false;
  }

  int callback_ref = after ? hook->after_ref : hook->before_ref;
  if (callback_ref == LUA_NOREF || callback_ref == LUA_REFNIL) {
    return false;
  }

  uclass_t *target = (uclass_t *)unreal_fweak_object_resolve(hook->target);
  return target && unreal_uobject_is_a(object, target);
}

static void
lua_overdub_unreal_post_load_call(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime, bool *in_callback, lua_overdub_hook_t *hook, uobject_t *object, bool after)
{
  if (!runtime || !runtime->state || !in_callback || *in_callback) {
    return;
  }

  if (!lua_overdub_unreal_post_load_matches(hook, object, after)) {
    return;
  }

  int *ref_slot = after ? &hook->after_ref : &hook->before_ref;
  int  ref      = *ref_slot;
  lua_unreal_object_push(runtime->state, object);

  *in_callback = true;
  bool called = lua_runtime_call_ref(runtime, ref, 1, 0);
  *in_callback = false;

  if (!called && hook->active && *ref_slot == ref) {
    luaL_unref(runtime->state, LUA_REGISTRYINDEX, ref);
    *ref_slot = LUA_NOREF;

    if (hook->before_ref == LUA_NOREF && hook->after_ref == LUA_NOREF) {
      lua_overdub_unreal_hook_deactivate(NULL, hook);
    }

    const char *phase = after ? "After" : "Before";
    LOG_ERROR("Overdub Lua mod '%.*s': disabling failed %s PostLoad callback", STR_ARG(context->mod_id), phase);
  }
}

void
lua_overdub_unreal_post_load(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime, bool *in_callback, uobject_t *object, bool after)
{
  if (!context || !runtime || !in_callback || !object || *in_callback || !lua_overdub_unreal_access_check(context)) {
    return;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_HOOKS; ++i) {
    lua_overdub_hook_t *hook = &context->hooks[i];
    if (lua_overdub_unreal_post_load_matches(hook, object, after)) {
      lua_overdub_unreal_post_load_call(context, runtime, in_callback, hook, object, after);
    }
  }
}

bool
lua_overdub_unreal_has_post_load_hooks(void)
{
  return g_lua_overdub_post_load_hook_count > 0;
}

int
lua_overdub_unreal_post_load_hook_count(void)
{
  return g_lua_overdub_post_load_hook_count;
}

static void
lua_overdub_unreal_queue_constructed(lua_overdub_unreal_context_t *context, uobject_t *object)
{
  if (!context || !object) {
    return;
  }

  EnterCriticalSection(&context->notification_lock);
  bool notify = false;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS && !notify; ++i) {
    if (lua_overdub_unreal_notification_matches(&context->notifications[i], object)) {
      notify = true;
    }
  }

  bool has_space = context->pending_object_count < CONFIG_LUA_OVERDUB_MAX_PENDING_OBJECTS;
  if (notify && !has_space) {
    context->pending_object_overflow = true;
  }

  fweak_object_ptr_t weak = lua_overdub_unreal_invalid_weak();
  if (notify && has_space && unreal_fweak_object_from_object(object, &weak)) {
    int tail = (context->pending_object_head + context->pending_object_count) % CONFIG_LUA_OVERDUB_MAX_PENDING_OBJECTS;
    context->pending_objects[tail] = (lua_overdub_pending_object_t){.object = weak};
    context->pending_object_count += 1;
  }

  LeaveCriticalSection(&context->notification_lock);
}

void
lua_overdub_unreal_notify_constructed(uobject_t *object)
{
  if (!object || InterlockedCompareExchange(&g_lua_overdub_notification_context_count, 0, 0) <= 0) {
    return;
  }

  AcquireSRWLockShared(&g_lua_overdub_notification_lock);
  for (lua_overdub_unreal_context_t *context = g_lua_overdub_notification_first; context; context = context->notification_next) {
    lua_overdub_unreal_queue_constructed(context, object);
  }
  ReleaseSRWLockShared(&g_lua_overdub_notification_lock);
}

static void
lua_overdub_unreal_queue_deleted(lua_overdub_unreal_context_t *context, uobject_t *object, int32_t idx)
{
  if (!context || !object || idx < 0) {
    return;
  }

  EnterCriticalSection(&context->notification_lock);
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS; ++i) {
    lua_overdub_notification_t *notification = &context->notifications[i];
    if (lua_overdub_unreal_delete_notification_matches(notification, object, idx)) {
      notification->delete_queued = true;
      context->pending_delete_count += 1;
    }
  }
  LeaveCriticalSection(&context->notification_lock);
}

void
lua_overdub_unreal_notify_deleted(uobject_t *object, int32_t idx)
{
  LONG context_count = InterlockedCompareExchange(&g_lua_overdub_notification_context_count, 0, 0);
  if (!object || idx < 0 || context_count <= 0) {
    return;
  }

  AcquireSRWLockShared(&g_lua_overdub_notification_lock);
  for (lua_overdub_unreal_context_t *context = g_lua_overdub_notification_first; context; context = context->notification_next) {
    lua_overdub_unreal_queue_deleted(context, object, idx);
  }
  ReleaseSRWLockShared(&g_lua_overdub_notification_lock);
}

typedef enum {
  LUA_OVERDUB_NOTIFICATION_NONE,
  LUA_OVERDUB_NOTIFICATION_OBJECT_DONE,
  LUA_OVERDUB_NOTIFICATION_CALLBACK,
} lua_overdub_notification_dispatch_t;

static lua_overdub_notification_dispatch_t
lua_overdub_unreal_next_notification(lua_overdub_unreal_context_t *context, uobject_t **out_object, uint64_t *out_id, int *out_callback_ref)
{
  lua_overdub_notification_dispatch_t result = LUA_OVERDUB_NOTIFICATION_NONE;
  EnterCriticalSection(&context->notification_lock);
  if (context->pending_object_count > 0) {
    lua_overdub_pending_object_t *pending = &context->pending_objects[context->pending_object_head];
    uobject_t                    *object  = unreal_fweak_object_resolve(pending->object);
    int                           index   = pending->next_notification;
    while (object && index < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS && result != LUA_OVERDUB_NOTIFICATION_CALLBACK) {
      int i = index++;
      pending->next_notification = i + 1;
      lua_overdub_notification_t *notification = &context->notifications[i];
      if (lua_overdub_unreal_notification_matches(notification, object)) {
        *out_object       = object;
        *out_id           = notification->id;
        *out_callback_ref = notification->callback_ref;
        result            = LUA_OVERDUB_NOTIFICATION_CALLBACK;
      }
    }

    if (result != LUA_OVERDUB_NOTIFICATION_CALLBACK) {
      *pending = (lua_overdub_pending_object_t){0};
      context->pending_object_head = (context->pending_object_head + 1) % CONFIG_LUA_OVERDUB_MAX_PENDING_OBJECTS;
      context->pending_object_count -= 1;
      result = LUA_OVERDUB_NOTIFICATION_OBJECT_DONE;
    }
  }

  LeaveCriticalSection(&context->notification_lock);
  return result;
}

static bool
lua_overdub_unreal_take_delete(lua_overdub_unreal_context_t *context, int *out_callback_ref)
{
  bool found  = false;
  bool detach = false;

  EnterCriticalSection(&context->notification_lock);
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS && !found; ++i) {
    lua_overdub_notification_t *notification = &context->notifications[i];
    bool queued = notification->active && notification->kind == LUA_OVERDUB_NOTIFICATION_DELETED && notification->delete_queued;
    if (queued) {
      *out_callback_ref              = notification->callback_ref;
      notification->callback_ref     = LUA_NOREF;
      notification->active           = false;
      notification->delete_queued    = false;
      context->pending_delete_count -= 1;
      context->notification_count   -= 1;
      detach                         = context->notification_count == 0;
      found                          = true;
    }
  }
  LeaveCriticalSection(&context->notification_lock);

  if (detach) {
    lua_overdub_unreal_notifications_detach(context);
  }

  return found;
}

static void
lua_overdub_unreal_disable_notification(lua_overdub_unreal_context_t *context, uint64_t id, int callback_ref)
{
  bool detach = false;
  bool found  = false;
  EnterCriticalSection(&context->notification_lock);
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS && !found; ++i) {
    lua_overdub_notification_t *notification = &context->notifications[i];
    if (notification->active && notification->id == id && notification->callback_ref == callback_ref) {
      notification->active       = false;
      notification->callback_ref = LUA_NOREF;
      context->notification_count -= 1;
      detach = context->notification_count == 0;
      found  = true;
    }
  }
  LeaveCriticalSection(&context->notification_lock);

  if (detach) {
    lua_overdub_unreal_notifications_detach(context);
  }
}

static uint64_t
lua_overdub_unreal_dispatch_notifications(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime, bool *in_callback, uint64_t budget_us)
{
  uint64_t start_us = time_now_us();

  EnterCriticalSection(&context->notification_lock);
  int  objects_left = context->pending_object_count;
  bool overflow     = context->pending_object_overflow;
  context->pending_object_overflow = false;
  LeaveCriticalSection(&context->notification_lock);

  if (overflow) {
    LOG_WARN("Overdub Lua mod '%.*s' new-object notification queue overflowed; some callbacks were dropped", STR_ARG(context->mod_id));
  }

  while (objects_left > 0 && time_now_us() - start_us < budget_us) {
    uobject_t *object       = NULL;
    uint64_t   id           = 0;
    int        callback_ref = LUA_NOREF;
    lua_overdub_notification_dispatch_t next = lua_overdub_unreal_next_notification(context, &object, &id, &callback_ref);
    if (next == LUA_OVERDUB_NOTIFICATION_NONE) {
      break;
    }

    if (next == LUA_OVERDUB_NOTIFICATION_OBJECT_DONE) {
      objects_left -= 1;
      continue;
    }

    int stack_base = lua_gettop(runtime->state);
    lua_unreal_object_push(runtime->state, object);
    *in_callback = true;
    bool called = lua_runtime_call_ref(runtime, callback_ref, 1, 0);
    *in_callback = false;
    lua_settop(runtime->state, stack_base);

    if (!called) {
      lua_overdub_unreal_disable_notification(context, id, callback_ref);
      luaL_unref(runtime->state, LUA_REGISTRYINDEX, callback_ref);
      LOG_ERROR("Overdub Lua mod '%.*s': disabling failed new-object notification", STR_ARG(context->mod_id));
    }
  }
  return time_now_us() - start_us;
}

static uint64_t
lua_overdub_unreal_dispatch_deletes(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime, bool *in_callback, uint64_t budget_us)
{
  uint64_t start_us = time_now_us();

  EnterCriticalSection(&context->notification_lock);
  int deletes_left = context->pending_delete_count;
  LeaveCriticalSection(&context->notification_lock);

  while (deletes_left > 0 && time_now_us() - start_us < budget_us) {
    int  callback_ref = LUA_NOREF;
    bool found        = lua_overdub_unreal_take_delete(context, &callback_ref);
    if (found) {
      *in_callback = true;
      bool called = lua_runtime_call_ref(runtime, callback_ref, 0, 0);
      *in_callback = false;
      luaL_unref(runtime->state, LUA_REGISTRYINDEX, callback_ref);
      deletes_left -= 1;

      if (!called) {
        LOG_ERROR("Overdub Lua mod '%.*s': delete notification callback failed", STR_ARG(context->mod_id));
      }
    } else {
      deletes_left = 0;
    }
  }

  return time_now_us() - start_us;
}

static uint64_t
lua_overdub_unreal_dispatch_reflected_funcs(lua_overdub_unreal_context_t *context, uint64_t budget_us)
{
  uint64_t start_us       = time_now_us();
  int      dispatch_count = context->pending_func_count;
  for (int i = 0; i < dispatch_count && context->pending_func_count > 0; ++i) {
    if (time_now_us() - start_us >= budget_us) {
      break;
    }

    lua_overdub_pending_func_t pending = context->pending_funcs[0];
    context->pending_func_count -= 1;
    if (context->pending_func_count > 0) {
      mem_move(context->pending_funcs, context->pending_funcs + 1, sizeof(*context->pending_funcs) * (uint64_t)context->pending_func_count);
    }
    context->pending_funcs[context->pending_func_count] = (lua_overdub_pending_func_t){0};

    uobject_t *object   = unreal_fweak_object_resolve(pending.object);
    ufunc_t   *function = (ufunc_t *)unreal_fweak_object_resolve(pending.function);

    unreal_func_call_t call = {
      .object   = object,
      .function = function,
      .params   = pending.params,
      .user     = pending.impl,
    };

    if (function == pending.function_raw && lua_overdub_unreal_reflected_func_ready(pending.impl, &call)) {
      lua_overdub_unreal_reflected_func_invoke(pending.impl, &call);
    }

    lua_overdub_unreal_pending_func_destroy(&pending);
  }
  return time_now_us() - start_us;
}

uint64_t
lua_overdub_unreal_dispatch_pending(lua_overdub_unreal_context_t *context, lua_runtime_t *runtime, bool *in_callback, uint64_t budget_us)
{
  if (!context || !runtime || !in_callback || *in_callback || budget_us == 0 || !lua_overdub_unreal_access_check(context)) {
    return 0;
  }

  if (context->pending_func_overflow) {
    LOG_WARN("Overdub Lua mod '%.*s' nested Lua-backed UFunction queue overflowed; some calls were dropped", STR_ARG(context->mod_id));
    context->pending_func_overflow = false;
  }

  if (context->pending_hook_overflow) {
    LOG_WARN("Overdub Lua mod '%.*s' nested UFunction hook queue overflowed; some callbacks were dropped", STR_ARG(context->mod_id));
    context->pending_hook_overflow = false;
  }

  uint64_t start_us = time_now_us();
  lua_overdub_unreal_dispatch_reflected_funcs(context, budget_us);

  uint64_t elapsed_us = time_now_us() - start_us;
  if (elapsed_us < budget_us) {
    lua_overdub_unreal_dispatch_notifications(context, runtime, in_callback, budget_us - elapsed_us);
  }

  elapsed_us = time_now_us() - start_us;
  if (elapsed_us < budget_us) {
    lua_overdub_unreal_dispatch_deletes(context, runtime, in_callback, budget_us - elapsed_us);
  }

  int dispatch_count = context->pending_hook_count;
  for (int i = 0; i < dispatch_count && context->pending_hook_count > 0; ++i) {
    if (time_now_us() - start_us >= budget_us) {
      break;
    }

    lua_overdub_pending_hook_t pending = context->pending_hooks[0];
    context->pending_hook_count -= 1;
    if (context->pending_hook_count > 0) {
      mem_move(context->pending_hooks, context->pending_hooks + 1, sizeof(*context->pending_hooks) * (uint64_t)context->pending_hook_count);
    }
    context->pending_hooks[context->pending_hook_count] = (lua_overdub_pending_hook_t){0};

    uobject_t *object   = unreal_fweak_object_resolve(pending.object);
    ufunc_t   *function = (ufunc_t *)unreal_fweak_object_resolve(pending.function);
    if (object && function == pending.function_raw) {
      lua_overdub_unreal_dispatch_hook(context, runtime, in_callback, object, function, pending.params, pending.after, pending.consumed, true);
    }
    lua_overdub_unreal_pending_hook_destroy(&pending);
  }

  lua_overdub_unreal_param_arena_reset_if_empty(context);
  return time_now_us() - start_us;
}

int
lua_overdub_unreal_pending_count(lua_overdub_unreal_context_t *context)
{
  if (!context) {
    return 0;
  }

  int count = context->pending_func_count + context->pending_hook_count;
  EnterCriticalSection(&context->notification_lock);
  count += context->pending_object_count + context->pending_delete_count;
  LeaveCriticalSection(&context->notification_lock);
  return count;
}

static int
luaopen_overdub_unreal(lua_State *state)
{
  lua_overdub_unreal_context_check(state);
  static const luaL_Reg functions[] = {
    {"FindObject",           lua_overdub_unreal_find_object_lua             },
    {"GetObjectCount",       lua_overdub_unreal_get_object_count_lua        },
    {"GetObjectByIndex",     lua_overdub_unreal_get_object_by_index_lua     },
    {"Objects",              lua_overdub_unreal_objects_lua                 },
    {"LoadObject",           lua_overdub_unreal_load_object_lua             },
    {"ConstructObject",      lua_overdub_unreal_construct_object_lua        },
    {"SpawnActor",           lua_overdub_unreal_spawn_actor_lua             },
    {"DefineClass",          lua_overdub_unreal_define_class_lua            },
    {"BindDelegate",         lua_overdub_unreal_bind_delegate_lua           },
    {"NotifyOnNewObject",    lua_overdub_unreal_notify_on_new_object_lua    },
    {"NotifyOnDeleteObject", lua_overdub_unreal_notify_on_delete_object_lua },
    {"Hook",                 lua_overdub_unreal_hook_lua                    },
    {"HookPostLoad",         lua_overdub_unreal_hook_post_load_lua          },
    {"GetCurrentWorld",      lua_overdub_unreal_get_current_world_lua       },
    {"GetTransientPackage",  lua_overdub_unreal_get_transient_package_lua   },
    {"FName",                lua_unreal_fname_construct                     },
    {"FText",                lua_unreal_ftext_construct                     },
    {NULL,                   NULL                                           },
  };

  luaL_newlib(state, functions);
  lua_unreal_object_flags_push(state);
  lua_setfield(state, -2, "EObjectFlags");
  lua_unreal_internal_object_flags_push(state);
  lua_setfield(state, -2, "EInternalObjectFlags");
  lua_overdub_unreal_class_flags_push(state);
  lua_setfield(state, -2, "EClassFlags");
  lua_overdub_unreal_property_flags_push(state);
  lua_setfield(state, -2, "EPropertyFlags");
  lua_overdub_unreal_function_flags_push(state);
  lua_setfield(state, -2, "EFunctionFlags");
  lua_unreal_property_types_push(state);
  lua_setfield(state, -2, "PropertyTypes");
  lua_unreal_find_name_modes_push(state);
  lua_setfield(state, -2, "EFindName");
  lua_unreal_name_none_push(state);
  lua_setfield(state, -2, "NAME_None");
  return 1;
}

lua_overdub_unreal_context_t *
lua_overdub_unreal_context_create(arena_t *arena, lua_runtime_t *runtime, bool *in_callback, str_t mod_id, uint32_t owner_thread_id)
{
  if (!arena || !arena->backing || !runtime || !in_callback || str_is_empty(mod_id) || owner_thread_id == 0) {
    return NULL;
  }

  lua_overdub_unreal_context_t *context = ARENA_PUSH_ZERO(arena, lua_overdub_unreal_context_t);
  if (!context) {
    return NULL;
  }

  context->param_arena = arena_new_dynamic(CONFIG_LUA_OVERDUB_PARAM_ARENA_RESERVE_SIZE, CONFIG_LUA_OVERDUB_PARAM_ARENA_COMMIT_SIZE);
  if (!context->param_arena.backing) {
    return NULL;
  }

  context->arena                = arena;
  context->runtime              = runtime;
  context->in_callback          = in_callback;
  context->mod_id               = mod_id;
  context->owner_thread_id      = owner_thread_id;
  context->next_hook_id         = 1;
  context->next_delegate_id     = 1;
  context->next_notification_id = 1;
  InitializeCriticalSection(&context->notification_lock);

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_HOOKS; ++i) {
    context->hooks[i].before_ref = LUA_NOREF;
    context->hooks[i].after_ref  = LUA_NOREF;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_NOTIFICATIONS; ++i) {
    context->notifications[i].target       = lua_overdub_unreal_invalid_weak();
    context->notifications[i].callback_ref = LUA_NOREF;
  }

  lua_unreal_context_init(&context->codec);
  context->codec.user          = context;
  context->codec.access_check  = lua_overdub_unreal_access_check;
  context->codec.find_object   = lua_overdub_unreal_find_object;
  context->codec.process_event = lua_overdub_unreal_process_event;
  return context;
}

void
lua_overdub_unreal_context_deactivate(lua_overdub_unreal_context_t *context)
{
  if (context) {
    for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_DELEGATE_BINDINGS; ++i) {
      lua_overdub_unreal_delegate_deactivate(&context->delegate_bindings[i]);
    }

    if (context->reflected_active) {
      unreal_reflect_disable_owner((unreal_reflect_owner_t)(uintptr_t)context);
      context->reflected_active = false;
    }
  }
}

void
lua_overdub_unreal_context_destroy(lua_overdub_unreal_context_t *context)
{
  if (!context) {
    return;
  }

  lua_overdub_unreal_context_deactivate(context);
  lua_overdub_unreal_notifications_detach(context);

  while (context->root_claims) {
    lua_overdub_root_claim_t *claim = context->root_claims;
    context->root_claims = claim->next;
    lua_overdub_unreal_global_root_release(claim->object);
    lua_overdub_unreal_mod_claim_free(context, claim);
  }

  for (int i = 0; i < context->pending_hook_count; ++i) {
    lua_overdub_unreal_pending_hook_destroy(&context->pending_hooks[i]);
  }

  for (int i = 0; i < context->pending_func_count; ++i) {
    lua_overdub_unreal_pending_func_destroy(&context->pending_funcs[i]);
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_HOOKS; ++i) {
    lua_overdub_unreal_hook_deactivate(NULL, &context->hooks[i]);
  }

  DeleteCriticalSection(&context->notification_lock);
  arena_destroy(&context->param_arena);
}

bool
lua_overdub_unreal_register(lua_State *state, lua_overdub_unreal_context_t *context)
{
  if (!state || !context || !lua_unreal_register_codec(state, &context->codec)) {
    return false;
  }

  lua_pushlightuserdata(state, &g_lua_overdub_unreal_context_key);
  lua_pushlightuserdata(state, context);
  lua_rawset(state, LUA_REGISTRYINDEX);

  if (!lua_unreal_object_metatable_push(state)) {
    return false;
  }

  static const luaL_Reg methods[] = {
    {"GetName",          lua_overdub_unreal_object_get_name_lua          },
    {"GetPathName",      lua_overdub_unreal_object_get_path_name_lua     },
    {"GetInternalIndex", lua_overdub_unreal_object_get_internal_index_lua},
    {"GetFlags",         lua_overdub_unreal_object_get_flags_lua         },
    {"GetInternalFlags", lua_overdub_unreal_object_get_internal_flags_lua},
    {"IsRooted",         lua_overdub_unreal_object_is_rooted_lua         },
    {"AddToRoot",        lua_overdub_unreal_object_add_to_root_lua       },
    {"RemoveFromRoot",   lua_overdub_unreal_object_remove_from_root_lua  },
    {NULL,                NULL                                            },
  };

  luaL_setfuncs(state, methods, 0);
  lua_getfield(state, -1, "__index");
  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 2);
    return false;
  }

  lua_pushcclosure(state, lua_overdub_unreal_object_index_lua, 1);
  lua_setfield(state, -2, "__index");
  lua_pop(state, 1);
  lua_unreal_use_camel_case_names(state);

  if (luaL_newmetatable(state, LUA_OVERDUB_HOOK_HANDLE_META)) {
    static const luaL_Reg methods[] = {
      {"Remove",   lua_overdub_unreal_hook_handle_remove_lua   },
      {"IsActive", lua_overdub_unreal_hook_handle_is_active_lua},
      {NULL,       NULL                                        },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushliteral(state, "Overdub Unreal Hook");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  if (luaL_newmetatable(state, LUA_OVERDUB_DELEGATE_HANDLE_META)) {
    static const luaL_Reg methods[] = {
      {"Remove",   lua_overdub_unreal_delegate_handle_remove_lua   },
      {"IsActive", lua_overdub_unreal_delegate_handle_is_active_lua},
      {NULL,       NULL                                            },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushliteral(state, "Overdub Unreal delegate binding");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  if (luaL_newmetatable(state, LUA_OVERDUB_NOTIFY_HANDLE_META)) {
    static const luaL_Reg methods[] = {
      {"Remove",   lua_overdub_unreal_notification_remove_lua   },
      {"IsActive", lua_overdub_unreal_notification_is_active_lua},
      {NULL,       NULL                                         },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, lua_overdub_unreal_notification_handle_gc_lua);
    lua_setfield(state, -2, "__gc");
    lua_pushliteral(state, "Overdub Unreal notification");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  if (luaL_newmetatable(state, LUA_OVERDUB_CALL_META)) {
    static const luaL_Reg methods[] = {
      {"IsValid",       lua_overdub_unreal_call_is_valid_lua      },
      {"GetObject",     lua_overdub_unreal_call_get_object_lua    },
      {"GetFunction",   lua_overdub_unreal_call_get_function_lua  },
      {"GetPhase",      lua_overdub_unreal_call_get_phase_lua     },
      {"IsDeferred",    lua_overdub_unreal_call_is_deferred_lua   },
      {"IsSkipped",     lua_overdub_unreal_call_is_skipped_lua    },
      {"Get",           lua_overdub_unreal_call_get_lua           },
      {"Set",           lua_overdub_unreal_call_set_lua           },
      {"GetReturnValue",lua_overdub_unreal_call_get_return_lua    },
      {"SetReturnValue",lua_overdub_unreal_call_set_return_lua    },
      {"Skip",          lua_overdub_unreal_call_skip_lua          },
      {NULL,            NULL                                      },
    };
    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushliteral(state, "Overdub Unreal Call");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  return lua_overdub_preload_module(state, "overdub.unreal", luaopen_overdub_unreal);
}
