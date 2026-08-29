#include "unreal_reflect.h"

#include "globals.h"
#include "log.h"
#include "scratch.h"
#include "signatures.h"

#include <float.h>

typedef uint8_t unreal_reflect_class_state_t;
enum {
  UNREAL_REFLECT_CLASS_BUILDING,
  UNREAL_REFLECT_CLASS_READY,
  UNREAL_REFLECT_CLASS_FAILED,
};

typedef struct unreal_reflect_class_s unreal_reflect_class_t;
typedef struct unreal_reflect_func_s  unreal_reflect_func_t;

struct unreal_reflect_func_s {
  ufunc_t               *function;
  fprop_t              **params;
  uint32_t               num_params;
  fprop_t               *ret;
  unreal_func_impl_fn_t  impl;
  void                  *user;
  bool                   active;
};

struct unreal_reflect_class_s {
  unreal_reflect_class_t      *next;
  str_t                        scope;
  str_t                        name;
  fname_t                      engine_name;
  unreal_reflect_owner_t       owner;
  uclass_t                    *parent;
  uclass_t                    *cls;
  fprop_t                    **props;
  uint32_t                     num_props;
  unreal_reflect_func_t       *funcs;
  uint32_t                     num_funcs;
  uint64_t                     schema_hash;
  unreal_reflect_class_state_t state;
};

static unreal_reflect_class_t *g_classes;
static ufunc_t                *g_linking_func;

/* NOTE: class schemas and their rooted UClass objects intentionally survive every mod generation */

static bool
unreal_reflect_str_has_null(str_t text)
{
  bool found = text.len > 0 && !text.data;
  for (uint64_t i = 0; i < text.len && !found; ++i) {
    found = text.data[i] == 0;
  }
  return found;
}

static uint64_t
unreal_reflect_hash_bytes(uint64_t hash, const void *data, uint64_t size)
{
  const uint8_t *bytes = data;
  for (uint64_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static uint64_t
unreal_reflect_hash_append(uint64_t hash, str_t text)
{
  return unreal_reflect_hash_bytes(hash, text.data, text.len);
}

static uint64_t
unreal_reflect_class_hash(str_t scope, str_t name)
{
  uint64_t hash = unreal_reflect_hash_append(14695981039346656037ULL, scope);
  hash ^= 0;
  hash *= 1099511628211ULL;
  return unreal_reflect_hash_append(hash, name);
}

static uint64_t
unreal_reflect_hash_u64(uint64_t hash, uint64_t value)
{
  return unreal_reflect_hash_append(hash, str_make(&value, sizeof(value)));
}

static bool
unreal_reflect_class_def_has_props(const unreal_class_def_t *def)
{
  uint32_t props_end = offsetof(unreal_class_def_t, num_props) + sizeof(def->num_props);
  return def->struct_size >= props_end;
}

static uint32_t
unreal_reflect_class_prop_count(const unreal_class_def_t *def)
{
  uint32_t count = 0;
  if (unreal_reflect_class_def_has_props(def)) {
    count = def->num_props;
  }
  return count;
}

static bool
unreal_reflect_class_def_has_funcs(const unreal_class_def_t *def)
{
  uint32_t funcs_end = offsetof(unreal_class_def_t, num_funcs) + sizeof(def->num_funcs);
  return def->struct_size >= funcs_end;
}

static uint32_t
unreal_reflect_class_func_count(const unreal_class_def_t *def)
{
  uint32_t count = 0;
  if (unreal_reflect_class_def_has_funcs(def)) {
    count = def->num_funcs;
  }
  return count;
}

static unreal_prop_type_t
unreal_reflect_prop_type(const unreal_prop_def_t *def)
{
  unreal_prop_type_t type = {
    .type        = def->type,
    .cls         = def->cls,
    .struct_type = def->struct_type,
    .enum_type   = def->enum_type,
  };

  if (def->type == UNREAL_PROP_KIND_ARRAY || def->type == UNREAL_PROP_KIND_SET || def->type == UNREAL_PROP_KIND_MAP || def->type == UNREAL_PROP_KIND_ENUM) {
    type.inner = &def->inner;
  }

  if (def->type == UNREAL_PROP_KIND_MAP) {
    type.value = &def->value;
  }
  return type;
}

static bool
unreal_reflect_kind_is_signed_integer(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_INT8 || type == UNREAL_PROP_KIND_INT16 || type == UNREAL_PROP_KIND_INT32 || type == UNREAL_PROP_KIND_INT64;
}

static bool
unreal_reflect_kind_is_unsigned_integer(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_BYTE || type == UNREAL_PROP_KIND_UINT16 || type == UNREAL_PROP_KIND_UINT32 || type == UNREAL_PROP_KIND_UINT64;
}

static bool
unreal_reflect_kind_is_integer(unreal_prop_kind_t type)
{
  return unreal_reflect_kind_is_signed_integer(type) || unreal_reflect_kind_is_unsigned_integer(type);
}

static bool
unreal_reflect_kind_is_object_reference(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_OBJECT || type == UNREAL_PROP_KIND_CLASS || type == UNREAL_PROP_KIND_WEAK_OBJECT || type == UNREAL_PROP_KIND_LAZY_OBJECT;
}

static bool
unreal_reflect_kind_is_soft_reference(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_SOFT_OBJECT || type == UNREAL_PROP_KIND_SOFT_CLASS;
}

static bool
unreal_reflect_kind_is_string(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_STRING || type == UNREAL_PROP_KIND_TEXT;
}

static bool
unreal_reflect_kind_has_inner(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_ARRAY || type == UNREAL_PROP_KIND_SET || type == UNREAL_PROP_KIND_MAP || type == UNREAL_PROP_KIND_ENUM;
}

static uint32_t
unreal_reflect_scalar_size(unreal_prop_kind_t type)
{
  uint32_t size = 0;
  switch (type) {
    case UNREAL_PROP_KIND_BOOL:
    case UNREAL_PROP_KIND_BYTE:
    case UNREAL_PROP_KIND_INT8: {
      size = 1;
      break;
    }

    case UNREAL_PROP_KIND_INT16:
    case UNREAL_PROP_KIND_UINT16: {
      size = 2;
      break;
    }

    case UNREAL_PROP_KIND_INT32:
    case UNREAL_PROP_KIND_UINT32:
    case UNREAL_PROP_KIND_FLOAT: {
      size = 4;
      break;
    }

    case UNREAL_PROP_KIND_INT64:
    case UNREAL_PROP_KIND_UINT64:
    case UNREAL_PROP_KIND_DOUBLE: {
      size = 8;
      break;
    }

    default: {
      break;
    }
  }
  return size;
}

static uint64_t
unreal_reflect_type_hash(uint64_t hash, const unreal_prop_type_t *type)
{
  hash = unreal_reflect_hash_u64(hash, type->type);
  if (unreal_reflect_kind_is_object_reference(type->type) || unreal_reflect_kind_is_soft_reference(type->type) || type->type == UNREAL_PROP_KIND_INTERFACE) {
    hash = unreal_reflect_hash_u64(hash, (uintptr_t)type->cls);
  } else if (type->type == UNREAL_PROP_KIND_STRUCT) {
    hash = unreal_reflect_hash_u64(hash, (uintptr_t)type->struct_type);
  } else if (type->type == UNREAL_PROP_KIND_ENUM) {
    hash = unreal_reflect_hash_u64(hash, (uintptr_t)type->enum_type);
  }

  if (unreal_reflect_kind_has_inner(type->type) && type->inner) {
    hash = unreal_reflect_type_hash(hash, type->inner);
  }

  if (type->type == UNREAL_PROP_KIND_MAP && type->value) {
    hash = unreal_reflect_type_hash(hash, type->value);
  }
  return hash;
}

static uint64_t
unreal_reflect_items_hash(uint64_t hash, const unreal_prop_type_t *type, const void *items, uint32_t count)
{
  uint32_t scalar_size = unreal_reflect_scalar_size(type->type);
  if (scalar_size > 0) {
    return unreal_reflect_hash_bytes(hash, items, (uint64_t)scalar_size * count);
  }

  for (uint32_t i = 0; i < count; ++i) {
    if (unreal_reflect_kind_is_object_reference(type->type) || type->type == UNREAL_PROP_KIND_INTERFACE) {
      uobject_t *const *objects = items;
      hash = unreal_reflect_hash_u64(hash, (uintptr_t)objects[i]);
    } else if (type->type == UNREAL_PROP_KIND_NAME               ||
               unreal_reflect_kind_is_string(type->type)         ||
               unreal_reflect_kind_is_soft_reference(type->type) ||
               type->type == UNREAL_PROP_KIND_STRUCT) {
      const str_t *texts = items;
      hash = unreal_reflect_hash_append(hash, texts[i]);
    } else if (type->type == UNREAL_PROP_KIND_ENUM) {
      const int64_t *values = items;
      hash = unreal_reflect_hash_u64(hash, (uint64_t)values[i]);
    }
  }
  return hash;
}

static uint64_t
unreal_reflect_prop_hash(uint64_t hash, const unreal_prop_def_t *def)
{
  unreal_prop_type_t type = unreal_reflect_prop_type(def);

  hash = unreal_reflect_hash_append(hash, def->name);
  hash = unreal_reflect_type_hash(hash, &type);
  hash = unreal_reflect_hash_u64(hash, def->flags);

  switch (def->type) {
    case UNREAL_PROP_KIND_BOOL: {
      hash = unreal_reflect_hash_u64(hash, def->boolean.default_val);
      break;
    }

    case UNREAL_PROP_KIND_INT8:
    case UNREAL_PROP_KIND_INT16:
    case UNREAL_PROP_KIND_INT32:
    case UNREAL_PROP_KIND_INT64: {
      hash = unreal_reflect_hash_u64(hash, (uint64_t)def->integer.default_val);
      break;
    }

    case UNREAL_PROP_KIND_BYTE:
    case UNREAL_PROP_KIND_UINT16:
    case UNREAL_PROP_KIND_UINT32:
    case UNREAL_PROP_KIND_UINT64: {
      hash = unreal_reflect_hash_u64(hash, def->unsigned_integer.default_val);
      break;
    }

    case UNREAL_PROP_KIND_FLOAT:
    case UNREAL_PROP_KIND_DOUBLE: {
      hash = unreal_reflect_hash_bytes(hash, &def->real.default_val, sizeof(def->real.default_val));
      break;
    }

    case UNREAL_PROP_KIND_OBJECT:
    case UNREAL_PROP_KIND_CLASS:
    case UNREAL_PROP_KIND_WEAK_OBJECT:
    case UNREAL_PROP_KIND_LAZY_OBJECT:
    case UNREAL_PROP_KIND_INTERFACE: {
      hash = unreal_reflect_hash_u64(hash, (uintptr_t)def->object.default_val);
      break;
    }

    case UNREAL_PROP_KIND_NAME: {
      hash = unreal_reflect_hash_append(hash, def->fname.default_val);
      break;
    }

    case UNREAL_PROP_KIND_STRING:
    case UNREAL_PROP_KIND_TEXT: {
      hash = unreal_reflect_hash_append(hash, def->string.default_val);
      break;
    }

    case UNREAL_PROP_KIND_SOFT_OBJECT:
    case UNREAL_PROP_KIND_SOFT_CLASS: {
      hash = unreal_reflect_hash_append(hash, def->soft.default_val);
      break;
    }

    case UNREAL_PROP_KIND_STRUCT: {
      hash = unreal_reflect_hash_append(hash, def->structure.default_val);
      break;
    }

    case UNREAL_PROP_KIND_ENUM: {
      hash = unreal_reflect_hash_u64(hash, (uint64_t)def->enumeration.default_val);
      break;
    }

    case UNREAL_PROP_KIND_ARRAY:
    case UNREAL_PROP_KIND_SET: {
      const unreal_prop_array_default_t *defaults = &def->array.default_val;
      if (def->type == UNREAL_PROP_KIND_SET) {
        defaults = &def->set.default_val;
      }

      hash = unreal_reflect_hash_u64(hash, defaults->count);
      hash = unreal_reflect_items_hash(hash, &def->inner, defaults->items, defaults->count);
      break;
    }

    case UNREAL_PROP_KIND_MAP: {
      hash = unreal_reflect_hash_u64(hash, def->map.default_val.count);
      hash = unreal_reflect_items_hash(hash, &def->inner, def->map.default_val.keys, def->map.default_val.count);
      hash = unreal_reflect_items_hash(hash, &def->value, def->map.default_val.values, def->map.default_val.count);
      break;
    }

    default: {
      break;
    }
  }
  return hash;
}

static uint64_t
unreal_reflect_func_hash(uint64_t hash, const unreal_func_def_t *def)
{
  hash = unreal_reflect_hash_append(hash, def->name);
  hash = unreal_reflect_hash_u64(hash, def->flags);
  hash = unreal_reflect_hash_u64(hash, def->num_params);

  for (uint32_t i = 0; i < def->num_params; ++i) {
    unreal_prop_type_t type = unreal_reflect_prop_type(&def->params[i]);

    hash = unreal_reflect_hash_append(hash, def->params[i].name);
    hash = unreal_reflect_type_hash(hash, &type);
    hash = unreal_reflect_hash_u64(hash, def->params[i].flags);
  }

  hash = unreal_reflect_type_hash(hash, &def->ret);
  return hash;
}

static uint64_t
unreal_reflect_schema_hash(const unreal_class_def_t *def)
{
  uint32_t prop_count = unreal_reflect_class_prop_count(def);
  uint32_t func_count = unreal_reflect_class_func_count(def);
  uint64_t hash       = unreal_reflect_hash_u64(14695981039346656037ULL, (uintptr_t)def->parent);

  hash = unreal_reflect_hash_u64(hash, prop_count);
  for (uint32_t i = 0; i < prop_count; ++i) {
    hash = unreal_reflect_prop_hash(hash, &def->props[i]);
  }

  hash = unreal_reflect_hash_u64(hash, func_count);
  for (uint32_t i = 0; i < func_count; ++i) {
    hash = unreal_reflect_func_hash(hash, &def->funcs[i]);
  }
  return hash;
}

static str_t
unreal_reflect_engine_name(uint8_t *buf, uint64_t cap, str_t scope, str_t name)
{
  uint64_t len = str_write_fmt(buf, cap, "OD_%016llX_", (unsigned long long)unreal_reflect_class_hash(scope, name));
  if (len >= cap) {
    len = cap - 1;
  }

  for (uint64_t i = 0; i < name.len && len + 1 < cap; ++i) {
    uint8_t ch    = name.data[i];
    bool    valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
    if (!valid) {
      ch = '_';
    }
    buf[len++] = ch;
  }

  buf[len] = 0;
  return str_make(buf, len);
}

static unreal_reflect_class_t *
unreal_reflect_class_find(str_t scope, str_t name)
{
  unreal_reflect_class_t *found = NULL;
  for (unreal_reflect_class_t *entry = g_classes; entry && !found; entry = entry->next) {
    if (str_equal(entry->scope, scope, 0) && str_equal(entry->name, name, 0)) {
      found = entry;
    }
  }
  return found;
}

static uobject_t *
unreal_reflect_object_find(uobject_t *outer, fname_t name)
{
  uobject_t *found = NULL;
  for (int i = 0, count = unreal_uobject_array_count(); i < count && !found; ++i) {
    uobject_t *object = unreal_uobject_array_get_obj(i);
    if (object && object->outer == outer && unreal_fname_equal(object->name, name, false)) {
      found = object;
    }
  }
  return found;
}

static bool
unreal_reflect_runtime_ready(void)
{
  bool on_game_thread = unreal_is_in_game_thread();
  bool have_types     = globals.unreal.core_class && globals.unreal.core_object && globals.unreal.transient_package;
  bool have_funcs     = static_construct_object && create_default_object && ustruct_static_link && uclass_assemble_ref_tok_stream;
  return globals.engine_inited && on_game_thread && have_types && have_funcs;
}

static bool
unreal_reflect_type_is_hashable(const unreal_prop_type_t *type)
{
  bool hashable = false;
  if (type) {
    hashable = type->type == UNREAL_PROP_KIND_BOOL || unreal_reflect_kind_is_integer(type->type);
    hashable = hashable || type->type == UNREAL_PROP_KIND_FLOAT || type->type == UNREAL_PROP_KIND_DOUBLE;
    hashable = hashable || type->type == UNREAL_PROP_KIND_NAME  || type->type == UNREAL_PROP_KIND_STRING;
    hashable = hashable || unreal_reflect_kind_is_object_reference(type->type) || unreal_reflect_kind_is_soft_reference(type->type);
    hashable = hashable || type->type == UNREAL_PROP_KIND_ENUM;
  }
  return hashable;
}

static bool
unreal_reflect_type_valid(const unreal_prop_type_t *type, bool allow_container)
{
  bool valid = type != NULL;
  if (valid) {
    switch (type->type) {
      case UNREAL_PROP_KIND_BOOL:
      case UNREAL_PROP_KIND_BYTE:
      case UNREAL_PROP_KIND_INT8:
      case UNREAL_PROP_KIND_INT16:
      case UNREAL_PROP_KIND_INT32:
      case UNREAL_PROP_KIND_INT64:
      case UNREAL_PROP_KIND_UINT16:
      case UNREAL_PROP_KIND_UINT32:
      case UNREAL_PROP_KIND_UINT64:
      case UNREAL_PROP_KIND_FLOAT:
      case UNREAL_PROP_KIND_DOUBLE:
      case UNREAL_PROP_KIND_NAME:
      case UNREAL_PROP_KIND_STRING:
      case UNREAL_PROP_KIND_TEXT: {
        break;
      }

      case UNREAL_PROP_KIND_OBJECT:
      case UNREAL_PROP_KIND_CLASS:
      case UNREAL_PROP_KIND_SOFT_OBJECT:
      case UNREAL_PROP_KIND_SOFT_CLASS:
      case UNREAL_PROP_KIND_WEAK_OBJECT:
      case UNREAL_PROP_KIND_LAZY_OBJECT: {
        valid = type->cls && unreal_uobject_is_a((uobject_t *)type->cls, globals.unreal.core_class);
        break;
      }

      case UNREAL_PROP_KIND_INTERFACE: {
        valid = type->cls && unreal_uobject_is_a((uobject_t *)type->cls, globals.unreal.core_class) && (type->cls->class_flags & CLASS_INTERFACE);
        break;
      }

      case UNREAL_PROP_KIND_STRUCT: {
        valid = type->struct_type && globals.unreal.core_scriptstruct &&
                unreal_uobject_is_a((uobject_t *)type->struct_type, globals.unreal.core_scriptstruct) &&
                type->struct_type->props_size >= 0 && type->struct_type->min_alignment > 0;
        break;
      }

      case UNREAL_PROP_KIND_ENUM: {
        valid = type->enum_type && globals.unreal.core_enum && unreal_uobject_is_a((uobject_t *)type->enum_type, globals.unreal.core_enum) &&
                type->inner && unreal_reflect_kind_is_integer(type->inner->type);
        break;
      }

      case UNREAL_PROP_KIND_ARRAY: {
        valid = allow_container && type->inner && unreal_reflect_type_valid(type->inner, false);
        break;
      }

      case UNREAL_PROP_KIND_SET: {
        valid = allow_container && type->inner;
        if (valid) {
          valid = unreal_reflect_type_valid(type->inner, false) && unreal_reflect_type_is_hashable(type->inner);
        }
        break;
      }

      case UNREAL_PROP_KIND_MAP: {
        valid = allow_container && type->inner && type->value;
        if (valid) {
          valid = unreal_reflect_type_valid(type->inner, false) && unreal_reflect_type_valid(type->value, false);
        }

        if (valid) {
          valid = unreal_reflect_type_is_hashable(type->inner);
        }
        break;
      }

      default: {
        valid = false;
        break;
      }
    }
  }
  return valid;
}

static bool
unreal_reflect_signed_default_valid(unreal_prop_kind_t type, int64_t value)
{
  bool valid = true;
  if (type == UNREAL_PROP_KIND_INT8) {
    valid = value >= INT8_MIN && value <= INT8_MAX;
  } else if (type == UNREAL_PROP_KIND_INT16) {
    valid = value >= INT16_MIN && value <= INT16_MAX;
  } else if (type == UNREAL_PROP_KIND_INT32) {
    valid = value >= INT32_MIN && value <= INT32_MAX;
  }
  return valid;
}

static bool
unreal_reflect_unsigned_default_valid(unreal_prop_kind_t type, uint64_t value)
{
  bool valid = true;
  if (type == UNREAL_PROP_KIND_BYTE) {
    valid = value <= UINT8_MAX;
  } else if (type == UNREAL_PROP_KIND_UINT16) {
    valid = value <= UINT16_MAX;
  } else if (type == UNREAL_PROP_KIND_UINT32) {
    valid = value <= UINT32_MAX;
  }
  return valid;
}

static bool
unreal_reflect_object_default_valid(const unreal_prop_type_t *type, uobject_t *object)
{
  bool valid = true;
  if (object && type->type == UNREAL_PROP_KIND_INTERFACE) {
    valid = unreal_uobject_get_interface_address(object, type->cls) != NULL;
  } else if (object && type->type == UNREAL_PROP_KIND_CLASS) {
    valid = unreal_uobject_is_a(object, globals.unreal.core_class);
    if (valid) {
      valid = unreal_uclass_is_child_of((uclass_t *)object, type->cls);
    }
  } else if (object) {
    valid = unreal_uobject_is_a(object, type->cls);
  }
  return valid;
}

static bool
unreal_reflect_text_default_valid(str_t text)
{
  return !unreal_reflect_str_has_null(text);
}

static bool
unreal_reflect_enum_default_valid(const unreal_prop_type_t *type, int64_t value)
{
  bool valid = type && type->inner;
  if (valid && unreal_reflect_kind_is_signed_integer(type->inner->type)) {
    valid = unreal_reflect_signed_default_valid(type->inner->type, value);
  } else if (valid && unreal_reflect_kind_is_unsigned_integer(type->inner->type)) {
    valid = value >= 0 && unreal_reflect_unsigned_default_valid(type->inner->type, (uint64_t)value);
  } else {
    valid = false;
  }
  return valid;
}

static bool
unreal_reflect_item_valid(const unreal_prop_type_t *type, const void *items, uint32_t index)
{
  bool valid = true;
  if (unreal_reflect_kind_is_object_reference(type->type) || type->type == UNREAL_PROP_KIND_INTERFACE) {
    uobject_t *const *objects = items;
    valid = unreal_reflect_object_default_valid(type, objects[index]);
  } else if (type->type == UNREAL_PROP_KIND_NAME) {
    const str_t *texts = items;
    valid = !unreal_reflect_str_has_null(texts[index]) && texts[index].len < FNAME_ENTRY_NAME_SIZE;
  } else if (unreal_reflect_kind_is_string(type->type)         ||
             unreal_reflect_kind_is_soft_reference(type->type) ||
             type->type == UNREAL_PROP_KIND_STRUCT) {
    const str_t *texts = items;
    valid = unreal_reflect_text_default_valid(texts[index]);
  } else if (type->type == UNREAL_PROP_KIND_ENUM) {
    const int64_t *values = items;
    valid = unreal_reflect_enum_default_valid(type, values[index]);
  }
  return valid;
}

static bool
unreal_reflect_items_valid(const unreal_prop_type_t *type, const void *items, uint32_t count)
{
  bool valid = count <= INT32_MAX && (count == 0 || items);
  for (uint32_t i = 0; i < count && valid; ++i) {
    valid = unreal_reflect_item_valid(type, items, i);
  }
  return valid;
}

static bool
unreal_reflect_default_valid(const unreal_prop_def_t *def)
{
  unreal_prop_type_t type  = unreal_reflect_prop_type(def);
  bool valid = true;
  if (unreal_reflect_kind_is_signed_integer(def->type)) {
    valid = unreal_reflect_signed_default_valid(def->type, def->integer.default_val);
  } else if (unreal_reflect_kind_is_unsigned_integer(def->type)) {
    valid = unreal_reflect_unsigned_default_valid(def->type, def->unsigned_integer.default_val);
  } else if (def->type == UNREAL_PROP_KIND_FLOAT) {
    double value = def->real.default_val;
    valid        = value != value || (value >= -FLT_MAX && value <= FLT_MAX);
  } else if (unreal_reflect_kind_is_object_reference(def->type) || def->type == UNREAL_PROP_KIND_INTERFACE) {
    valid = unreal_reflect_object_default_valid(&type, def->object.default_val);
  } else if (def->type == UNREAL_PROP_KIND_NAME) {
    valid = !unreal_reflect_str_has_null(def->fname.default_val) && def->fname.default_val.len < FNAME_ENTRY_NAME_SIZE;
  } else if (unreal_reflect_kind_is_string(def->type)) {
    valid = unreal_reflect_text_default_valid(def->string.default_val);
  } else if (unreal_reflect_kind_is_soft_reference(def->type)) {
    valid = unreal_reflect_text_default_valid(def->soft.default_val);
  } else if (def->type == UNREAL_PROP_KIND_STRUCT) {
    valid = unreal_reflect_text_default_valid(def->structure.default_val);
  } else if (def->type == UNREAL_PROP_KIND_ENUM) {
    valid = unreal_reflect_enum_default_valid(&type, def->enumeration.default_val);
  } else if (def->type == UNREAL_PROP_KIND_ARRAY) {
    valid = unreal_reflect_items_valid(&def->inner, def->array.default_val.items, def->array.default_val.count);
  } else if (def->type == UNREAL_PROP_KIND_SET) {
    valid = unreal_reflect_items_valid(&def->inner, def->set.default_val.items, def->set.default_val.count);
  } else if (def->type == UNREAL_PROP_KIND_MAP) {
    valid = unreal_reflect_items_valid(&def->inner, def->map.default_val.keys, def->map.default_val.count);
    if (valid) {
      valid = unreal_reflect_items_valid(&def->value, def->map.default_val.values, def->map.default_val.count);
    }
  }
  return valid;
}

static bool
unreal_reflect_prop_valid(const unreal_class_def_t *class_def, uint32_t index)
{
  const unreal_prop_def_t *def  = &class_def->props[index];
  unreal_prop_type_t       type = unreal_reflect_prop_type(def);
  bool valid = !str_is_empty(def->name) && def->name.len <= 256 && !unreal_reflect_str_has_null(def->name);
  if (valid) {
    valid = unreal_reflect_type_valid(&type, true) && unreal_reflect_default_valid(def);
  }

  eprop_flags_t invalid_flags = CPF_PARM | CPF_OUT_PARM | CPF_RETURN_PARM | CPF_REFERENCE_PARM | CPF_CONST_PARM | CPF_NET | CPF_REP_NOTIFY;
  if (valid) {
    valid = (def->flags & invalid_flags) == 0;
  }

  for (fprop_t *prop = class_def->parent->prop_link; prop && valid; prop = prop->prop_link_next) {
    valid = !unreal_fname_match_text(prop->name, def->name, true, true);
  }

  for (uint32_t i = 0; i < index && valid; ++i) {
    valid = !str_equal(class_def->props[i].name, def->name, STR_CMP_FLAG_IGNORE_CASE);
  }
  return valid;
}

static bool
unreal_reflect_func_param_valid(const unreal_func_def_t *func, uint32_t index)
{
  const unreal_prop_def_t *def  = &func->params[index];
  unreal_prop_type_t       type = unreal_reflect_prop_type(def);

  bool valid = !str_is_empty(def->name) && def->name.len <= 256 && !unreal_reflect_str_has_null(def->name);
  if (valid) {
    valid = !str_equal(def->name, STR_LIT("ReturnValue"), STR_CMP_FLAG_IGNORE_CASE);
  }

  if (valid) {
    valid = unreal_reflect_type_valid(&type, true) && (def->flags & ~CPF_CONST_PARM) == 0;
  }

  for (uint32_t i = 0; i < index && valid; ++i) {
    valid = !str_equal(func->params[i].name, def->name, STR_CMP_FLAG_IGNORE_CASE);
  }
  return valid;
}

static bool
unreal_reflect_func_valid(const unreal_class_def_t *class_def, uint32_t index)
{
  const unreal_func_def_t *def = &class_def->funcs[index];

  bool valid = !str_is_empty(def->name) && def->name.len <= 256 && !unreal_reflect_str_has_null(def->name) && def->impl;
  if (valid) {
    valid = def->num_params <= UINT8_MAX && (def->num_params == 0 || def->params);
  }

  efunc_flags_t allowed_flags = FUNC_FLAG_FINAL | FUNC_FLAG_PUBLIC | FUNC_FLAG_PRIVATE | FUNC_FLAG_PROTECTED | FUNC_FLAG_BLUEPRINT_CALLABLE | FUNC_FLAG_BLUEPRINT_PURE | FUNC_FLAG_CONST;
  if (valid) {
    valid = (def->flags & ~allowed_flags) == 0;
  }

  efunc_flags_t visibility = def->flags & (FUNC_FLAG_PUBLIC | FUNC_FLAG_PRIVATE | FUNC_FLAG_PROTECTED);
  if (valid) {
    valid = visibility == 0 || visibility == FUNC_FLAG_PUBLIC || visibility == FUNC_FLAG_PRIVATE || visibility == FUNC_FLAG_PROTECTED;
  }

  for (uint32_t i = 0; i < def->num_params && valid; ++i) {
    valid = unreal_reflect_func_param_valid(def, i);
  }

  bool has_ret = def->ret.type != UNREAL_PROP_KIND_UNKNOWN;
  if (valid && has_ret) {
    valid = unreal_reflect_type_valid(&def->ret, true) && def->num_params < UINT8_MAX;
  }

  if (valid) {
    valid = !unreal_ustruct_find_func((ustruct_t *)class_def->parent, def->name);
  }

  for (uint32_t i = 0; i < index && valid; ++i) {
    valid = !str_equal(class_def->funcs[i].name, def->name, STR_CMP_FLAG_IGNORE_CASE);
  }
  return valid;
}

static bool
unreal_reflect_class_def_valid(str_t scope, str_t name, const unreal_class_def_t *def)
{
  bool text_valid = !str_is_empty(scope) && !str_is_empty(name) && name.len <= 256;
  if (text_valid) {
    text_valid = !unreal_reflect_str_has_null(scope) && !unreal_reflect_str_has_null(name);
  }

  uint32_t min_def_size = offsetof(unreal_class_def_t, parent) + sizeof(def->parent);
  bool     def_valid    = def && def->struct_size >= min_def_size && def->parent;
  if (def_valid) {
    def_valid = unreal_uobject_is_a((uobject_t *)def->parent, globals.unreal.core_class);
  }

  uint32_t count = 0;
  if (def_valid) {
    count = unreal_reflect_class_prop_count(def);
  }

  if (def_valid && count > 0) {
    def_valid = def->props != NULL;
  }

  for (uint32_t i = 0; i < count && def_valid; ++i) {
    def_valid = unreal_reflect_prop_valid(def, i);
  }

  uint32_t func_count = 0;
  if (def_valid) {
    func_count = unreal_reflect_class_func_count(def);
  }

  if (def_valid && func_count > 0) {
    def_valid = def->funcs != NULL;
  }

  for (uint32_t i = 0; i < func_count && def_valid; ++i) {
    def_valid = unreal_reflect_func_valid(def, i);
  }
  return text_valid && def_valid;
}

static fname_t
unreal_reflect_type_name(unreal_prop_kind_t type)
{
  fname_t name = FNAME_NONE;
  switch (type) {
    case UNREAL_PROP_KIND_BOOL: {
      name = globals.unreal.bool_prop;
      break;
    }

    case UNREAL_PROP_KIND_BYTE: {
      name = globals.unreal.byte_prop;
      break;
    }

    case UNREAL_PROP_KIND_INT8: {
      name = globals.unreal.int8_prop;
      break;
    }

    case UNREAL_PROP_KIND_INT16: {
      name = globals.unreal.int16_prop;
      break;
    }

    case UNREAL_PROP_KIND_INT32: {
      name = globals.unreal.int_prop;
      break;
    }

    case UNREAL_PROP_KIND_INT64: {
      name = globals.unreal.int64_prop;
      break;
    }

    case UNREAL_PROP_KIND_UINT16: {
      name = globals.unreal.uint16_prop;
      break;
    }

    case UNREAL_PROP_KIND_UINT32: {
      name = globals.unreal.uint32_prop;
      break;
    }

    case UNREAL_PROP_KIND_UINT64: {
      name = globals.unreal.uint64_prop;
      break;
    }

    case UNREAL_PROP_KIND_FLOAT: {
      name = globals.unreal.float_prop;
      break;
    }

    case UNREAL_PROP_KIND_DOUBLE: {
      name = globals.unreal.double_prop;
      break;
    }

    case UNREAL_PROP_KIND_OBJECT: {
      name = globals.unreal.obj_prop;
      break;
    }

    case UNREAL_PROP_KIND_NAME: {
      name = globals.unreal.name_prop;
      break;
    }

    case UNREAL_PROP_KIND_STRING: {
      name = globals.unreal.str_prop;
      break;
    }

    case UNREAL_PROP_KIND_TEXT: {
      name = globals.unreal.text_prop;
      break;
    }

    case UNREAL_PROP_KIND_CLASS: {
      name = globals.unreal.class_prop;
      break;
    }

    case UNREAL_PROP_KIND_SOFT_OBJECT: {
      name = globals.unreal.soft_obj_prop;
      break;
    }

    case UNREAL_PROP_KIND_SOFT_CLASS: {
      name = globals.unreal.soft_class_prop;
      break;
    }

    case UNREAL_PROP_KIND_WEAK_OBJECT: {
      name = globals.unreal.weak_obj_prop;
      break;
    }

    case UNREAL_PROP_KIND_LAZY_OBJECT: {
      name = globals.unreal.lazy_obj_prop;
      break;
    }

    case UNREAL_PROP_KIND_INTERFACE: {
      name = globals.unreal.interface_prop;
      break;
    }

    case UNREAL_PROP_KIND_STRUCT: {
      name = globals.unreal.struct_prop;
      break;
    }

    case UNREAL_PROP_KIND_ENUM: {
      name = globals.unreal.enum_prop;
      break;
    }

    case UNREAL_PROP_KIND_ARRAY: {
      name = globals.unreal.array_prop;
      break;
    }

    case UNREAL_PROP_KIND_SET: {
      name = globals.unreal.set_prop;
      break;
    }

    case UNREAL_PROP_KIND_MAP: {
      name = globals.unreal.map_prop;
      break;
    }

    default: {
      break;
    }
  }
  return name;
}

static bool
unreal_reflect_type_runtime_ready(const unreal_prop_type_t *type)
{
  bool ready = type && !unreal_fname_is_none(unreal_reflect_type_name(type->type));
  if (ready && unreal_reflect_kind_has_inner(type->type)) {
    ready = unreal_reflect_type_runtime_ready(type->inner);
  }

  if (ready && type->type == UNREAL_PROP_KIND_MAP) {
    ready = unreal_reflect_type_runtime_ready(type->value);
  }
  return ready;
}

static bool
unreal_reflect_schema_runtime_ready(const unreal_class_def_t *def)
{
  uint32_t prop_count = unreal_reflect_class_prop_count(def);
  uint32_t func_count = unreal_reflect_class_func_count(def);

  bool ready = (prop_count == 0 && func_count == 0) || ffield_construct;
  for (uint32_t i = 0; i < prop_count && ready; ++i) {
    unreal_prop_type_t type = unreal_reflect_prop_type(&def->props[i]);
    ready                   = unreal_reflect_type_runtime_ready(&type);

    if (ready && type.type == UNREAL_PROP_KIND_ARRAY && def->props[i].array.default_val.count > 0) {
      ready = generic_array_resize != NULL;
    } else if (ready && type.type == UNREAL_PROP_KIND_SET && def->props[i].set.default_val.count > 0) {
      ready = fscript_set_add_elem != NULL;
    } else if (ready && type.type == UNREAL_PROP_KIND_MAP && def->props[i].map.default_val.count > 0) {
      ready = fscript_map_add_pair != NULL;
    }
  }

  for (uint32_t i = 0; i < func_count && ready; ++i) {
    const unreal_func_def_t *func = &def->funcs[i];
    for (uint32_t j = 0; j < func->num_params && ready; ++j) {
      unreal_prop_type_t type = unreal_reflect_prop_type(&func->params[j]);
      ready                   = unreal_reflect_type_runtime_ready(&type);
    }

    if (ready && func->ret.type != UNREAL_PROP_KIND_UNKNOWN) {
      ready = unreal_reflect_type_runtime_ready(&func->ret);
    }
  }

  if (ready && func_count > 0) {
    ready = globals.unreal.core_func && uclass_create_link_and_add_child_funcs_to_map && uclass_add_native_func;
  }
  return ready;
}

static bool
unreal_reflect_attach_prop(ffield_variant_t owner, fprop_t *prop)
{
  bool attached = prop != NULL;
  if (attached && owner.is_uobject) {
    ufield_t *field = (ufield_t *)owner.container.obj;
    attached        = field && field->vtable && field->vtable->add_cpp_prop;
    if (attached) {
      field->vtable->add_cpp_prop(field, prop);
    }
  } else if (attached) {
    ffield_t *field = owner.container.field;
    attached        = field && field->vtable && field->vtable->add_cpp_prop;
    if (attached) {
      field->vtable->add_cpp_prop(field, prop);
    }
  }
  return attached;
}

static bool
unreal_reflect_configure_prop(fprop_t *prop, const unreal_prop_type_t *type)
{
  bool configured  = prop != NULL;
  bool object_kind = unreal_reflect_kind_is_object_reference(type->type) || unreal_reflect_kind_is_soft_reference(type->type);
  if (configured && object_kind) {
    ((fprop_obj_base_t *)prop)->prop_class = type->cls;

    if (type->type == UNREAL_PROP_KIND_CLASS) {
      ((fprop_obj_base_t *)prop)->prop_class = globals.unreal.core_class;
      ((fprop_class_t *)prop)->meta_class     = type->cls;
    } else if (type->type == UNREAL_PROP_KIND_SOFT_CLASS) {
      ((fprop_obj_base_t *)prop)->prop_class = globals.unreal.core_class;
      ((fprop_class_soft_t *)prop)->meta_class = type->cls;
    }
  } else if (configured && type->type == UNREAL_PROP_KIND_STRUCT) {
    ((fprop_struct_t *)prop)->script_struct = type->struct_type;
  } else if (configured && type->type == UNREAL_PROP_KIND_ENUM) {
    ((fprop_enum_t *)prop)->uenum = type->enum_type;
  } else if (configured && type->type == UNREAL_PROP_KIND_INTERFACE) {
    ((fprop_iface_t *)prop)->iface_class = type->cls;
  }
  return configured;
}

static bool
unreal_reflect_prop_has_inner(unreal_prop_kind_t type)
{
  return type == UNREAL_PROP_KIND_ARRAY || type == UNREAL_PROP_KIND_SET || type == UNREAL_PROP_KIND_ENUM;
}

static bool
unreal_reflect_inner_matches(fprop_t *prop, unreal_prop_kind_t type, fprop_t *inner)
{
  bool matches = false;
  if (type == UNREAL_PROP_KIND_ARRAY) {
    matches = ((fprop_array_t *)prop)->inner == inner;
  } else if (type == UNREAL_PROP_KIND_SET) {
    matches = ((fprop_set_t *)prop)->elem_prop == inner;
  } else if (type == UNREAL_PROP_KIND_ENUM) {
    matches = (fprop_t *)((fprop_enum_t *)prop)->underlying_prop == inner;
  }
  return matches;
}

static fprop_t *
unreal_reflect_construct_prop(ffield_variant_t owner, fname_t name, const unreal_prop_type_t *type, bool hash_required)
{
  fname_t type_name = unreal_reflect_type_name(type->type);
  fprop_t *prop     = (fprop_t *)ffield_construct(&type_name, &owner, &name, RF_TRANSIENT);
  if (!unreal_reflect_configure_prop(prop, type)) {
    prop = NULL;
  }

  if (prop && hash_required) {
    /* NOTE: Set/Map AddCppProperty checks this before StaticLink computes the property type's normal flags;
       validation limits this to known hashable types */
    prop->prop_flags |= CPF_HAS_GET_VALUE_TYPE_HASH;
  }

  if (prop && unreal_reflect_prop_has_inner(type->type)) {
    ffield_variant_t inner_owner = {
      .container.field = (ffield_t *)prop,
      .is_uobject      = false,
    };

    bool     inner_hash_required = type->type == UNREAL_PROP_KIND_SET;
    fprop_t *inner               = unreal_reflect_construct_prop(inner_owner, FNAME_NONE, type->inner, inner_hash_required);
    if (!inner || !unreal_reflect_inner_matches(prop, type->type, inner)) {
      prop = NULL;
    }
  }

  if (prop && type->type == UNREAL_PROP_KIND_MAP) {
    ffield_variant_t inner_owner = {
      .container.field = (ffield_t *)prop,
      .is_uobject      = false,
    };

    fprop_t *key = unreal_reflect_construct_prop(inner_owner, FNAME_NONE, type->inner, true);
    fprop_t *val = NULL;
    if (key) {
      val = unreal_reflect_construct_prop(inner_owner, FNAME_NONE, type->value, false);
    }

    if (!key || !val || ((fprop_map_t *)prop)->key_prop != key || ((fprop_map_t *)prop)->val_prop != val) {
      prop = NULL;
    }
  }

  if (prop && !unreal_reflect_attach_prop(owner, prop)) {
    prop = NULL;
  }
  return prop;
}

static bool
unreal_reflect_construct_props(unreal_reflect_class_t *entry, const unreal_class_def_t *def)
{
  ffield_variant_t owner = {
    .container.obj = (uobject_t *)entry->cls,
    .is_uobject    = true,
  };

  bool built = true;
  for (uint32_t remaining = entry->num_props; remaining > 0 && built; --remaining) {
    uint32_t                 index    = remaining - 1;
    const unreal_prop_def_t *prop_def = &def->props[index];
    unreal_prop_type_t       type     = unreal_reflect_prop_type(prop_def);
    fname_t                  name     = unreal_fname_from_str(prop_def->name, FNAME_FIND_OR_ADD);
    fprop_t                 *prop     = unreal_reflect_construct_prop(owner, name, &type, false);
    if (prop) {
      prop->prop_flags    |= prop_def->flags;
      entry->props[index] = prop;
    } else {
      built = false;
    }
  }
  return built;
}

static unreal_reflect_func_t *
unreal_reflect_func_find(ufunc_t *function)
{
  unreal_reflect_func_t *found = NULL;
  for (unreal_reflect_class_t *cls = g_classes; cls && !found; cls = cls->next) {
    for (uint32_t i = 0; i < cls->num_funcs && !found; ++i) {
      if (cls->funcs[i].function == function) {
        found = &cls->funcs[i];
      }
    }
  }
  return found;
}

static void
unreal_reflect_func_call(unreal_reflect_func_t *entry, uobject_t *object, void *params, void *result)
{
  if (entry->active && entry->impl) {
    unreal_func_call_t call = {
      .object       = object,
      .function     = entry->function,
      .params       = params,
      .return_value = entry->ret ? result : NULL,
      .user         = entry->user,
    };
    entry->impl(&call);
  }
}

static bool
unreal_reflect_func_step_params(unreal_reflect_func_t *entry, fframe_t *stack, void *params)
{
  bool stepped = true;
  for (uint32_t i = 0; i < entry->num_params && stepped; ++i) {
    void *value = unreal_fprop_value_in_container(entry->params[i], params, 0);
    stepped     = value && unreal_fframe_step(stack, value);
  }

  if (stepped) {
    stepped = stack->code && *stack->code == EX_END_FUNCTION_PARMS;
  }

  if (stepped) {
    stack->code += 1;
  }
  return stepped;
}

static void __fastcall
unreal_reflect_func_thunk(uobject_t *object, fframe_t *stack, void *result)
{
  ufunc_t *function = stack ? stack->current_native_func : NULL;
  if (!function && stack) {
    function = stack->node;
  }

  unreal_reflect_func_t *entry = unreal_reflect_func_find(function);
  if (!entry || !stack) {
    LOG_ERROR("cannot dispatch an unregistered custom Unreal function");
    return;
  }

  if (!stack->code) {
    unreal_reflect_func_call(entry, object, stack->locals, result);
    return;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    uint64_t size      = MAX_VAL((uint64_t)function->params_size, 1ULL);
    uint64_t alignment = MAX_VAL((uint64_t)function->min_alignment, 1ULL);

    void *params      = arena_push_zero_aligned(tmp.arena, size, alignment);
    bool  initialized = params && unreal_ustruct_initialize_struct((ustruct_t *)function, params, 1);

    bool stepped = initialized && unreal_reflect_func_step_params(entry, stack, params);
    if (stepped) {
      unreal_reflect_func_call(entry, object, params, result);
    } else {
      LOG_ERROR("cannot read parameters for custom Unreal function");
    }

    if (initialized) {
      unreal_ustruct_destroy_struct((ustruct_t *)function, params, 1);
    }
  }
  scratch_end(tmp);
}

static ufunc_t *
unreal_reflect_linking_func(void)
{
  return g_linking_func;
}

static efunc_flags_t
unreal_reflect_func_flags(const unreal_func_def_t *def)
{
  efunc_flags_t flags      = def->flags | FUNC_FLAG_NATIVE;
  efunc_flags_t visibility = flags & (FUNC_FLAG_PUBLIC | FUNC_FLAG_PRIVATE | FUNC_FLAG_PROTECTED);
  if (visibility == 0) {
    flags |= FUNC_FLAG_PUBLIC;
  }
  return flags;
}

static bool
unreal_reflect_construct_func_params(unreal_reflect_func_t *entry, const unreal_func_def_t *def)
{
  ffield_variant_t owner = {
    .container.obj = (uobject_t *)entry->function,
    .is_uobject    = true,
  };

  bool built = true;
  if (def->ret.type != UNREAL_PROP_KIND_UNKNOWN) {
    fname_t name = unreal_fname_from_str(STR_LIT("ReturnValue"), FNAME_FIND_OR_ADD);
    entry->ret   = unreal_reflect_construct_prop(owner, name, &def->ret, false);
    built        = entry->ret != NULL;
    if (built) {
      entry->ret->prop_flags |= CPF_PARM | CPF_OUT_PARM | CPF_RETURN_PARM;
    }
  }

  for (uint32_t remaining = def->num_params; remaining > 0 && built; --remaining) {
    uint32_t                 index     = remaining - 1;
    const unreal_prop_def_t *param_def = &def->params[index];
    unreal_prop_type_t       type      = unreal_reflect_prop_type(param_def);
    fname_t                  name      = unreal_fname_from_str(param_def->name, FNAME_FIND_OR_ADD);

    fprop_t *param = unreal_reflect_construct_prop(owner, name, &type, false);
    if (param) {
      param->prop_flags    |= CPF_PARM | param_def->flags;
      entry->params[index] = param;
    } else {
      built = false;
    }
  }
  return built;
}

static bool
unreal_reflect_link_func(unreal_reflect_class_t *cls, unreal_reflect_func_t *entry, const unreal_func_def_t *def)
{
  bool        linked = false;
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    char    *name8  = str_push_cstr(tmp.arena, def->name);
    str16_t  name16 = str16_from_str(tmp.arena, def->name);
    wchar_t *wide   = str16_push_wstr(tmp.arena, name16);
    if (name8 && wide) {
      uclass_add_native_func(cls->cls, wide, unreal_reflect_func_thunk);
      entry->function->vtable->bind((ufield_t *)entry->function);
      ustruct_static_link((ustruct_t *)entry->function, true);

      fclass_func_link_info_t info = {
        .create_func_ptr = unreal_reflect_linking_func,
        .func_name       = name8,
      };

      ufunc_t *previous = g_linking_func;
      g_linking_func    = entry->function;
      uclass_create_link_and_add_child_funcs_to_map(cls->cls, &info, 1);
      g_linking_func = previous;

      ufunc_t *found = unreal_ustruct_find_func((ustruct_t *)cls->cls, def->name);
      linked = found == entry->function && entry->function->func != NULL;
    }
  }
  scratch_end(tmp);
  return linked;
}

static bool
unreal_reflect_func_layout_valid(unreal_reflect_func_t *entry, uint32_t expected_params)
{
  ufunc_t *function = entry->function;
  bool valid = function->num_params == expected_params && function->props_size >= 0 && function->props_size <= UINT16_MAX && function->min_alignment > 0;
  if (valid && expected_params > 0) {
    valid = function->params_size > 0;
  }

  uint32_t count = 0;
  for (ffield_t *field = function->child_props; field && valid; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    valid         = (prop->prop_flags & CPF_PARM) != 0 && prop->offset_internal >= 0;
    if (valid) {
      uint64_t size = unreal_fprop_complete_size(prop);
      valid = size > 0 && (uint64_t)prop->offset_internal <= function->params_size && size <= function->params_size - (uint64_t)prop->offset_internal;
    }
    count += 1;
  }

  valid = valid && count == expected_params;
  if (valid && entry->ret) {
    valid = function->return_val_offset == entry->ret->offset_internal;
  } else if (valid) {
    valid = function->return_val_offset == UINT16_MAX;
  }
  return valid;
}

static bool
unreal_reflect_construct_func(unreal_reflect_class_t *cls, uint32_t index, const unreal_func_def_t *def)
{
  unreal_reflect_func_t *entry = &cls->funcs[index];
  fstatic_construct_obj_params_t params = {
    .cls       = globals.unreal.core_func,
    .outer     = (uobject_t *)cls->cls,
    .name      = unreal_fname_from_str(def->name, FNAME_FIND_OR_ADD),
    .set_flags = RF_PUBLIC | RF_TRANSIENT,
  };

  entry->function   = (ufunc_t *)unreal_static_construct_object(&params);
  entry->num_params = def->num_params;
  entry->impl       = def->impl;
  entry->user       = def->user;
  entry->active     = false;
  if (def->num_params > 0) {
    entry->params = ARENA_PUSH_ARRAY_ZERO(&globals.perm, fprop_t *, def->num_params);
  }

  bool built = entry->function && (def->num_params == 0 || entry->params);
  if (built) {
    entry->function->func_flags = unreal_reflect_func_flags(def);
    built = unreal_reflect_construct_func_params(entry, def);
  }

  if (built) {
    built = unreal_reflect_link_func(cls, entry, def);
  }

  uint32_t expected_params = def->num_params + (def->ret.type != UNREAL_PROP_KIND_UNKNOWN);
  if (built) {
    built = unreal_reflect_func_layout_valid(entry, expected_params);
  }
  return built;
}

static bool
unreal_reflect_construct_funcs(unreal_reflect_class_t *entry, const unreal_class_def_t *def)
{
  bool built = true;
  for (uint32_t i = 0; i < entry->num_funcs && built; ++i) {
    built = unreal_reflect_construct_func(entry, i, &def->funcs[i]);
  }
  return built;
}

static void
unreal_reflect_update_funcs(unreal_reflect_class_t *entry, const unreal_class_def_t *def)
{
  for (uint32_t i = 0; i < entry->num_funcs; ++i) {
    entry->funcs[i].impl   = def->funcs[i].impl;
    entry->funcs[i].user   = def->funcs[i].user;
    entry->funcs[i].active = true;
  }
}

static bool
unreal_reflect_write_text(fprop_t *prop, void *value, uobject_t *owner, str_t text)
{
  bool written = false;
  if (str_is_empty(text)) {
    written = unreal_fprop_clear_single_value(prop, value);
  } else {
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      written = unreal_fprop_import_text(prop, value, owner, text, tmp.arena);
    }
    scratch_end(tmp);
  }
  return written;
}

static bool
unreal_reflect_write_struct(fprop_t *prop, void *value, uobject_t *owner, str_t text)
{
  bool written = true;
  if (!str_is_empty(text)) {
    written = unreal_reflect_write_text(prop, value, owner, text);
  }
  return written;
}

static bool
unreal_reflect_write_enum(fprop_t *prop, void *value, const unreal_prop_type_t *type, int64_t input)
{
  unreal_prop_integer_t integer = {
    .value     = (uint64_t)input,
    .is_signed = unreal_reflect_kind_is_signed_integer(type->inner->type),
  };
  return unreal_fprop_write_integer(prop, value, integer);
}

static bool
unreal_reflect_write_item(fprop_t *prop, void *value, uobject_t *owner, const unreal_prop_type_t *type, const void *items, uint32_t index)
{
  bool     written     = false;
  uint32_t scalar_size = unreal_reflect_scalar_size(type->type);
  if (scalar_size > 0) {
    const uint8_t *bytes = items;
    written = unreal_fprop_copy_single_value(prop, value, bytes + (uint64_t)index * scalar_size);
  } else if (unreal_reflect_kind_is_object_reference(type->type)) {
    uobject_t *const *objects = items;
    written = unreal_fprop_set_object((fprop_obj_base_t *)prop, value, objects[index]);
  } else if (type->type == UNREAL_PROP_KIND_INTERFACE) {
    uobject_t *const *objects = items;
    written = unreal_fprop_set_interface((fprop_iface_t *)prop, value, objects[index]);
  } else if (type->type == UNREAL_PROP_KIND_NAME) {
    const str_t *texts = items;
    fname_t      name  = FNAME_NONE;
    if (!str_is_empty(texts[index])) {
      name = unreal_fname_from_str(texts[index], FNAME_FIND_OR_ADD);
    }
    written = unreal_fprop_write_name(prop, value, name);
  } else if (unreal_reflect_kind_is_string(type->type) || unreal_reflect_kind_is_soft_reference(type->type)) {
    const str_t *texts = items;
    written = unreal_reflect_write_text(prop, value, owner, texts[index]);
  } else if (type->type == UNREAL_PROP_KIND_STRUCT) {
    const str_t *texts = items;
    written = unreal_reflect_write_struct(prop, value, owner, texts[index]);
  } else if (type->type == UNREAL_PROP_KIND_ENUM) {
    const int64_t *values = items;
    written = unreal_reflect_write_enum(prop, value, type, values[index]);
  }
  return written;
}

static void *
unreal_reflect_push_prop_value(arena_t *arena, fprop_t *prop)
{
  uint64_t size      = unreal_fprop_complete_size(prop);
  int32_t  alignment = unreal_fprop_min_alignment(prop);
  void    *value     = NULL;
  if (size > 0 && alignment > 0) {
    value = arena_push_zero_aligned(arena, size, (uint64_t)alignment);
  }
  return value;
}

static bool
unreal_reflect_array_default(fprop_array_t *prop, void *value, uobject_t *owner, const unreal_prop_def_t *def)
{
  int32_t count = (int32_t)def->array.default_val.count;
  if (count > 0) {
    unreal_array_resize(value, prop, count);
  }

  bool written = unreal_array_num(value, prop) == count;
  for (int32_t i = 0; i < count && written; ++i) {
    void *dst = unreal_array_get(value, prop, i);
    written  = unreal_reflect_write_item(prop->inner, dst, owner, &def->inner, def->array.default_val.items, (uint32_t)i);
  }
  return written;
}

static bool
unreal_reflect_set_default(fprop_set_t *prop, void *value, uobject_t *owner, const unreal_prop_def_t *def)
{
  bool        written = unreal_set_clear(value, prop);
  tmp_arena_t tmp     = scratch_begin(NULL);
  {
    void *item = unreal_reflect_push_prop_value(tmp.arena, prop->elem_prop);
    for (uint32_t i = 0; i < def->set.default_val.count && written; ++i) {
      bool initialized = item && unreal_fprop_initialize_value(prop->elem_prop, item);

      written = initialized && unreal_reflect_write_item(prop->elem_prop, item, owner, &def->inner, def->set.default_val.items, i);
      if (written) {
        unreal_set_add(value, prop, item);
        written = unreal_set_contains(value, prop, item);
      }

      if (initialized) {
        unreal_fprop_destroy_value(prop->elem_prop, item);
      }
    }
  }
  scratch_end(tmp);
  return written;
}

static bool
unreal_reflect_map_default(fprop_map_t *prop, void *value, uobject_t *owner, const unreal_prop_def_t *def)
{
  bool        written = unreal_map_clear(value, prop);
  tmp_arena_t tmp     = scratch_begin(NULL);
  {
    void *key = unreal_reflect_push_prop_value(tmp.arena, prop->key_prop);
    void *val = unreal_reflect_push_prop_value(tmp.arena, prop->val_prop);
    for (uint32_t i = 0; i < def->map.default_val.count && written; ++i) {
      bool  key_initialized = key && unreal_fprop_initialize_value(prop->key_prop, key);
      bool  val_initialized = val && unreal_fprop_initialize_value(prop->val_prop, val);

      written = key_initialized && val_initialized;
      if (written) {
        written = unreal_reflect_write_item(prop->key_prop, key, owner, &def->inner, def->map.default_val.keys, i);
      }

      if (written) {
        written = unreal_reflect_write_item(prop->val_prop, val, owner, &def->value, def->map.default_val.values, i);
      }

      if (written) {
        unreal_map_add(value, prop, key, val);
        void *stored = unreal_map_find_value_ptr(value, prop, key);
        written = stored && unreal_fprop_single_values_identical(prop->val_prop, stored, val, 0);
      }

      if (val_initialized) {
        unreal_fprop_destroy_value(prop->val_prop, val);
      }

      if (key_initialized) {
        unreal_fprop_destroy_value(prop->key_prop, key);
      }
    }
  }
  scratch_end(tmp);
  return written;
}

static bool
unreal_reflect_apply_default(fprop_t *prop, uobject_t *cdo, const unreal_prop_def_t *def)
{
  void *value   = unreal_fprop_value_in_container(prop, cdo, 0);
  bool  written = value != NULL;
  if (written && def->type == UNREAL_PROP_KIND_BOOL) {
    written = unreal_fprop_write_bool(prop, value, def->boolean.default_val);
  } else if (written && unreal_reflect_kind_is_signed_integer(def->type)) {
    unreal_prop_integer_t integer = {
      .value     = (uint64_t)def->integer.default_val,
      .is_signed = true,
    };
    written = unreal_fprop_write_integer(prop, value, integer);
  } else if (written && unreal_reflect_kind_is_unsigned_integer(def->type)) {
    unreal_prop_integer_t integer = {
      .value = def->unsigned_integer.default_val,
    };
    written = unreal_fprop_write_integer(prop, value, integer);
  } else if (written && (def->type == UNREAL_PROP_KIND_FLOAT || def->type == UNREAL_PROP_KIND_DOUBLE)) {
    written = unreal_fprop_write_real(prop, value, def->real.default_val);
  } else if (written && unreal_reflect_kind_is_object_reference(def->type)) {
    written = unreal_fprop_set_object((fprop_obj_base_t *)prop, value, def->object.default_val);
  } else if (written && def->type == UNREAL_PROP_KIND_INTERFACE) {
    written = unreal_fprop_set_interface((fprop_iface_t *)prop, value, def->object.default_val);
  } else if (written && def->type == UNREAL_PROP_KIND_NAME) {
    fname_t name = FNAME_NONE;
    if (!str_is_empty(def->fname.default_val)) {
      name = unreal_fname_from_str(def->fname.default_val, FNAME_FIND_OR_ADD);
    }
    written = unreal_fprop_write_name(prop, value, name);
  } else if (written && unreal_reflect_kind_is_string(def->type)) {
    written = unreal_reflect_write_text(prop, value, cdo, def->string.default_val);
  } else if (written && unreal_reflect_kind_is_soft_reference(def->type)) {
    written = unreal_reflect_write_text(prop, value, cdo, def->soft.default_val);
  } else if (written && def->type == UNREAL_PROP_KIND_STRUCT) {
    written = unreal_reflect_write_struct(prop, value, cdo, def->structure.default_val);
  } else if (written && def->type == UNREAL_PROP_KIND_ENUM) {
    unreal_prop_type_t type = unreal_reflect_prop_type(def);
    written = unreal_reflect_write_enum(prop, value, &type, def->enumeration.default_val);
  } else if (written && def->type == UNREAL_PROP_KIND_ARRAY) {
    written = unreal_reflect_array_default((fprop_array_t *)prop, value, cdo, def);
  } else if (written && def->type == UNREAL_PROP_KIND_SET) {
    written = unreal_reflect_set_default((fprop_set_t *)prop, value, cdo, def);
  } else if (written && def->type == UNREAL_PROP_KIND_MAP) {
    written = unreal_reflect_map_default((fprop_map_t *)prop, value, cdo, def);
  }
  return written;
}

static bool
unreal_reflect_apply_defaults(unreal_reflect_class_t *entry, const unreal_class_def_t *def)
{
  bool applied = entry->cls->cdo != NULL;
  for (uint32_t i = 0; i < entry->num_props && applied; ++i) {
    applied = unreal_reflect_apply_default(entry->props[i], entry->cls->cdo, &def->props[i]);
    if (!applied) {
      fprop_t *prop   = entry->props[i];
      int32_t  offset = prop ? prop->offset_internal : -1;
      int32_t  size   = prop ? prop->elem_size : 0;
      LOG_ERROR("cannot apply default for Unreal property '%.*s' (type=%u, offset=%d, size=%d)", STR_ARG(def->props[i].name), def->props[i].type, offset, size);
    }
  }
  return applied;
}

uclass_t *
unreal_reflect_define_class_owned(unreal_reflect_owner_t owner, str_t scope, str_t name, const unreal_class_def_t *def)
{
  uclass_t *result = NULL;
  if (!unreal_reflect_runtime_ready()) {
    LOG_ERROR("cannot define Unreal class '%.*s': custom reflection is unavailable or the caller is not on the game thread", STR_ARG(name));
    return NULL;
  }

  if (!unreal_reflect_class_def_valid(scope, name, def)) {
    LOG_ERROR("cannot define Unreal class '%.*s': invalid scope, name, definition, or parent class", STR_ARG(name));
    return NULL;
  }

  if (!unreal_reflect_schema_runtime_ready(def)) {
    LOG_ERROR("cannot define Unreal class '%.*s': required reflection constructors, linkers, or container operations are unavailable", STR_ARG(name));
    return NULL;
  }

  uint64_t schema_hash = unreal_reflect_schema_hash(def);
  unreal_reflect_class_t *entry = unreal_reflect_class_find(scope, name);
  if (entry) {
    if (entry->schema_hash != schema_hash) {
      LOG_ERROR("cannot redefine Unreal class '%.*s' with a different schema", STR_ARG(name));
    } else if (entry->state == UNREAL_REFLECT_CLASS_READY) {
      entry->owner = owner;
      unreal_reflect_update_funcs(entry, def);
      result = entry->cls;
    } else {
      LOG_ERROR("cannot use Unreal class '%.*s' because its first definition did not complete", STR_ARG(name));
    }
    return result;
  }

  entry = ARENA_PUSH_ZERO(&globals.perm, unreal_reflect_class_t);

  str_t    scope_copy = str_push_copy(&globals.perm, scope);
  str_t    name_copy  = str_push_copy(&globals.perm, name);
  uint32_t num_props  = unreal_reflect_class_prop_count(def);
  uint32_t num_funcs  = unreal_reflect_class_func_count(def);

  fprop_t **props = NULL;
  if (num_props > 0) {
    props = ARENA_PUSH_ARRAY_ZERO(&globals.perm, fprop_t *, num_props);
  }

  unreal_reflect_func_t *funcs = NULL;
  if (num_funcs > 0) {
    funcs = ARENA_PUSH_ARRAY_ZERO(&globals.perm, unreal_reflect_func_t, num_funcs);
  }

  bool allocated = entry && scope_copy.data && name_copy.data;
  allocated      = allocated && (num_props == 0 || props) && (num_funcs == 0 || funcs);
  if (!allocated) {
    LOG_ERROR("cannot define Unreal class '%.*s': permanent registry allocation failed", STR_ARG(name));
    return NULL;
  }

  uint8_t engine_name_buf[320];
  str_t   engine_name  = unreal_reflect_engine_name(engine_name_buf, sizeof(engine_name_buf), scope, name);
  fname_t engine_fname = unreal_fname_from_str(engine_name, FNAME_FIND_OR_ADD);
  if (unreal_reflect_object_find(globals.unreal.transient_package, engine_fname)) {
    LOG_ERROR("cannot define Unreal class '%.*s': generated engine name '%.*s' is already in use", STR_ARG(name), STR_ARG(engine_name));
    return NULL;
  }

  fstatic_construct_obj_params_t params = {
    .cls       = globals.unreal.core_class,
    .outer     = globals.unreal.transient_package,
    .name      = engine_fname,
    .set_flags = RF_PUBLIC | RF_STANDALONE | RF_TRANSIENT,
  };

  uclass_t *cls = (uclass_t *)unreal_static_construct_object(&params);
  if (!cls || !unreal_uobject_add_to_root((uobject_t *)cls)) {
    LOG_ERROR("cannot define Unreal class '%.*s': UClass construction or rooting failed", STR_ARG(name));
    return NULL;
  }

  entry->scope       = scope_copy;
  entry->name        = name_copy;
  entry->engine_name = engine_fname;
  entry->owner       = owner;
  entry->parent      = def->parent;
  entry->cls         = cls;
  entry->props       = props;
  entry->num_props   = num_props;
  entry->funcs       = funcs;
  entry->num_funcs   = num_funcs;
  entry->schema_hash = schema_hash;
  entry->state       = UNREAL_REFLECT_CLASS_BUILDING;
  entry->next        = g_classes;
  g_classes          = entry;

  cls->vtable->set_super_struct((ustruct_t *)cls, (ustruct_t *)def->parent);
  cls->class_flags       = def->parent->class_flags & CLASS_SCRIPT_INHERIT;
  cls->class_within      = def->parent->class_within;
  cls->class_config_name = def->parent->class_config_name;

  if (!unreal_reflect_construct_props(entry, def)) {
    entry->state = UNREAL_REFLECT_CLASS_FAILED;
    LOG_ERROR("cannot define Unreal class '%.*s': reflected property construction failed", STR_ARG(name));
    return NULL;
  }

  if (!unreal_reflect_construct_funcs(entry, def)) {
    entry->state = UNREAL_REFLECT_CLASS_FAILED;
    LOG_ERROR("cannot define Unreal class '%.*s': reflected function construction failed", STR_ARG(name));
    return NULL;
  }

  cls->vtable->bind((ufield_t *)cls);
  ustruct_static_link((ustruct_t *)cls, true);
  uclass_assemble_ref_tok_stream(cls, true);

  cls->cdo = create_default_object(cls);
  if (!cls->cdo) {
    entry->state = UNREAL_REFLECT_CLASS_FAILED;
    LOG_ERROR("cannot define Unreal class '%.*s': default object construction failed", STR_ARG(name));
  } else if (!unreal_reflect_apply_defaults(entry, def)) {
    entry->state = UNREAL_REFLECT_CLASS_FAILED;
    LOG_ERROR("cannot define Unreal class '%.*s': applying default property values failed", STR_ARG(name));
  } else {
    unreal_reflect_update_funcs(entry, def);
    entry->state = UNREAL_REFLECT_CLASS_READY;
    result       = cls;
  }
  return result;
}

uclass_t *
unreal_reflect_define_class(str_t scope, str_t name, const unreal_class_def_t *def)
{
  return unreal_reflect_define_class_owned(UNREAL_REFLECT_OWNER_PERSISTENT, scope, name, def);
}

void
unreal_reflect_disable_owner(unreal_reflect_owner_t owner)
{
  for (unreal_reflect_class_t *cls = g_classes; cls; cls = cls->next) {
    if (owner != UNREAL_REFLECT_OWNER_PERSISTENT && cls->owner == owner) {
      for (uint32_t i = 0; i < cls->num_funcs; ++i) {
        cls->funcs[i].active = false;
        cls->funcs[i].impl   = NULL;
        cls->funcs[i].user   = NULL;
      }
    }
  }
}
