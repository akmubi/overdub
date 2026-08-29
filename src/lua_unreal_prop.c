#include "lua_unreal_prop.h"

#include "globals.h"
#include "scratch.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <limits.h>
#include <stdio.h>

#define LUA_UNREAL_OBJECT_MT         "overdub.Unreal.Object"
#define LUA_UNREAL_PROXY_MT          "overdub.Unreal.StructProxy"
#define LUA_UNREAL_OWNED_MT          "overdub.Unreal.OwnedValue"
#define LUA_UNREAL_FNAME_MT          "overdub.Unreal.FName"
#define LUA_UNREAL_PARAM_MT          "overdub.Unreal.Param"
#define LUA_UNREAL_FTEXT_MT          "overdub.Unreal.FTextLiteral"
#define LUA_UNREAL_DATA_TABLE_ROW_MT "overdub.Unreal.DataTableRow"
#define LUA_UNREAL_PROPERTY_MT       "overdub.Unreal.Property"
#define LUA_UNREAL_FIELD_CLASS_MT    "overdub.Unreal.FieldClass"
#define LUA_UNREAL_REFLECTION_MT     "overdub.Unreal.Reflection"

#define LUA_UNREAL_MAX_ALIGNMENT (4096)
#define LUA_UNREAL_ERROR_CAP     (256)

typedef struct lua_unreal_object_s lua_unreal_object_t;
struct lua_unreal_object_s {
  lua_unreal_context_t *context;
  fweak_object_ptr_t    weak;
};

typedef struct lua_unreal_data_table_row_s lua_unreal_data_table_row_t;
struct lua_unreal_data_table_row_s {
  lua_unreal_context_t *context;
  fweak_object_ptr_t    table;
  fname_t               row_name;
};

typedef struct lua_unreal_proxy_s lua_unreal_proxy_t;
typedef int lua_unreal_selector_kind_t;
enum {
  LUA_UNREAL_SELECTOR_FIELD,
  LUA_UNREAL_SELECTOR_FIXED_ARRAY_ELEMENT,
  LUA_UNREAL_SELECTOR_ARRAY_ELEMENT,
  LUA_UNREAL_SELECTOR_MAP_KEY,
  LUA_UNREAL_SELECTOR_MAP_VALUE,
};

struct lua_unreal_proxy_s {
  lua_unreal_context_t      *context;
  fprop_t                   *prop;
  lua_unreal_selector_kind_t selector_kind;
  int32_t                    selector_index;
};

typedef struct lua_unreal_owned_s lua_unreal_owned_t;
struct lua_unreal_owned_s {
  lua_unreal_context_t *context;
  fprop_t              *prop;
  fweak_object_ptr_t    owner;
  uint32_t              value_offset;
  bool                  initialized;
};

typedef struct lua_unreal_param_s       lua_unreal_param_t;
typedef struct lua_unreal_param_scope_s lua_unreal_param_scope_t;

typedef enum {
  LUA_UNREAL_PARAM_PROXY,
  LUA_UNREAL_PARAM_BORROWED,
  LUA_UNREAL_PARAM_OBJECT,
} lua_unreal_param_kind_t;

struct lua_unreal_param_scope_s {
  bool active;
};

struct lua_unreal_param_s {
  lua_unreal_context_t     *context;
  lua_unreal_param_scope_t *scope;
  lua_unreal_param_kind_t   kind;
  fprop_t                  *prop;
  void                     *value;
  fweak_object_ptr_t        owner;
  fweak_object_ptr_t        object;
  bool                      writable;
  bool                      is_local;
};

typedef struct lua_unreal_ftext_literal_s lua_unreal_ftext_literal_t;
struct lua_unreal_ftext_literal_s {
  lua_unreal_context_t *context;
  size_t                len;
  char                  text[1];
};

typedef struct lua_unreal_property_s lua_unreal_property_t;
struct lua_unreal_property_s {
  lua_unreal_context_t *context;
  fprop_t              *prop;
};

typedef struct lua_unreal_field_class_s lua_unreal_field_class_t;
struct lua_unreal_field_class_s {
  lua_unreal_context_t *context;
  ffield_class_t       *cls;
};

typedef struct lua_unreal_reflection_s lua_unreal_reflection_t;
struct lua_unreal_reflection_s {
  lua_unreal_context_t *context;
  fweak_object_ptr_t    object;
};

typedef struct lua_unreal_method_s lua_unreal_method_t;
struct lua_unreal_method_s {
  lua_unreal_context_t *context;
  fweak_object_ptr_t    receiver;
  fweak_object_ptr_t    function;
};

typedef struct lua_unreal_fname_s lua_unreal_fname_t;
struct lua_unreal_fname_s {
  fname_t value;
};

static const char g_lua_unreal_context_registry_key = 0;

static bool lua_unreal_write_transactional     (lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap);
static int  lua_unreal_prop_push_internal      (lua_State *state, fprop_t *prop, void *value, uobject_t *owner, int parent_idx, bool live_proxy);
static int  lua_unreal_prop_push_selected      (lua_State *state, fprop_t *prop, void *value, uobject_t *owner, int parent_idx, bool live_proxy, lua_unreal_selector_kind_t selector_kind, int32_t selector_index, bool single_element);
static int  lua_unreal_object_index            (lua_State *state);
static int  lua_unreal_object_newindex         (lua_State *state);
static int  lua_unreal_object_call_function_lua(lua_State *state);
static int  lua_unreal_ufunction_call_lua      (lua_State *state);
static bool lua_unreal_value_is_fixed_element  (lua_State *state, int idx);
static bool lua_unreal_data_table_row_resolve  (lua_unreal_data_table_row_t *row, udata_table_t **out_table, uscript_struct_t **out_row_struct, uint8_t **out_value);

static void
lua_unreal_error_set(char *error, size_t cap, const char *fmt, ...)
{
  if (!error || cap == 0 || error[0] != '\0') {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(error, cap, fmt, args);
  va_end(args);
  error[cap - 1] = '\0';
}

static uint32_t
lua_unreal_check_u32(lua_State *state, int idx, const char *description)
{
  lua_Integer value = luaL_checkinteger(state, idx);
  if (value < 0 || (lua_Unsigned)value > UINT32_MAX) {
    luaL_argerror(state, idx, description);
  }
  return (uint32_t)value;
}

void
lua_unreal_context_init(lua_unreal_context_t *context)
{
  if (context) {
    *context = (lua_unreal_context_t){0};
  }
}

static lua_unreal_context_t *
lua_unreal_context_from_state(lua_State *state)
{
  lua_rawgetp(state, LUA_REGISTRYINDEX, &g_lua_unreal_context_registry_key);
  lua_unreal_context_t *context = (lua_unreal_context_t *)lua_touserdata(state, -1);
  lua_pop(state, 1);
  return context;
}

static bool
lua_unreal_access_allowed(lua_unreal_context_t *context)
{
  if (!context) {
    return false;
  }

  if (context->access_check) {
    return context->access_check(context->user);
  }

  return globals.engine_inited && unreal_is_in_game_thread();
}

static int
lua_unreal_require_access(lua_State *state, lua_unreal_context_t *context)
{
  if (!lua_unreal_access_allowed(context)) {
    return luaL_error(state, "Unreal access is only available on the initialized game thread");
  }
  return 0;
}

static lua_unreal_object_t *
lua_unreal_object_test(lua_State *state, int idx)
{
  return (lua_unreal_object_t *)luaL_testudata(state, idx, LUA_UNREAL_OBJECT_MT);
}

static uobject_t *
lua_unreal_object_resolve(lua_unreal_object_t *userdata)
{
  return userdata ? unreal_fweak_object_resolve(userdata->weak) : NULL;
}

uobject_t *
lua_unreal_object_get(lua_State *state, int idx)
{
  return state ? lua_unreal_object_resolve(lua_unreal_object_test(state, idx)) : NULL;
}

int
lua_unreal_object_push(lua_State *state, uobject_t *object)
{
  if (!state || !object) {
    if (state) {
      lua_pushnil(state);
      return 1;
    }
    return 0;
  }

  fweak_object_ptr_t weak = {0};
  if (!unreal_fweak_object_from_object(object, &weak)) {
    lua_pushnil(state);
    return 1;
  }

  lua_unreal_object_t *userdata = (lua_unreal_object_t *)lua_newuserdatauv(state, sizeof(*userdata), 0);
  userdata->context = lua_unreal_context_from_state(state);
  userdata->weak    = weak;
  luaL_setmetatable(state, LUA_UNREAL_OBJECT_MT);
  return 1;
}

int
lua_unreal_object_push_invalid(lua_State *state)
{
  if (!state) {
    return 0;
  }

  lua_unreal_object_t *userdata = (lua_unreal_object_t *)lua_newuserdatauv(state, sizeof(*userdata), 0);
  *userdata = (lua_unreal_object_t){
    .context = lua_unreal_context_from_state(state),
    .weak    = {
      .object_idx = -1,
    },
  };
  luaL_setmetatable(state, LUA_UNREAL_OBJECT_MT);
  return 1;
}

static int
lua_unreal_find_result_push(lua_State *state, uobject_t *object)
{
  return object ? lua_unreal_object_push(state, object) : lua_unreal_object_push_invalid(state);
}

static int
lua_unreal_create_invalid_object_lua(lua_State *state)
{
  return lua_unreal_object_push_invalid(state);
}

static lua_unreal_fname_t *
lua_unreal_fname_test(lua_State *state, int idx)
{
  return (lua_unreal_fname_t *)luaL_testudata(state, idx, LUA_UNREAL_FNAME_MT);
}

static lua_unreal_ftext_literal_t *
lua_unreal_ftext_literal_test(lua_State *state, int idx)
{
  return (lua_unreal_ftext_literal_t *)luaL_testudata(state, idx, LUA_UNREAL_FTEXT_MT);
}

static int
lua_unreal_fname_push(lua_State *state, fname_t value)
{
  lua_unreal_fname_t *userdata = (lua_unreal_fname_t *)lua_newuserdatauv(state, sizeof(*userdata), 0);
  userdata->value = value;
  luaL_setmetatable(state, LUA_UNREAL_FNAME_MT);
  return 1;
}

static bool
lua_unreal_string_or_fname(lua_State *state, int idx, str_t *out)
{
  idx  = lua_absindex(state, idx);
  *out = STR_NULL;

  if (lua_type(state, idx) == LUA_TSTRING) {
    size_t      len  = 0;
    const char *text = lua_tolstring(state, idx, &len);
    *out             = str_make((uint8_t *)text, (uint64_t)len);
    return true;
  }

  lua_unreal_fname_t *name = lua_unreal_fname_test(state, idx);
  if (!name) {
    return false;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t text = unreal_fname_to_str(name->value, tmp.arena);
    lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
    lua_replace(state, idx);
  }
  scratch_end(tmp);

  size_t      len  = 0;
  const char *text = lua_tolstring(state, idx, &len);
  *out             = str_make((uint8_t *)text, (uint64_t)len);
  return true;
}

static bool
lua_unreal_property_belongs_to_struct(ustruct_t *type, fprop_t *prop)
{
  if (!type || !prop) {
    return false;
  }

  for (fprop_t *current = type->prop_link; current; current = current->prop_link_next) {
    if (current == prop) {
      return true;
    }
  }
  return false;
}

static bool
lua_unreal_property_fits(fprop_t *prop, uint64_t container_size)
{
  if (!prop || prop->offset_internal < 0 || prop->elem_size <= 0 || prop->array_dim <= 0) {
    return false;
  }

  uint64_t complete = unreal_fprop_complete_size(prop);
  return complete > 0 && (uint64_t)prop->offset_internal <= container_size && complete <= container_size - (uint64_t)prop->offset_internal;
}

static void *
lua_unreal_owned_value(lua_unreal_owned_t *owned)
{
  return owned ? (uint8_t *)owned + owned->value_offset : NULL;
}

static bool
lua_unreal_data_table_row_resolve(lua_unreal_data_table_row_t *row, udata_table_t **out_table, uscript_struct_t **out_row_struct, uint8_t **out_value)
{
  udata_table_t *table = row ? (udata_table_t *)unreal_fweak_object_resolve(row->table) : NULL;
  if (!table || !globals.unreal.data_table || !unreal_uobject_is_a((uobject_t *)table, globals.unreal.data_table)) {
    return false;
  }

  uscript_struct_t *row_struct = table->row_struct;
  if (!row_struct || !globals.unreal.core_scriptstruct || !unreal_uobject_is_valid((uobject_t *)row_struct) ||
      !unreal_uobject_is_a((uobject_t *)row_struct, globals.unreal.core_scriptstruct) || row_struct->props_size < 0) {
    return false;
  }

  uint8_t *value = unreal_udata_table_find_row(table, row->row_name);
  if (!value) {
    return false;
  }

  if (out_table) {
    *out_table = table;
  }

  if (out_row_struct) {
    *out_row_struct = row_struct;
  }

  if (out_value) {
    *out_value = value;
  }

  return true;
}

static bool
lua_unreal_value_resolve(lua_State *state, int idx, fprop_t **out_prop, void **out_value, uobject_t **out_owner)
{
  idx = lua_absindex(state, idx);

  lua_unreal_owned_t *owned = (lua_unreal_owned_t *)luaL_testudata(state, idx, LUA_UNREAL_OWNED_MT);
  if (owned) {
    if (!owned->initialized || !owned->prop) {
      return false;
    }

    if (out_prop) {
      *out_prop = owned->prop;
    }

    if (out_value) {
      *out_value = lua_unreal_owned_value(owned);
    }

    if (out_owner) {
      *out_owner = unreal_fweak_object_resolve(owned->owner);
    }
    return true;
  }

  lua_unreal_proxy_t *proxy = (lua_unreal_proxy_t *)luaL_testudata(state, idx, LUA_UNREAL_PROXY_MT);
  if (!proxy || !proxy->prop) {
    return false;
  }

  if (lua_getiuservalue(state, idx, 1) == LUA_TNONE) {
    lua_pop(state, 1);
    return false;
  }

  int parent_idx = lua_gettop(state);

  void      *parent_value = NULL;
  uobject_t *owner        = NULL;
  fprop_t   *parent_prop  = NULL;
  ustruct_t *parent_type  = NULL;

  lua_unreal_object_t *parent_object = lua_unreal_object_test(state, parent_idx);
  if (parent_object) {
    owner = lua_unreal_object_resolve(parent_object);
    if (owner) {
      parent_value = owner;
      parent_type  = (ustruct_t *)owner->cls;
    }
  } else {
    lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)luaL_testudata(state, parent_idx, LUA_UNREAL_DATA_TABLE_ROW_MT);
    if (row) {
      udata_table_t    *table      = NULL;
      uscript_struct_t *row_struct = NULL;
      if (lua_unreal_data_table_row_resolve(row, &table, &row_struct, (uint8_t **)&parent_value)) {
        owner       = (uobject_t *)table;
        parent_type = (ustruct_t *)row_struct;
      }
    } else if (lua_unreal_value_resolve(state, parent_idx, &parent_prop, &parent_value, &owner) &&
               unreal_fprop_get_kind(parent_prop) == UNREAL_PROP_KIND_STRUCT) {
      fprop_struct_t *struct_prop = (fprop_struct_t *)parent_prop;
      parent_type = (ustruct_t *)struct_prop->script_struct;
    }
  }

  void *value = NULL;
  switch (proxy->selector_kind) {
    case LUA_UNREAL_SELECTOR_FIELD: {
      bool valid = parent_value && parent_type &&
                   lua_unreal_property_belongs_to_struct(parent_type, proxy->prop) &&
                   lua_unreal_property_fits(proxy->prop, (uint64_t)parent_type->props_size);
      value = valid ? unreal_fprop_value_in_container(proxy->prop, parent_value, 0) : NULL;
    } break;

    case LUA_UNREAL_SELECTOR_FIXED_ARRAY_ELEMENT: {
      if (parent_value && parent_prop == proxy->prop) {
        value = unreal_fprop_value_at(proxy->prop, parent_value, proxy->selector_index);
      }
    } break;

    case LUA_UNREAL_SELECTOR_ARRAY_ELEMENT: {
      if (parent_value && parent_prop && unreal_fprop_get_kind(parent_prop) == UNREAL_PROP_KIND_ARRAY) {
        fprop_array_t *array_prop = (fprop_array_t *)parent_prop;
        if (array_prop->inner == proxy->prop) {
          value = unreal_array_get(parent_value, array_prop, proxy->selector_index);
        }
      }
    } break;

    case LUA_UNREAL_SELECTOR_MAP_KEY:
    case LUA_UNREAL_SELECTOR_MAP_VALUE: {
      if (parent_value && parent_prop && unreal_fprop_get_kind(parent_prop) == UNREAL_PROP_KIND_MAP) {
        fprop_map_t *map_prop = (fprop_map_t *)parent_prop;
        if (lua_getiuservalue(state, idx, 2) != LUA_TNONE) {
          fprop_t *key_prop = NULL;
          void    *key      = NULL;
          if (lua_unreal_value_resolve(state, -1, &key_prop, &key, NULL) && key_prop == map_prop->key_prop) {
            if (proxy->selector_kind == LUA_UNREAL_SELECTOR_MAP_VALUE && map_prop->val_prop == proxy->prop) {
              value = unreal_map_find_value_ptr(parent_value, map_prop, key);
            } else if (proxy->selector_kind == LUA_UNREAL_SELECTOR_MAP_KEY && map_prop->key_prop == proxy->prop) {
              int32_t sparse_index = unreal_map_find_index(parent_value, map_prop, key);
              value = sparse_index >= 0 ? (void *)unreal_map_get_key(parent_value, map_prop, sparse_index) : NULL;
            }
          }
        }
        lua_pop(state, 1);
      }
    } break;

    default: break;
  }
  lua_pop(state, 1);

  if (!value) {
    return false;
  }

  if (out_prop) {
    *out_prop = proxy->prop;
  }

  if (out_value) {
    *out_value = value;
  }

  if (out_owner) {
    *out_owner = owner;
  }
  return true;
}

static int
lua_unreal_proxy_push(lua_State *state, fprop_t *prop, int parent_idx, lua_unreal_selector_kind_t selector_kind, int32_t selector_index)
{
  parent_idx = lua_absindex(state, parent_idx);

  lua_unreal_proxy_t *proxy = (lua_unreal_proxy_t *)lua_newuserdatauv(state, sizeof(*proxy), 2);
  proxy->context        = lua_unreal_context_from_state(state);
  proxy->prop           = prop;
  proxy->selector_kind  = selector_kind;
  proxy->selector_index = selector_index;

  luaL_setmetatable(state, LUA_UNREAL_PROXY_MT);
  lua_pushvalue(state, parent_idx);
  lua_setiuservalue(state, -2, 1);
  return 1;
}

static int
lua_unreal_owned_push(lua_State *state, fprop_t *prop, const void *value, uobject_t *owner)
{
  uint64_t value_size = unreal_fprop_complete_size(prop);
  if (!prop || !value || value_size == 0) {
    return 0;
  }

  int32_t alignment = unreal_fprop_min_alignment(prop);
  if (alignment <= 0) {
    alignment = 1;
  }

  if (alignment > LUA_UNREAL_MAX_ALIGNMENT) {
    return 0;
  }

  uint64_t extra = (uint64_t)alignment - 1;
  uint64_t total = sizeof(lua_unreal_owned_t) + extra + value_size;
  if (total > SIZE_MAX) {
    return 0;
  }

  lua_unreal_owned_t *owned = (lua_unreal_owned_t *)lua_newuserdatauv(state, (size_t)total, 0);

  *owned = (lua_unreal_owned_t){
    .context = lua_unreal_context_from_state(state),
    .prop    = prop,
    .owner   = {
      .object_idx = -1,
    },
  };
  unreal_fweak_object_from_object(owner, &owned->owner);

  uintptr_t start   = (uintptr_t)(owned + 1);
  uintptr_t rem     = start % (uintptr_t)alignment;
  uintptr_t aligned = rem ? start + ((uintptr_t)alignment - rem) : start;

  owned->value_offset = (uint32_t)(aligned - (uintptr_t)owned);

  void *dst = lua_unreal_owned_value(owned);
  if (!unreal_fprop_initialize_value(prop, dst)) {
    lua_pop(state, 1);
    return 0;
  }

  owned->initialized = true;
  luaL_setmetatable(state, LUA_UNREAL_OWNED_MT);

  bool copied = prop->array_dim == 1
                ? unreal_fprop_copy_single_value(prop, dst, value)
                : unreal_fprop_copy_complete_value(prop, dst, value);
  if (!copied) {
    unreal_fprop_destroy_value(prop, dst);
    owned->initialized = false;
    lua_pop(state, 1);
    return 0;
  }
  return 1;
}

static bool
lua_unreal_value_is_local(lua_State *state, int idx, int depth)
{
  idx = lua_absindex(state, idx);
  if (luaL_testudata(state, idx, LUA_UNREAL_OWNED_MT)) {
    return true;
  }

  if (depth >= 64 || !luaL_testudata(state, idx, LUA_UNREAL_PROXY_MT)) {
    return false;
  }

  bool result = false;
  if (lua_getiuservalue(state, idx, 1) != LUA_TNONE) {
    result = lua_unreal_value_is_local(state, -1, depth + 1);
  }
  lua_pop(state, 1);
  return result;
}

static int
lua_unreal_owned_gc(lua_State *state)
{
  lua_unreal_owned_t *owned = (lua_unreal_owned_t *)luaL_testudata(state, 1, LUA_UNREAL_OWNED_MT);
  if (owned && owned->initialized && owned->prop) {
    unreal_fprop_destroy_value(owned->prop, lua_unreal_owned_value(owned));
    owned->initialized = false;
  }
  return 0;
}

static bool
lua_unreal_kind_is_integer(unreal_prop_kind_t kind)
{
  return kind == UNREAL_PROP_KIND_BYTE   || kind == UNREAL_PROP_KIND_INT8   ||
         kind == UNREAL_PROP_KIND_INT16  || kind == UNREAL_PROP_KIND_INT32  ||
         kind == UNREAL_PROP_KIND_INT64  || kind == UNREAL_PROP_KIND_UINT16 ||
         kind == UNREAL_PROP_KIND_UINT32 || kind == UNREAL_PROP_KIND_UINT64 ||
         kind == UNREAL_PROP_KIND_ENUM;
}

static bool
lua_unreal_kind_is_object(unreal_prop_kind_t kind)
{
  return kind == UNREAL_PROP_KIND_OBJECT      || kind == UNREAL_PROP_KIND_CLASS      ||
         kind == UNREAL_PROP_KIND_WEAK_OBJECT || kind == UNREAL_PROP_KIND_LAZY_OBJECT;
}

static bool
lua_unreal_kind_is_soft_object(unreal_prop_kind_t kind)
{
  return kind == UNREAL_PROP_KIND_SOFT_OBJECT || kind == UNREAL_PROP_KIND_SOFT_CLASS;
}

static int
lua_unreal_prop_push_selected(lua_State *state, fprop_t *prop, void *value, uobject_t *owner, int parent_idx, bool live_proxy,
                             lua_unreal_selector_kind_t selector_kind, int32_t selector_index, bool single_element)
{
  if (!state || !prop || !value || prop->array_dim <= 0) {
    return 0;
  }

  if (!single_element && prop->array_dim > 1) {
    return live_proxy && parent_idx != 0
           ? lua_unreal_proxy_push(state, prop, parent_idx, selector_kind, selector_index)
           : lua_unreal_owned_push(state, prop, value, owner);
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_BOOL) {
    bool result = false;
    if (!unreal_fprop_read_bool(prop, value, &result)) {
      return 0;
    }
    lua_pushboolean(state, result);
    return 1;
  }

  if (lua_unreal_kind_is_integer(kind)) {
    unreal_prop_integer_t integer = {0};
    if (!unreal_fprop_read_integer(prop, value, &integer)) {
      return 0;
    }

    if (!integer.is_signed && integer.value > (uint64_t)LUA_MAXINTEGER) {
      return 0;
    }

    lua_pushinteger(state, integer.is_signed ? (lua_Integer)(int64_t)integer.value : (lua_Integer)integer.value);
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_FLOAT || kind == UNREAL_PROP_KIND_DOUBLE) {
    double result = 0.0;
    if (!unreal_fprop_read_real(prop, value, &result)) {
      return 0;
    }
    lua_pushnumber(state, (lua_Number)result);
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_NAME) {
    fname_t name = {0};
    if (!unreal_fprop_read_name(prop, value, &name)) {
      return 0;
    }
    return lua_unreal_fname_push(state, name);
  }

  if (kind == UNREAL_PROP_KIND_TEXT) {
    /* FText has no mutating Lua methods. Return an owned value so mods can cache and later restore a property after its source UObject is gone */
    return lua_unreal_owned_push(state, prop, value, owner);
  }

  if (kind == UNREAL_PROP_KIND_STRING) {
    return live_proxy && parent_idx != 0
           ? lua_unreal_proxy_push(state, prop, parent_idx, selector_kind, selector_index)
           : lua_unreal_owned_push(state, prop, value, owner);
  }

  if (lua_unreal_kind_is_soft_object(kind)) {
    return live_proxy && parent_idx != 0
           ? lua_unreal_proxy_push(state, prop, parent_idx, selector_kind, selector_index)
           : lua_unreal_owned_push(state, prop, value, owner);
  }

  if (lua_unreal_kind_is_object(kind) || kind == UNREAL_PROP_KIND_INTERFACE) {
    return lua_unreal_object_push(state, unreal_fprop_get_referenced_object(prop, value));
  }

  if (kind == UNREAL_PROP_KIND_STRUCT || kind == UNREAL_PROP_KIND_ARRAY || kind == UNREAL_PROP_KIND_SET || kind == UNREAL_PROP_KIND_MAP) {
    return live_proxy && parent_idx != 0
           ? lua_unreal_proxy_push(state, prop, parent_idx, selector_kind, selector_index)
           : lua_unreal_owned_push(state, prop, value, owner);
  }

  return 0;
}

static int
lua_unreal_map_value_push(lua_State *state, fprop_map_t *map_prop, void *value, const void *key,
                          uobject_t *owner, int parent_idx)
{
  if (!map_prop || !map_prop->key_prop || !map_prop->val_prop || !value || !key) {
    return 0;
  }

  int result = lua_unreal_prop_push_selected(state, map_prop->val_prop, value, owner, parent_idx, true, LUA_UNREAL_SELECTOR_MAP_VALUE, 0, true);
  lua_unreal_proxy_t *proxy = result ? (lua_unreal_proxy_t *)luaL_testudata(state, -1, LUA_UNREAL_PROXY_MT) : NULL;
  if (proxy) {
    if (!lua_unreal_owned_push(state, map_prop->key_prop, key, owner)) {
      lua_pop(state, 1);
      return 0;
    }
    lua_setiuservalue(state, -2, 2);
  }
  return result;
}

static int
lua_unreal_proxy_param_push(lua_State *state, fprop_t *prop, int parent_idx,
                            lua_unreal_selector_kind_t selector_kind, int32_t selector_index,
                            fprop_t *key_prop, const void *key, uobject_t *owner, bool writable)
{
  parent_idx = lua_absindex(state, parent_idx);
  if (!lua_unreal_proxy_push(state, prop, parent_idx, selector_kind, selector_index)) {
    return 0;
  }

  int proxy_idx = lua_gettop(state);
  if (key_prop && key) {
    if (!lua_unreal_owned_push(state, key_prop, key, owner)) {
      lua_pop(state, 1);
      return 0;
    }
    lua_setiuservalue(state, proxy_idx, 2);
  }

  lua_unreal_param_t *param = (lua_unreal_param_t *)lua_newuserdatauv(state, sizeof(*param), 1);

  *param = (lua_unreal_param_t){
    .context  = lua_unreal_context_from_state(state),
    .kind     = LUA_UNREAL_PARAM_PROXY,
    .owner    = {.object_idx = -1},
    .object   = {.object_idx = -1},
    .writable = writable,
    .is_local = lua_unreal_value_is_local(state, parent_idx, 0),
  };

  luaL_setmetatable(state, LUA_UNREAL_PARAM_MT);
  lua_pushvalue(state, proxy_idx);
  lua_setiuservalue(state, -2, 1);
  lua_remove(state, proxy_idx);
  return 1;
}

int
lua_unreal_param_scope_begin(lua_State *state)
{
  if (!state) {
    return LUA_NOREF;
  }

  lua_unreal_param_scope_t *scope = (lua_unreal_param_scope_t *)lua_newuserdatauv(state, sizeof(*scope), 0);
  scope->active = true;
  return luaL_ref(state, LUA_REGISTRYINDEX);
}

void
lua_unreal_param_scope_end(lua_State *state, int scope_ref)
{
  if (!state || scope_ref == LUA_NOREF || scope_ref == LUA_REFNIL) {
    return;
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, scope_ref);
  lua_unreal_param_scope_t *scope = (lua_unreal_param_scope_t *)lua_touserdata(state, -1);
  if (scope) {
    scope->active = false;
  }
  lua_pop(state, 1);
  luaL_unref(state, LUA_REGISTRYINDEX, scope_ref);
}

static lua_unreal_param_t *
lua_unreal_remote_param_new(lua_State *state, int scope_ref, lua_unreal_param_kind_t kind)
{
  lua_rawgeti(state, LUA_REGISTRYINDEX, scope_ref);
  lua_unreal_param_scope_t *scope = (lua_unreal_param_scope_t *)lua_touserdata(state, -1);
  if (!scope || !scope->active) {
    lua_pop(state, 1);
    return NULL;
  }

  int scope_idx = lua_gettop(state);
  lua_unreal_param_t *param = (lua_unreal_param_t *)lua_newuserdatauv(state, sizeof(*param), 1);

  *param = (lua_unreal_param_t){
    .context = lua_unreal_context_from_state(state),
    .scope   = scope,
    .kind    = kind,
    .owner   = {.object_idx = -1},
    .object  = {.object_idx = -1},
  };

  luaL_setmetatable(state, LUA_UNREAL_PARAM_MT);
  lua_pushvalue(state, scope_idx);
  lua_setiuservalue(state, -2, 1);
  lua_remove(state, scope_idx);
  return param;
}

int
lua_unreal_remote_param_push(lua_State *state, int scope_ref, fprop_t *prop, void *value, uobject_t *owner, bool writable)
{
  if (!state || !prop || !value) {
    return 0;
  }

  lua_unreal_param_t *param = lua_unreal_remote_param_new(state, scope_ref, LUA_UNREAL_PARAM_BORROWED);
  if (!param) {
    return 0;
  }
  param->prop     = prop;
  param->value    = value;
  param->writable = writable;
  unreal_fweak_object_from_object(owner, &param->owner);
  return 1;
}

int
lua_unreal_remote_object_param_push(lua_State *state, int scope_ref, uobject_t *object)
{
  if (!state || !object) {
    return 0;
  }

  lua_unreal_param_t *param = lua_unreal_remote_param_new(state, scope_ref, LUA_UNREAL_PARAM_OBJECT);
  if (!param) {
    return 0;
  }

  if (!unreal_fweak_object_from_object(object, &param->object)) {
    lua_pop(state, 1);
    return 0;
  }
  return 1;
}

static int
lua_unreal_prop_push_internal(lua_State *state, fprop_t *prop, void *value, uobject_t *owner, int parent_idx, bool live_proxy)
{
  return lua_unreal_prop_push_selected(state, prop, value, owner, parent_idx, live_proxy, LUA_UNREAL_SELECTOR_FIELD, 0, false);
}

int
lua_unreal_prop_push(lua_State *state, fprop_t *prop, void *value, uobject_t *owner)
{
  return lua_unreal_prop_push_internal(state, prop, value, owner, 0, false);
}

static bool
lua_unreal_copy_compatible_value(lua_State *state, int value_idx, fprop_t *prop, void *dst, bool allow_same_kind, bool *recognized)
{
  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;

  *recognized = lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL);
  if (!*recognized) {
    return false;
  }

  bool compatible = unreal_fprop_same_type(prop, source_prop);
  if (!compatible && allow_same_kind) {
    unreal_prop_kind_t dst_kind = unreal_fprop_get_kind(prop);
    unreal_prop_kind_t src_kind = unreal_fprop_get_kind(source_prop);
    /* TSoftObjectPtr and TSoftClassPtr share FSoftObjectPtr storage.
     * UE4SS permits values returned by Conv_SoftObjPathToSoftObjRef to initialize either reflected property kind. */
    compatible = (dst_kind == src_kind || (lua_unreal_kind_is_soft_object(dst_kind) && lua_unreal_kind_is_soft_object(src_kind))) && prop->elem_size == source_prop->elem_size;
  }
  return compatible && unreal_fprop_copy_single_value(prop, dst, source_value);
}

static bool
lua_unreal_string_like(lua_State *state, int value_idx, arena_t *arena, str_t *out)
{
  value_idx = lua_absindex(state, value_idx);
  *out = STR_NULL;

  if (lua_type(state, value_idx) == LUA_TSTRING) {
    size_t      len  = 0;
    const char *text = lua_tolstring(state, value_idx, &len);
    *out = str_make((uint8_t *)text, (uint64_t)len);
    return text != NULL;
  }

  lua_unreal_ftext_literal_t *literal = lua_unreal_ftext_literal_test(state, value_idx);
  if (literal) {
    *out = str_make((uint8_t *)literal->text, (uint64_t)literal->len);
    return true;
  }

  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;
  return lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL) &&
         unreal_fprop_read_string(source_prop, source_value, arena, out);
}

static bool
lua_unreal_string_has_embedded_null(str_t text)
{
  for (uint64_t i = 0; i < text.len; ++i) {
    if (text.data[i] == 0) {
      return true;
    }
  }
  return false;
}

static bool
lua_unreal_write_struct(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap);
static bool
lua_unreal_write_fixed_array(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap);
static bool
lua_unreal_write_array(lua_State *state, int value_idx, fprop_array_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap);
static bool
lua_unreal_write_set(lua_State *state, int value_idx, fprop_set_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap);
static bool
lua_unreal_write_map(lua_State *state, int value_idx, fprop_map_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap);

static bool
lua_unreal_write_initialized_ex(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap, bool single_element)
{
  if (!state || !prop || !dst || prop->array_dim <= 0) {
    lua_unreal_error_set(error, error_cap, "invalid reflected property layout");
    return false;
  }

  if (!single_element && prop->array_dim > 1) {
    return lua_unreal_write_fixed_array(state, value_idx, prop, dst, owner, error, error_cap);
  }

  value_idx = lua_absindex(state, value_idx);
  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);

  if (kind == UNREAL_PROP_KIND_BOOL) {
    if (lua_type(state, value_idx) != LUA_TBOOLEAN || !unreal_fprop_write_bool(prop, dst, lua_toboolean(state, value_idx) != 0)) {
      lua_unreal_error_set(error, error_cap, "expected a boolean");
      return false;
    }
    return true;
  }

  if (lua_unreal_kind_is_integer(kind)) {
    int                   is_integer = 0;
    lua_Integer           value      = lua_tointegerx(state, value_idx, &is_integer);
    unreal_prop_integer_t integer    = {
      .value     = (uint64_t)(int64_t)value,
      .is_signed = value < 0,
    };

    if (!is_integer || !unreal_fprop_write_integer(prop, dst, integer)) {
      lua_unreal_error_set(error, error_cap, "expected an in-range integer");
      return false;
    }
    return true;
  }

  if (kind == UNREAL_PROP_KIND_FLOAT || kind == UNREAL_PROP_KIND_DOUBLE) {
    int        is_number = 0;
    lua_Number value     = lua_tonumberx(state, value_idx, &is_number);
    if (!is_number || !unreal_fprop_write_real(prop, dst, (double)value)) {
      lua_unreal_error_set(error, error_cap, "expected an in-range number");
      return false;
    }
    return true;
  }

  if (kind == UNREAL_PROP_KIND_NAME) {
    fname_t             name      = {0};
    lua_unreal_fname_t *name_data = lua_unreal_fname_test(state, value_idx);
    if (name_data) {
      name = name_data->value;
    } else {
      str_t text = STR_NULL;
      if (!lua_unreal_string_or_fname(state, value_idx, &text)) {
        lua_unreal_error_set(error, error_cap, "expected a string or FName");
        return false;
      }
      name = unreal_fname_from_str(text, FNAME_FIND_OR_ADD);
    }

    if (!unreal_fprop_write_name(prop, dst, name)) {
      lua_unreal_error_set(error, error_cap, "could not create the FName");
      return false;
    }
    return true;
  }

  if (kind == UNREAL_PROP_KIND_STRING || kind == UNREAL_PROP_KIND_TEXT) {
    bool recognized = false;
    if (lua_unreal_copy_compatible_value(state, value_idx, prop, dst, true, &recognized)) {
      return true;
    }

    bool        ok  = false;
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      str_t text = STR_NULL;
      if (!lua_unreal_string_like(state, value_idx, tmp.arena, &text)) {
        lua_unreal_error_set(error, error_cap, "expected a string, FString, or FText");
      } else if (lua_unreal_string_has_embedded_null(text)) {
        lua_unreal_error_set(error, error_cap, "Unreal strings cannot contain embedded null bytes");
      } else if (text.len == 0) {
        ok = unreal_fprop_clear_single_value(prop, dst);
        if (!ok) {
          lua_unreal_error_set(error, error_cap, "could not clear the string value");
        }
      } else {
        /* unreal_fprop_import_text uses PPF_None. FString and plain FText input therefore consume raw text; quoting here would store the quotes */
        ok = unreal_fprop_import_text(prop, dst, owner, text, tmp.arena);
        if (!ok) {
          lua_unreal_error_set(error, error_cap, "could not import the string value");
        }
      }
    }
    scratch_end(tmp);
    return ok;
  }

  if (lua_unreal_kind_is_soft_object(kind)) {
    bool recognized = false;
    if (lua_unreal_copy_compatible_value(state, value_idx, prop, dst, true, &recognized)) {
      return true;
    }

    if (recognized) {
      lua_unreal_error_set(error, error_cap, "expected a compatible soft object reference");
      return false;
    }

    if (lua_isnil(state, value_idx)) {
      return true;
    }

    lua_unreal_object_t *userdata = lua_unreal_object_test(state, value_idx);
    if (userdata) {
      uobject_t *object = lua_unreal_object_resolve(userdata);
      if (!object) {
        lua_unreal_error_set(error, error_cap, "expected a live UObject");
        return false;
      }

      if (!unreal_fprop_set_object((fprop_obj_base_t *)prop, dst, object)) {
        lua_unreal_error_set(error, error_cap, "the UObject is incompatible with this soft reference");
        return false;
      }
      return true;
    }

    if (lua_type(state, value_idx) != LUA_TSTRING) {
      lua_unreal_error_set(error, error_cap, "expected nil, a path string, a UObject, or a compatible soft reference");
      return false;
    }

    size_t      len  = 0;
    const char *text = lua_tolstring(state, value_idx, &len);
    str_t       path = str_make((uint8_t *)text, (uint64_t)len);
    if (!text || lua_unreal_string_has_embedded_null(path)) {
      lua_unreal_error_set(error, error_cap, "soft reference paths cannot contain embedded null bytes");
      return false;
    }

    bool        ok  = false;
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      ok = unreal_fprop_import_text(prop, dst, owner, path, tmp.arena);
    }
    scratch_end(tmp);

    if (!ok) {
      lua_unreal_error_set(error, error_cap, "could not import the soft reference path");
    }
    return ok;
  }

  if (lua_unreal_kind_is_object(kind)) {
    uobject_t *object = NULL;
    if (!lua_isnil(state, value_idx)) {
      lua_unreal_object_t *userdata = lua_unreal_object_test(state, value_idx);

      object = lua_unreal_object_resolve(userdata);
      if (!userdata || !object) {
        lua_unreal_error_set(error, error_cap, "expected nil or a live UObject");
        return false;
      }
    }

    if (!unreal_fprop_set_object((fprop_obj_base_t *)prop, dst, object)) {
      lua_unreal_error_set(error, error_cap, "the UObject is incompatible with this property");
      return false;
    }
    return true;
  }

  if (kind == UNREAL_PROP_KIND_INTERFACE) {
    uobject_t *object = NULL;
    if (!lua_isnil(state, value_idx)) {
      lua_unreal_object_t *userdata = lua_unreal_object_test(state, value_idx);
      object = lua_unreal_object_resolve(userdata);
      if (!userdata || !object) {
        lua_unreal_error_set(error, error_cap, "expected nil or a live UObject");
        return false;
      }
    }

    bool written = unreal_fprop_set_interface((fprop_iface_t *)prop, dst, object);
    if (!written) {
      lua_unreal_error_set(error, error_cap, "the UObject is incompatible with this interface property");
    }
    return written;
  }

  if (kind == UNREAL_PROP_KIND_STRUCT) {
    return lua_unreal_write_struct(state, value_idx, prop, dst, owner, error, error_cap);
  }

  if (kind == UNREAL_PROP_KIND_ARRAY) {
    return lua_unreal_write_array(state, value_idx, (fprop_array_t *)prop, dst, owner, error, error_cap);
  }

  if (kind == UNREAL_PROP_KIND_SET) {
    return lua_unreal_write_set(state, value_idx, (fprop_set_t *)prop, dst, owner, error, error_cap);
  }

  if (kind == UNREAL_PROP_KIND_MAP) {
    return lua_unreal_write_map(state, value_idx, (fprop_map_t *)prop, dst, owner, error, error_cap);
  }

  lua_unreal_error_set(error, error_cap, "unsupported reflected property kind");
  return false;
}

static bool
lua_unreal_write_initialized(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  return lua_unreal_write_initialized_ex(state, value_idx, prop, dst, owner, error, error_cap, false);
}

static bool
lua_unreal_temporary_from_lua(lua_State *state, int value_idx, fprop_t *prop, uobject_t *owner, arena_t *arena,
                              void **out_value, char *error, size_t error_cap)
{
  *out_value = NULL;
  if (!prop || prop->array_dim != 1 || prop->elem_size <= 0 || !arena) {
    lua_unreal_error_set(error, error_cap, "invalid container element property");
    return false;
  }

  int32_t alignment = unreal_fprop_min_alignment(prop);
  if (alignment <= 0) {
    alignment = 1;
  }

  if (alignment > LUA_UNREAL_MAX_ALIGNMENT) {
    lua_unreal_error_set(error, error_cap, "container element alignment %d is unsupported", alignment);
    return false;
  }

  void *temporary = arena_push_zero_aligned(arena, (uint64_t)prop->elem_size, (uint64_t)alignment);
  if (!temporary) {
    lua_unreal_error_set(error, error_cap, "could not allocate a temporary container element");
    return false;
  }

  if (!unreal_fprop_initialize_value(prop, temporary)) {
    lua_unreal_error_set(error, error_cap, "could not initialize a temporary container element");
    return false;
  }

  if (!lua_unreal_write_initialized(state, value_idx, prop, temporary, owner, error, error_cap)) {
    unreal_fprop_destroy_value(prop, temporary);
    return false;
  }

  *out_value = temporary;
  return true;
}

static lua_Unsigned
lua_unreal_sequence_count(lua_State *state, int table_idx)
{
  table_idx = lua_absindex(state, table_idx);
  lua_Unsigned count = (lua_Unsigned)lua_rawlen(state, table_idx);
  lua_getfield(state, table_idx, "n");
  if (lua_isinteger(state, -1)) {
    lua_Integer explicit_count = lua_tointeger(state, -1);
    if (explicit_count >= 0) {
      count = (lua_Unsigned)explicit_count;
    }
  }
  lua_pop(state, 1);
  return count;
}

static bool
lua_unreal_write_fixed_array(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  value_idx = lua_absindex(state, value_idx);

  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;
  if (lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL)) {
    if (lua_unreal_value_is_fixed_element(state, value_idx) || source_prop != prop ||
        !unreal_fprop_copy_complete_value(prop, dst, source_value)) {
      lua_unreal_error_set(error, error_cap, "expected a compatible fixed-array value");
      return false;
    }
    return true;
  }

  if (lua_type(state, value_idx) != LUA_TTABLE) {
    lua_unreal_error_set(error, error_cap, "expected a table with %d fixed-array elements", prop->array_dim);
    return false;
  }

  lua_pushnil(state);
  while (lua_next(state, value_idx) != 0) {
    lua_Integer index = lua_isinteger(state, -2) ? lua_tointeger(state, -2) : 0;
    if (index < 1 || index > prop->array_dim) {
      lua_pop(state, 2);
      lua_unreal_error_set(error, error_cap, "fixed-array keys must be integers from 1 through %d", prop->array_dim);
      return false;
    }
    lua_pop(state, 1);
  }

  for (int32_t i = 0; i < prop->array_dim; ++i) {
    void *element = unreal_fprop_value_at(prop, dst, i);
    lua_rawgeti(state, value_idx, (lua_Integer)i + 1);
    bool ok = element && lua_unreal_write_initialized_ex(state, -1, prop, element, owner, error, error_cap, true);
    lua_pop(state, 1);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static bool
lua_unreal_write_array(lua_State *state, int value_idx, fprop_array_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  value_idx = lua_absindex(state, value_idx);

  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;
  if (lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL)) {
    if (!unreal_fprop_same_type(&prop->base, source_prop) || !unreal_fprop_copy_single_value(&prop->base, dst, source_value)) {
      lua_unreal_error_set(error, error_cap, "expected a compatible array value");
      return false;
    }
    return true;
  }

  if (!prop->inner || prop->inner->array_dim != 1 || lua_type(state, value_idx) != LUA_TTABLE) {
    lua_unreal_error_set(error, error_cap, "expected a table or compatible array value");
    return false;
  }

  lua_Unsigned count = lua_unreal_sequence_count(state, value_idx);
  if (count > INT32_MAX || !unreal_array_clear(dst, prop)) {
    lua_unreal_error_set(error, error_cap, "could not initialize the dynamic array");
    return false;
  }

  for (lua_Unsigned i = 1; i <= count; ++i) {
    lua_rawgeti(state, value_idx, (lua_Integer)i);

    bool        ok  = false;
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      void *element = NULL;

      ok = lua_unreal_temporary_from_lua(state, -1, prop->inner, owner, tmp.arena, &element, error, error_cap);
      if (ok) {
        ok = unreal_array_add(dst, prop, element) >= 0;
        unreal_fprop_destroy_value(prop->inner, element);
        if (!ok) {
          lua_unreal_error_set(error, error_cap, "could not append dynamic-array element %llu", (unsigned long long)i);
        }
      }
    }
    scratch_end(tmp);

    lua_pop(state, 1);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static bool
lua_unreal_write_set(lua_State *state, int value_idx, fprop_set_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  value_idx = lua_absindex(state, value_idx);

  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;
  if (lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL)) {
    if (!unreal_fprop_same_type(&prop->base, source_prop) || !unreal_fprop_copy_single_value(&prop->base, dst, source_value)) {
      lua_unreal_error_set(error, error_cap, "expected a compatible set value");
      return false;
    }
    return true;
  }

  if (!prop->elem_prop || prop->elem_prop->array_dim != 1 || lua_type(state, value_idx) != LUA_TTABLE) {
    lua_unreal_error_set(error, error_cap, "expected a sequence table or compatible set value");
    return false;
  }

  lua_Unsigned count = lua_unreal_sequence_count(state, value_idx);
  if (count > INT32_MAX || !unreal_set_clear(dst, prop)) {
    lua_unreal_error_set(error, error_cap, "could not initialize the set");
    return false;
  }

  for (lua_Unsigned i = 1; i <= count; ++i) {
    lua_rawgeti(state, value_idx, (lua_Integer)i);
    bool        ok  = false;
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      void *element = NULL;

      ok = lua_unreal_temporary_from_lua(state, -1, prop->elem_prop, owner, tmp.arena, &element, error, error_cap);
      if (ok) {
        unreal_set_add(dst, prop, element);
        ok = unreal_set_contains(dst, prop, element);
        unreal_fprop_destroy_value(prop->elem_prop, element);
        if (!ok) {
          lua_unreal_error_set(error, error_cap, "could not add set element %llu", (unsigned long long)i);
        }
      }
    }
    scratch_end(tmp);

    lua_pop(state, 1);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static bool
lua_unreal_write_map(lua_State *state, int value_idx, fprop_map_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  value_idx = lua_absindex(state, value_idx);

  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;
  if (lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL)) {
    if (!unreal_fprop_same_type(&prop->base, source_prop) || !unreal_fprop_copy_single_value(&prop->base, dst, source_value)) {
      lua_unreal_error_set(error, error_cap, "expected a compatible map value");
      return false;
    }
    return true;
  }

  if (!prop->key_prop || !prop->val_prop || prop->key_prop->array_dim != 1 || prop->val_prop->array_dim != 1 ||
      lua_type(state, value_idx) != LUA_TTABLE) {
    lua_unreal_error_set(error, error_cap, "expected a table or compatible map value");
    return false;
  }

  if (!unreal_map_clear(dst, prop)) {
    lua_unreal_error_set(error, error_cap, "could not initialize the map");
    return false;
  }

  lua_pushnil(state);
  while (lua_next(state, value_idx) != 0) {
    bool        added = false;
    tmp_arena_t tmp   = scratch_begin(NULL);
    {
      void *key    = NULL;
      void *val    = NULL;
      bool  key_ok = lua_unreal_temporary_from_lua(state, -2, prop->key_prop, owner, tmp.arena, &key, error, error_cap);
      bool  val_ok = key_ok && lua_unreal_temporary_from_lua(state, -1, prop->val_prop, owner, tmp.arena, &val, error, error_cap);

      if (val_ok) {
        unreal_map_add(dst, prop, key, val);
        added = unreal_map_find_value_ptr(dst, prop, key) != NULL;
        if (!added) {
          lua_unreal_error_set(error, error_cap, "could not add a map pair");
        }
      }

      if (val) {
        unreal_fprop_destroy_value(prop->val_prop, val);
      }

      if (key) {
        unreal_fprop_destroy_value(prop->key_prop, key);
      }
    }
    scratch_end(tmp);

    lua_pop(state, 1);
    if (!added) {
      lua_pop(state, 1);
      return false;
    }
  }
  return true;
}

static bool
lua_unreal_write_struct_table(lua_State *state, int value_idx, ustruct_t *type, uint64_t container_size,
                              void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  value_idx = lua_absindex(state, value_idx);
  if (lua_type(state, value_idx) != LUA_TTABLE) {
    lua_unreal_error_set(error, error_cap, "expected a table");
    return false;
  }

  lua_pushnil(state);
  while (lua_next(state, value_idx) != 0) {
    if (lua_type(state, -2) != LUA_TSTRING) {
      lua_pop(state, 2);
      lua_unreal_error_set(error, error_cap, "struct table keys must be field-name strings");
      return false;
    }

    size_t      key_len = 0;
    const char *key     = lua_tolstring(state, -2, &key_len);
    fprop_t    *field   = unreal_ustruct_find_prop(type, str_make((uint8_t *)key, (uint64_t)key_len));
    if (!field) {
      lua_pop(state, 2);
      lua_unreal_error_set(error, error_cap, "unknown struct field '%.*s'", (int)MIN_VAL(key_len, (size_t)INT_MAX), key);
      return false;
    }

    if (!lua_unreal_property_fits(field, container_size)) {
      lua_pop(state, 2);
      lua_unreal_error_set(error, error_cap, "unsupported layout for struct field '%.*s'", (int)MIN_VAL(key_len, (size_t)INT_MAX), key);
      return false;
    }
    lua_pop(state, 1);
  }

  for (fprop_t *field = type->prop_link; field; field = field->prop_link_next) {
    if (!lua_unreal_property_fits(field, container_size)) {
      continue;
    }

    bool        ok  = false;
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      str_t name = unreal_fname_to_str(field->name, tmp.arena);
      lua_pushlstring(state, (const char *)name.data, (size_t)name.len);
      lua_rawget(state, value_idx);

      bool present = !lua_isnil(state, -1);
      if (present) {
        void *field_value = unreal_fprop_value_in_container(field, dst, 0);

        ok = field_value && lua_unreal_write_initialized(state, -1, field, field_value, owner, error, error_cap);
      } else {
        ok = true;
      }
      lua_pop(state, 1);
    }
    scratch_end(tmp);

    if (!ok) {
      if (error && error[0] == '\0') {
        lua_unreal_error_set(error, error_cap, "could not write struct field");
      }
      return false;
    }
  }
  return true;
}

static bool
lua_unreal_write_struct(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  value_idx = lua_absindex(state, value_idx);

  fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
  ustruct_t      *type        = (ustruct_t *)struct_prop->script_struct;
  if (!type || type->props_size < 0 || (uint64_t)type->props_size > (uint64_t)prop->elem_size) {
    lua_unreal_error_set(error, error_cap, "invalid reflected struct layout");
    return false;
  }

  fprop_t *source_prop  = NULL;
  void    *source_value = NULL;
  if (lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL)) {
    if (!unreal_fprop_same_type(prop, source_prop) || !unreal_fprop_copy_single_value(prop, dst, source_value)) {
      lua_unreal_error_set(error, error_cap, "expected a compatible struct value");
      return false;
    }
    return true;
  }

  if (!lua_unreal_write_struct_table(state, value_idx, type, (uint64_t)prop->elem_size, dst, owner, error, error_cap)) {
    if (lua_type(state, value_idx) != LUA_TTABLE && error && error[0] != '\0') {
      error[0] = '\0';
      lua_unreal_error_set(error, error_cap, "expected a table or compatible struct value");
    }
    return false;
  }
  return true;
}

static bool
lua_unreal_write_transactional(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner, char *error, size_t error_cap)
{
  uint64_t value_size = unreal_fprop_complete_size(prop);
  if (!state || !prop || !dst || value_size == 0) {
    lua_unreal_error_set(error, error_cap, "invalid reflected property layout");
    return false;
  }

  int32_t alignment = unreal_fprop_min_alignment(prop);
  if (alignment <= 0) {
    alignment = 1;
  }

  if (alignment > LUA_UNREAL_MAX_ALIGNMENT) {
    lua_unreal_error_set(error, error_cap, "property alignment %d is unsupported", alignment);
    return false;
  }

  bool        copied = false;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    void *temporary   = arena_push_zero_aligned(tmp.arena, value_size, (uint64_t)alignment);
    bool  initialized = temporary   && unreal_fprop_initialize_value(prop, temporary);
    bool  written     = initialized && lua_unreal_write_initialized(state, value_idx, prop, temporary, owner, error, error_cap);

    copied = written && (prop->array_dim == 1
                         ? unreal_fprop_copy_single_value(prop, dst, temporary)
                         : unreal_fprop_copy_complete_value(prop, dst, temporary));
    if (written && !copied) {
      lua_unreal_error_set(error, error_cap, "could not commit the reflected value");
    }

    if (!temporary) {
      lua_unreal_error_set(error, error_cap, "could not allocate temporary reflected storage");
    } else if (!initialized) {
      lua_unreal_error_set(error, error_cap, "could not initialize temporary reflected storage");
    }

    if (initialized) {
      unreal_fprop_destroy_value(prop, temporary);
    }
  }

  scratch_end(tmp);
  return copied;
}

bool
lua_unreal_prop_write(lua_State *state, int value_idx, fprop_t *prop, void *dst, uobject_t *owner)
{
  char error[LUA_UNREAL_ERROR_CAP] = {0};
  return lua_unreal_write_transactional(state, value_idx, prop, dst, owner, error, sizeof(error));
}

static bool
lua_unreal_write_fixed_element_transactional(lua_State *state, int value_idx, fprop_t *prop, void *array,
                                              int32_t element_idx, uobject_t *owner, char *error, size_t error_cap)
{
  uint64_t value_size = unreal_fprop_complete_size(prop);
  if (!state || !prop || !array || prop->array_dim <= 1 || element_idx < 0 || element_idx >= prop->array_dim || value_size == 0) {
    lua_unreal_error_set(error, error_cap, "invalid fixed-array element");
    return false;
  }

  int32_t alignment = unreal_fprop_min_alignment(prop);
  if (alignment <= 0) {
    alignment = 1;
  }

  if (alignment > LUA_UNREAL_MAX_ALIGNMENT) {
    lua_unreal_error_set(error, error_cap, "property alignment %d is unsupported", alignment);
    return false;
  }

  bool        copied = false;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    void *temporary   = arena_push_zero_aligned(tmp.arena, value_size, (uint64_t)alignment);
    bool  initialized = temporary && unreal_fprop_initialize_value(prop, temporary);
    bool  cloned      = initialized && unreal_fprop_copy_complete_value(prop, temporary, array);
    void *element     = cloned ? unreal_fprop_value_at(prop, temporary, element_idx) : NULL;
    bool  written     = element && lua_unreal_write_initialized_ex(state, value_idx, prop, element, owner, error, error_cap, true);

    copied = written && unreal_fprop_copy_complete_value(prop, array, temporary);
    if (written && !copied) {
      lua_unreal_error_set(error, error_cap, "could not commit the fixed-array element");
    } else if (!temporary) {
      lua_unreal_error_set(error, error_cap, "could not allocate temporary fixed-array storage");
    } else if (!initialized || !cloned) {
      lua_unreal_error_set(error, error_cap, "could not initialize temporary fixed-array storage");
    }

    if (initialized) {
      unreal_fprop_destroy_value(prop, temporary);
    }
  }
  scratch_end(tmp);
  return copied;
}

static int
lua_unreal_value_metatable_lookup(lua_State *state)
{
  if (lua_getmetatable(state, 1)) {
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);
    lua_remove(state, -2);
    return 1;
  }
  lua_pushnil(state);
  return 1;
}

static bool
lua_unreal_value_is_fixed_element(lua_State *state, int idx)
{
  lua_unreal_proxy_t *proxy = (lua_unreal_proxy_t *)luaL_testudata(state, idx, LUA_UNREAL_PROXY_MT);
  return proxy && proxy->selector_kind == LUA_UNREAL_SELECTOR_FIXED_ARRAY_ELEMENT;
}

static int32_t
lua_unreal_set_sparse_index_from_lua(const void *set, fprop_set_t *prop, lua_Integer lua_index)
{
  if (lua_index < 1) {
    return -1;
  }

  int32_t ordinal   = 0;
  int32_t max_index = unreal_set_max_index(set, prop);
  for (int32_t sparse_index = 0; sparse_index < max_index; ++sparse_index) {
    if (unreal_set_is_valid_index(set, prop, sparse_index) && ++ordinal == lua_index) {
      return sparse_index;
    }
  }
  return -1;
}

static int
lua_unreal_value_index(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  fprop_t   *parent_prop  = NULL;
  void      *parent_value = NULL;
  uobject_t *owner        = NULL;

  if (!lua_unreal_value_resolve(state, 1, &parent_prop, &parent_value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  if (parent_prop->array_dim > 1 && !lua_unreal_value_is_fixed_element(state, 1)) {
    if (!lua_isinteger(state, 2)) {
      return lua_unreal_value_metatable_lookup(state);
    }

    lua_Integer lua_index = lua_tointeger(state, 2);
    if (lua_index < 1 || lua_index > parent_prop->array_dim) {
      lua_pushnil(state);
      return 1;
    }

    int32_t index = (int32_t)lua_index - 1;
    void   *value = unreal_fprop_value_at(parent_prop, parent_value, index);
    if (!value || !lua_unreal_prop_push_selected(state, parent_prop, value, owner, 1, true,
                                                  LUA_UNREAL_SELECTOR_FIXED_ARRAY_ELEMENT, index, true)) {
      return luaL_error(state, "fixed-array element %lld has an unsupported reflected type", (long long)lua_index);
    }
    return 1;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(parent_prop);
  if (kind == UNREAL_PROP_KIND_STRUCT) {
    if (lua_type(state, 2) != LUA_TSTRING) {
      return lua_unreal_value_metatable_lookup(state);
    }

    size_t          key_len     = 0;
    const char     *key         = lua_tolstring(state, 2, &key_len);
    fprop_struct_t *struct_prop = (fprop_struct_t *)parent_prop;
    ustruct_t      *type        = (ustruct_t *)struct_prop->script_struct;
    fprop_t        *field       = unreal_ustruct_find_prop(type, str_make((uint8_t *)key, (uint64_t)key_len));

    if (!field) {
      return lua_unreal_value_metatable_lookup(state);
    }

    if (!lua_unreal_property_fits(field, (uint64_t)parent_prop->elem_size)) {
      return luaL_error(state, "field '%s' has an unsupported reflected layout", key);
    }

    void *value = unreal_fprop_value_in_container(field, parent_value, 0);
    if (!value || !lua_unreal_prop_push_selected(state, field, value, owner, 1, true,
                                                  LUA_UNREAL_SELECTOR_FIELD, 0, false)) {
      return luaL_error(state, "field '%s' has an unsupported reflected type", key);
    }
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_ARRAY) {
    if (!lua_isinteger(state, 2)) {
      return lua_unreal_value_metatable_lookup(state);
    }

    fprop_array_t *array_prop = (fprop_array_t *)parent_prop;
    lua_Integer    lua_index  = lua_tointeger(state, 2);
    int32_t        count      = unreal_array_num(parent_value, array_prop);
    if (lua_index < 1 || lua_index > count) {
      lua_pushnil(state);
      return 1;
    }

    int32_t index = (int32_t)lua_index - 1;
    void   *value = unreal_array_get(parent_value, array_prop, index);
    if (!array_prop->inner || !value ||
        !lua_unreal_prop_push_selected(state, array_prop->inner, value, owner, 1, true,
                                       LUA_UNREAL_SELECTOR_ARRAY_ELEMENT, index, true)) {
      return luaL_error(state, "array element %lld has an unsupported reflected type", (long long)lua_index);
    }
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_SET) {
    if (!lua_isinteger(state, 2)) {
      return lua_unreal_value_metatable_lookup(state);
    }

    fprop_set_t *set_prop     = (fprop_set_t *)parent_prop;
    int32_t      sparse_index = lua_unreal_set_sparse_index_from_lua(parent_value, set_prop, lua_tointeger(state, 2));
    const void  *value        = sparse_index >= 0 ? unreal_set_get(parent_value, set_prop, sparse_index) : NULL;
    if (!value) {
      lua_pushnil(state);
      return 1;
    }

    if (!set_prop->elem_prop ||
        !lua_unreal_prop_push_selected(state, set_prop->elem_prop, (void *)value, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true)) {
      return luaL_error(state, "set element has an unsupported reflected type");
    }
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_MAP) {
    fprop_map_t *map_prop = (fprop_map_t *)parent_prop;
    if (!map_prop->key_prop || !map_prop->val_prop) {
      return luaL_error(state, "map metadata is incomplete");
    }

    char        conversion_error[LUA_UNREAL_ERROR_CAP] = {0};
    int         result = 0;
    tmp_arena_t tmp    = scratch_begin(NULL);
    {
      void *key       = NULL;
      bool  converted = lua_unreal_temporary_from_lua(state, 2, map_prop->key_prop, owner, tmp.arena, &key, conversion_error, sizeof(conversion_error));
      void *value     = converted ? unreal_map_find_value_ptr(parent_value, map_prop, key) : NULL;
      if (value) {
        result = lua_unreal_map_value_push(state, map_prop, value, key, owner, 1);
      }

      if (key) {
        unreal_fprop_destroy_value(map_prop->key_prop, key);
      }
    }
    scratch_end(tmp);

    if (result) {
      return result;
    }

    if (lua_type(state, 2) == LUA_TSTRING) {
      return lua_unreal_value_metatable_lookup(state);
    }

    lua_pushnil(state);
    return 1;
  }

  return lua_unreal_value_metatable_lookup(state);
}

static int
lua_unreal_value_newindex(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  fprop_t    *parent_prop  = NULL;
  void       *parent_value = NULL;
  uobject_t  *owner        = NULL;

  if (!lua_unreal_value_resolve(state, 1, &parent_prop, &parent_value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  char error[LUA_UNREAL_ERROR_CAP] = {0};
  if (parent_prop->array_dim > 1 && !lua_unreal_value_is_fixed_element(state, 1)) {
    lua_Integer lua_index = luaL_checkinteger(state, 2);
    if (lua_index < 1 || lua_index > parent_prop->array_dim ||
        !lua_unreal_write_fixed_element_transactional(state, 3, parent_prop, parent_value, (int32_t)lua_index - 1, owner, error, sizeof(error))) {
      return luaL_error(state, "could not assign fixed-array element: %s", error[0] ? error : "index out of range");
    }
    return 0;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(parent_prop);
  if (kind == UNREAL_PROP_KIND_STRUCT) {
    size_t          key_len     = 0;
    const char     *key         = luaL_checklstring(state, 2, &key_len);
    fprop_struct_t *struct_prop = (fprop_struct_t *)parent_prop;
    ustruct_t      *type        = (ustruct_t *)struct_prop->script_struct;
    fprop_t        *field       = unreal_ustruct_find_prop(type, str_make((uint8_t *)key, (uint64_t)key_len));

    if (!field || !lua_unreal_property_fits(field, (uint64_t)parent_prop->elem_size)) {
      return luaL_error(state, "unknown or unsupported struct field '%s'", key);
    }

    void *dst = unreal_fprop_value_in_container(field, parent_value, 0);
    if (!dst || !lua_unreal_write_transactional(state, 3, field, dst, owner, error, sizeof(error))) {
      return luaL_error(state, "could not assign '%s': %s", key, error[0] ? error : "conversion failed");
    }
    return 0;
  }

  if (kind == UNREAL_PROP_KIND_ARRAY) {
    fprop_array_t *array_prop = (fprop_array_t *)parent_prop;
    lua_Integer    lua_index  = luaL_checkinteger(state, 2);
    int32_t        count      = unreal_array_num(parent_value, array_prop);
    if (!array_prop->inner || count < 0 || lua_index < 1 || lua_index > (lua_Integer)count + 1) {
      return luaL_error(state, "array index is out of range");
    }

    bool        converted = false;
    tmp_arena_t tmp       = scratch_begin(NULL);
    {
      void *element = NULL;

      converted = lua_unreal_temporary_from_lua(state, 3, array_prop->inner, owner, tmp.arena, &element, error, sizeof(error));
      if (converted) {
        int32_t index = (int32_t)lua_index - 1;
        bool    append = index == count;
        if (append) {
          converted = unreal_array_add(parent_value, array_prop, element) == index;
        } else {
          unreal_array_set(parent_value, array_prop, index, element, false);
        }

        void *committed = converted || !append ? unreal_array_get(parent_value, array_prop, index) : NULL;

        converted = committed && unreal_fprop_single_values_identical(array_prop->inner, committed, element, 0);
        if (!converted && append && unreal_array_num(parent_value, array_prop) == count + 1) {
          unreal_array_remove(parent_value, array_prop, index);
        }
        unreal_fprop_destroy_value(array_prop->inner, element);

        if (!converted) {
          lua_unreal_error_set(error, sizeof(error), "could not commit the array element");
        }
      }
    }
    scratch_end(tmp);

    if (!converted) {
      return luaL_error(state, "could not assign array element: %s", error[0] ? error : "conversion failed");
    }
    return 0;
  }

  if (kind == UNREAL_PROP_KIND_MAP) {
    fprop_map_t *map_prop = (fprop_map_t *)parent_prop;
    if (!map_prop->key_prop || !map_prop->val_prop) {
      return luaL_error(state, "map metadata is incomplete");
    }

    bool        ok  = false;
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      void *key    = NULL;
      void *val    = NULL;

      ok = lua_unreal_temporary_from_lua(state, 2, map_prop->key_prop, owner, tmp.arena, &key, error, sizeof(error));
      if (ok && lua_isnil(state, 3)) {
        unreal_map_remove(parent_value, map_prop, key);
      } else if (ok) {
        ok = lua_unreal_temporary_from_lua(state, 3, map_prop->val_prop, owner, tmp.arena, &val, error, sizeof(error));
        if (ok) {
          unreal_map_add(parent_value, map_prop, key, val);
          ok = unreal_map_find_value_ptr(parent_value, map_prop, key) != NULL;
        }
      }

      if (val) {
        unreal_fprop_destroy_value(map_prop->val_prop, val);
      }

      if (key) {
        unreal_fprop_destroy_value(map_prop->key_prop, key);
      }
    }
    scratch_end(tmp);

    if (!ok) {
      return luaL_error(state, "could not assign map value: %s", error[0] ? error : "conversion failed");
    }
    return 0;
  }

  return luaL_error(state, "this reflected value does not support indexed assignment");
}

static int
lua_unreal_value_len(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  if (prop->array_dim > 1 && !lua_unreal_value_is_fixed_element(state, 1)) {
    lua_pushinteger(state, prop->array_dim);
    return 1;
  }

  int32_t count = -1;
  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_ARRAY: count = unreal_array_num(value, (fprop_array_t *)prop); break;
    case UNREAL_PROP_KIND_SET:   count = unreal_set_num(value, (fprop_set_t *)prop); break;
    case UNREAL_PROP_KIND_MAP:   count = unreal_map_num(value, (fprop_map_t *)prop); break;
    default: break;
  }

  if (count < 0) {
    return luaL_error(state, "this reflected value has no length");
  }

  lua_pushinteger(state, count);
  return 1;
}

static int
lua_unreal_value_get_array_num_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL) || unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_ARRAY) {
    return luaL_error(state, "GetArrayNum requires a live TArray");
  }

  int32_t count = unreal_array_num(value, (fprop_array_t *)prop);
  if (count < 0) {
    return luaL_error(state, "the TArray is invalid");
  }
  lua_pushinteger(state, count);
  return 1;
}

static int
lua_unreal_value_get_array_address_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL) ||
      unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_ARRAY ||
      unreal_array_num(value, (fprop_array_t *)prop) < 0) {
    return luaL_error(state, "GetArrayAddress requires a live TArray");
  }

  lua_pushinteger(state, (lua_Integer)(uintptr_t)value);
  return 1;
}

static int
lua_unreal_value_get_array_max_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL) ||
      unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_ARRAY ||
      unreal_array_num(value, (fprop_array_t *)prop) < 0) {
    return luaL_error(state, "GetArrayMax requires a live TArray");
  }

  lua_pushinteger(state, ((const fscript_array_t *)value)->max);
  return 1;
}

static int
lua_unreal_value_get_array_data_address_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL) ||
      unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_ARRAY ||
      unreal_array_num(value, (fprop_array_t *)prop) < 0) {
    return luaL_error(state, "GetArrayDataAddress requires a live TArray");
  }

  lua_pushinteger(state, (lua_Integer)(uintptr_t)((const fscript_array_t *)value)->data);
  return 1;
}

static int
lua_unreal_value_for_each_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));
  luaL_checktype(state, 2, LUA_TFUNCTION);

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  int callback_idx = lua_absindex(state, 2);
  if (prop->array_dim > 1 && !lua_unreal_value_is_fixed_element(state, 1)) {
    for (int32_t i = 0; i < prop->array_dim; ++i) {
      lua_pushvalue(state, callback_idx);
      lua_pushinteger(state, (lua_Integer)i + 1);

      if (!lua_unreal_proxy_param_push(state, prop, 1, LUA_UNREAL_SELECTOR_FIXED_ARRAY_ELEMENT, i, NULL, NULL, owner, true)) {
        lua_pop(state, 1);
        return luaL_error(state, "could not marshal fixed-array element %d", i + 1);
      }

      if (lua_pcall(state, 2, 1, 0) != LUA_OK) {
        return lua_error(state);
      }

      bool stop = lua_toboolean(state, -1) != 0;
      lua_pop(state, 1);
      if (stop) {
        break;
      }
    }
    return 0;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_ARRAY) {
    fprop_array_t *array_prop = (fprop_array_t *)prop;
    int32_t count = unreal_array_num(value, array_prop);
    if (count < 0 || !array_prop->inner) {
      return luaL_error(state, "TArray metadata is unavailable");
    }

    for (int32_t i = 0; i < count; ++i) {
      lua_pushvalue(state, callback_idx);
      lua_pushinteger(state, (lua_Integer)i + 1);

      if (!lua_unreal_proxy_param_push(state, array_prop->inner, 1, LUA_UNREAL_SELECTOR_ARRAY_ELEMENT, i, NULL, NULL, owner, true)) {
        lua_pop(state, 1);
        return luaL_error(state, "could not marshal TArray element %d", i + 1);
      }

      if (lua_pcall(state, 2, 1, 0) != LUA_OK) {
        return lua_error(state);
      }

      bool stop = lua_toboolean(state, -1) != 0;
      lua_pop(state, 1);
      if (stop) {
        break;
      }
    }
    return 0;
  }

  if (kind == UNREAL_PROP_KIND_SET) {
    fprop_set_t *set_prop = (fprop_set_t *)prop;
    if (!set_prop->elem_prop) {
      return luaL_error(state, "TSet metadata is unavailable");
    }

    int32_t max_index = unreal_set_max_index(value, set_prop);
    for (int32_t i = 0; i < max_index; ++i) {
      const void *element = unreal_set_get(value, set_prop, i);
      if (!element) {
        continue;
      }

      lua_pushvalue(state, callback_idx);
      if (!lua_unreal_prop_push_selected(state, set_prop->elem_prop, (void *)element, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true)) {
        lua_pop(state, 1);
        return luaL_error(state, "could not marshal a TSet element");
      }

      if (lua_pcall(state, 1, 1, 0) != LUA_OK) {
        return lua_error(state);
      }

      bool stop = lua_toboolean(state, -1) != 0;
      lua_pop(state, 1);
      if (stop) {
        break;
      }
    }
    return 0;
  }

  if (kind == UNREAL_PROP_KIND_MAP) {
    fprop_map_t *map_prop = (fprop_map_t *)prop;
    if (!map_prop->key_prop || !map_prop->val_prop) {
      return luaL_error(state, "TMap metadata is unavailable");
    }

    int32_t max_index = unreal_map_max_index(value, map_prop);
    for (int32_t i = 0; i < max_index; ++i) {
      const void *key = unreal_map_get_key(value, map_prop, i);
      void       *val = unreal_map_get_value(value, map_prop, i);
      if (!key || !val) {
        continue;
      }

      lua_pushvalue(state, callback_idx);
      if (!lua_unreal_proxy_param_push(state, map_prop->key_prop, 1, LUA_UNREAL_SELECTOR_MAP_KEY, 0, map_prop->key_prop, key, owner, false)) {
        lua_pop(state, 1);
        return luaL_error(state, "could not marshal a TMap key");
      }

      if (!lua_unreal_proxy_param_push(state, map_prop->val_prop, 1, LUA_UNREAL_SELECTOR_MAP_VALUE, 0, map_prop->key_prop, key, owner, true)) {
        lua_pop(state, 2);
        return luaL_error(state, "could not marshal a TMap value");
      }

      if (lua_pcall(state, 2, 1, 0) != LUA_OK) {
        return lua_error(state);
      }

      bool stop = lua_toboolean(state, -1) != 0;
      lua_pop(state, 1);
      if (stop) {
        break;
      }
    }
    return 0;
  }

  return luaL_error(state, "ForEach requires a fixed array, TArray, TSet, or TMap");
}

static int
lua_unreal_value_type_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, NULL, NULL)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  const char *type = "UnrealValue";
  if (prop->array_dim > 1 && !lua_unreal_value_is_fixed_element(state, 1)) {
    type = "FixedArray";
  } else {
    switch (unreal_fprop_get_kind(prop)) {
      case UNREAL_PROP_KIND_STRING:      type = "FString";        break;
      case UNREAL_PROP_KIND_TEXT:        type = "FText";          break;
      case UNREAL_PROP_KIND_SOFT_OBJECT: type = "TSoftObjectPtr"; break;
      case UNREAL_PROP_KIND_SOFT_CLASS:  type = "TSoftClassPtr";  break;
      case UNREAL_PROP_KIND_STRUCT:      type = "UScriptStruct";  break;
      case UNREAL_PROP_KIND_ARRAY:       type = "TArray";         break;
      case UNREAL_PROP_KIND_SET:         type = "TSet";           break;
      case UNREAL_PROP_KIND_MAP:         type = "TMap";           break;
      default: break;
    }
  }
  lua_pushstring(state, type);
  return 1;
}

static int
lua_unreal_value_is_valid_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));
  lua_pushboolean(state, lua_unreal_value_resolve(state, 1, NULL, NULL, NULL));
  return 1;
}

static lua_unreal_param_t *
lua_unreal_param_check(lua_State *state, int idx)
{
  lua_unreal_param_t *param = (lua_unreal_param_t *)luaL_checkudata(state, idx, LUA_UNREAL_PARAM_MT);
  lua_unreal_require_access(state, param->context);
  return param;
}

static bool
lua_unreal_remote_param_is_valid(lua_unreal_param_t *param)
{
  if (!param || param->kind == LUA_UNREAL_PARAM_PROXY || !param->scope || !param->scope->active) {
    return false;
  }

  if (param->kind == LUA_UNREAL_PARAM_OBJECT) {
    return unreal_fweak_object_resolve(param->object) != NULL;
  }

  return param->kind == LUA_UNREAL_PARAM_BORROWED && param->prop && param->value;
}

static int
lua_unreal_param_get_lua(lua_State *state)
{
  lua_unreal_param_t *param = lua_unreal_param_check(state, 1);
  if (param->kind != LUA_UNREAL_PARAM_PROXY) {
    if (!lua_unreal_remote_param_is_valid(param)) {
      return luaL_error(state, "the Unreal parameter is no longer valid");
    }

    if (param->kind == LUA_UNREAL_PARAM_OBJECT) {
      return lua_unreal_object_push(state, unreal_fweak_object_resolve(param->object));
    }

    uobject_t *owner = unreal_fweak_object_resolve(param->owner);
    if (!lua_unreal_prop_push(state, param->prop, param->value, owner)) {
      return luaL_error(state, "the Unreal parameter has an unsupported reflected type");
    }
    return 1;
  }

  if (lua_getiuservalue(state, 1, 1) == LUA_TNONE) {
    lua_pop(state, 1);
    return luaL_error(state, "the Unreal parameter is no longer valid");
  }

  int        proxy_idx = lua_gettop(state);
  fprop_t   *prop      = NULL;
  void      *value     = NULL;
  uobject_t *owner     = NULL;
  if (!lua_unreal_value_resolve(state, proxy_idx, &prop, &value, &owner)) {
    lua_pop(state, 1);
    return luaL_error(state, "the Unreal parameter is no longer valid");
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  bool return_proxy = prop->array_dim > 1 || kind == UNREAL_PROP_KIND_STRING || kind == UNREAL_PROP_KIND_TEXT ||
                      lua_unreal_kind_is_soft_object(kind) || kind == UNREAL_PROP_KIND_STRUCT ||
                      kind == UNREAL_PROP_KIND_ARRAY || kind == UNREAL_PROP_KIND_SET || kind == UNREAL_PROP_KIND_MAP;
  if (return_proxy) {
    return 1;
  }

  if (!lua_unreal_prop_push_selected(state, prop, value, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true)) {
    lua_pop(state, 1);
    return luaL_error(state, "the Unreal parameter has an unsupported reflected type");
  }
  lua_remove(state, proxy_idx);
  return 1;
}

static int
lua_unreal_param_set_lua(lua_State *state)
{
  lua_unreal_param_t *param = lua_unreal_param_check(state, 1);
  if (!param->writable) {
    return luaL_error(state, "this Unreal parameter is read-only");
  }

  if (param->kind != LUA_UNREAL_PARAM_PROXY) {
    if (!lua_unreal_remote_param_is_valid(param) || param->kind != LUA_UNREAL_PARAM_BORROWED) {
      return luaL_error(state, "the Unreal parameter is no longer valid");
    }

    char error[LUA_UNREAL_ERROR_CAP] = {0};
    uobject_t *owner = unreal_fweak_object_resolve(param->owner);
    if (!lua_unreal_write_transactional(state, 2, param->prop, param->value, owner, error, sizeof(error))) {
      return luaL_error(state, "could not set the Unreal parameter: %s", error[0] ? error : "conversion failed");
    }
    return 0;
  }

  if (lua_getiuservalue(state, 1, 1) == LUA_TNONE) {
    lua_pop(state, 1);
    return luaL_error(state, "the Unreal parameter is no longer valid");
  }

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, -1, &prop, &value, &owner)) {
    lua_pop(state, 1);
    return luaL_error(state, "the Unreal parameter is no longer valid");
  }

  char error[LUA_UNREAL_ERROR_CAP] = {0};
  bool ok = lua_unreal_write_transactional(state, 2, prop, value, owner, error, sizeof(error));
  lua_pop(state, 1);
  if (!ok) {
    return luaL_error(state, "could not set the Unreal parameter: %s", error[0] ? error : "conversion failed");
  }
  return 0;
}

static int
lua_unreal_param_is_valid_lua(lua_State *state)
{
  lua_unreal_param_t *param = lua_unreal_param_check(state, 1);
  if (param->kind != LUA_UNREAL_PARAM_PROXY) {
    lua_pushboolean(state, lua_unreal_remote_param_is_valid(param));
    return 1;
  }

  bool valid = false;
  if (lua_getiuservalue(state, 1, 1) != LUA_TNONE) {
    valid = lua_unreal_value_resolve(state, -1, NULL, NULL, NULL);
  }
  lua_pop(state, 1);
  lua_pushboolean(state, valid);
  return 1;
}

static int
lua_unreal_param_type_lua(lua_State *state)
{
  lua_unreal_param_t *param = lua_unreal_param_check(state, 1);
  lua_pushstring(state, param->kind == LUA_UNREAL_PARAM_PROXY && param->is_local ? "LocalUnrealParam" : "RemoteUnrealParam");
  return 1;
}

static int
lua_unreal_value_to_string_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  bool        ok   = false;
  tmp_arena_t tmp  = scratch_begin(NULL);
  {
    str_t text = STR_NULL;
    unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
    if (kind == UNREAL_PROP_KIND_STRING || kind == UNREAL_PROP_KIND_TEXT) {
      ok = unreal_fprop_read_string(prop, value, tmp.arena, &text);
    } else if (lua_unreal_kind_is_soft_object(kind)) {
      ok = unreal_fprop_read_soft_path(prop, value, tmp.arena, &text);
    }

    if (ok) {
      lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
    }
  }
  scratch_end(tmp);

  if (!ok) {
    return luaL_error(state, "ToString is only available for FString, FText, and soft references");
  }
  return 1;
}

static bool
lua_unreal_fstring_resolve(lua_State *state, fprop_t **out_prop, void **out_value, uobject_t **out_owner)
{
  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  bool ok = lua_unreal_value_resolve(state, 1, &prop, &value, &owner) && unreal_fprop_get_kind(prop) == UNREAL_PROP_KIND_STRING;
  if (ok) {
    if (out_prop) {
      *out_prop = prop;
    }

    if (out_value) {
      *out_value = value;
    }

    if (out_owner) {
      *out_owner = owner;
    }
  }
  return ok;
}

static int
lua_unreal_fstring_len_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  void *value = NULL;
  if (!lua_unreal_fstring_resolve(state, NULL, &value, NULL)) {
    return luaL_error(state, "Len requires a live FString");
  }

  const fstring_t *string = (const fstring_t *)value;
  if (string->len < 0 || string->max < string->len || (string->len > 0 && !string->data)) {
    return luaL_error(state, "the FString is invalid");
  }
  lua_pushinteger(state, string->len > 0 ? string->len - 1 : 0);
  return 1;
}

static int
lua_unreal_fstring_is_empty_lua(lua_State *state)
{
  lua_unreal_fstring_len_lua(state);
  lua_pushboolean(state, lua_tointeger(state, -1) == 0);
  lua_remove(state, -2);
  return 1;
}

static int
lua_unreal_fstring_append_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_fstring_resolve(state, &prop, &value, &owner)) {
    return luaL_error(state, "Append requires a live FString");
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};
  bool        ok  = false;
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t current = STR_NULL;
    str_t suffix  = STR_NULL;
    if (unreal_fprop_read_string(prop, value, tmp.arena, &current) && lua_unreal_string_like(state, 2, tmp.arena, &suffix)) {
      str_t combined = str_push_concat(tmp.arena, current, suffix);
      lua_pushlstring(state, (const char *)combined.data, (size_t)combined.len);
      ok = lua_unreal_write_transactional(state, -1, prop, value, owner, error, sizeof(error));
      lua_pop(state, 1);
    } else {
      lua_unreal_error_set(error, sizeof(error), "expected a string or FString");
    }
  }
  scratch_end(tmp);

  if (!ok) {
    return luaL_error(state, "could not append to FString: %s", error[0] ? error : "conversion failed");
  }
  return 0;
}

static int
lua_unreal_fstring_find_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_fstring_resolve(state, &prop, &value, NULL)) {
    return luaL_error(state, "Find requires a live FString");
  }

  int         result   = 0;
  bool        readable = false;
  tmp_arena_t tmp      = scratch_begin(NULL);
  {
    str_t current = STR_NULL;
    str_t search  = STR_NULL;
    readable = unreal_fprop_read_string(prop, value, tmp.arena, &current) && lua_unreal_string_like(state, 2, tmp.arena, &search);
    if (readable) {
      str16_t haystack = str16_from_str(tmp.arena, current);
      str16_t needle   = str16_from_str(tmp.arena, search);
      bool    found    = false;
      for (uint64_t i = 0; i + needle.len <= haystack.len && !found; ++i) {
        bool match = true;
        for (uint64_t j = 0; j < needle.len; ++j) {
          if (haystack.data[i + j] != needle.data[j]) {
            match = false;
            break;
          }
        }

        if (match) {
          lua_pushinteger(state, (lua_Integer)i + 1);
          found = true;
          result = 1;
        }
      }

      if (!found) {
        lua_pushnil(state);
        result = 1;
      }
    }
  }
  scratch_end(tmp);

  if (!readable) {
    return luaL_error(state, "Find expects a string or FString");
  }
  return result;
}

static int
lua_unreal_fstring_has_affix_lua(lua_State *state, bool suffix)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_fstring_resolve(state, &prop, &value, NULL)) {
    return luaL_error(state, "%s requires a live FString", suffix ? "EndsWith" : "StartsWith");
  }

  bool        matches = false;
  int         result  = 1;
  tmp_arena_t tmp     = scratch_begin(NULL);
  {
    str_t current = STR_NULL;
    str_t affix   = STR_NULL;
    if (unreal_fprop_read_string(prop, value, tmp.arena, &current) && lua_unreal_string_like(state, 2, tmp.arena, &affix)) {
      str16_t haystack = str16_from_str(tmp.arena, current);
      str16_t needle   = str16_from_str(tmp.arena, affix);
      if (needle.len <= haystack.len) {
        uint64_t offset = suffix ? haystack.len - needle.len : 0;
        matches = true;
        for (uint64_t i = 0; i < needle.len; ++i) {
          if (haystack.data[offset + i] != needle.data[i]) {
            matches = false;
            break;
          }
        }
      }
    } else {
      result = luaL_error(state, "%s expects a string or FString", suffix ? "EndsWith" : "StartsWith");
    }
  }
  scratch_end(tmp);

  lua_pushboolean(state, matches);
  return result;
}

static int
lua_unreal_fstring_starts_with_lua(lua_State *state)
{
  return lua_unreal_fstring_has_affix_lua(state, false);
}

static int
lua_unreal_fstring_ends_with_lua(lua_State *state)
{
  return lua_unreal_fstring_has_affix_lua(state, true);
}

static int
lua_unreal_fstring_change_case_lua(lua_State *state, bool upper)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_fstring_resolve(state, &prop, &value, &owner)) {
    return luaL_error(state, "%s requires a live FString", upper ? "ToUpper" : "ToLower");
  }

  int         result = 0;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str_t current = STR_NULL;
    if (unreal_fprop_read_string(prop, value, tmp.arena, &current)) {
      str_t changed = upper ? str_to_upper(tmp.arena, current) : str_to_lower(tmp.arena, current);
      lua_pushlstring(state, (const char *)changed.data, (size_t)changed.len);

      char  error[LUA_UNREAL_ERROR_CAP] = {0};
      void *temporary = NULL;
      if (lua_unreal_temporary_from_lua(state, -1, prop, owner, tmp.arena, &temporary, error, sizeof(error))) {
        lua_pop(state, 1);
        result = lua_unreal_owned_push(state, prop, temporary, owner);
        unreal_fprop_destroy_value(prop, temporary);
      } else {
        lua_pop(state, 1);
      }
    }
  }
  scratch_end(tmp);

  if (!result) {
    return luaL_error(state, "could not create the converted FString");
  }
  return result;
}

static int
lua_unreal_fstring_to_upper_lua(lua_State *state)
{
  return lua_unreal_fstring_change_case_lua(state, true);
}

static int
lua_unreal_fstring_to_lower_lua(lua_State *state)
{
  return lua_unreal_fstring_change_case_lua(state, false);
}

static int
lua_unreal_value_to_table_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  lua_newtable(state);
  if (prop->array_dim > 1 && !lua_unreal_value_is_fixed_element(state, 1)) {
    for (int32_t i = 0; i < prop->array_dim; ++i) {
      void *element = unreal_fprop_value_at(prop, value, i);
      if (!element || !lua_unreal_prop_push_selected(state, prop, element, owner, 1, true, LUA_UNREAL_SELECTOR_FIXED_ARRAY_ELEMENT, i, true)) {
        return luaL_error(state, "could not marshal fixed-array element %d", i + 1);
      }
      lua_rawseti(state, -2, (lua_Integer)i + 1);
    }
    return 1;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_STRUCT) {
    fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
    ustruct_t      *type        = (ustruct_t *)struct_prop->script_struct;
    if (!type) {
      return luaL_error(state, "struct metadata is unavailable");
    }

    for (fprop_t *field = type->prop_link; field; field = field->prop_link_next) {
      if (!lua_unreal_property_fits(field, (uint64_t)prop->elem_size)) {
        continue;
      }

      void *field_value = unreal_fprop_value_in_container(field, value, 0);
      if (!field_value || !lua_unreal_prop_push_selected(state, field, field_value, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, false)) {
        return luaL_error(state, "could not marshal a struct field");
      }

      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t name = unreal_fname_to_str(field->name, tmp.arena);
        lua_setfield(state, -2, (const char *)name.data);
      }
      scratch_end(tmp);
    }
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_ARRAY) {
    fprop_array_t *array_prop = (fprop_array_t *)prop;
    int32_t count = unreal_array_num(value, array_prop);
    if (count < 0 || !array_prop->inner) {
      return luaL_error(state, "dynamic-array metadata is unavailable");
    }

    for (int32_t i = 0; i < count; ++i) {
      void *element = unreal_array_get(value, array_prop, i);
      if (!element || !lua_unreal_prop_push_selected(state, array_prop->inner, element, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true)) {
        return luaL_error(state, "could not marshal dynamic-array element %d", i + 1);
      }
      lua_rawseti(state, -2, (lua_Integer)i + 1);
    }

    lua_pushinteger(state, count);
    lua_setfield(state, -2, "n");
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_SET) {
    fprop_set_t *set_prop = (fprop_set_t *)prop;
    int32_t count = unreal_set_num(value, set_prop);
    if (count < 0 || !set_prop->elem_prop) {
      return luaL_error(state, "set metadata is unavailable");
    }

    int32_t out_index = 1;
    int32_t max_index = unreal_set_max_index(value, set_prop);
    for (int32_t i = 0; i < max_index; ++i) {
      const void *element = unreal_set_get(value, set_prop, i);
      if (!element) {
        continue;
      }

      if (!lua_unreal_prop_push_selected(state, set_prop->elem_prop, (void *)element, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true)) {
        return luaL_error(state, "could not marshal a set element");
      }

      lua_rawseti(state, -2, out_index++);
    }
    lua_pushinteger(state, count);
    lua_setfield(state, -2, "n");
    return 1;
  }

  if (kind == UNREAL_PROP_KIND_MAP) {
    fprop_map_t *map_prop = (fprop_map_t *)prop;
    int32_t count = unreal_map_num(value, map_prop);
    if (count < 0 || !map_prop->key_prop || !map_prop->val_prop) {
      return luaL_error(state, "map metadata is unavailable");
    }

    int32_t max_index = unreal_map_max_index(value, map_prop);
    for (int32_t i = 0; i < max_index; ++i) {
      const void *key = unreal_map_get_key(value, map_prop, i);
      void       *val = unreal_map_get_value(value, map_prop, i);

      if (!key || !val) {
        continue;
      }

      if (!lua_unreal_prop_push_selected(state, map_prop->key_prop, (void *)key, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true) ||
          !lua_unreal_prop_push_selected(state, map_prop->val_prop, val, owner, 0, false, LUA_UNREAL_SELECTOR_FIELD, 0, true)) {
        return luaL_error(state, "could not marshal a map pair");
      }
      lua_rawset(state, -3);
    }
    return 1;
  }

  return luaL_error(state, "this reflected value cannot be converted to a table");
}

static int
lua_unreal_value_clear_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  bool cleared = false;
  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_STRING: {
      char error[LUA_UNREAL_ERROR_CAP] = {0};
      lua_pushliteral(state, "");
      cleared = lua_unreal_write_transactional(state, -1, prop, value, owner, error, sizeof(error));
      lua_pop(state, 1);
      break;
    }

    case UNREAL_PROP_KIND_ARRAY: cleared = unreal_array_clear(value, (fprop_array_t *)prop); break;
    case UNREAL_PROP_KIND_SET:   cleared = unreal_set_clear(value, (fprop_set_t *)prop);     break;
    case UNREAL_PROP_KIND_MAP:   cleared = unreal_map_clear(value, (fprop_map_t *)prop);     break;
    default: break;
  }

  if (!cleared) {
    return luaL_error(state, "this reflected value cannot be cleared");
  }
  return 0;
}

static int
lua_unreal_value_add_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};

  int         result_count = 0;
  tmp_arena_t tmp          = scratch_begin(NULL);
  {
    unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
    switch (kind) {
      case UNREAL_PROP_KIND_ARRAY: {
        fprop_array_t *array_prop = (fprop_array_t *)prop;
        void          *element    = NULL;
        bool           ok         = array_prop->inner && lua_unreal_temporary_from_lua(state, 2, array_prop->inner, owner, tmp.arena, &element, error, sizeof(error));
        int32_t        index      = ok ? unreal_array_add(value, array_prop, element) : -1;
        if (element) {
          unreal_fprop_destroy_value(array_prop->inner, element);
        }

        if (index >= 0) {
          lua_pushinteger(state, (lua_Integer)index + 1);
          result_count = 1;
        } else {
          result_count = luaL_error(state, "could not add array element: %s", error[0] ? error : "mutation failed");
        }
        break;
      }

      case UNREAL_PROP_KIND_SET: {
        fprop_set_t *set_prop = (fprop_set_t *)prop;
        void        *element  = NULL;
        bool         ok       = AS_BOOL(set_prop->elem_prop);

        ok = ok && lua_unreal_temporary_from_lua(state, 2, set_prop->elem_prop, owner, tmp.arena, &element, error, sizeof(error));
        if (ok) {
          unreal_set_add(value, set_prop, element);
          ok = unreal_set_contains(value, set_prop, element);
        }

        if (element) {
          unreal_fprop_destroy_value(set_prop->elem_prop, element);
        }

        if (ok) {
          lua_pushboolean(state, true);
          result_count = 1;
        } else {
          result_count = luaL_error(state, "could not add set element: %s", error[0] ? error : "mutation failed");
        }
        break;
      }

      case UNREAL_PROP_KIND_MAP: {
        fprop_map_t *map_prop = (fprop_map_t *)prop;
        void        *key      = NULL;
        void        *val      = NULL;
        bool         ok       = map_prop->key_prop && map_prop->val_prop &&
                                lua_unreal_temporary_from_lua(state, 2, map_prop->key_prop, owner, tmp.arena, &key, error, sizeof(error)) &&
                                lua_unreal_temporary_from_lua(state, 3, map_prop->val_prop, owner, tmp.arena, &val, error, sizeof(error));
        if (ok) {
          unreal_map_add(value, map_prop, key, val);
          ok = unreal_map_find_value_ptr(value, map_prop, key) != NULL;
        }

        if (val) {
          unreal_fprop_destroy_value(map_prop->val_prop, val);
        }

        if (key) {
          unreal_fprop_destroy_value(map_prop->key_prop, key);
        }

        if (ok) {
          lua_pushboolean(state, true);
          result_count = 1;
        } else {
          result_count = luaL_error(state, "could not add map pair: %s", error[0] ? error : "mutation failed");
        }
        break;
      }

      default: {
        result_count = luaL_error(state, "this reflected value does not support Add");
        break;
      }
    }
  }
  scratch_end(tmp);
  return result_count;
}

static int
lua_unreal_value_insert_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner) ||
      unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_ARRAY) {
    return luaL_error(state, "Insert requires a live TArray");
  }

  fprop_array_t *array_prop = (fprop_array_t *)prop;
  lua_Integer    lua_index  = luaL_checkinteger(state, 2);
  int32_t        count      = unreal_array_num(value, array_prop);
  if (lua_index < 1 || lua_index > (lua_Integer)count + 1 || !array_prop->inner) {
    return luaL_error(state, "array insertion index is out of range");
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};
  bool        ok  = false;
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    void *element = NULL;

    ok = lua_unreal_temporary_from_lua(state, 3, array_prop->inner, owner, tmp.arena, &element, error, sizeof(error));
    if (ok) {
      unreal_array_insert(value, array_prop, (int32_t)lua_index - 1, element);
      ok = unreal_array_num(value, array_prop) == count + 1;
      unreal_fprop_destroy_value(array_prop->inner, element);
    }
  }
  scratch_end(tmp);

  if (!ok) {
    return luaL_error(state, "could not insert array element: %s", error[0] ? error : "mutation failed");
  }
  return 0;
}

static int
lua_unreal_value_contains_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  fprop_t *element_prop = NULL;
  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_ARRAY) {
    element_prop = ((fprop_array_t *)prop)->inner;
  } else if (kind == UNREAL_PROP_KIND_SET) {
    element_prop = ((fprop_set_t *)prop)->elem_prop;
  } else if (kind == UNREAL_PROP_KIND_MAP) {
    element_prop = ((fprop_map_t *)prop)->key_prop;
  }

  if (!element_prop) {
    return luaL_error(state, "this reflected value does not support Contains");
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};
  bool        converted = false;
  bool        found     = false;
  tmp_arena_t tmp       = scratch_begin(NULL);
  {
    void  *element = NULL;

    converted = lua_unreal_temporary_from_lua(state, 2, element_prop, owner, tmp.arena, &element, error, sizeof(error));
    if (converted && kind == UNREAL_PROP_KIND_ARRAY) {
      fprop_array_t *array_prop = (fprop_array_t *)prop;
      int32_t count = unreal_array_num(value, array_prop);
      for (int32_t i = 0; i < count && !found; ++i) {
        void *candidate = unreal_array_get(value, array_prop, i);
        found = candidate && unreal_fprop_single_values_identical(element_prop, candidate, element, 0);
      }
    } else if (converted && kind == UNREAL_PROP_KIND_SET) {
      found = unreal_set_contains(value, (fprop_set_t *)prop, element);
    } else if (converted && kind == UNREAL_PROP_KIND_MAP) {
      found = unreal_map_find_value_ptr(value, (fprop_map_t *)prop, element) != NULL;
    }

    if (element) {
      unreal_fprop_destroy_value(element_prop, element);
    }
  }
  scratch_end(tmp);

  if (!converted) {
    return luaL_error(state, "could not convert the lookup value: %s", error[0] ? error : "conversion failed");
  }

  lua_pushboolean(state, found);
  return 1;
}

static int
lua_unreal_value_find_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;

  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "Find requires a live FString or TMap");
  }

  if (unreal_fprop_get_kind(prop) == UNREAL_PROP_KIND_STRING) {
    return lua_unreal_fstring_find_lua(state);
  }

  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_MAP) {
    return luaL_error(state, "Find requires a live FString or TMap");
  }

  fprop_map_t *map_prop = (fprop_map_t *)prop;
  if (!map_prop->key_prop || !map_prop->val_prop) {
    return luaL_error(state, "TMap metadata is incomplete");
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};
  int         result = 0;
  bool        converted = false;
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    void *key = NULL;
    converted = lua_unreal_temporary_from_lua(state, 2, map_prop->key_prop, owner, tmp.arena, &key, error, sizeof(error));
    void *found = converted ? unreal_map_find_value_ptr(value, map_prop, key) : NULL;
    if (found) {
      result = lua_unreal_map_value_push(state, map_prop, found, key, owner, 1);
    }

    if (key) {
      unreal_fprop_destroy_value(map_prop->key_prop, key);
    }
  }
  scratch_end(tmp);

  if (!converted) {
    return luaL_error(state, "could not convert the map key: %s", error[0] ? error : "conversion failed");
  }

  if (!result) {
    return luaL_error(state, "the key was not found in the TMap");
  }
  return result;
}

static int
lua_unreal_value_remove_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t   *prop  = NULL;
  void      *value = NULL;
  uobject_t *owner = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, &owner)) {
    return luaL_error(state, "the reflected value is no longer valid");
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_ARRAY) {
    fprop_array_t *array_prop = (fprop_array_t *)prop;
    lua_Integer lua_index = luaL_checkinteger(state, 2);
    int32_t count = unreal_array_num(value, array_prop);
    if (lua_index < 1 || lua_index > count) {
      return luaL_error(state, "array removal index is out of range");
    }
    unreal_array_remove(value, array_prop, (int32_t)lua_index - 1);
    lua_pushboolean(state, unreal_array_num(value, array_prop) == count - 1);
    return 1;
  }

  fprop_t *element_prop = NULL;
  if (kind == UNREAL_PROP_KIND_SET) {
    element_prop = ((fprop_set_t *)prop)->elem_prop;
  } else if (kind == UNREAL_PROP_KIND_MAP) {
    element_prop = ((fprop_map_t *)prop)->key_prop;
  }

  if (!element_prop) {
    return luaL_error(state, "this reflected value does not support Remove");
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};
  bool        converted = false;
  bool        removed   = false;
  tmp_arena_t tmp       = scratch_begin(NULL);
  {
    void *element = NULL;

    converted = lua_unreal_temporary_from_lua(state, 2, element_prop, owner, tmp.arena, &element, error, sizeof(error));
    if (converted && kind == UNREAL_PROP_KIND_SET) {
      removed = unreal_set_remove(value, (fprop_set_t *)prop, element);
    } else if (converted) {
      removed = unreal_map_remove(value, (fprop_map_t *)prop, element);
    }

    if (element) {
      unreal_fprop_destroy_value(element_prop, element);
    }
  }
  scratch_end(tmp);

  if (!converted) {
    return luaL_error(state, "could not convert the removal value: %s", error[0] ? error : "conversion failed");
  }

  lua_pushboolean(state, removed);
  return 1;
}

static int
lua_unreal_value_tostring(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  fprop_t *prop  = NULL;
  void    *value = NULL;
  if (!lua_unreal_value_resolve(state, 1, &prop, &value, NULL)) {
    lua_pushliteral(state, "<stale Unreal value>");
    return 1;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_STRING || kind == UNREAL_PROP_KIND_TEXT || lua_unreal_kind_is_soft_object(kind)) {
    return lua_unreal_value_to_string_lua(state);
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t type    = unreal_fprop_push_type_name(prop, tmp.arena);
    str_t summary = unreal_fprop_push_value_summary(prop, value, prop->elem_size, tmp.arena);
    str_t text    = str_push_fmt(tmp.arena, "%.*s: %.*s", STR_ARG(type), STR_ARG(summary));
    lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
  }
  scratch_end(tmp);
  return 1;
}

static int
lua_unreal_object_is_valid_lua(lua_State *state)
{
  lua_unreal_object_t *userdata = lua_unreal_object_test(state, 1);
  lua_unreal_require_access(state, userdata ? userdata->context : NULL);

  lua_pushboolean(state, lua_unreal_object_resolve(userdata) != NULL);
  return 1;
}

static uobject_t *
lua_unreal_object_check_live(lua_State *state, int idx)
{
  lua_unreal_object_t *userdata = (lua_unreal_object_t *)luaL_checkudata(state, idx, LUA_UNREAL_OBJECT_MT);
  lua_unreal_require_access(state, userdata->context);

  uobject_t *object = lua_unreal_object_resolve(userdata);
  if (!object) {
    luaL_error(state, "the UObject is no longer valid");
  }
  return object;
}

static int
lua_unreal_object_get_full_name_lua(lua_State *state)
{
  uobject_t  *object = lua_unreal_object_check_live(state, 1);
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str_t path       = unreal_uobject_push_ue_path_name(object, tmp.arena);
    str_t class_name = object->cls ? unreal_fname_to_str(object->cls->name, tmp.arena) : STR_NULL;
    str_t full_name  = str_is_empty(class_name) ? path : str_push_fmt(tmp.arena, "%.*s %.*s", STR_ARG(class_name), STR_ARG(path));
    lua_pushlstring(state, (const char *)full_name.data, (size_t)full_name.len);
  }
  scratch_end(tmp);
  return 1;
}

static int
lua_unreal_object_get_fname_lua(lua_State *state)
{
  return lua_unreal_fname_push(state, lua_unreal_object_check_live(state, 1)->name);
}

static int
lua_unreal_object_get_address_lua(lua_State *state)
{
  lua_pushinteger(state, (lua_Integer)(uintptr_t)lua_unreal_object_check_live(state, 1));
  return 1;
}

static int
lua_unreal_object_get_class_lua(lua_State *state)
{
  return lua_unreal_object_push(state, (uobject_t *)lua_unreal_object_check_live(state, 1)->cls);
}

static int
lua_unreal_object_get_outer_lua(lua_State *state)
{
  return lua_unreal_object_push(state, lua_unreal_object_check_live(state, 1)->outer);
}

static int
lua_unreal_object_is_class_lua(lua_State *state)
{
  uobject_t *object = lua_unreal_object_check_live(state, 1);
  lua_pushboolean(state, unreal_uobject_is_a(object, globals.unreal.core_class));
  return 1;
}

static int
lua_unreal_object_is_a_lua(lua_State *state)
{
  uobject_t *object = lua_unreal_object_check_live(state, 1);
  uclass_t  *cls    = NULL;

  if (lua_type(state, 2) == LUA_TSTRING) {
    size_t      len  = 0;
    const char *text = lua_tolstring(state, 2, &len);
    str_t       name = str_make((uint8_t *)text, (uint64_t)len);
    cls = unreal_uobject_find_class_by_full_name(name);
    if (!cls) {
      cls = unreal_uclass_find(name, true, true);
    }
  } else {
    uobject_t *class_object = lua_unreal_object_get(state, 2);
    if (class_object && unreal_uobject_is_a(class_object, globals.unreal.core_class)) {
      cls = (uclass_t *)class_object;
    }
  }

  if (!cls) {
    return luaL_argerror(state, 2, "expected a live UClass or valid class name");
  }
  lua_pushboolean(state, unreal_uobject_is_a(object, cls));
  return 1;
}

static int
lua_unreal_object_has_all_flags_lua(lua_State *state)
{
  uobject_t    *object = lua_unreal_object_check_live(state, 1);
  eobj_flags_t flags   = (eobj_flags_t)lua_unreal_check_u32(state, 2, "flags must fit in an unsigned 32-bit integer");
  lua_pushboolean(state, (object->obj_flags & flags) == flags);
  return 1;
}

static int
lua_unreal_object_has_any_flags_lua(lua_State *state)
{
  uobject_t    *object = lua_unreal_object_check_live(state, 1);
  eobj_flags_t flags   = (eobj_flags_t)lua_unreal_check_u32(state, 2, "flags must fit in an unsigned 32-bit integer");
  lua_pushboolean(state, (object->obj_flags & flags) != 0);
  return 1;
}

static int
lua_unreal_object_has_any_internal_flags_lua(lua_State *state)
{
  uobject_t      *object = lua_unreal_object_check_live(state, 1);
  uint32_t        flags  = lua_unreal_check_u32(state, 2, "internal flags must fit in an unsigned 32-bit integer");
  fuobject_item_t *item  = unreal_uobject_array_get_item(object->internal_idx);
  bool valid = item && unreal_uobject_array_item_is_valid(item) && item->obj == object;
  if (!valid) {
    return luaL_error(state, "the UObject array entry is no longer valid");
  }
  lua_pushboolean(state, ((uint32_t)item->flags & flags) != 0);
  return 1;
}

static int
lua_unreal_object_type_lua(lua_State *state)
{
  uobject_t  *object = lua_unreal_object_check_live(state, 1);
  const char *type   = "UObject";
  if (globals.unreal.data_table && unreal_uobject_is_a(object, globals.unreal.data_table)) {
    type = "UDataTable";
  } else if (unreal_uobject_is_a(object, globals.unreal.core_func)) {
    type = "UFunction";
  } else if (unreal_uobject_is_a(object, globals.unreal.core_class)) {
    type = "UClass";
  } else if (unreal_uobject_is_a(object, globals.unreal.core_scriptstruct)) {
    type = "UScriptStruct";
  } else if (unreal_uobject_is_a(object, globals.unreal.core_enum)) {
    type = "UEnum";
  }
  lua_pushstring(state, type);
  return 1;
}

static int
lua_unreal_object_equal_lua(lua_State *state)
{
  lua_unreal_object_t *left  = lua_unreal_object_test(state, 1);
  lua_unreal_object_t *right = lua_unreal_object_test(state, 2);
  bool equal = left && right && left->weak.object_idx == right->weak.object_idx && left->weak.object_serial == right->weak.object_serial;
  lua_pushboolean(state, equal);
  return 1;
}

static int
lua_unreal_object_tostring(lua_State *state)
{
  lua_unreal_object_t *userdata = lua_unreal_object_test(state, 1);
  lua_unreal_require_access(state, userdata ? userdata->context : NULL);

  uobject_t *object = lua_unreal_object_resolve(userdata);
  if (!object) {
    lua_pushliteral(state, "<stale UObject>");
    return 1;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t name = unreal_uobject_push_full_name(object, tmp.arena);
    lua_pushlstring(state, (const char *)name.data, (size_t)name.len);
  }
  scratch_end(tmp);
  return 1;
}

static udata_table_t *
lua_unreal_data_table_check(lua_State *state, int idx)
{
  uobject_t *object = lua_unreal_object_check_live(state, idx);
  if (!globals.unreal.data_table || !unreal_uobject_is_a(object, globals.unreal.data_table)) {
    luaL_argerror(state, idx, "expected a UDataTable");
  }
  return (udata_table_t *)object;
}

static uscript_struct_t *
lua_unreal_data_table_row_struct_check(lua_State *state, udata_table_t *table)
{
  uscript_struct_t *row_struct = table ? table->row_struct : NULL;
  if (!row_struct || !globals.unreal.core_scriptstruct || !unreal_uobject_is_valid((uobject_t *)row_struct) ||
      !unreal_uobject_is_a((uobject_t *)row_struct, globals.unreal.core_scriptstruct) || row_struct->props_size < 0) {
    luaL_error(state, "the UDataTable has no valid row structure");
  }
  return row_struct;
}

static fname_t
lua_unreal_data_table_row_name_check(lua_State *state, int idx)
{
  lua_unreal_fname_t *name_value = lua_unreal_fname_test(state, idx);
  fname_t             name       = name_value ? name_value->value : (fname_t){0};
  if (!name_value) {
    size_t      len   = 0;
    const char *text  = luaL_checklstring(state, idx, &len);
    str_t       value = str_make((uint8_t *)text, (uint64_t)len);
    if (str_is_empty(value) || lua_unreal_string_has_embedded_null(value)) {
      luaL_argerror(state, idx, "row name cannot be empty or contain null bytes");
    }
    name = unreal_fname_from_str(value, FNAME_FIND_OR_ADD);
  }

  if (unreal_fname_is_none(name)) {
    luaL_argerror(state, idx, "row name cannot be None");
  }
  return name;
}

static int
lua_unreal_data_table_row_push(lua_State *state, udata_table_t *table, fname_t row_name)
{
  fweak_object_ptr_t weak = {0};
  if (!table || !unreal_fweak_object_from_object((uobject_t *)table, &weak) || !unreal_udata_table_find_row(table, row_name)) {
    lua_pushnil(state);
    return 1;
  }

  lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)lua_newuserdatauv(state, sizeof(*row), 0);
  row->context  = lua_unreal_context_from_state(state);
  row->table    = weak;
  row->row_name = row_name;
  luaL_setmetatable(state, LUA_UNREAL_DATA_TABLE_ROW_MT);
  return 1;
}

static int
lua_unreal_data_table_row_is_valid_lua(lua_State *state)
{
  lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)luaL_checkudata(state, 1, LUA_UNREAL_DATA_TABLE_ROW_MT);
  lua_unreal_require_access(state, row->context);
  lua_pushboolean(state, lua_unreal_data_table_row_resolve(row, NULL, NULL, NULL));
  return 1;
}

static int
lua_unreal_data_table_row_type_lua(lua_State *state)
{
  lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)luaL_checkudata(state, 1, LUA_UNREAL_DATA_TABLE_ROW_MT);
  lua_unreal_require_access(state, row->context);
  if (!lua_unreal_data_table_row_resolve(row, NULL, NULL, NULL)) {
    return luaL_error(state, "the UDataTable row is no longer valid");
  }
  lua_pushliteral(state, "UScriptStruct");
  return 1;
}

static int
lua_unreal_data_table_row_tostring_lua(lua_State *state)
{
  lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)luaL_checkudata(state, 1, LUA_UNREAL_DATA_TABLE_ROW_MT);
  lua_unreal_require_access(state, row->context);
  if (!lua_unreal_data_table_row_resolve(row, NULL, NULL, NULL)) {
    lua_pushliteral(state, "<stale UDataTable row>");
    return 1;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t name = unreal_fname_to_str(row->row_name, tmp.arena);
    lua_pushlstring(state, (const char *)name.data, (size_t)name.len);
  }
  scratch_end(tmp);
  return 1;
}

static int
lua_unreal_data_table_row_index_lua(lua_State *state)
{
  lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)luaL_checkudata(state, 1, LUA_UNREAL_DATA_TABLE_ROW_MT);
  lua_unreal_require_access(state, row->context);

  size_t            key_len    = 0;
  const char       *key        = luaL_checklstring(state, 2, &key_len);
  udata_table_t    *table      = NULL;
  uscript_struct_t *row_struct = NULL;
  uint8_t          *value      = NULL;
  if (lua_unreal_data_table_row_resolve(row, &table, &row_struct, &value)) {
    fprop_t *field = unreal_ustruct_find_prop((ustruct_t *)row_struct, str_make((uint8_t *)key, (uint64_t)key_len));
    if (field) {
      if (!lua_unreal_property_fits(field, (uint64_t)row_struct->props_size)) {
        return luaL_error(state, "row field '%s' has an unsupported reflected layout", key);
      }

      void *field_value = unreal_fprop_value_in_container(field, value, 0);
      if (!field_value || !lua_unreal_prop_push_selected(state, field, field_value, (uobject_t *)table, 1, true, LUA_UNREAL_SELECTOR_FIELD, 0, false)) {
        return luaL_error(state, "row field '%s' has an unsupported reflected type", key);
      }
      return 1;
    }
  }

  if (lua_getmetatable(state, 1)) {
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);
    lua_remove(state, -2);
    if (!lua_isnil(state, -1)) {
      return 1;
    }
    lua_pop(state, 1);
  }

  if (!value) {
    return luaL_error(state, "the UDataTable row is no longer valid");
  }
  lua_pushnil(state);
  return 1;
}

static int
lua_unreal_data_table_row_newindex_lua(lua_State *state)
{
  lua_unreal_data_table_row_t *row = (lua_unreal_data_table_row_t *)luaL_checkudata(state, 1, LUA_UNREAL_DATA_TABLE_ROW_MT);
  lua_unreal_require_access(state, row->context);

  size_t            key_len    = 0;
  const char       *key        = luaL_checklstring(state, 2, &key_len);
  udata_table_t    *table      = NULL;
  uscript_struct_t *row_struct = NULL;
  uint8_t          *value      = NULL;

  if (!lua_unreal_data_table_row_resolve(row, &table, &row_struct, &value)) {
    return luaL_error(state, "the UDataTable row is no longer valid");
  }

  fprop_t *field = unreal_ustruct_find_prop((ustruct_t *)row_struct, str_make((uint8_t *)key, (uint64_t)key_len));
  if (!field || !lua_unreal_property_fits(field, (uint64_t)row_struct->props_size)) {
    return luaL_error(state, "unknown or unsupported row field '%s'", key);
  }

  void *dst = unreal_fprop_value_in_container(field, value, 0);
  char  error[LUA_UNREAL_ERROR_CAP] = {0};
  if (!dst || !lua_unreal_write_transactional(state, 3, field, dst, (uobject_t *)table, error, sizeof(error))) {
    return luaL_error(state, "could not assign row field '%s': %s", key, error[0] ? error : "conversion failed");
  }
  return 0;
}

static int
lua_unreal_data_table_get_row_struct_lua(lua_State *state)
{
  udata_table_t    *table      = lua_unreal_data_table_check(state, 1);
  uscript_struct_t *row_struct = table->row_struct;
  if (!row_struct || !unreal_uobject_is_valid((uobject_t *)row_struct)) {
    return lua_unreal_object_push(state, NULL);
  }
  return lua_unreal_object_push(state, (uobject_t *)row_struct);
}

static int
lua_unreal_data_table_get_row_map_lua(lua_State *state)
{
  udata_table_t          *table = lua_unreal_data_table_check(state, 1);
  tmap_fname_uint8ptr_t  *map   = unreal_udata_table_get_row_map(table);
  int32_t                 count = map ? TMAP_NUM(map) : 0;
  lua_createtable(state, 0, MAX_VAL(count, 0));

  if (map) {
    TMAP_FOR_EACH_INDEX(map, idx) {
      fname_t name = TMAP_KEY_AT(map, idx);
      if (!TMAP_VALUE_AT(map, idx)) {
        continue;
      }

      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t text = unreal_fname_to_str(name, tmp.arena);
        lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
      }
      scratch_end(tmp);

      lua_unreal_data_table_row_push(state, table, name);
      lua_rawset(state, -3);
    }
  }
  return 1;
}

static int
lua_unreal_data_table_find_row_lua(lua_State *state)
{
  udata_table_t *table = lua_unreal_data_table_check(state, 1);
  fname_t        name  = lua_unreal_data_table_row_name_check(state, 2);
  return lua_unreal_data_table_row_push(state, table, name);
}

static int
lua_unreal_data_table_add_row_lua(lua_State *state)
{
  udata_table_t    *table      = lua_unreal_data_table_check(state, 1);
  fname_t           name       = lua_unreal_data_table_row_name_check(state, 2);
  uscript_struct_t *row_struct = lua_unreal_data_table_row_struct_check(state, table);
  int               value_idx  = lua_absindex(state, 3);

  const void *source = NULL;
  lua_unreal_data_table_row_t *source_row = (lua_unreal_data_table_row_t *)luaL_testudata(state, value_idx, LUA_UNREAL_DATA_TABLE_ROW_MT);
  if (source_row) {
    udata_table_t    *source_table      = NULL;
    uscript_struct_t *source_row_struct = NULL;
    uint8_t          *source_value      = NULL;
    if (!lua_unreal_data_table_row_resolve(source_row, &source_table, &source_row_struct, &source_value) || source_row_struct != row_struct) {
      return luaL_argerror(state, 3, "expected a live row with the same row structure");
    }

    if (source_table == table && source_row->row_name.cmp_idx == name.cmp_idx && source_row->row_name.num == name.num) {
      return 0;
    }
    source = source_value;
  } else {
    fprop_t *source_prop  = NULL;
    void    *source_value = NULL;
    if (lua_unreal_value_resolve(state, value_idx, &source_prop, &source_value, NULL)) {
      bool compatible = unreal_fprop_get_kind(source_prop) == UNREAL_PROP_KIND_STRUCT && ((fprop_struct_t *)source_prop)->script_struct == row_struct;
      if (!compatible) {
        return luaL_argerror(state, 3, "expected a compatible UScriptStruct value");
      }
      source = source_value;
    }
  }

  char        error[LUA_UNREAL_ERROR_CAP] = {0};
  bool        added = false;
  tmp_arena_t tmp   = scratch_begin(NULL);
  {
    void *temporary   = NULL;
    bool  initialized = false;
    if (!source && lua_type(state, value_idx) == LUA_TTABLE) {
      int32_t alignment = MAX_VAL(row_struct->min_alignment, 1);
      if (alignment <= LUA_UNREAL_MAX_ALIGNMENT) {
        temporary = arena_push_zero_aligned(tmp.arena, MAX_VAL((uint64_t)row_struct->props_size, 1ULL), (uint64_t)alignment);
      }

      initialized = temporary && unreal_ustruct_initialize_struct((ustruct_t *)row_struct, temporary, 1);
      if (!temporary) {
        lua_unreal_error_set(error, sizeof(error), "could not allocate temporary row storage");
      } else if (!initialized) {
        lua_unreal_error_set(error, sizeof(error), "could not initialize temporary row storage");
      } else if (!lua_unreal_write_struct_table(state, value_idx, (ustruct_t *)row_struct, (uint64_t)row_struct->props_size, temporary, (uobject_t *)table, error, sizeof(error))) {
        /* the field writer provides the error */
      } else {
        source = temporary;
      }
    } else if (!source) {
      lua_unreal_error_set(error, sizeof(error), "expected a table or compatible UScriptStruct value");
    }

    if (source) {
      unreal_udata_table_add_row(table, name, (const ftable_row_base_t *)source);
      added = unreal_udata_table_find_row(table, name) != NULL;
      if (!added) {
        lua_unreal_error_set(error, sizeof(error), "the row was not added");
      }
    }

    if (initialized) {
      unreal_ustruct_destroy_struct((ustruct_t *)row_struct, temporary, 1);
    }
  }
  scratch_end(tmp);

  if (!added) {
    return luaL_error(state, "could not add UDataTable row: %s", error[0] ? error : "conversion failed");
  }
  return 0;
}

static int
lua_unreal_data_table_remove_row_lua(lua_State *state)
{
  udata_table_t *table = lua_unreal_data_table_check(state, 1);
  fname_t        name  = lua_unreal_data_table_row_name_check(state, 2);
  unreal_udata_table_remove_row(table, name);
  return 0;
}

static int
lua_unreal_data_table_empty_lua(lua_State *state)
{
  unreal_udata_table_empty(lua_unreal_data_table_check(state, 1));
  return 0;
}

static int
lua_unreal_data_table_get_row_names_lua(lua_State *state)
{
  udata_table_t         *table = lua_unreal_data_table_check(state, 1);
  tmap_fname_uint8ptr_t *map   = unreal_udata_table_get_row_map(table);
  int32_t                count = map ? TMAP_NUM(map) : 0;
  lua_createtable(state, MAX_VAL(count, 0), 0);

  lua_Integer output_idx = 0;
  if (map) {
    TMAP_FOR_EACH_INDEX(map, idx) {
      if (!TMAP_VALUE_AT(map, idx)) {
        continue;
      }

      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t text = unreal_fname_to_str(TMAP_KEY_AT(map, idx), tmp.arena);
        lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
      }
      scratch_end(tmp);

      lua_rawseti(state, -2, ++output_idx);
    }
  }
  return 1;
}

static int
lua_unreal_data_table_get_all_rows_lua(lua_State *state)
{
  udata_table_t         *table = lua_unreal_data_table_check(state, 1);
  tmap_fname_uint8ptr_t *map   = unreal_udata_table_get_row_map(table);
  int32_t                count = map ? TMAP_NUM(map) : 0;
  lua_createtable(state, MAX_VAL(count, 0), 0);

  lua_Integer output_idx = 0;
  if (map) {
    TMAP_FOR_EACH_INDEX(map, idx) {
      fname_t name = TMAP_KEY_AT(map, idx);
      if (!TMAP_VALUE_AT(map, idx)) {
        continue;
      }

      lua_createtable(state, 0, 2);
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t text = unreal_fname_to_str(name, tmp.arena);
        lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
      }
      scratch_end(tmp);

      lua_setfield(state, -2, "Name");
      lua_unreal_data_table_row_push(state, table, name);
      lua_setfield(state, -2, "Data");
      lua_rawseti(state, -2, ++output_idx);
    }
  }
  return 1;
}

static int
lua_unreal_data_table_for_each_row_lua(lua_State *state)
{
  udata_table_t *table = lua_unreal_data_table_check(state, 1);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  int                    callback_idx = lua_absindex(state, 2);
  tmap_fname_uint8ptr_t *map          = unreal_udata_table_get_row_map(table);
  int32_t                count        = map ? TMAP_NUM(map) : 0;

  int         result = 0;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    fname_t *names = count > 0 ? ARENA_PUSH_ARRAY(tmp.arena, fname_t, count) : NULL;
    if (count <= 0 || names) {
      int32_t name_count = 0;
      if (map) {
        TMAP_FOR_EACH_INDEX(map, idx) {
          if (TMAP_VALUE_AT(map, idx) && name_count < count) {
            names[name_count++] = TMAP_KEY_AT(map, idx);
          }
        }
      }

      for (int32_t i = 0; i < name_count; ++i) {
        fname_t name = names[i];
        if (!unreal_udata_table_find_row(table, name)) {
          continue;
        }

        lua_pushvalue(state, callback_idx);
        str_t text = unreal_fname_to_str(name, tmp.arena);
        lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
        lua_unreal_data_table_row_push(state, table, name);

        if (lua_pcall(state, 2, 1, 0) != LUA_OK) {
          result = lua_error(state);
          break;
        }

        bool stop = lua_toboolean(state, -1) != 0;
        lua_pop(state, 1);
        if (stop) {
          break;
        }
      }
    } else {
      result = luaL_error(state, "could not allocate UDataTable row-name snapshot");
    }
  }
  scratch_end(tmp);

  return result;
}

static int
lua_unreal_object_get_world_lua(lua_State *state)
{
  uobject_t *object = lua_unreal_object_check_live(state, 1);
  uworld_t  *world  = object->vtable && object->vtable->get_world ? object->vtable->get_world(object) : NULL;
  return lua_unreal_object_push(state, (uobject_t *)world);
}

static bool
lua_unreal_object_is_struct_type(uobject_t *object)
{
  return object &&
         (unreal_uobject_is_a(object, globals.unreal.core_class)        ||
          unreal_uobject_is_a(object, globals.unreal.core_scriptstruct) ||
          unreal_uobject_is_a(object, globals.unreal.core_func));
}

static ustruct_t *
lua_unreal_ustruct_check(lua_State *state, int idx)
{
  uobject_t *object = lua_unreal_object_check_live(state, idx);
  if (!lua_unreal_object_is_struct_type(object)) {
    luaL_argerror(state, idx, "expected a live UStruct, UClass, UScriptStruct, or UFunction");
  }
  return (ustruct_t *)object;
}

static bool
lua_unreal_ffield_is_property(ffield_t *field)
{
  for (ffield_class_t *cls = field ? field->cls : NULL; cls; cls = cls->super_class) {
    if (unreal_fname_match_text(cls->name, STR_LIT("Property"), false, true)) {
      return true;
    }
  }
  return false;
}

static bool
lua_unreal_property_is_direct_member(ustruct_t *type, fprop_t *prop)
{
  if (!type || !prop) {
    return false;
  }

  for (ffield_t *field = type->child_props; field; field = field->next) {
    if (field == (ffield_t *)prop && lua_unreal_ffield_is_property(field)) {
      return true;
    }
  }
  return false;
}

static bool
lua_unreal_property_is_member(ustruct_t *type, fprop_t *prop)
{
  return lua_unreal_property_is_direct_member(type, prop) || lua_unreal_property_belongs_to_struct(type, prop);
}

static bool
lua_unreal_property_is_nested_member(fprop_t *parent, fprop_t *prop)
{
  if (!parent || !prop) {
    return false;
  }

  switch (unreal_fprop_get_kind(parent)) {
    case UNREAL_PROP_KIND_ARRAY: {
      return ((fprop_array_t *)parent)->inner == prop;
    }

    case UNREAL_PROP_KIND_SET: {
      return ((fprop_set_t *)parent)->elem_prop == prop;
    }

    case UNREAL_PROP_KIND_MAP: {
      fprop_map_t *map = (fprop_map_t *)parent;
      return map->key_prop == prop || map->val_prop == prop;
    }

    case UNREAL_PROP_KIND_ENUM: {
      return (fprop_t *)((fprop_enum_t *)parent)->underlying_prop == prop;
    }

    default: return false;
  }
}

static bool
lua_unreal_property_resolve_depth(lua_State *state, int idx, fprop_t **out_prop, int depth)
{
  idx = lua_absindex(state, idx);
  lua_unreal_property_t *property = (lua_unreal_property_t *)luaL_testudata(state, idx, LUA_UNREAL_PROPERTY_MT);
  if (!property || !property->prop || depth >= 64) {
    return false;
  }

  int anchor_type = lua_getiuservalue(state, idx, 1);
  int anchor_idx  = lua_gettop(state);
  bool valid      = false;

  if (anchor_type == LUA_TUSERDATA) {
    lua_unreal_object_t *owner_value = lua_unreal_object_test(state, anchor_idx);
    if (owner_value) {
      uobject_t *owner = lua_unreal_object_resolve(owner_value);
      valid = lua_unreal_object_is_struct_type(owner) && lua_unreal_property_is_member((ustruct_t *)owner, property->prop);
    } else {
      fprop_t *parent = NULL;
      valid = lua_unreal_property_resolve_depth(state, anchor_idx, &parent, depth + 1) && lua_unreal_property_is_nested_member(parent, property->prop);
    }
  }

  lua_pop(state, 1);
  if (valid && out_prop) {
    *out_prop = property->prop;
  }
  return valid;
}

static bool
lua_unreal_property_resolve(lua_State *state, int idx, fprop_t **out_prop)
{
  return lua_unreal_property_resolve_depth(state, idx, out_prop, 0);
}

static int
lua_unreal_property_push_root(lua_State *state, fprop_t *prop, ustruct_t *fallback_owner)
{
  lua_unreal_property_t *property = (lua_unreal_property_t *)lua_newuserdatauv(state, sizeof(*property), 1);
  property->context = lua_unreal_context_from_state(state);
  property->prop    = prop;
  luaL_setmetatable(state, LUA_UNREAL_PROPERTY_MT);

  if (!prop) {
    return 1;
  }

  ustruct_t *owner = fallback_owner;
  if (prop->owner.is_uobject) {
    uobject_t *candidate = prop->owner.container.obj;
    if (unreal_uobject_is_valid(candidate) && lua_unreal_object_is_struct_type(candidate) && lua_unreal_property_is_member((ustruct_t *)candidate, prop)) {
      owner = (ustruct_t *)candidate;
    }
  }

  lua_unreal_object_push(state, (uobject_t *)owner);
  lua_setiuservalue(state, -2, 1);
  return 1;
}

static int
lua_unreal_property_push_nested(lua_State *state, fprop_t *prop, int parent_idx)
{
  parent_idx = lua_absindex(state, parent_idx);
  lua_unreal_property_t *property = (lua_unreal_property_t *)lua_newuserdatauv(state, sizeof(*property), 1);
  property->context = lua_unreal_context_from_state(state);
  property->prop    = prop;
  luaL_setmetatable(state, LUA_UNREAL_PROPERTY_MT);
  lua_pushvalue(state, parent_idx);
  lua_setiuservalue(state, -2, 1);
  return 1;
}

static fprop_t *
lua_unreal_property_check_live(lua_State *state, int idx)
{
  lua_unreal_property_t *property = (lua_unreal_property_t *)luaL_checkudata(state, idx, LUA_UNREAL_PROPERTY_MT);
  lua_unreal_require_access(state, property->context);

  fprop_t *prop = NULL;
  if (!lua_unreal_property_resolve(state, idx, &prop)) {
    luaL_error(state, "the reflected property is no longer valid");
  }
  return prop;
}

static uobject_t *
lua_unreal_property_owner_object_depth(lua_State *state, int idx, int depth)
{
  if (depth >= 64) {
    return NULL;
  }

  idx = lua_absindex(state, idx);
  int anchor_type = lua_getiuservalue(state, idx, 1);
  int anchor_idx  = lua_gettop(state);
  uobject_t *owner = NULL;
  if (anchor_type == LUA_TUSERDATA) {
    lua_unreal_object_t *object = lua_unreal_object_test(state, anchor_idx);
    if (object) {
      owner = lua_unreal_object_resolve(object);
    } else if (luaL_testudata(state, anchor_idx, LUA_UNREAL_PROPERTY_MT)) {
      owner = lua_unreal_property_owner_object_depth(state, anchor_idx, depth + 1);
    }
  }
  lua_pop(state, 1);
  return owner;
}

static uobject_t *
lua_unreal_property_owner_object(lua_State *state, int idx)
{
  return lua_unreal_property_owner_object_depth(state, idx, 0);
}

static int
lua_unreal_property_is_valid_lua(lua_State *state)
{
  lua_unreal_property_t *property = (lua_unreal_property_t *)luaL_checkudata(state, 1, LUA_UNREAL_PROPERTY_MT);
  lua_unreal_require_access(state, property->context);
  lua_pushboolean(state, lua_unreal_property_resolve(state, 1, NULL));
  return 1;
}

static int
lua_unreal_property_get_fname_lua(lua_State *state)
{
  return lua_unreal_fname_push(state, lua_unreal_property_check_live(state, 1)->name);
}

static int
lua_unreal_property_get_full_name_lua(lua_State *state)
{
  fprop_t   *prop  = lua_unreal_property_check_live(state, 1);
  uobject_t *owner = lua_unreal_property_owner_object(state, 1);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t class_name = prop->cls ? unreal_fname_to_str(prop->cls->name, tmp.arena) : STR_LIT("Property");
    str_t prop_name  = unreal_fname_to_str(prop->name, tmp.arena);
    str_t owner_name = owner ? unreal_uobject_push_full_name(owner, tmp.arena) : STR_LIT("<unknown>");
    str_t full_name  = str_push_fmt(tmp.arena, "%.*s %.*s.%.*s", STR_ARG(class_name), STR_ARG(owner_name), STR_ARG(prop_name));
    lua_pushlstring(state, (const char *)full_name.data, (size_t)full_name.len);
  }
  scratch_end(tmp);
  return 1;
}

static bool
lua_unreal_field_class_resolve(lua_State *state, int idx, ffield_class_t **out_class)
{
  idx = lua_absindex(state, idx);
  lua_unreal_field_class_t *field_class = (lua_unreal_field_class_t *)luaL_testudata(state, idx, LUA_UNREAL_FIELD_CLASS_MT);
  if (!field_class || !field_class->cls) {
    return false;
  }

  if (lua_getiuservalue(state, idx, 1) != LUA_TUSERDATA) {
    lua_pop(state, 1);
    return false;
  }

  int      property_idx = lua_gettop(state);
  fprop_t *prop         = NULL;
  bool     valid        = lua_unreal_property_resolve(state, property_idx, &prop);
  bool     found        = false;
  for (ffield_class_t *cls = valid ? prop->cls : NULL; cls; cls = cls->super_class) {
    if (cls == field_class->cls) {
      found = true;
      break;
    }
  }
  lua_pop(state, 1);

  if (found && out_class) {
    *out_class = field_class->cls;
  }
  return found;
}

static int
lua_unreal_field_class_push(lua_State *state, ffield_class_t *cls, int property_idx)
{
  property_idx = lua_absindex(state, property_idx);
  lua_unreal_field_class_t *field_class = (lua_unreal_field_class_t *)lua_newuserdatauv(state, sizeof(*field_class), 1);
  field_class->context = lua_unreal_context_from_state(state);
  field_class->cls     = cls;
  luaL_setmetatable(state, LUA_UNREAL_FIELD_CLASS_MT);
  lua_pushvalue(state, property_idx);
  lua_setiuservalue(state, -2, 1);
  return 1;
}

static int
lua_unreal_property_get_class_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  return lua_unreal_field_class_push(state, prop->cls, 1);
}

static int
lua_unreal_property_is_a_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);

  lua_unreal_field_class_t *field_class = (lua_unreal_field_class_t *)luaL_testudata(state, 2, LUA_UNREAL_FIELD_CLASS_MT);
  if (field_class) {
    ffield_class_t *wanted = NULL;
    if (!lua_unreal_field_class_resolve(state, 2, &wanted)) {
      return luaL_argerror(state, 2, "the FieldClass is no longer valid");
    }

    for (ffield_class_t *cls = prop->cls; cls; cls = cls->super_class) {
      if (cls == wanted) {
        lua_pushboolean(state, true);
        return 1;
      }
    }
    lua_pushboolean(state, false);
    return 1;
  }

  str_t class_name = STR_NULL;
  if (!lua_unreal_string_or_fname(state, 2, &class_name) || str_is_empty(class_name)) {
    return luaL_argerror(state, 2, "expected PropertyTypes value, property class name, FName, or FieldClass");
  }

  for (ffield_class_t *cls = prop->cls; cls; cls = cls->super_class) {
    if (unreal_fname_match_text(cls->name, class_name, false, true)) {
      lua_pushboolean(state, true);
      return 1;
    }
  }
  lua_pushboolean(state, false);
  return 1;
}

static int
lua_unreal_property_get_offset_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_property_check_live(state, 1)->offset_internal);
  return 1;
}

static int
lua_unreal_property_get_array_dim_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_property_check_live(state, 1)->array_dim);
  return 1;
}

static int
lua_unreal_property_get_element_size_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_property_check_live(state, 1)->elem_size);
  return 1;
}

static int
lua_unreal_property_get_property_class_lua(lua_State *state)
{
  fprop_t            *prop = lua_unreal_property_check_live(state, 1);
  unreal_prop_kind_t  kind = unreal_fprop_get_kind(prop);
  uclass_t           *cls  = NULL;

  if (kind == UNREAL_PROP_KIND_OBJECT      || kind == UNREAL_PROP_KIND_CLASS       ||
      kind == UNREAL_PROP_KIND_WEAK_OBJECT || kind == UNREAL_PROP_KIND_LAZY_OBJECT ||
      kind == UNREAL_PROP_KIND_SOFT_OBJECT || kind == UNREAL_PROP_KIND_SOFT_CLASS) {
    cls = ((fprop_obj_base_t *)prop)->prop_class;
  } else if (kind == UNREAL_PROP_KIND_INTERFACE) {
    cls = ((fprop_iface_t *)prop)->iface_class;
  } else {
    return luaL_error(state, "GetPropertyClass requires an object or interface property");
  }
  return lua_unreal_object_push(state, (uobject_t *)cls);
}

static fprop_bool_t *
lua_unreal_bool_property_check(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_BOOL) {
    luaL_error(state, "this property is not a BoolProperty");
  }
  return (fprop_bool_t *)prop;
}

static int
lua_unreal_bool_property_get_byte_mask_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_bool_property_check(state)->byte_mask);
  return 1;
}

static int
lua_unreal_bool_property_get_byte_offset_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_bool_property_check(state)->byte_offset);
  return 1;
}

static int
lua_unreal_bool_property_get_field_mask_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_bool_property_check(state)->field_mask);
  return 1;
}

static int
lua_unreal_bool_property_get_field_size_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_bool_property_check(state)->field_size);
  return 1;
}

static int
lua_unreal_property_get_struct_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_STRUCT) {
    return luaL_error(state, "GetStruct requires a StructProperty");
  }
  return lua_unreal_object_push(state, (uobject_t *)((fprop_struct_t *)prop)->script_struct);
}

static int
lua_unreal_property_get_inner_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_ARRAY || !((fprop_array_t *)prop)->inner) {
    return luaL_error(state, "GetInner requires an ArrayProperty with valid inner metadata");
  }
  return lua_unreal_property_push_nested(state, ((fprop_array_t *)prop)->inner, 1);
}

static int
lua_unreal_property_get_element_property_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_SET || !((fprop_set_t *)prop)->elem_prop) {
    return luaL_error(state, "GetElementProperty requires a SetProperty with valid element metadata");
  }
  return lua_unreal_property_push_nested(state, ((fprop_set_t *)prop)->elem_prop, 1);
}

static int
lua_unreal_property_get_key_property_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_MAP || !((fprop_map_t *)prop)->key_prop) {
    return luaL_error(state, "GetKeyProperty requires a MapProperty with valid key metadata");
  }
  return lua_unreal_property_push_nested(state, ((fprop_map_t *)prop)->key_prop, 1);
}

static int
lua_unreal_property_get_value_property_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_MAP || !((fprop_map_t *)prop)->val_prop) {
    return luaL_error(state, "GetValueProperty requires a MapProperty with valid value metadata");
  }
  return lua_unreal_property_push_nested(state, ((fprop_map_t *)prop)->val_prop, 1);
}

static int
lua_unreal_property_get_underlying_property_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  fprop_t *underlying = unreal_fprop_get_kind(prop) == UNREAL_PROP_KIND_ENUM
                        ? (fprop_t *)((fprop_enum_t *)prop)->underlying_prop
                        : NULL;
  if (!underlying) {
    return luaL_error(state, "GetUnderlyingProperty requires an EnumProperty with valid underlying metadata");
  }
  return lua_unreal_property_push_nested(state, underlying, 1);
}

static int
lua_unreal_property_get_enum_lua(lua_State *state)
{
  fprop_t *prop = lua_unreal_property_check_live(state, 1);
  uenum_t *uenum = NULL;
  if (unreal_fprop_get_kind(prop) == UNREAL_PROP_KIND_ENUM) {
    uenum = ((fprop_enum_t *)prop)->uenum;
  } else if (unreal_fprop_get_kind(prop) == UNREAL_PROP_KIND_BYTE) {
    uenum = ((fprop_byte_t *)prop)->uenum;
  } else {
    return luaL_error(state, "GetEnum requires an EnumProperty or ByteProperty");
  }
  return lua_unreal_object_push(state, (uobject_t *)uenum);
}

static int
lua_unreal_property_type_lua(lua_State *state)
{
  (void)lua_unreal_property_check_live(state, 1);
  lua_pushliteral(state, "Property");
  return 1;
}

static int
lua_unreal_property_tostring_lua(lua_State *state)
{
  lua_unreal_property_t *property = (lua_unreal_property_t *)luaL_checkudata(state, 1, LUA_UNREAL_PROPERTY_MT);
  lua_unreal_require_access(state, property->context);
  if (!lua_unreal_property_resolve(state, 1, NULL)) {
    lua_pushliteral(state, "<stale Property>");
    return 1;
  }
  return lua_unreal_property_get_full_name_lua(state);
}

static int
lua_unreal_field_class_is_valid_lua(lua_State *state)
{
  lua_unreal_field_class_t *field_class = (lua_unreal_field_class_t *)luaL_checkudata(state, 1, LUA_UNREAL_FIELD_CLASS_MT);
  lua_unreal_require_access(state, field_class->context);
  lua_pushboolean(state, lua_unreal_field_class_resolve(state, 1, NULL));
  return 1;
}

static ffield_class_t *
lua_unreal_field_class_check_live(lua_State *state, int idx)
{
  lua_unreal_field_class_t *field_class = (lua_unreal_field_class_t *)luaL_checkudata(state, idx, LUA_UNREAL_FIELD_CLASS_MT);
  lua_unreal_require_access(state, field_class->context);
  ffield_class_t *cls = NULL;
  if (!lua_unreal_field_class_resolve(state, idx, &cls)) {
    luaL_error(state, "the FieldClass is no longer valid");
  }
  return cls;
}

static int
lua_unreal_field_class_get_fname_lua(lua_State *state)
{
  return lua_unreal_fname_push(state, lua_unreal_field_class_check_live(state, 1)->name);
}

static int
lua_unreal_field_class_type_lua(lua_State *state)
{
  (void)lua_unreal_field_class_check_live(state, 1);
  lua_pushliteral(state, "FieldClass");
  return 1;
}

static int
lua_unreal_field_class_tostring_lua(lua_State *state)
{
  ffield_class_t *cls = lua_unreal_field_class_check_live(state, 1);
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t name = unreal_fname_to_str(cls->name, tmp.arena);
    lua_pushlstring(state, (const char *)name.data, (size_t)name.len);
  }
  scratch_end(tmp);
  return 1;
}

static uobject_t *
lua_unreal_reflection_resolve(lua_unreal_reflection_t *reflection)
{
  return reflection ? unreal_fweak_object_resolve(reflection->object) : NULL;
}

static ustruct_t *
lua_unreal_reflection_get_type(uobject_t *object)
{
  return lua_unreal_object_is_struct_type(object) ? (ustruct_t *)object : (object ? (ustruct_t *)object->cls : NULL);
}

static int
lua_unreal_object_reflection_lua(lua_State *state)
{
  lua_unreal_object_t *object = (lua_unreal_object_t *)luaL_checkudata(state, 1, LUA_UNREAL_OBJECT_MT);
  (void)lua_unreal_object_check_live(state, 1);

  lua_unreal_reflection_t *reflection = (lua_unreal_reflection_t *)lua_newuserdatauv(state, sizeof(*reflection), 0);
  reflection->context = object->context;
  reflection->object  = object->weak;
  luaL_setmetatable(state, LUA_UNREAL_REFLECTION_MT);
  return 1;
}

static int
lua_unreal_reflection_is_valid_lua(lua_State *state)
{
  lua_unreal_reflection_t *reflection = (lua_unreal_reflection_t *)luaL_checkudata(state, 1, LUA_UNREAL_REFLECTION_MT);
  lua_unreal_require_access(state, reflection->context);
  lua_pushboolean(state, lua_unreal_reflection_get_type(lua_unreal_reflection_resolve(reflection)) != NULL);
  return 1;
}

static int
lua_unreal_reflection_get_property_lua(lua_State *state)
{
  lua_unreal_reflection_t *reflection = (lua_unreal_reflection_t *)luaL_checkudata(state, 1, LUA_UNREAL_REFLECTION_MT);
  lua_unreal_require_access(state, reflection->context);

  str_t name = STR_NULL;
  if (!lua_unreal_string_or_fname(state, 2, &name) || str_is_empty(name)) {
    return luaL_argerror(state, 2, "expected a non-empty property name or FName");
  }

  ustruct_t *type = lua_unreal_reflection_get_type(lua_unreal_reflection_resolve(reflection));
  if (!type) {
    return luaL_error(state, "the reflected UObject is no longer valid");
  }

  fprop_t *prop = unreal_ustruct_find_prop(type, name);
  return lua_unreal_property_push_root(state, prop, type);
}

static int
lua_unreal_reflection_type_lua(lua_State *state)
{
  lua_unreal_reflection_t *reflection = (lua_unreal_reflection_t *)luaL_checkudata(state, 1, LUA_UNREAL_REFLECTION_MT);
  lua_unreal_require_access(state, reflection->context);
  if (!lua_unreal_reflection_get_type(lua_unreal_reflection_resolve(reflection))) {
    return luaL_error(state, "the reflected UObject is no longer valid");
  }
  lua_pushliteral(state, "UObjectReflection");
  return 1;
}

static int
lua_unreal_ustruct_for_each_property_lua(lua_State *state)
{
  ustruct_t *type = lua_unreal_ustruct_check(state, 1);
  luaL_checktype(state, 2, LUA_TFUNCTION);
  int callback_idx = lua_absindex(state, 2);

  for (ffield_t *field = type->child_props; field;) {
    ffield_t *next = field->next;
    if (lua_unreal_ffield_is_property(field)) {
      lua_pushvalue(state, callback_idx);
      lua_unreal_property_push_root(state, (fprop_t *)field, type);
      if (lua_pcall(state, 1, 1, 0) != LUA_OK) {
        return lua_error(state);
      }

      bool stop = lua_toboolean(state, -1) != 0;
      lua_pop(state, 1);
      if (stop) {
        break;
      }
    }
    field = next;
  }
  return 0;
}

static int
lua_unreal_ustruct_get_super_lua(lua_State *state)
{
  return lua_unreal_object_push(state, (uobject_t *)lua_unreal_ustruct_check(state, 1)->super_struct);
}

static int
lua_unreal_ustruct_for_each_function_lua(lua_State *state)
{
  ustruct_t *type = lua_unreal_ustruct_check(state, 1);
  luaL_checktype(state, 2, LUA_TFUNCTION);
  int callback_idx = lua_absindex(state, 2);

  for (ufield_t *field = type->children; field; field = field->next) {
    uobject_t *object = (uobject_t *)field;
    if (!unreal_uobject_is_valid(object) || !unreal_uobject_is_a(object, globals.unreal.core_func)) {
      continue;
    }

    lua_pushvalue(state, callback_idx);
    if (!lua_unreal_object_push(state, object)) {
      lua_pop(state, 1);
      return luaL_error(state, "could not marshal a UFunction");
    }

    if (lua_pcall(state, 1, 1, 0) != LUA_OK) {
      return lua_error(state);
    }

    bool stop = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (stop) {
      break;
    }
  }
  return 0;
}

static uclass_t *
lua_unreal_uclass_check(lua_State *state, int idx)
{
  uobject_t *object = lua_unreal_object_check_live(state, idx);
  if (!unreal_uobject_is_a(object, globals.unreal.core_class)) {
    luaL_argerror(state, idx, "expected a live UClass");
  }
  return (uclass_t *)object;
}

static int
lua_unreal_uclass_get_cdo_lua(lua_State *state)
{
  return lua_unreal_object_push(state, lua_unreal_uclass_check(state, 1)->cdo);
}

static int
lua_unreal_uclass_is_child_of_lua(lua_State *state)
{
  uclass_t *child  = lua_unreal_uclass_check(state, 1);
  uclass_t *parent = lua_unreal_uclass_check(state, 2);
  bool      result = false;
  for (uclass_t *current = child; current; current = (uclass_t *)current->super_struct) {
    if (current == parent) {
      result = true;
      break;
    }
  }
  lua_pushboolean(state, result);
  return 1;
}

static ufunc_t *
lua_unreal_ufunction_check(lua_State *state, int idx)
{
  uobject_t *object = lua_unreal_object_check_live(state, idx);
  if (!unreal_uobject_is_a(object, globals.unreal.core_func)) {
    luaL_argerror(state, idx, "expected a live UFunction");
  }
  return (ufunc_t *)object;
}

static int
lua_unreal_ufunction_get_flags_lua(lua_State *state)
{
  lua_pushinteger(state, lua_unreal_ufunction_check(state, 1)->func_flags);
  return 1;
}

static int
lua_unreal_ufunction_set_flags_lua(lua_State *state)
{
  ufunc_t *function = lua_unreal_ufunction_check(state, 1);
  function->func_flags = lua_unreal_check_u32(state, 2, "function flags must fit in an unsigned 32-bit integer");
  return 0;
}

static uenum_t *
lua_unreal_uenum_check(lua_State *state, int idx)
{
  uobject_t *object = lua_unreal_object_check_live(state, idx);
  if (!unreal_uobject_is_a(object, globals.unreal.core_enum)) {
    luaL_argerror(state, idx, "expected a live UEnum");
  }

  uenum_t *uenum = (uenum_t *)object;
  if (uenum->names.num < 0 || uenum->names.max < uenum->names.num || (uenum->names.num > 0 && !uenum->names.data)) {
    luaL_error(state, "the UEnum name table is invalid");
  }
  return uenum;
}

static int
lua_unreal_uenum_get_name_by_value_lua(lua_State *state)
{
  uenum_t    *uenum = lua_unreal_uenum_check(state, 1);
  lua_Integer value = luaL_checkinteger(state, 2);
  for (int32_t i = 0; i < uenum->names.num; ++i) {
    if (uenum->names.data[i].value == (int64_t)value) {
      return lua_unreal_fname_push(state, uenum->names.data[i].key);
    }
  }
  lua_pushnil(state);
  return 1;
}

static int
lua_unreal_uenum_for_each_name_lua(lua_State *state)
{
  uenum_t *uenum = lua_unreal_uenum_check(state, 1);
  luaL_checktype(state, 2, LUA_TFUNCTION);
  int callback_idx = lua_absindex(state, 2);

  for (int32_t i = 0; i < uenum->names.num; ++i) {
    lua_pushvalue(state, callback_idx);
    lua_unreal_fname_push(state, uenum->names.data[i].key);
    lua_pushinteger(state, (lua_Integer)uenum->names.data[i].value);
    if (lua_pcall(state, 2, 1, 0) != LUA_OK) {
      return lua_error(state);
    }

    bool stop = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (stop) {
      break;
    }
  }
  return 0;
}

static int
lua_unreal_uenum_get_name_by_index_lua(lua_State *state)
{
  uenum_t    *uenum = lua_unreal_uenum_check(state, 1);
  lua_Integer index = luaL_checkinteger(state, 2);
  if (index < 0 || index >= uenum->names.num) {
    lua_pushnil(state);
    return 1;
  }

  lua_unreal_fname_push(state, uenum->names.data[index].key);
  lua_pushinteger(state, (lua_Integer)uenum->names.data[index].value);
  return 2;
}

static int
lua_unreal_uenum_edit_name_at_lua(lua_State *state)
{
  uenum_t    *uenum = lua_unreal_uenum_check(state, 1);
  lua_Integer index = luaL_checkinteger(state, 2);
  if (index < 0 || index >= uenum->names.num) {
    return luaL_argerror(state, 2, "enum index is out of range");
  }

  lua_unreal_fname_t *name = lua_unreal_fname_test(state, 3);
  if (name) {
    uenum->names.data[index].key = name->value;
  } else {
    size_t      len  = 0;
    const char *text = luaL_checklstring(state, 3, &len);
    uenum->names.data[index].key = unreal_fname_from_str(str_make((uint8_t *)text, (uint64_t)len), FNAME_FIND_OR_ADD);
  }
  return 0;
}

static int
lua_unreal_uenum_edit_value_at_lua(lua_State *state)
{
  uenum_t    *uenum = lua_unreal_uenum_check(state, 1);
  lua_Integer index = luaL_checkinteger(state, 2);
  if (index < 0 || index >= uenum->names.num) {
    return luaL_argerror(state, 2, "enum index is out of range");
  }
  uenum->names.data[index].value = (int64_t)luaL_checkinteger(state, 3);
  return 0;
}

static bool
lua_unreal_native_method_push(lua_State *state, uobject_t *object, str_t name)
{
  lua_CFunction function = NULL;
  if (str_equal(name, STR_LIT("GetWorld"), 0)) {
    function = lua_unreal_object_get_world_lua;
  } else if (str_equal(name, STR_LIT("CallFunction"), 0)) {
    function = lua_unreal_object_call_function_lua;
  }

  if (!function && globals.unreal.data_table && unreal_uobject_is_a(object, globals.unreal.data_table)) {
    if (str_equal(name, STR_LIT("GetRowStruct"), 0)) {
      function = lua_unreal_data_table_get_row_struct_lua;
    } else if (str_equal(name, STR_LIT("GetRowMap"), 0)) {
      function = lua_unreal_data_table_get_row_map_lua;
    } else if (str_equal(name, STR_LIT("FindRow"), 0)) {
      function = lua_unreal_data_table_find_row_lua;
    } else if (str_equal(name, STR_LIT("AddRow"), 0)) {
      function = lua_unreal_data_table_add_row_lua;
    } else if (str_equal(name, STR_LIT("RemoveRow"), 0)) {
      function = lua_unreal_data_table_remove_row_lua;
    } else if (str_equal(name, STR_LIT("EmptyTable"), 0)) {
      function = lua_unreal_data_table_empty_lua;
    } else if (str_equal(name, STR_LIT("GetRowNames"), 0)) {
      function = lua_unreal_data_table_get_row_names_lua;
    } else if (str_equal(name, STR_LIT("GetAllRows"), 0)) {
      function = lua_unreal_data_table_get_all_rows_lua;
    } else if (str_equal(name, STR_LIT("ForEachRow"), 0)) {
      function = lua_unreal_data_table_for_each_row_lua;
    }
  }

  if (!function && unreal_uobject_is_a(object, globals.unreal.core_func)) {
    if (str_equal(name, STR_LIT("GetFunctionFlags"), 0)) {
      function = lua_unreal_ufunction_get_flags_lua;
    } else if (str_equal(name, STR_LIT("SetFunctionFlags"), 0)) {
      function = lua_unreal_ufunction_set_flags_lua;
    }
  }

  if (!function && unreal_uobject_is_a(object, globals.unreal.core_enum)) {
    if (str_equal(name, STR_LIT("GetNameByValue"), 0)) {
      function = lua_unreal_uenum_get_name_by_value_lua;
    } else if (str_equal(name, STR_LIT("ForEachName"), 0)) {
      function = lua_unreal_uenum_for_each_name_lua;
    } else if (str_equal(name, STR_LIT("GetEnumNameByIndex"), 0)) {
      function = lua_unreal_uenum_get_name_by_index_lua;
    } else if (str_equal(name, STR_LIT("EditNameAt"), 0)) {
      function = lua_unreal_uenum_edit_name_at_lua;
    } else if (str_equal(name, STR_LIT("EditValueAt"), 0)) {
      function = lua_unreal_uenum_edit_value_at_lua;
    }
  }

  if (!function && unreal_uobject_is_a(object, globals.unreal.core_class)) {
    if (str_equal(name, STR_LIT("GetCDO"), 0)) {
      function = lua_unreal_uclass_get_cdo_lua;
    } else if (str_equal(name, STR_LIT("IsChildOf"), 0)) {
      function = lua_unreal_uclass_is_child_of_lua;
    }
  }

  if (!function && lua_unreal_object_is_struct_type(object)) {
    if (str_equal(name, STR_LIT("GetSuperStruct"), 0)) {
      function = lua_unreal_ustruct_get_super_lua;
    } else if (str_equal(name, STR_LIT("ForEachFunction"), 0)) {
      function = lua_unreal_ustruct_for_each_function_lua;
    } else if (str_equal(name, STR_LIT("ForEachProperty"), 0)) {
      function = lua_unreal_ustruct_for_each_property_lua;
    }
  }

  if (!function && str_equal(name, STR_LIT("Reflection"), 0)) {
    function = lua_unreal_object_reflection_lua;
  }

  if (!function) {
    return false;
  }
  lua_pushcfunction(state, function);
  return true;
}

static int
lua_unreal_object_len_lua(lua_State *state)
{
  uobject_t *object = lua_unreal_object_check_live(state, 1);
  if (!globals.unreal.data_table || !unreal_uobject_is_a(object, globals.unreal.data_table)) {
    return luaL_error(state, "only UDataTable objects have a length");
  }

  tmap_fname_uint8ptr_t *map = unreal_udata_table_get_row_map((udata_table_t *)object);
  lua_pushinteger(state, map ? TMAP_NUM(map) : 0);
  return 1;
}

static bool
lua_unreal_method_same_receiver(lua_State *state, int idx, fweak_object_ptr_t receiver)
{
  lua_unreal_object_t *object = lua_unreal_object_test(state, idx);
  return object && object->weak.object_idx == receiver.object_idx && object->weak.object_serial == receiver.object_serial;
}

static bool
lua_unreal_call_prop_supported(fprop_t *prop, uint64_t container_size, char *error, size_t error_cap)
{
  if (!prop || !lua_unreal_property_fits(prop, container_size)) {
    lua_unreal_error_set(error, error_cap, "invalid parameter layout");
    return false;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  if (kind == UNREAL_PROP_KIND_BOOL  || lua_unreal_kind_is_integer(kind) ||
      kind == UNREAL_PROP_KIND_FLOAT || kind == UNREAL_PROP_KIND_DOUBLE  ||
      kind == UNREAL_PROP_KIND_NAME  || kind == UNREAL_PROP_KIND_STRING  ||
      kind == UNREAL_PROP_KIND_TEXT  || lua_unreal_kind_is_object(kind)  || lua_unreal_kind_is_soft_object(kind) ||
      kind == UNREAL_PROP_KIND_INTERFACE) {
    return true;
  }

  if (kind == UNREAL_PROP_KIND_STRUCT) {
    fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
    ustruct_t      *type        = (ustruct_t *)struct_prop->script_struct;
    if (!type || type->props_size < 0 || (uint64_t)type->props_size > (uint64_t)prop->elem_size ||
        unreal_fname_match_text(type->name, STR_LIT("LatentActionInfo"), false, true)) {
      lua_unreal_error_set(error, error_cap, "unsupported or latent struct parameter");
      return false;
    }

    for (fprop_t *field = type->prop_link; field; field = field->prop_link_next) {
      if (!lua_unreal_call_prop_supported(field, (uint64_t)prop->elem_size, error, error_cap)) {
        return false;
      }
    }
    return true;
  }

  if (kind == UNREAL_PROP_KIND_ARRAY) {
    fprop_t *inner = ((fprop_array_t *)prop)->inner;
    return inner && lua_unreal_call_prop_supported(inner, unreal_fprop_complete_size(inner), error, error_cap);
  }

  if (kind == UNREAL_PROP_KIND_SET) {
    fprop_t *element = ((fprop_set_t *)prop)->elem_prop;
    return element && lua_unreal_call_prop_supported(element, unreal_fprop_complete_size(element), error, error_cap);
  }

  if (kind == UNREAL_PROP_KIND_MAP) {
    fprop_map_t *map = (fprop_map_t *)prop;
    return map->key_prop && map->val_prop &&
           lua_unreal_call_prop_supported(map->key_prop, unreal_fprop_complete_size(map->key_prop), error, error_cap) &&
           lua_unreal_call_prop_supported(map->val_prop, unreal_fprop_complete_size(map->val_prop), error, error_cap);
  }

  lua_unreal_error_set(error, error_cap, "unsupported function parameter type");
  return false;
}

static int
lua_unreal_call_reflected_function(lua_State *state, uobject_t *receiver, ufunc_t *function, int first_arg)
{
  uclass_t  *owner_class = function ? (uclass_t *)function->outer : NULL;

  if (!receiver                                                                 ||
      !function                                                                 ||
      !owner_class                                                              ||
      !unreal_uobject_is_a((uobject_t *)function, globals.unreal.core_func)     ||
      !unreal_uobject_is_valid((uobject_t *)owner_class)                        ||
      !unreal_uobject_is_a((uobject_t *)owner_class, globals.unreal.core_class) ||
      !unreal_uobject_is_a(receiver, owner_class)                               ||
      unreal_ustruct_find_func_fname((ustruct_t *)receiver->cls, function->name, false) != function) {
    return luaL_error(state, "the Unreal receiver or function is no longer valid");
  }

  char     error[LUA_UNREAL_ERROR_CAP] = {0};
  uint32_t reflected_count             = 0;
  int      input_count                 = 0;
  for (ffield_t *field = function->child_props; field; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (!(prop->prop_flags & CPF_PARM)) {
      continue;
    }

    reflected_count += 1;
    if (!lua_unreal_call_prop_supported(prop, function->params_size, error, sizeof(error))) {
      return luaL_error(state, "unsupported function schema: %s", error);
    }

    bool is_return     = (prop->prop_flags & CPF_RETURN_PARM) != 0;
    bool is_pure_output = (prop->prop_flags & CPF_OUT_PARM) != 0 &&
                          (prop->prop_flags & CPF_REFERENCE_PARM) == 0;
    if (!is_return && !is_pure_output) {
      input_count += 1;
    }
  }

  if (reflected_count != function->num_params) {
    return luaL_error(state, "reflected parameter count mismatch (%u != %u)", reflected_count, (uint32_t)function->num_params);
  }

  int supplied  = lua_gettop(state) - first_arg + 1;
  if (supplied != input_count) {
    return luaL_error(state, "expected %d arguments, got %d", input_count, supplied);
  }

  int32_t alignment = function->min_alignment;
  if (alignment < 16) {
    alignment = 16;
  }

  if (alignment > LUA_UNREAL_MAX_ALIGNMENT) {
    return luaL_error(state, "function parameter alignment %d is unsupported", alignment);
  }

  int         stack_base   = lua_gettop(state);
  int         result_count = 0;
  tmp_arena_t tmp          = scratch_begin(NULL);
  {
    uint64_t params_size = MAX_VAL((uint64_t)function->params_size, 1ULL);
    uint8_t *params = (uint8_t *)arena_push_zero_aligned(tmp.arena, params_size, (uint64_t)alignment);
    if (params) {
      uint32_t initialized_count  = 0;
      bool     params_initialized = true;
      for (ffield_t *field = function->child_props; field; field = field->next) {
        fprop_t *prop = (fprop_t *)field;
        if (prop->prop_flags & CPF_PARM) {
          void *value = unreal_fprop_value_in_container(prop, params, 0);
          if (!value || !unreal_fprop_initialize_value(prop, value)) {
            params_initialized = false;
            lua_unreal_error_set(error, sizeof(error), "could not initialize function parameters");
            break;
          }
          initialized_count += 1;
        }
      }

      int  arg_idx   = first_arg;
      bool converted = params_initialized;
      for (ffield_t *field = function->child_props; field && converted; field = field->next) {
        fprop_t *prop = (fprop_t *)field;
        bool is_return      = (prop->prop_flags & CPF_RETURN_PARM) != 0;
        bool is_pure_output = (prop->prop_flags & CPF_OUT_PARM) != 0 &&
                              (prop->prop_flags & CPF_REFERENCE_PARM) == 0;
        if (!(prop->prop_flags & CPF_PARM) || is_return || is_pure_output) {
          continue;
        }

        void *dst = unreal_fprop_value_in_container(prop, params, 0);
        converted = dst && lua_unreal_write_transactional(state, arg_idx, prop, dst, receiver, error, sizeof(error));
        arg_idx  += 1;
      }

      bool pushed = converted;
      if (converted) {
        lua_unreal_context_t *context = lua_unreal_context_from_state(state);
        bool called = context && context->process_event
                    ? context->process_event(context->user, receiver, function, function->params_size ? params : NULL)
                    : (unreal_process_event_observed(receiver, function, function->params_size ? params : NULL), true);
        if (!called) {
          lua_unreal_error_set(error, sizeof(error), "could not dispatch the Unreal function on the game thread");
          pushed = false;
        }

        for (int pass = 0; pass < 2 && pushed; ++pass) {
          for (ffield_t *field = function->child_props; field; field = field->next) {
            fprop_t *prop = (fprop_t *)field;
            if (!(prop->prop_flags & CPF_PARM)) {
              continue;
            }

            bool is_return = (prop->prop_flags & CPF_RETURN_PARM) != 0;
            bool is_output = (prop->prop_flags & CPF_OUT_PARM)    != 0 && (prop->prop_flags & CPF_CONST_PARM) == 0 && !is_return;
            if ((pass == 0 && !is_return) || (pass == 1 && !is_output)) {
              continue;
            }

            void *value = unreal_fprop_value_in_container(prop, params, 0);
            if (!value || !lua_unreal_prop_push(state, prop, value, receiver)) {
              lua_unreal_error_set(error, sizeof(error), "could not marshal a function result");
              pushed = false;
              break;
            }
            result_count += 1;
          }
        }
      }

      uint32_t destroy_idx = 0;
      for (ffield_t *field = function->child_props; field && destroy_idx < initialized_count; field = field->next) {
        fprop_t *prop = (fprop_t *)field;
        if (prop->prop_flags & CPF_PARM) {
          unreal_fprop_destroy_in_container(prop, params);
          destroy_idx += 1;
        }
      }

      if (!converted || !pushed) {
        lua_settop(state, stack_base);
        result_count = luaL_error(state, "Unreal call failed: %s", error[0] ? error : "conversion failed");
      }
    } else {
      result_count = luaL_error(state, "could not allocate 0x%X bytes for function parameters", function->params_size);
    }
  }
  scratch_end(tmp);

  return result_count;
}

static int
lua_unreal_method_call(lua_State *state)
{
  lua_unreal_method_t *method = (lua_unreal_method_t *)lua_touserdata(state, lua_upvalueindex(1));
  if (!method) {
    return luaL_error(state, "invalid Unreal method closure");
  }
  lua_unreal_require_access(state, method->context);

  uobject_t *receiver  = unreal_fweak_object_resolve(method->receiver);
  ufunc_t   *function  = (ufunc_t *)unreal_fweak_object_resolve(method->function);
  int        first_arg = lua_unreal_method_same_receiver(state, 1, method->receiver) ? 2 : 1;
  return lua_unreal_call_reflected_function(state, receiver, function, first_arg);
}

static int
lua_unreal_object_call_function_lua(lua_State *state)
{
  uobject_t *receiver = lua_unreal_object_check_live(state, 1);
  ufunc_t   *function = lua_unreal_ufunction_check(state, 2);
  return lua_unreal_call_reflected_function(state, receiver, function, 3);
}

static int
lua_unreal_ufunction_call_lua(lua_State *state)
{
  ufunc_t   *function = lua_unreal_ufunction_check(state, 1);
  uobject_t *receiver = lua_unreal_object_check_live(state, 2);
  return lua_unreal_call_reflected_function(state, receiver, function, 3);
}

static int
lua_unreal_object_index(lua_State *state)
{
  lua_unreal_object_t *userdata = (lua_unreal_object_t *)luaL_checkudata(state, 1, LUA_UNREAL_OBJECT_MT);
  lua_unreal_require_access(state, userdata->context);

  size_t      key_len = 0;
  const char *key     = luaL_checklstring(state, 2, &key_len);
  uobject_t  *object  = lua_unreal_object_resolve(userdata);
  if (!object) {
    if (lua_getmetatable(state, 1)) {
      lua_pushvalue(state, 2);
      lua_rawget(state, -2);
      lua_remove(state, -2);
      if (!lua_isnil(state, -1)) {
        return 1;
      }
      lua_pop(state, 1);
    }
    return luaL_error(state, "the UObject is no longer valid");
  }

  str_t    name = str_make((uint8_t *)key, (uint64_t)key_len);
  fprop_t *prop = unreal_ustruct_find_prop((ustruct_t *)object->cls, name);
  if (prop) {
    if (!lua_unreal_property_fits(prop, (uint64_t)((ustruct_t *)object->cls)->props_size)) {
      return luaL_error(state, "property '%s' has an unsupported reflected layout", key);
    }

    void *value = unreal_fprop_value_in_container(prop, object, 0);
    if (!value || !lua_unreal_prop_push_internal(state, prop, value, object, 1, true)) {
      return luaL_error(state, "property '%s' has an unsupported reflected type", key);
    }
    return 1;
  }

  if (lua_unreal_native_method_push(state, object, name)) {
    return 1;
  }

  /* Core UObject methods are part of the Lua wrapper API and must win over
   * reflected functions with the same name.  KismetSystemLibrary, for
   * example, exposes a static IsValid(Object) UFunction; resolving it before
   * the wrapper's IsValid() makes ordinary UE4SS-style object validity checks
   * fail with a missing-argument error. */
  if (lua_getmetatable(state, 1)) {
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);
    lua_remove(state, -2);
    if (!lua_isnil(state, -1)) {
      return 1;
    }
    lua_pop(state, 1);
  }

  ufunc_t           *func      = unreal_ustruct_find_func((ustruct_t *)object->cls, name);
  fweak_object_ptr_t func_weak = {0};
  if (func && unreal_uobject_is_a((uobject_t *)func, globals.unreal.core_func) && unreal_fweak_object_from_object((uobject_t *)func, &func_weak)) {
    lua_unreal_method_t *method = (lua_unreal_method_t *)lua_newuserdatauv(state, sizeof(*method), 0);
    method->context  = userdata->context;
    method->receiver = userdata->weak;
    method->function = func_weak;
    lua_pushcclosure(state, lua_unreal_method_call, 1);
    return 1;
  }

  lua_pushnil(state);
  return 1;
}

static int
lua_unreal_object_newindex(lua_State *state)
{
  lua_unreal_object_t *userdata = (lua_unreal_object_t *)luaL_checkudata(state, 1, LUA_UNREAL_OBJECT_MT);
  lua_unreal_require_access(state, userdata->context);

  size_t      key_len = 0;
  const char *key     = luaL_checklstring(state, 2, &key_len);
  uobject_t  *object  = lua_unreal_object_resolve(userdata);
  if (!object) {
    return luaL_error(state, "the UObject is no longer valid");
  }

  fprop_t *prop = unreal_ustruct_find_prop((ustruct_t *)object->cls, str_make((uint8_t *)key, (uint64_t)key_len));
  if (!prop || !lua_unreal_property_fits(prop, (uint64_t)((ustruct_t *)object->cls)->props_size)) {
    return luaL_error(state, "unknown or unsupported property '%s'", key);
  }

  void *dst = unreal_fprop_value_in_container(prop, object, 0);
  char  error[LUA_UNREAL_ERROR_CAP] = {0};
  if (!dst || !lua_unreal_write_transactional(state, 3, prop, dst, object, error, sizeof(error))) {
    return luaL_error(state, "could not assign '%s': %s", key, error[0] ? error : "conversion failed");
  }
  return 0;
}

static int
lua_unreal_object_count(lua_unreal_context_t *context)
{
  if (context && context->object_count && context->object_at) {
    int count = context->object_count(context->user);
    return MAX_VAL(count, 0);
  }
  return unreal_uobject_array_count();
}

static uobject_t *
lua_unreal_object_at(lua_unreal_context_t *context, int idx)
{
  if (context && context->object_count && context->object_at) {
    return context->object_at(context->user, idx);
  }
  return unreal_uobject_array_get_obj(idx);
}

static bool
lua_unreal_class_arg(lua_State *state, int idx, uclass_t **out_class, str_t *out_name)
{
  *out_class = NULL;
  *out_name  = STR_NULL;

  if (lua_isnoneornil(state, idx)) {
    return true;
  }

  if (lua_type(state, idx) == LUA_TSTRING || lua_unreal_fname_test(state, idx)) {
    return lua_unreal_string_or_fname(state, idx, out_name) && !str_is_empty(*out_name);
  }

  uobject_t *object = lua_unreal_object_get(state, idx);
  if (object && unreal_uobject_is_a(object, globals.unreal.core_class)) {
    *out_class = (uclass_t *)object;
    return true;
  }
  return false;
}

static bool
lua_unreal_optional_name(lua_State *state, int idx, str_t *out_name)
{
  *out_name = STR_NULL;

  if (lua_isnoneornil(state, idx)) {
    return true;
  }

  return lua_unreal_string_or_fname(state, idx, out_name) && !str_is_empty(*out_name);
}

static eobj_flags_t
lua_unreal_flags_arg(lua_State *state, int idx)
{
  if (lua_isnoneornil(state, idx)) {
    return RF_NO_FLAGS;
  }
  return (eobj_flags_t)lua_unreal_check_u32(state, idx, "object flags must fit in an unsigned 32-bit integer");
}

static bool
lua_unreal_object_matches_class(uobject_t *object, uclass_t *cls, str_t class_name, bool exact_class)
{
  if (!object) {
    return false;
  }

  if (cls) {
    return exact_class ? object->cls == cls : unreal_uobject_is_a(object, cls);
  }

  if (!str_is_empty(class_name)) {
    if (exact_class) {
      return object->cls && unreal_fname_match_text(object->cls->name, class_name, true, true);
    }
    return unreal_super_chain_contains(object->cls, class_name, true, true);
  }
  return true;
}

static bool
lua_unreal_object_matches(uobject_t *object, uclass_t *cls, str_t class_name, str_t object_name, eobj_flags_t required_flags, eobj_flags_t banned_flags, bool exact_class)
{
  if (!lua_unreal_object_matches_class(object, cls, class_name, exact_class)) {
    return false;
  }

  if (!str_is_empty(object_name) && !unreal_fname_match_text(object->name, object_name, true, true)) {
    return false;
  }

  if ((object->obj_flags & required_flags) != required_flags || (object->obj_flags & banned_flags) != 0) {
    return false;
  }

  return true;
}

static uobject_t *
lua_unreal_find_legacy(lua_unreal_context_t *context, str_t name)
{
  uobject_t *object = context->find_object ? context->find_object(context->user, name) : unreal_uobject_find_by_full_name(NULL, name);
  if (!object && !context->find_object) {
    object = unreal_uobject_find(name, false, true);
  }
  return object;
}

static int
lua_unreal_find_object_lua(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  int arg_count = lua_gettop(state);
  if (arg_count == 1 && lua_type(state, 1) == LUA_TSTRING) {
    size_t      len  = 0;
    const char *text = lua_tolstring(state, 1, &len);
    return lua_unreal_find_result_push(state, lua_unreal_find_legacy(context, str_make((uint8_t *)text, (uint64_t)len)));
  }

  /* UE4SS overload: FindObject(UClass, UObject InOuter, string Name, bool ExactClass). */
  if (arg_count >= 3 && (lua_unreal_object_test(state, 1) || lua_isnil(state, 1)) && (lua_unreal_object_test(state, 2) || lua_isnil(state, 2)) && lua_type(state, 3) == LUA_TSTRING) {
    uclass_t *cls = NULL;
    str_t     ignored_class_name = STR_NULL;
    if (!lua_unreal_class_arg(state, 1, &cls, &ignored_class_name)) {
      return luaL_argerror(state, 1, "expected a UClass or nil");
    }

    uobject_t *outer = lua_isnil(state, 2) ? NULL : lua_unreal_object_get(state, 2);
    if (!outer && !lua_isnil(state, 2)) {
      return luaL_argerror(state, 2, "expected a live UObject or nil");
    }

    size_t      name_len = 0;
    const char *name     = lua_tolstring(state, 3, &name_len);
    str_t       wanted   = str_make((uint8_t *)name, (uint64_t)name_len);
    bool        exact    = lua_toboolean(state, 4) != 0;

    for (int i = 0, count = lua_unreal_object_count(context); i < count; ++i) {
      uobject_t *object = lua_unreal_object_at(context, i);
      if (!lua_unreal_object_matches_class(object, cls, STR_NULL, exact)) {
        continue;
      }

      bool name_matches = false;
      if (outer) {
        if (object->outer == outer && unreal_fname_match_text(object->name, wanted, false, true)) {
          name_matches = true;
        } else {
          tmp_arena_t tmp = scratch_begin(NULL);
          str_t expected = str_push_fmt(tmp.arena, "%.*s.%.*s", STR_ARG(unreal_uobject_push_full_name(outer, tmp.arena)), STR_ARG(wanted));
          name_matches = str_equal(unreal_uobject_push_full_name(object, tmp.arena), expected, 0);
          scratch_end(tmp);
        }
      } else {
        tmp_arena_t tmp = scratch_begin(NULL);
        name_matches = str_equal(unreal_uobject_push_full_name(object, tmp.arena), wanted, 0);
        scratch_end(tmp);
      }

      if (name_matches) {
        return lua_unreal_object_push(state, object);
      }
    }
    return lua_unreal_object_push_invalid(state);
  }

  /* UE4SS overload: FindObject(ClassName|UClass|nil, ObjectName|nil, RequiredFlags=0, BannedFlags=0). */
  uclass_t *cls         = NULL;
  str_t     class_name  = STR_NULL;
  str_t     object_name = STR_NULL;

  if (!lua_unreal_class_arg(state, 1, &cls, &class_name)) {
    return luaL_argerror(state, 1, "expected a class-name string, UClass, or nil");
  }

  if (!lua_unreal_optional_name(state, 2, &object_name)) {
    return luaL_argerror(state, 2, "expected an object-name string or nil");
  }

  if (!cls && str_is_empty(class_name) && str_is_empty(object_name)) {
    return luaL_error(state, "class and object name cannot both be nil");
  }

  eobj_flags_t required = lua_unreal_flags_arg(state, 3);
  eobj_flags_t banned   = lua_unreal_flags_arg(state, 4);
  for (int i = 0, count = lua_unreal_object_count(context); i < count; ++i) {
    uobject_t *object = lua_unreal_object_at(context, i);
    if (lua_unreal_object_matches(object, cls, class_name, object_name, required, banned, false)) {
      return lua_unreal_object_push(state, object);
    }
  }
  return lua_unreal_object_push_invalid(state);
}

static int
lua_unreal_load_asset_lua(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  size_t      len  = 0;
  const char *text = luaL_checklstring(state, 1, &len);
  str_t       path = str_make((uint8_t *)text, (uint64_t)len);

  if (str_is_empty(path) || lua_unreal_string_has_embedded_null(path)) {
    return luaL_argerror(state, 1, "asset path cannot be empty or contain null bytes");
  }

  if (!globals.unreal.core_object) {
    return luaL_error(state, "the UObject class is unavailable");
  }

  uobject_t *asset = unreal_static_load_object(globals.unreal.core_object, NULL, path, STR_LIT(""), 0, NULL, true, NULL);
  return lua_unreal_object_push(state, asset);
}

static int
lua_unreal_static_find_object_lua(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  if (lua_gettop(state) == 1) {
    size_t      len  = 0;
    const char *text = luaL_checklstring(state, 1, &len);
    str_t       name = str_make((uint8_t *)text, (uint64_t)len);
    uobject_t  *object = context->find_object ? context->find_object(context->user, name) : unreal_uobject_find_by_full_name(NULL, name);
    return lua_unreal_find_result_push(state, object);
  }

  /* the four-argument overload is identical to FindObject's UE-style overload */
  return lua_unreal_find_object_lua(state);
}

static int
lua_unreal_find_objects_lua(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  lua_Integer limit = 0;
  if (!lua_isnoneornil(state, 1)) {
    limit = luaL_checkinteger(state, 1);
    if (limit < 0) {
      return luaL_argerror(state, 1, "object limit cannot be negative");
    }
  }

  uclass_t *cls         = NULL;
  str_t     class_name  = STR_NULL;
  str_t     object_name = STR_NULL;

  if (!lua_unreal_class_arg(state, 2, &cls, &class_name)) {
    return luaL_argerror(state, 2, "expected a class-name string, UClass, or nil");
  }

  if (!lua_unreal_optional_name(state, 3, &object_name)) {
    return luaL_argerror(state, 3, "expected an object-name string or nil");
  }

  if (!cls && str_is_empty(class_name) && str_is_empty(object_name)) {
    return luaL_error(state, "class and object name cannot both be nil");
  }

  eobj_flags_t required = lua_unreal_flags_arg(state, 4);
  eobj_flags_t banned   = lua_unreal_flags_arg(state, 5);
  bool          exact   = lua_toboolean(state, 6) != 0;
  lua_Integer   found   = 0;
  lua_newtable(state);

  for (int i = 0, count = lua_unreal_object_count(context); i < count && (limit == 0 || found < limit); ++i) {
    uobject_t *object = lua_unreal_object_at(context, i);
    if (!lua_unreal_object_matches(object, cls, class_name, object_name, required, banned, exact)) {
      continue;
    }

    lua_unreal_object_push(state, object);
    lua_rawseti(state, -2, ++found);
  }
  return 1;
}

static int
lua_unreal_find_of_lua(lua_State *state, bool all)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  size_t      len        = 0;
  const char *text       = luaL_checklstring(state, 1, &len);
  str_t       class_name = str_make((uint8_t *)text, (uint64_t)len);
  lua_Integer found      = 0;

  if (all) {
    lua_newtable(state);
  }

  for (int i = 0, count = lua_unreal_object_count(context); i < count; ++i) {
    uobject_t *object = lua_unreal_object_at(context, i);
    if (!object || unreal_uobject_is_default(object) || !lua_unreal_object_matches_class(object, NULL, class_name, false)) {
      continue;
    }

    if (!all) {
      return lua_unreal_object_push(state, object);
    }

    lua_unreal_object_push(state, object);
    lua_rawseti(state, -2, ++found);
  }

  if (all && found > 0) {
    return 1;
  }

  if (all) {
    lua_pop(state, 1);
    lua_pushnil(state);
    return 1;
  }

  return lua_unreal_object_push_invalid(state);
}

static int
lua_unreal_find_first_of_lua(lua_State *state)
{
  return lua_unreal_find_of_lua(state, false);
}

static int
lua_unreal_find_all_of_lua(lua_State *state)
{
  return lua_unreal_find_of_lua(state, true);
}

static int
lua_unreal_fname_to_string_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));
  lua_unreal_fname_t *name = (lua_unreal_fname_t *)luaL_checkudata(state, 1, LUA_UNREAL_FNAME_MT);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t text = unreal_fname_to_str(name->value, tmp.arena);
    lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
  }
  scratch_end(tmp);
  return 1;
}

static int
lua_unreal_fname_comparison_index_lua(lua_State *state)
{
  lua_unreal_fname_t *name = (lua_unreal_fname_t *)luaL_checkudata(state, 1, LUA_UNREAL_FNAME_MT);
  lua_pushinteger(state, (lua_Integer)name->value.cmp_idx);
  return 1;
}

static int
lua_unreal_fname_type_lua(lua_State *state)
{
  (void)luaL_checkudata(state, 1, LUA_UNREAL_FNAME_MT);
  lua_pushliteral(state, "FName");
  return 1;
}

static int
lua_unreal_fname_equal_lua(lua_State *state)
{
  lua_unreal_fname_t *left  = lua_unreal_fname_test(state, 1);
  lua_unreal_fname_t *right = lua_unreal_fname_test(state, 2);
  lua_pushboolean(state, left && right && left->value.cmp_idx == right->value.cmp_idx && left->value.num == right->value.num);
  return 1;
}

static int
lua_unreal_fname_equals_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));
  lua_unreal_fname_t *left  = (lua_unreal_fname_t *)luaL_checkudata(state, 1, LUA_UNREAL_FNAME_MT);
  lua_unreal_fname_t *right = (lua_unreal_fname_t *)luaL_checkudata(state, 2, LUA_UNREAL_FNAME_MT);
  lua_pushboolean(state, left->value.cmp_idx == right->value.cmp_idx && left->value.num == right->value.num);
  return 1;
}

static int
lua_unreal_fname_lua(lua_State *state)
{
  lua_unreal_require_access(state, lua_unreal_context_from_state(state));

  efind_name_t find_type = (efind_name_t)luaL_optinteger(state, 2, FNAME_FIND_OR_ADD);
  if (find_type != FNAME_FIND && find_type != FNAME_FIND_OR_ADD) {
    return luaL_argerror(state, 2, "expected EFindName.FNAME_Find or EFindName.FNAME_Add");
  }

  fname_t value = {0};
  if (lua_type(state, 1) == LUA_TSTRING) {
    size_t      len  = 0;
    const char *text = lua_tolstring(state, 1, &len);
    value = unreal_fname_from_str(str_make((uint8_t *)text, (uint64_t)len), find_type);
  } else if (lua_isinteger(state, 1)) {
    uint32_t       comparison_idx = lua_unreal_check_u32(state, 1, "comparison index must fit in an unsigned 32-bit integer");
    fname_entry_t *entry          = globals.name_pool ? unreal_fname_entry_get(comparison_idx) : NULL;
    if (entry && entry->header.len > 0 && entry->header.len <= FNAME_ENTRY_NAME_SIZE) {
      value.cmp_idx = comparison_idx;
    }
  } else {
    lua_unreal_fname_t *source = lua_unreal_fname_test(state, 1);
    if (!source) {
      return luaL_argerror(state, 1, "expected a string, comparison index, or FName");
    }
    value = source->value;
  }

  return lua_unreal_fname_push(state, value);
}

static int
lua_unreal_ftext_literal_to_string_lua(lua_State *state)
{
  lua_unreal_ftext_literal_t *literal = (lua_unreal_ftext_literal_t *)luaL_checkudata(state, 1, LUA_UNREAL_FTEXT_MT);
  lua_unreal_require_access(state, literal->context);
  lua_pushlstring(state, literal->text, literal->len);
  return 1;
}

static int
lua_unreal_ftext_literal_type_lua(lua_State *state)
{
  lua_unreal_ftext_literal_t *literal = (lua_unreal_ftext_literal_t *)luaL_checkudata(state, 1, LUA_UNREAL_FTEXT_MT);
  lua_unreal_require_access(state, literal->context);
  lua_pushliteral(state, "FText");
  return 1;
}

static int
lua_unreal_ftext_lua(lua_State *state)
{
  lua_unreal_context_t *context = lua_unreal_context_from_state(state);
  lua_unreal_require_access(state, context);

  size_t      len  = 0;
  const char *text = luaL_checklstring(state, 1, &len);
  for (size_t i = 0; i < len; ++i) {
    if (text[i] == '\0') {
      return luaL_argerror(state, 1, "FText cannot contain embedded null bytes");
    }
  }

  if (len > SIZE_MAX - sizeof(lua_unreal_ftext_literal_t)) {
    return luaL_error(state, "FText is too large");
  }

  lua_unreal_ftext_literal_t *literal = (lua_unreal_ftext_literal_t *)lua_newuserdatauv(state, sizeof(*literal) + len, 0);
  literal->context = context;
  literal->len     = len;
  mem_copy(literal->text, (void *)text, len);

  literal->text[len] = '\0';
  luaL_setmetatable(state, LUA_UNREAL_FTEXT_MT);
  return 1;
}

static void
lua_unreal_register_ftext_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_FTEXT_MT);
  lua_pushcfunction(state, lua_unreal_ftext_literal_to_string_lua);
  lua_setfield(state, -2, "ToString");
  lua_pushcfunction(state, lua_unreal_ftext_literal_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushcfunction(state, lua_unreal_ftext_literal_to_string_lua);
  lua_setfield(state, -2, "__tostring");
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  lua_pushliteral(state, "FText");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_property_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_PROPERTY_MT);
  lua_pushcfunction(state, lua_unreal_property_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_property_get_full_name_lua);
  lua_setfield(state, -2, "GetFullName");
  lua_pushcfunction(state, lua_unreal_property_get_fname_lua);
  lua_setfield(state, -2, "GetFName");
  lua_pushcfunction(state, lua_unreal_property_is_a_lua);
  lua_setfield(state, -2, "IsA");
  lua_pushcfunction(state, lua_unreal_property_get_class_lua);
  lua_setfield(state, -2, "GetClass");
  lua_pushcfunction(state, lua_unreal_property_get_offset_lua);
  lua_setfield(state, -2, "GetOffset_Internal");
  lua_pushcfunction(state, lua_unreal_property_get_array_dim_lua);
  lua_setfield(state, -2, "GetArrayDim");
  lua_pushcfunction(state, lua_unreal_property_get_element_size_lua);
  lua_setfield(state, -2, "GetElementSize");
  lua_pushcfunction(state, lua_unreal_property_get_property_class_lua);
  lua_setfield(state, -2, "GetPropertyClass");
  lua_pushcfunction(state, lua_unreal_bool_property_get_byte_mask_lua);
  lua_setfield(state, -2, "GetByteMask");
  lua_pushcfunction(state, lua_unreal_bool_property_get_byte_offset_lua);
  lua_setfield(state, -2, "GetByteOffset");
  lua_pushcfunction(state, lua_unreal_bool_property_get_field_mask_lua);
  lua_setfield(state, -2, "GetFieldMask");
  lua_pushcfunction(state, lua_unreal_bool_property_get_field_size_lua);
  lua_setfield(state, -2, "GetFieldSize");
  lua_pushcfunction(state, lua_unreal_property_get_struct_lua);
  lua_setfield(state, -2, "GetStruct");
  lua_pushcfunction(state, lua_unreal_property_get_inner_lua);
  lua_setfield(state, -2, "GetInner");
  lua_pushcfunction(state, lua_unreal_property_get_element_property_lua);
  lua_setfield(state, -2, "GetElementProperty");
  lua_pushcfunction(state, lua_unreal_property_get_key_property_lua);
  lua_setfield(state, -2, "GetKeyProperty");
  lua_pushcfunction(state, lua_unreal_property_get_value_property_lua);
  lua_setfield(state, -2, "GetValueProperty");
  lua_pushcfunction(state, lua_unreal_property_get_underlying_property_lua);
  lua_setfield(state, -2, "GetUnderlyingProperty");
  lua_pushcfunction(state, lua_unreal_property_get_enum_lua);
  lua_setfield(state, -2, "GetEnum");
  lua_pushcfunction(state, lua_unreal_property_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushcfunction(state, lua_unreal_property_tostring_lua);
  lua_setfield(state, -2, "__tostring");
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  lua_pushliteral(state, "Property");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_field_class_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_FIELD_CLASS_MT);
  lua_pushcfunction(state, lua_unreal_field_class_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_field_class_get_fname_lua);
  lua_setfield(state, -2, "GetFName");
  lua_pushcfunction(state, lua_unreal_field_class_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushcfunction(state, lua_unreal_field_class_tostring_lua);
  lua_setfield(state, -2, "__tostring");
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  lua_pushliteral(state, "FieldClass");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_reflection_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_REFLECTION_MT);
  lua_pushcfunction(state, lua_unreal_reflection_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_reflection_get_property_lua);
  lua_setfield(state, -2, "GetProperty");
  lua_pushcfunction(state, lua_unreal_reflection_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  lua_pushliteral(state, "UObjectReflection");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_fname_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_FNAME_MT);
  lua_pushcfunction(state, lua_unreal_fname_to_string_lua);
  lua_setfield(state, -2, "__tostring");
  lua_pushcfunction(state, lua_unreal_fname_equal_lua);
  lua_setfield(state, -2, "__eq");
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  lua_pushcfunction(state, lua_unreal_fname_to_string_lua);
  lua_setfield(state, -2, "ToString");
  lua_pushcfunction(state, lua_unreal_fname_comparison_index_lua);
  lua_setfield(state, -2, "GetComparisonIndex");
  lua_pushcfunction(state, lua_unreal_fname_equals_lua);
  lua_setfield(state, -2, "Equals");
  lua_pushcfunction(state, lua_unreal_fname_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushliteral(state, "FName");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_push_find_name_table(lua_State *state)
{
  lua_newtable(state);
  lua_pushinteger(state, FNAME_FIND);
  lua_setfield(state, -2, "FNAME_Find");
  lua_pushinteger(state, FNAME_FIND_OR_ADD);
  lua_setfield(state, -2, "FNAME_Add");
}

static void
lua_unreal_register_object_flags(lua_State *state)
{
  static const struct {
    const char   *name;
    eobj_flags_t  value;
  } flags[] = {
    {"RF_NoFlags",                      RF_NO_FLAGS                      },
    {"RF_Public",                       RF_PUBLIC                        },
    {"RF_Standalone",                   RF_STANDALONE                    },
    {"RF_MarkAsNative",                 RF_MARK_AS_NATIVE                },
    {"RF_Transactional",                RF_TRANSACTIONAL                 },
    {"RF_ClassDefaultObject",           RF_CLASS_DEFAULT_OBJECT          },
    {"RF_ArchetypeObject",              RF_ARCHETYPE_OBJECT              },
    {"RF_Transient",                    RF_TRANSIENT                     },
    {"RF_MarkAsRootSet",                RF_MARK_AS_ROOT_SET              },
    {"RF_TagGarbageTemp",               RF_TAG_GARBAGE_TEMP              },
    {"RF_NeedInitialization",           RF_NEED_INITIALIZATION           },
    {"RF_NeedLoad",                     RF_NEED_LOAD                     },
    {"RF_KeepForCooker",                RF_KEEP_FOR_COOKER               },
    {"RF_NeedPostLoad",                 RF_NEED_POST_LOAD                },
    {"RF_NeedPostLoadSubobjects",       RF_NEED_POST_LOAD_SUBOBJECTS     },
    {"RF_NewerVersionExists",           RF_NEWER_VERSION_EXISTS          },
    {"RF_BeginDestroyed",               RF_BEGIN_DESTROYED               },
    {"RF_FinishDestroyed",              RF_FINISH_DESTROYED              },
    {"RF_BeingRegenerated",             RF_BEING_REGENERATED             },
    {"RF_DefaultSubObject",             RF_DEFAULT_SUB_OBJECT            },
    {"RF_WasLoaded",                    RF_WAS_LOADED                    },
    {"RF_TextExportTransient",          RF_TEXT_EXPORT_TRANSIENT         },
    {"RF_LoadCompleted",                RF_LOAD_COMPLETED                },
    {"RF_InheritableComponentTemplate", RF_INHERITABLE_COMPONENT_TEMPLATE},
    {"RF_DuplicateTransient",           RF_DUPLICATE_TRANSIENT           },
    {"RF_StrongRefOnFrame",             RF_STRONG_REF_ON_FRAME           },
    {"RF_NonPIEDuplicateTransient",     RF_NON_PIE_DUPLICATE_TRANSIENT   },
    {"RF_Dynamic",                      RF_DYNAMIC                       },
    {"RF_WillBeLoaded",                 RF_WILL_BE_LOADED                },
    {"RF_HasExternalPackage",           RF_HAS_EXTERNAL_PACKAGE          },
  };

  lua_newtable(state);
  for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
    lua_pushinteger(state, (lua_Integer)flags[i].value);
    lua_setfield(state, -2, flags[i].name);
  }
}

static void
lua_unreal_register_internal_object_flags(lua_State *state)
{
  static const struct {
    const char *name;
    uint32_t    value;
  } flags[] = {
    {"ReachableInCluster",         0x00800000u},
    {"ClusterRoot",                0x01000000u},
    {"Native",                     0x02000000u},
    {"Async",                      0x04000000u},
    {"AsyncLoading",               0x08000000u},
    {"Unreachable",                0x10000000u},
    {"PendingKill",                0x20000000u},
    {"RootSet",                    0x40000000u},
    {"GarbageCollectionKeepFlags", 0x0E000000u},
    {"AllFlags",                   0x7F800000u},
  };

  lua_newtable(state);
  for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
    lua_pushinteger(state, (lua_Integer)flags[i].value);
    lua_setfield(state, -2, flags[i].name);
  }
}

static void
lua_unreal_register_property_types(lua_State *state)
{
  static const struct {
    const char *name;
    const char *class_name;
  } types[] = {
    {"ObjectProperty",            "ObjectProperty"            },
    {"ObjectPtrProperty",         "ObjectProperty"            },
    {"Int8Property",              "Int8Property"              },
    {"Int16Property",             "Int16Property"             },
    {"IntProperty",               "IntProperty"               },
    {"Int32Property",             "Int32Property"             },
    {"Int64Property",             "Int64Property"             },
    {"NameProperty",              "NameProperty"              },
    {"FloatProperty",             "FloatProperty"             },
    {"DoubleProperty",            "DoubleProperty"            },
    {"StrProperty",               "StrProperty"               },
    {"ByteProperty",              "ByteProperty"              },
    {"UInt16Property",            "UInt16Property"            },
    {"UIntProperty",              "UInt32Property"            },
    {"UInt32Property",            "UInt32Property"            },
    {"UInt64Property",            "UInt64Property"            },
    {"BoolProperty",              "BoolProperty"              },
    {"ArrayProperty",             "ArrayProperty"             },
    {"SetProperty",               "SetProperty"               },
    {"MapProperty",               "MapProperty"               },
    {"StructProperty",            "StructProperty"            },
    {"ClassProperty",             "ClassProperty"             },
    {"SoftObjectProperty",        "SoftObjectProperty"        },
    {"SoftClassProperty",         "SoftClassProperty"         },
    {"WeakObjectProperty",        "WeakObjectProperty"        },
    {"LazyObjectProperty",        "LazyObjectProperty"        },
    {"EnumProperty",              "EnumProperty"              },
    {"TextProperty",              "TextProperty"              },
    {"InterfaceProperty",         "InterfaceProperty"         },
    {"DelegateProperty",          "DelegateProperty"          },
    {"MulticastDelegateProperty", "MulticastDelegateProperty" },
  };

  lua_newtable(state);
  for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
    lua_pushstring(state, types[i].class_name);
    lua_setfield(state, -2, types[i].name);
  }
}

static void
lua_unreal_register_object_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_OBJECT_MT);
  lua_pushcfunction(state, lua_unreal_object_index);
  lua_setfield(state, -2, "__index");
  lua_pushcfunction(state, lua_unreal_object_newindex);
  lua_setfield(state, -2, "__newindex");
  lua_pushcfunction(state, lua_unreal_object_tostring);
  lua_setfield(state, -2, "__tostring");
  lua_pushcfunction(state, lua_unreal_object_equal_lua);
  lua_setfield(state, -2, "__eq");
  lua_pushcfunction(state, lua_unreal_object_len_lua);
  lua_setfield(state, -2, "__len");
  lua_pushcfunction(state, lua_unreal_ufunction_call_lua);
  lua_setfield(state, -2, "__call");
  lua_pushcfunction(state, lua_unreal_object_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_object_get_full_name_lua);
  lua_setfield(state, -2, "GetFullName");
  lua_pushcfunction(state, lua_unreal_object_get_fname_lua);
  lua_setfield(state, -2, "GetFName");
  lua_pushcfunction(state, lua_unreal_object_get_address_lua);
  lua_setfield(state, -2, "GetAddress");
  lua_pushcfunction(state, lua_unreal_object_get_class_lua);
  lua_setfield(state, -2, "GetClass");
  lua_pushcfunction(state, lua_unreal_object_get_outer_lua);
  lua_setfield(state, -2, "GetOuter");
  lua_pushcfunction(state, lua_unreal_object_is_class_lua);
  lua_setfield(state, -2, "IsAnyClass");
  lua_pushcfunction(state, lua_unreal_object_is_class_lua);
  lua_setfield(state, -2, "IsClass");
  lua_pushcfunction(state, lua_unreal_object_is_a_lua);
  lua_setfield(state, -2, "IsA");
  lua_pushcfunction(state, lua_unreal_object_has_all_flags_lua);
  lua_setfield(state, -2, "HasAllFlags");
  lua_pushcfunction(state, lua_unreal_object_has_any_flags_lua);
  lua_setfield(state, -2, "HasAnyFlags");
  lua_pushcfunction(state, lua_unreal_object_has_any_internal_flags_lua);
  lua_setfield(state, -2, "HasAnyInternalFlags");
  lua_pushcfunction(state, lua_unreal_object_index);
  lua_setfield(state, -2, "GetPropertyValue");
  lua_pushcfunction(state, lua_unreal_object_newindex);
  lua_setfield(state, -2, "SetPropertyValue");
  lua_pushcfunction(state, lua_unreal_object_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushliteral(state, "UObject");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_data_table_row_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_DATA_TABLE_ROW_MT);
  lua_pushcfunction(state, lua_unreal_data_table_row_index_lua);
  lua_setfield(state, -2, "__index");
  lua_pushcfunction(state, lua_unreal_data_table_row_newindex_lua);
  lua_setfield(state, -2, "__newindex");
  lua_pushcfunction(state, lua_unreal_data_table_row_tostring_lua);
  lua_setfield(state, -2, "__tostring");
  lua_pushcfunction(state, lua_unreal_data_table_row_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_data_table_row_index_lua);
  lua_setfield(state, -2, "GetPropertyValue");
  lua_pushcfunction(state, lua_unreal_data_table_row_newindex_lua);
  lua_setfield(state, -2, "SetPropertyValue");
  lua_pushcfunction(state, lua_unreal_data_table_row_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushcfunction(state, lua_unreal_data_table_row_tostring_lua);
  lua_setfield(state, -2, "ToString");
  lua_pushliteral(state, "UScriptStruct");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_value_metatable(lua_State *state, const char *name, bool owned)
{
  luaL_newmetatable(state, name);
  lua_pushcfunction(state, lua_unreal_value_index);
  lua_setfield(state, -2, "__index");
  lua_pushcfunction(state, lua_unreal_value_newindex);
  lua_setfield(state, -2, "__newindex");
  lua_pushcfunction(state, lua_unreal_value_tostring);
  lua_setfield(state, -2, "__tostring");
  lua_pushcfunction(state, lua_unreal_value_len);
  lua_setfield(state, -2, "__len");
  lua_pushcfunction(state, lua_unreal_value_len);
  lua_setfield(state, -2, "Num");
  lua_pushcfunction(state, lua_unreal_value_get_array_num_lua);
  lua_setfield(state, -2, "GetArrayNum");
  lua_pushcfunction(state, lua_unreal_value_get_array_address_lua);
  lua_setfield(state, -2, "GetArrayAddress");
  lua_pushcfunction(state, lua_unreal_value_get_array_max_lua);
  lua_setfield(state, -2, "GetArrayMax");
  lua_pushcfunction(state, lua_unreal_value_get_array_data_address_lua);
  lua_setfield(state, -2, "GetArrayDataAddress");
  lua_pushcfunction(state, lua_unreal_value_for_each_lua);
  lua_setfield(state, -2, "ForEach");
  lua_pushcfunction(state, lua_unreal_value_to_table_lua);
  lua_setfield(state, -2, "ToTable");
  lua_pushcfunction(state, lua_unreal_value_clear_lua);
  lua_setfield(state, -2, "Clear");
  lua_pushcfunction(state, lua_unreal_value_clear_lua);
  lua_setfield(state, -2, "Empty");
  lua_pushcfunction(state, lua_unreal_value_add_lua);
  lua_setfield(state, -2, "Add");
  lua_pushcfunction(state, lua_unreal_value_insert_lua);
  lua_setfield(state, -2, "Insert");
  lua_pushcfunction(state, lua_unreal_value_contains_lua);
  lua_setfield(state, -2, "Contains");
  lua_pushcfunction(state, lua_unreal_value_find_lua);
  lua_setfield(state, -2, "Find");
  lua_pushcfunction(state, lua_unreal_value_remove_lua);
  lua_setfield(state, -2, "Remove");
  lua_pushcfunction(state, lua_unreal_value_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_value_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushcfunction(state, lua_unreal_value_to_string_lua);
  lua_setfield(state, -2, "ToString");
  lua_pushcfunction(state, lua_unreal_fstring_len_lua);
  lua_setfield(state, -2, "Len");
  lua_pushcfunction(state, lua_unreal_fstring_is_empty_lua);
  lua_setfield(state, -2, "IsEmpty");
  lua_pushcfunction(state, lua_unreal_fstring_append_lua);
  lua_setfield(state, -2, "Append");
  lua_pushcfunction(state, lua_unreal_fstring_starts_with_lua);
  lua_setfield(state, -2, "StartsWith");
  lua_pushcfunction(state, lua_unreal_fstring_ends_with_lua);
  lua_setfield(state, -2, "EndsWith");
  lua_pushcfunction(state, lua_unreal_fstring_to_upper_lua);
  lua_setfield(state, -2, "ToUpper");
  lua_pushcfunction(state, lua_unreal_fstring_to_lower_lua);
  lua_setfield(state, -2, "ToLower");

  if (owned) {
    lua_pushcfunction(state, lua_unreal_owned_gc);
    lua_setfield(state, -2, "__gc");
  }

  lua_pushliteral(state, "UnrealValue");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

static void
lua_unreal_register_param_metatable(lua_State *state)
{
  luaL_newmetatable(state, LUA_UNREAL_PARAM_MT);
  lua_pushcfunction(state, lua_unreal_param_get_lua);
  lua_setfield(state, -2, "get");
  lua_pushcfunction(state, lua_unreal_param_get_lua);
  lua_setfield(state, -2, "Get");
  lua_pushcfunction(state, lua_unreal_param_set_lua);
  lua_setfield(state, -2, "set");
  lua_pushcfunction(state, lua_unreal_param_set_lua);
  lua_setfield(state, -2, "Set");
  lua_pushcfunction(state, lua_unreal_param_is_valid_lua);
  lua_setfield(state, -2, "IsValid");
  lua_pushcfunction(state, lua_unreal_param_type_lua);
  lua_setfield(state, -2, "type");
  lua_pushvalue(state, -1);
  lua_setfield(state, -2, "__index");
  lua_pushliteral(state, "UnrealParam");
  lua_setfield(state, -2, "__name");
  lua_pop(state, 1);
}

bool
lua_unreal_register_static(lua_State *state, lua_unreal_context_t *context)
{
  if (!state || !context) {
    return false;
  }

  lua_pushlightuserdata(state, context);
  lua_rawsetp(state, LUA_REGISTRYINDEX, &g_lua_unreal_context_registry_key);
  lua_unreal_register_fname_metatable(state);
  lua_unreal_register_ftext_metatable(state);

  static const luaL_Reg static_functions[] = {
    {"StaticFindObject", lua_unreal_static_find_object_lua},
    {"FindObject",       lua_unreal_find_object_lua       },
    {"FindObjects",      lua_unreal_find_objects_lua      },
    {"FindFirstOf",      lua_unreal_find_first_of_lua     },
    {"FindAllOf",        lua_unreal_find_all_of_lua       },
    {"LoadAsset",        lua_unreal_load_asset_lua        },
    {"FText",            lua_unreal_ftext_lua             },
    {NULL, NULL},
  };

  lua_newtable(state);
  luaL_setfuncs(state, static_functions, 0);
  lua_unreal_register_object_flags(state);
  lua_setfield(state, -2, "EObjectFlags");
  lua_unreal_register_internal_object_flags(state);
  lua_setfield(state, -2, "EInternalObjectFlags");
  lua_unreal_register_property_types(state);
  lua_setfield(state, -2, "PropertyTypes");
  lua_pushcfunction(state, lua_unreal_fname_lua);
  lua_setfield(state, -2, "FName");
  lua_unreal_push_find_name_table(state);
  lua_setfield(state, -2, "EFindName");
  lua_unreal_fname_push(state, (fname_t){0});
  lua_setfield(state, -2, "NAME_None");

  for (const luaL_Reg *function = static_functions; function->name; ++function) {
    lua_getfield(state, -1, function->name);
    lua_setglobal(state, function->name);
  }

  static const char *aliases[] = {
    "EObjectFlags",
    "EInternalObjectFlags",
    "PropertyTypes",
    "FName",
    "EFindName",
    "NAME_None",
  };

  for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
    lua_getfield(state, -1, aliases[i]);
    lua_setglobal(state, aliases[i]);
  }
  lua_setglobal(state, "Unreal");
  return true;
}

bool
lua_unreal_register_codec(lua_State *state, lua_unreal_context_t *context)
{
  if (!state || !context) {
    return false;
  }

  lua_pushlightuserdata(state, context);
  lua_rawsetp(state, LUA_REGISTRYINDEX, &g_lua_unreal_context_registry_key);
  lua_unreal_register_object_metatable(state);
  lua_unreal_register_data_table_row_metatable(state);
  lua_unreal_register_property_metatable(state);
  lua_unreal_register_field_class_metatable(state);
  lua_unreal_register_reflection_metatable(state);
  lua_unreal_register_value_metatable(state, LUA_UNREAL_PROXY_MT, false);
  lua_unreal_register_value_metatable(state, LUA_UNREAL_OWNED_MT, true);
  lua_unreal_register_param_metatable(state);
  lua_unreal_register_fname_metatable(state);
  lua_unreal_register_ftext_metatable(state);
  return true;
}

bool
lua_unreal_object_metatable_push(lua_State *state)
{
  if (!state) {
    return false;
  }

  luaL_getmetatable(state, LUA_UNREAL_OBJECT_MT);
  if (!lua_istable(state, -1)) {
    lua_pop(state, 1);
    return false;
  }
  return true;
}

static void
lua_unreal_metatable_rename(lua_State *state, const char *metatable, const char *old_name, const char *new_name)
{
  luaL_getmetatable(state, metatable);
  if (!lua_istable(state, -1)) {
    lua_pop(state, 1);
    return;
  }

  lua_getfield(state, -1, old_name);
  if (!lua_isnil(state, -1)) {
    lua_setfield(state, -2, new_name);
  } else {
    lua_pop(state, 1);
  }

  lua_pushnil(state);
  lua_setfield(state, -2, old_name);
  lua_pop(state, 1);
}

void
lua_unreal_use_camel_case_names(lua_State *state)
{
  if (!state) {
    return;
  }

  static const char *type_metatables[] = {
    LUA_UNREAL_OBJECT_MT,
    LUA_UNREAL_PROXY_MT,
    LUA_UNREAL_OWNED_MT,
    LUA_UNREAL_FNAME_MT,
    LUA_UNREAL_PARAM_MT,
    LUA_UNREAL_FTEXT_MT,
    LUA_UNREAL_DATA_TABLE_ROW_MT,
    LUA_UNREAL_PROPERTY_MT,
    LUA_UNREAL_FIELD_CLASS_MT,
    LUA_UNREAL_REFLECTION_MT,
  };

  for (size_t i = 0; i < COUNTOF(type_metatables); ++i) {
    lua_unreal_metatable_rename(state, type_metatables[i], "type", "Type");
  }

  lua_unreal_metatable_rename(state, LUA_UNREAL_PARAM_MT, "get", "Get");
  lua_unreal_metatable_rename(state, LUA_UNREAL_PARAM_MT, "set", "Set");
}

int
lua_unreal_fname_construct(lua_State *state)
{
  return lua_unreal_fname_lua(state);
}

int
lua_unreal_ftext_construct(lua_State *state)
{
  return lua_unreal_ftext_lua(state);
}

int
lua_unreal_name_none_push(lua_State *state)
{
  return lua_unreal_fname_push(state, (fname_t){0});
}

void
lua_unreal_object_flags_push(lua_State *state)
{
  lua_unreal_register_object_flags(state);
}

void
lua_unreal_internal_object_flags_push(lua_State *state)
{
  lua_unreal_register_internal_object_flags(state);
}

void
lua_unreal_property_types_push(lua_State *state)
{
  lua_unreal_register_property_types(state);
}

void
lua_unreal_find_name_modes_push(lua_State *state)
{
  lua_unreal_push_find_name_table(state);
}

bool
lua_unreal_register(lua_State *state, lua_unreal_context_t *context)
{
  if (!lua_unreal_register_codec(state, context)) {
    return false;
  }

  if (!lua_unreal_register_static(state, context)) {
    return false;
  }

  lua_pushcfunction(state, lua_unreal_create_invalid_object_lua);
  lua_setglobal(state, "CreateInvalidObject");
  lua_getglobal(state, "Unreal");
  lua_pushcfunction(state, lua_unreal_create_invalid_object_lua);
  lua_setfield(state, -2, "CreateInvalidObject");
  lua_pop(state, 1);
  return true;
}
