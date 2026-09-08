#ifndef LUA_UNREAL_PROP_H
#define LUA_UNREAL_PROP_H

#include "unreal_prop.h"

typedef struct lua_State lua_State;

MOD_EXTERN_C_BEGIN

typedef struct lua_unreal_context_s lua_unreal_context_t;

typedef bool       (*lua_unreal_access_check_fn_t) (void *user);
typedef uobject_t *(*lua_unreal_find_object_fn_t)  (void *user, str_t name);
typedef int        (*lua_unreal_object_count_fn_t) (void *user);
typedef uobject_t *(*lua_unreal_object_at_fn_t)    (void *user, int idx);
typedef bool       (*lua_unreal_process_event_fn_t)(void *user, uobject_t *receiver, ufunc_t *function, void *params);

/* The context and every callback/user pointer must outlive the registered Lua state.
 * Lua states using Unreal values must be closed on the game thread. */
struct lua_unreal_context_s {
  void                         *user;
  lua_unreal_access_check_fn_t  access_check;
  lua_unreal_find_object_fn_t   find_object;
  lua_unreal_object_count_fn_t  object_count;
  lua_unreal_object_at_fn_t     object_at;
  lua_unreal_process_event_fn_t process_event;
};

MOD_API void
lua_unreal_context_init(lua_unreal_context_t *context);

/* Installs the UE4SS-compatible object-discovery globals, FName/FText helpers,
 * flag constants, and Unreal table. This smaller entry point is useful to
 * verify/embed discovery without registering reflected UObject property behavior. */
MOD_API bool
lua_unreal_register_static(lua_State *state, lua_unreal_context_t *context);

/* Registers metatables, the Unreal table, and UE4SS-compatible discovery/FName
 * globals (StaticFindObject, FindObject[s], FindFirst/AllOf, and FName) in an arbitrary state.
 * UObject reads resolve reflected properties, reflected functions, then metatable methods.
 * Unsigned values above LUA_MAXINTEGER are intentionally not marshalled lossily. */
MOD_API bool
lua_unreal_register(lua_State *state, lua_unreal_context_t *context);

/* Registers only the internal userdata/metatable layer used by the property
 * codec. It does not install Unreal, UE4SS globals, or object-discovery
 * functions. A model-specific runtime may layer its own module on top. */
MOD_API bool
lua_unreal_register_codec(lua_State *state, lua_unreal_context_t *context);

/* Pushes the shared UObject metatable so a model-specific layer can add its
 * own methods. Returns false and leaves the stack unchanged if unavailable. */
MOD_API bool
lua_unreal_object_metatable_push(lua_State *state);

/* Replaces the codec's lowercase compatibility aliases (type/get/set) with
 * CamelCase names in the current state. The UE4SS registration does not call
 * this function. */
MOD_API void
lua_unreal_use_camel_case_names(lua_State *state);

/* Model-neutral constructors/constants used by module-specific APIs. */
MOD_API int
lua_unreal_fname_construct(lua_State *state);
MOD_API int
lua_unreal_ftext_construct(lua_State *state);
MOD_API int
lua_unreal_name_none_push(lua_State *state);
MOD_API void
lua_unreal_object_flags_push(lua_State *state);
MOD_API void
lua_unreal_internal_object_flags_push(lua_State *state);
MOD_API void
lua_unreal_property_types_push(lua_State *state);
MOD_API void
lua_unreal_find_name_modes_push(lua_State *state);

/* Pushes nil for a null object and one validated UObject userdata otherwise */
MOD_API int
lua_unreal_object_push(lua_State *state, uobject_t *object);

/* Pushes a UObject userdata whose weak pointer is intentionally invalid. */
MOD_API int
lua_unreal_object_push_invalid(lua_State *state);

/* Returns NULL for a non-UObject value or a stale userdata */
MOD_API uobject_t *
lua_unreal_object_get(lua_State *state, int idx);

/* Pushes one scalar/object value or an owned FString/FText/soft-reference/
 * struct/fixed-array/container value. Returns zero for unsupported reflected
 * kinds such as delegates. */
MOD_API int
lua_unreal_prop_push(lua_State *state, fprop_t *prop, void *value, uobject_t *owner);

/* Transactionally converts and copies a scalar or complete reflected value. */
MOD_API bool
lua_unreal_prop_write(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner);

/* A scope keeps borrowed ProcessEvent parameter storage valid only while its
 * callback is executing. Retained RemoteUnrealParam values fail cleanly after
 * lua_unreal_param_scope_end instead of referring to a reused params block. */
MOD_API int
lua_unreal_param_scope_begin(lua_State *state);
MOD_API void
lua_unreal_param_scope_end(lua_State *state, int scope_ref);
MOD_API int
lua_unreal_remote_param_push(lua_State *state, int scope_ref, fprop_t *prop, void *value, uobject_t *owner, bool writable);
MOD_API int
lua_unreal_remote_object_param_push(lua_State *state, int scope_ref, uobject_t *object);

MOD_EXTERN_C_END

#endif /* LUA_UNREAL_PROP_H */
