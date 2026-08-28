#include "unreal_prop.h"

#include "globals.h"
#include "scratch.h"
#include "signatures.h"

typedef struct unreal_prop_vector4_s {
  float x;
  float y;
  float z;
  float w;
} unreal_prop_vector4_t;

typedef struct unreal_prop_linear_color_s {
  float r;
  float g;
  float b;
  float a;
} unreal_prop_linear_color_t;

typedef struct unreal_prop_int_point_s {
  int32_t x;
  int32_t y;
} unreal_prop_int_point_t;

typedef struct unreal_prop_int_vector_s {
  int32_t x;
  int32_t y;
  int32_t z;
} unreal_prop_int_vector_t;

typedef struct unreal_prop_lazy_object_ptr_s {
  fweak_object_ptr_t weak;
  int32_t            tag_at_last_test;
  fguid_t            object_id;
} unreal_prop_lazy_object_ptr_t;

typedef struct unreal_prop_soft_object_ptr_s {
  fweak_object_ptr_t weak;
  int32_t            tag_at_last_test;
  int32_t            pad;
  fsoft_object_path_t path;
} unreal_prop_soft_object_ptr_t;

typedef struct unreal_prop_script_interface_s {
  uobject_t *object;
  void      *interface_ptr;
} unreal_prop_script_interface_t;

fprop_t *
unreal_ustruct_find_prop(ustruct_t *s, str_t name)
{
  for (fprop_t *p = s->prop_link; p; p = p->prop_link_next) {
    if (unreal_fname_match_text(p->name, name, false, true)) {
      return p;
    }
  }

  return NULL;
}

static inline str_t
unreal_fprop_class_push_type_name(arena_t *arena, str_t prefix, uclass_t *cls)
{
  str_t cls_name = cls ? unreal_uobject_push_name((uobject_t *)cls, arena) : STR_LIT("<unknown>");
  return str_push_fmt(arena, "%.*s<%.*s>", STR_ARG(prefix), STR_ARG(cls_name));
}

unreal_prop_kind_t
unreal_fprop_get_kind(fprop_t *prop)
{
  if (!prop || !prop->cls) {
    return UNREAL_PROP_KIND_UNKNOWN;
  }

  struct {
    fname_t            class_name;
    unreal_prop_kind_t kind;
  } kinds[] = {
    {globals.unreal.bool_prop,                  UNREAL_PROP_KIND_BOOL                     },
    {globals.unreal.byte_prop,                  UNREAL_PROP_KIND_BYTE                     },
    {globals.unreal.int8_prop,                  UNREAL_PROP_KIND_INT8                     },
    {globals.unreal.int16_prop,                 UNREAL_PROP_KIND_INT16                    },
    {globals.unreal.int_prop,                   UNREAL_PROP_KIND_INT32                    },
    {globals.unreal.int32_prop,                 UNREAL_PROP_KIND_INT32                    },
    {globals.unreal.int64_prop,                 UNREAL_PROP_KIND_INT64                    },
    {globals.unreal.uint16_prop,                UNREAL_PROP_KIND_UINT16                   },
    {globals.unreal.uint32_prop,                UNREAL_PROP_KIND_UINT32                   },
    {globals.unreal.uint64_prop,                UNREAL_PROP_KIND_UINT64                   },
    {globals.unreal.float_prop,                 UNREAL_PROP_KIND_FLOAT                    },
    {globals.unreal.double_prop,                UNREAL_PROP_KIND_DOUBLE                   },
    {globals.unreal.name_prop,                  UNREAL_PROP_KIND_NAME                     },
    {globals.unreal.str_prop,                   UNREAL_PROP_KIND_STRING                   },
    {globals.unreal.text_prop,                  UNREAL_PROP_KIND_TEXT                     },
    {globals.unreal.obj_prop,                   UNREAL_PROP_KIND_OBJECT                   },
    {globals.unreal.class_prop,                 UNREAL_PROP_KIND_CLASS                    },
    {globals.unreal.soft_obj_prop,              UNREAL_PROP_KIND_SOFT_OBJECT              },
    {globals.unreal.soft_class_prop,            UNREAL_PROP_KIND_SOFT_CLASS               },
    {globals.unreal.weak_obj_prop,              UNREAL_PROP_KIND_WEAK_OBJECT              },
    {globals.unreal.lazy_obj_prop,              UNREAL_PROP_KIND_LAZY_OBJECT              },
    {globals.unreal.interface_prop,             UNREAL_PROP_KIND_INTERFACE                },
    {globals.unreal.struct_prop,                UNREAL_PROP_KIND_STRUCT                   },
    {globals.unreal.array_prop,                 UNREAL_PROP_KIND_ARRAY                    },
    {globals.unreal.set_prop,                   UNREAL_PROP_KIND_SET                      },
    {globals.unreal.map_prop,                   UNREAL_PROP_KIND_MAP                      },
    {globals.unreal.enum_prop,                  UNREAL_PROP_KIND_ENUM                     },
    {globals.unreal.delegate_prop,              UNREAL_PROP_KIND_DELEGATE                 },
    {globals.unreal.mcast_delegate_prop,        UNREAL_PROP_KIND_MULTICAST_DELEGATE       },
    {globals.unreal.mcast_inline_delegate_prop, UNREAL_PROP_KIND_MULTICAST_INLINE_DELEGATE},
    {globals.unreal.mcast_sparse_delegate_prop, UNREAL_PROP_KIND_MULTICAST_SPARSE_DELEGATE},
  };

  for (int32_t i = 0; i < COUNTOF(kinds); ++i) {
    if (unreal_fname_equal(prop->cls->name, kinds[i].class_name, false)) {
      return kinds[i].kind;
    }
  }
  return UNREAL_PROP_KIND_UNKNOWN;
}

bool
unreal_fprop_struct_is(fprop_t *prop, fname_t struct_name)
{
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_STRUCT) {
    return false;
  }

  uscript_struct_t *script_struct = ((fprop_struct_t *)prop)->script_struct;
  return script_struct && unreal_fname_equal(script_struct->name, struct_name, false);
}

bool
unreal_fprop_class_is(fprop_t *prop, fname_t name)
{
  if (prop && prop->cls) {
    return unreal_fname_equal(prop->cls->name, name, false);
  }
  return false;
}

bool
unreal_fprop_class_is_a(fprop_t *prop, fname_t name)
{
  for (ffield_class_t *cls = prop ? prop->cls : NULL; cls; cls = cls->super_class) {
    if (unreal_fname_equal(cls->name, name, false)) {
      return true;
    }
  }
  return false;
}

str_t
unreal_fprop_push_type_name(fprop_t *prop, arena_t *arena)
{
  if (!prop || !prop->cls || !arena) {
    return STR_LIT("<null>");
  }

  struct {
    str_t   name;
    fname_t fname;
  } simple_prop_names[] = {
    {STR_CLIT("bool"),     globals.unreal.bool_prop  },
    {STR_CLIT("uint8_t"),  globals.unreal.byte_prop  },
    {STR_CLIT("int8_t"),   globals.unreal.int8_prop  },
    {STR_CLIT("int16_t"),  globals.unreal.int16_prop },
    {STR_CLIT("int32_t"),  globals.unreal.int_prop   },
    {STR_CLIT("int32_t"),  globals.unreal.int32_prop },
    {STR_CLIT("int64_t"),  globals.unreal.int64_prop },
    {STR_CLIT("uint16_t"), globals.unreal.uint16_prop},
    {STR_CLIT("uint32_t"), globals.unreal.uint32_prop},
    {STR_CLIT("uint64_t"), globals.unreal.uint64_prop},
    {STR_CLIT("float"),    globals.unreal.float_prop },
    {STR_CLIT("double"),   globals.unreal.double_prop},
    {STR_CLIT("FName"),    globals.unreal.name_prop  },
    {STR_CLIT("FString"),  globals.unreal.str_prop   },
    {STR_CLIT("FText"),    globals.unreal.text_prop  },
  };

  for (int i = 0; i < COUNTOF(simple_prop_names); ++i) {
    str_t   type_name  = simple_prop_names[i].name;
    fname_t type_fname = simple_prop_names[i].fname;

    if (unreal_fprop_class_is(prop, type_fname)) {
      return type_name; // ok, because it's string literal
    }
  }

  if (unreal_fprop_class_is(prop, globals.unreal.obj_prop)) {
    fprop_obj_base_t *p = (fprop_obj_base_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("Object"), p->prop_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.class_prop)) {
    fprop_class_t *p = (fprop_class_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("Class"), p->meta_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.soft_obj_prop)) {
    fprop_obj_base_t *p = (fprop_obj_base_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("SoftObject"), p->prop_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.soft_class_prop)) {
    fprop_class_soft_t *p = (fprop_class_soft_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("SoftClass"), p->meta_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.weak_obj_prop)) {
    fprop_obj_base_t *p = (fprop_obj_base_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("WeakObject"), p->prop_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.lazy_obj_prop)) {
    fprop_obj_base_t *p = (fprop_obj_base_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("LazyObject"), p->prop_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.interface_prop)) {
    fprop_iface_t *p = (fprop_iface_t *)prop;
    return unreal_fprop_class_push_type_name(arena, STR_LIT("Interface"), p->iface_class);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.struct_prop)) {
    fprop_struct_t *p = (fprop_struct_t *)prop;

    if (!p->script_struct) {
      return STR_LIT("<unknown struct>");
    }

    return unreal_uobject_push_name((uobject_t *)p->script_struct, arena);
  }

  if (unreal_fprop_class_is(prop, globals.unreal.enum_prop)) {
    fprop_enum_t *p         = (fprop_enum_t *)prop;
    str_t         enum_name = p->uenum ? unreal_uobject_push_name((uobject_t *)p->uenum, arena) : STR_LIT("<unknown>");

    if (p->underlying_prop) {
      str_t underlying_type_name = unreal_fprop_push_type_name(&p->underlying_prop->base, arena);
      return str_push_fmt(arena, "Enum<%.*s:%.*s>", STR_ARG(enum_name), STR_ARG(underlying_type_name));
    }
    return str_push_fmt(arena, "Enum<%.*s>", STR_ARG(enum_name));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.array_prop)) {
    fprop_array_t *p               = (fprop_array_t *)prop;
    str_t          inner_type_name = unreal_fprop_push_type_name(p->inner, arena);
    return str_push_fmt(arena, "TArray<%.*s>", STR_ARG(inner_type_name));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.set_prop)) {
    fprop_set_t *p              = (fprop_set_t *)prop;
    str_t        elem_type_name = unreal_fprop_push_type_name(p->elem_prop, arena);
    return str_push_fmt(arena, "TSet<%.*s>", STR_ARG(elem_type_name));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.map_prop)) {
    fprop_map_t *p               = (fprop_map_t *)prop;
    str_t        key_type_name   = unreal_fprop_push_type_name(p->key_prop, arena);
    str_t        value_type_name = unreal_fprop_push_type_name(p->val_prop, arena);
    return str_push_fmt(arena, "TMap<%.*s, %.*s>", STR_ARG(key_type_name), STR_ARG(value_type_name));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.delegate_prop)) {
    fprop_delegate_t *p   = (fprop_delegate_t *)prop;
    str_t             sig = p->signature_func ? unreal_uobject_push_name((uobject_t *)p->signature_func, arena) : STR_LIT("<unknown>");
    return str_push_fmt(arena, "Delegate<%.*s>", STR_ARG(sig));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.mcast_delegate_prop)) {
    fprop_mcast_delegate_t *p   = (fprop_mcast_delegate_t *)prop;
    str_t                   sig = p->signature_func ? unreal_uobject_push_name((uobject_t *)p->signature_func, arena) : STR_LIT("<unknown>");
    return str_push_fmt(arena, "MulticastDelegate<%.*s>", STR_ARG(sig));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.mcast_inline_delegate_prop)) {
    fprop_mcast_delegate_t *p   = (fprop_mcast_delegate_t *)prop;
    str_t                   sig = p->signature_func ? unreal_uobject_push_name((uobject_t *)p->signature_func, arena) : STR_LIT("<unknown>");
    return str_push_fmt(arena, "MulticastInlineDelegate<%.*s>", STR_ARG(sig));
  }

  if (unreal_fprop_class_is(prop, globals.unreal.mcast_sparse_delegate_prop)) {
    fprop_mcast_delegate_t *p   = (fprop_mcast_delegate_t *)prop;
    str_t                   sig = p->signature_func ? unreal_uobject_push_name((uobject_t *)p->signature_func, arena) : STR_LIT("<unknown>");
    return str_push_fmt(arena, "MulticastSparseDelegate<%.*s>", STR_ARG(sig));
  }

  return unreal_fname_to_str(prop->cls->name, arena);
}

bool
unreal_fprop_read_bool(fprop_t *prop, const void *value, bool *out)
{
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_BOOL || !value || !out) {
    return false;
  }

  fprop_bool_t *bool_prop = (fprop_bool_t *)prop;
  if (prop->elem_size <= 0 || bool_prop->field_size == 0 ||
      bool_prop->byte_offset >= prop->elem_size || bool_prop->byte_offset >= bool_prop->field_size ||
      bool_prop->field_mask == 0) {
    return false;
  }

  uint8_t byte = *((const uint8_t *)value + bool_prop->byte_offset);
  *out         = (byte & bool_prop->field_mask) != 0;
  return true;
}

bool
unreal_fprop_read_integer(fprop_t *prop, const void *value, unreal_prop_integer_t *out)
{
  if (!prop || !value || !out) {
    return false;
  }

  unreal_prop_integer_t result = {0};
  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_BYTE: {
      if (prop->elem_size < (int32_t)sizeof(uint8_t)) {
        return false;
      }

      result.value = *(const uint8_t *)value;
      break;
    }

    case UNREAL_PROP_KIND_INT8: {
      if (prop->elem_size < (int32_t)sizeof(int8_t)) {
        return false;
      }

      result.value     = (uint64_t)*(const int8_t *)value;
      result.is_signed = true;
      break;
    }

    case UNREAL_PROP_KIND_INT16: {
      if (prop->elem_size < (int32_t)sizeof(int16_t)) {
        return false;
      }

      int16_t typed_value = 0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));
      result.value     = (uint64_t)typed_value;
      result.is_signed = true;
      break;
    }

    case UNREAL_PROP_KIND_INT32: {
      if (prop->elem_size < (int32_t)sizeof(int32_t)) {
        return false;
      }

      int32_t typed_value = 0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));

      result.value     = (uint64_t)typed_value;
      result.is_signed = true;
      break;
    }

    case UNREAL_PROP_KIND_INT64: {
      if (prop->elem_size < (int32_t)sizeof(int64_t)) {
        return false;
      }

      int64_t typed_value = 0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));

      result.value     = (uint64_t)typed_value;
      result.is_signed = true;
      break;
    }

    case UNREAL_PROP_KIND_UINT16: {
      if (prop->elem_size < (int32_t)sizeof(uint16_t)) {
        return false;
      }

      uint16_t typed_value = 0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));
      result.value = typed_value;
      break;
    }

    case UNREAL_PROP_KIND_UINT32: {
      if (prop->elem_size < (int32_t)sizeof(uint32_t)) {
        return false;
      }

      uint32_t typed_value = 0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));
      result.value = typed_value;
      break;
    }

    case UNREAL_PROP_KIND_UINT64: {
      if (prop->elem_size < (int32_t)sizeof(uint64_t)) {
        return false;
      }

      uint64_t typed_value = 0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));
      result.value = typed_value;
      break;
    }

    case UNREAL_PROP_KIND_ENUM: {
      fprop_enum_t *enum_prop = (fprop_enum_t *)prop;
      if (!enum_prop->underlying_prop) {
        return false;
      }

      return unreal_fprop_read_integer(&enum_prop->underlying_prop->base, value, out);
    }

    default: {
      return false;
    }
  }

  *out = result;
  return true;
}

bool
unreal_fprop_read_real(fprop_t *prop, const void *value, double *out)
{
  if (!prop || !value || !out) {
    return false;
  }

  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_FLOAT: {
      if (prop->elem_size < (int32_t)sizeof(float)) {
        return false;
      }

      float typed_value = 0.0f;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));
      *out = (double)typed_value;
      return true;
    }

    case UNREAL_PROP_KIND_DOUBLE: {
      if (prop->elem_size < (int32_t)sizeof(double)) {
        return false;
      }

      double typed_value = 0.0;
      mem_copy(&typed_value, (void *)value, sizeof(typed_value));
      *out = typed_value;
      return true;
    }

    default: {
      return false;
    }
  }
}

bool
unreal_fprop_read_name(fprop_t *prop, const void *value, fname_t *out)
{
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_NAME || !value || !out ||
      prop->elem_size < (int32_t)sizeof(*out)) {
    return false;
  }

  mem_copy(out, (void *)value, sizeof(*out));
  return true;
}

bool
unreal_fprop_write_bool(fprop_t *prop, void *value, bool input)
{
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_BOOL || !value) {
    return false;
  }

  fprop_bool_t *bool_prop = (fprop_bool_t *)prop;
  if (prop->elem_size <= 0 || bool_prop->field_size == 0 ||
      bool_prop->byte_offset >= prop->elem_size || bool_prop->byte_offset >= bool_prop->field_size ||
      bool_prop->byte_mask == 0 || bool_prop->field_mask == 0) {
    return false;
  }

  uint8_t *byte = (uint8_t *)value + bool_prop->byte_offset;
  *byte = (uint8_t)((*byte & (uint8_t)~bool_prop->field_mask) | (input ? bool_prop->byte_mask : 0));
  return true;
}

static bool
unreal_prop_integer_as_signed(unreal_prop_integer_t input, int64_t min, int64_t max, int64_t *out)
{
  if (!out) {
    return false;
  }

  if (!input.is_signed) {
    if (input.value > (uint64_t)max) {
      return false;
    }
    *out = (int64_t)input.value;
    return true;
  }

  int64_t value = (int64_t)input.value;
  if (value < min || value > max) {
    return false;
  }

  *out = value;
  return true;
}

static bool
unreal_prop_integer_as_unsigned(unreal_prop_integer_t input, uint64_t max, uint64_t *out)
{
  if (!out) {
    return false;
  }

  uint64_t value = input.value;
  if (input.is_signed) {
    int64_t signed_value = (int64_t)input.value;
    if (signed_value < 0) {
      return false;
    }
    value = (uint64_t)signed_value;
  }

  if (value > max) {
    return false;
  }

  *out = value;
  return true;
}

bool
unreal_fprop_write_integer(fprop_t *prop, void *value, unreal_prop_integer_t input)
{
  if (!prop || !value) {
    return false;
  }

  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_BYTE: {
      uint64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(uint8_t) ||
          !unreal_prop_integer_as_unsigned(input, UINT8_MAX, &converted)) {
        return false;
      }

      *(uint8_t *)value = (uint8_t)converted;
      return true;
    }

    case UNREAL_PROP_KIND_INT8: {
      int64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(int8_t) ||
          !unreal_prop_integer_as_signed(input, INT8_MIN, INT8_MAX, &converted)) {
        return false;
      }

      *(int8_t *)value = (int8_t)converted;
      return true;
    }

    case UNREAL_PROP_KIND_INT16: {
      int64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(int16_t) ||
          !unreal_prop_integer_as_signed(input, INT16_MIN, INT16_MAX, &converted)) {
        return false;
      }

      int16_t typed_value = (int16_t)converted;
      mem_copy(value, &typed_value, sizeof(typed_value));
      return true;
    }

    case UNREAL_PROP_KIND_INT32: {
      int64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(int32_t) ||
          !unreal_prop_integer_as_signed(input, INT32_MIN, INT32_MAX, &converted)) {
        return false;
      }

      int32_t typed_value = (int32_t)converted;
      mem_copy(value, &typed_value, sizeof(typed_value));
      return true;
    }

    case UNREAL_PROP_KIND_INT64: {
      int64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(int64_t) ||
          !unreal_prop_integer_as_signed(input, INT64_MIN, INT64_MAX, &converted)) {
        return false;
      }

      mem_copy(value, &converted, sizeof(converted));
      return true;
    }

    case UNREAL_PROP_KIND_UINT16: {
      uint64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(uint16_t) ||
          !unreal_prop_integer_as_unsigned(input, UINT16_MAX, &converted)) {
        return false;
      }

      uint16_t typed_value = (uint16_t)converted;
      mem_copy(value, &typed_value, sizeof(typed_value));
      return true;
    }

    case UNREAL_PROP_KIND_UINT32: {
      uint64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(uint32_t) ||
          !unreal_prop_integer_as_unsigned(input, UINT32_MAX, &converted)) {
        return false;
      }

      uint32_t typed_value = (uint32_t)converted;
      mem_copy(value, &typed_value, sizeof(typed_value));
      return true;
    }

    case UNREAL_PROP_KIND_UINT64: {
      uint64_t converted = 0;
      if (prop->elem_size < (int32_t)sizeof(uint64_t) ||
          !unreal_prop_integer_as_unsigned(input, UINT64_MAX, &converted)) {
        return false;
      }

      mem_copy(value, &converted, sizeof(converted));
      return true;
    }

    case UNREAL_PROP_KIND_ENUM: {
      fprop_enum_t *enum_prop = (fprop_enum_t *)prop;
      return enum_prop->underlying_prop && unreal_fprop_write_integer(&enum_prop->underlying_prop->base, value, input);
    }

    default: {
      return false;
    }
  }
}

bool
unreal_fprop_write_real(fprop_t *prop, void *value, double input)
{
  if (!prop || !value) {
    return false;
  }

  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_FLOAT: {
      if (prop->elem_size < (int32_t)sizeof(float) ||
          (input == input && (input < -FLT_MAX || input > FLT_MAX))) {
        return false;
      }

      float typed_value = (float)input;
      mem_copy(value, &typed_value, sizeof(typed_value));
      return true;
    }

    case UNREAL_PROP_KIND_DOUBLE: {
      if (prop->elem_size < (int32_t)sizeof(double)) {
        return false;
      }

      mem_copy(value, &input, sizeof(input));
      return true;
    }

    default: {
      return false;
    }
  }
}

bool
unreal_fprop_write_name(fprop_t *prop, void *value, fname_t input)
{
  if (unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_NAME || !value ||
      prop->elem_size < (int32_t)sizeof(input)) {
    return false;
  }

  mem_copy(value, &input, sizeof(input));
  return true;
}

uobject_t *
unreal_fweak_object_resolve(fweak_object_ptr_t weak)
{
  if (weak.object_idx < 0 || weak.object_serial <= 0) {
    return NULL;
  }

  fuobject_item_t *item = unreal_uobject_array_get_item(weak.object_idx);
  if (!item || item->serial_num != weak.object_serial || !unreal_uobject_array_item_is_valid(item)) {
    return NULL;
  }

  return item->obj;
}

static str_t
unreal_fstring_push_plain(const fstring_t *string, arena_t *arena)
{
  if (!string || !string->data || string->len <= 0 || string->len > 4096) {
    return STR_NULL;
  }
  return str_from_str16(arena, str16_make(string->data, (uint64_t)string->len));
}

static str_t
unreal_fstring_push_quoted(const fstring_t *string, arena_t *arena)
{
  str_t plain = unreal_fstring_push_plain(string, arena);
  return str_push_fmt(arena, "\"%.*s\"", STR_ARG(plain));
}

static str_t
unreal_uobject_push_value_summary(uobject_t *object, arena_t *arena)
{
  if (!object) {
    return STR_LIT("null");
  }

  if (!unreal_uobject_is_valid(object)) {
    return str_push_fmt(arena, "<invalid %p>", object);
  }

  return unreal_uobject_push_full_name(object, arena);
}

static str_t
unreal_fweak_object_push_summary(fweak_object_ptr_t weak, arena_t *arena)
{
  if (weak.object_idx < 0 || weak.object_serial <= 0) {
    return STR_LIT("null");
  }

  uobject_t *object = unreal_fweak_object_resolve(weak);
  if (object) {
    return unreal_uobject_push_full_name(object, arena);
  }

  return str_push_fmt(arena, "<stale Index=%d Serial=%d>", weak.object_idx, weak.object_serial);
}

str_t
unreal_fscript_delegate_push_summary(const fscript_delegate_t *delegate, arena_t *arena)
{
  if (!delegate || !arena) {
    return STR_NULL;
  }

  str_t function_name = unreal_fname_to_str(delegate->function_name, arena);
  str_t object_name   = unreal_fweak_object_push_summary(delegate->object, arena);
  if (str_is_empty(function_name)) {
    return str_push_fmt(arena, "%.*s.<none>", STR_ARG(object_name));
  }
  return str_push_fmt(arena, "%.*s.%.*s", STR_ARG(object_name), STR_ARG(function_name));
}

static str_t
unreal_fprop_push_hex_summary(const void *value, int32_t value_size, arena_t *arena)
{
  if (!value || value_size <= 0 || !arena) {
    return STR_NULL;
  }

  const uint8_t *bytes  = (const uint8_t *)value;
  str_t          result = STR_NULL;
  tmp_arena_t    tmp    = scratch_begin(arena);
  {
    str_list_t list = {0};
    for (int32_t i = 0; i < value_size; ++i) {
      str_list_push(tmp.arena, &list, str_push_fmt(tmp.arena, "%02X", (uint32_t)bytes[i]));
    }

    str_t body = str_list_join(tmp.arena, list, STR_NULL, STR_LIT(" "), STR_NULL);
    result     = str_push_fmt(arena, "raw bytes (%d): %.*s", value_size, STR_ARG(body));
  }
  scratch_end(tmp);
  return result;
}

static str_t
unreal_fsoft_object_path_push_summary(const fsoft_object_path_t *path, arena_t *arena)
{
  if (!path) {
    return STR_LIT("null");
  }

  str_t asset = unreal_fname_to_str(path->asset_path_name, arena);
  str_t sub   = unreal_fstring_push_plain(&path->sub_path_string, arena);
  if (str_is_empty(asset) && str_is_empty(sub)) {
    return STR_LIT("null");
  }

  if (str_is_empty(sub)) {
    return str_push_fmt(arena, "\"%.*s\"", STR_ARG(asset));
  }
  return str_push_fmt(arena, "\"%.*s:%.*s\"", STR_ARG(asset), STR_ARG(sub));
}

static str_t
unreal_fsoft_object_push_summary(const void *value, int32_t value_size, arena_t *arena)
{
  if (value_size == (int32_t)sizeof(fsoft_object_path_t)) {
    return unreal_fsoft_object_path_push_summary((const fsoft_object_path_t *)value, arena);
  }

  if (value_size >= (int32_t)sizeof(unreal_prop_soft_object_ptr_t)) {
    const unreal_prop_soft_object_ptr_t *soft = (const unreal_prop_soft_object_ptr_t *)value;
    uobject_t *object = unreal_fweak_object_resolve(soft->weak);
    return object ? unreal_uobject_push_full_name(object, arena) : unreal_fsoft_object_path_push_summary(&soft->path, arena);
  }
  return STR_NULL;
}

static str_t
unreal_ftext_push_summary(const ftext_t *text, arena_t *arena)
{
  if (!text || !text->text_data.obj) {
    return STR_LIT("null");
  }

  itext_data_t *data = text->text_data.obj;
  if (!data->vtable || !data->vtable->get_display_string) {
    return str_push_fmt(arena, "\"\" Flags=0x%X", (uint32_t)text->flags);
  }

  fstring_t *display_string = data->vtable->get_display_string(data);
  str_t      display        = unreal_fstring_push_plain(display_string, arena);
  return str_push_fmt(arena, "\"%.*s\" Flags=0x%X", STR_ARG(display), (uint32_t)text->flags);
}

static str_t
unreal_fprop_push_struct_summary(fprop_t *prop, const void *value, int32_t value_size, arena_t *arena)
{
  if (unreal_fprop_struct_is(prop, globals.unreal.vector)                 ||
      unreal_fprop_struct_is(prop, globals.unreal.vector_net_quantize)    ||
      unreal_fprop_struct_is(prop, globals.unreal.vector_net_quantize10)  ||
      unreal_fprop_struct_is(prop, globals.unreal.vector_net_quantize100) ||
      unreal_fprop_struct_is(prop, globals.unreal.vector_net_quantize_normal)) {
    if (value_size >= (int32_t)sizeof(fvector_t)) {
      const fvector_t *v = (const fvector_t *)value;
      return str_push_fmt(arena, "{X=%.3f Y=%.3f Z=%.3f}", v->x, v->y, v->z);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.vector2d)) {
    if (value_size >= (int32_t)sizeof(fvector2d_t)) {
      const fvector2d_t *v = (const fvector2d_t *)value;
      return str_push_fmt(arena, "{X=%.3f Y=%.3f}", v->x, v->y);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.vector4)) {
    if (value_size >= (int32_t)sizeof(unreal_prop_vector4_t)) {
      const unreal_prop_vector4_t *v = (const unreal_prop_vector4_t *)value;
      return str_push_fmt(arena, "{X=%.3f Y=%.3f Z=%.3f W=%.3f}", v->x, v->y, v->z, v->w);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.rotator)) {
    if (value_size >= 3 * (int32_t)sizeof(float)) {
      const float *r = (const float *)value;
      return str_push_fmt(arena, "{Pitch=%.3f Yaw=%.3f Roll=%.3f}", r[0], r[1], r[2]);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.quat)) {
    if (value_size >= (int32_t)sizeof(fquat_t)) {
      const fquat_t *q = (const fquat_t *)value;
      return str_push_fmt(arena, "{X=%.3f Y=%.3f Z=%.3f W=%.3f}", q->x, q->y, q->z, q->w);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.color)) {
    if (value_size >= 4) {
      const uint8_t *c = (const uint8_t *)value;
      return str_push_fmt(arena, "{R=%u G=%u B=%u A=%u}", c[2], c[1], c[0], c[3]);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.linear_color)) {
    if (value_size >= (int32_t)sizeof(unreal_prop_linear_color_t)) {
      const unreal_prop_linear_color_t *c = (const unreal_prop_linear_color_t *)value;
      return str_push_fmt(arena, "{R=%.3f G=%.3f B=%.3f A=%.3f}", c->r, c->g, c->b, c->a);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.key)) {
    if (value_size >= (int32_t)sizeof(fkey_t)) {
      str_t name = unreal_fname_to_str(((const fkey_t *)value)->name, arena);
      return str_is_empty(name) ? STR_LIT("None") : name;
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.gameplay_tag)) {
    if (value_size >= (int32_t)sizeof(fgameplay_tag_t)) {
      str_t name = unreal_fname_to_str(((const fgameplay_tag_t *)value)->tag_name, arena);
      return str_is_empty(name) ? STR_LIT("None") : name;
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.gameplay_tag_container)) {
    if (value_size >= (int32_t)sizeof(fgameplay_tag_container_t)) {
      const fgameplay_tag_container_t *container = (const fgameplay_tag_container_t *)value;
      if (!container->gameplay_tags.data || container->gameplay_tags.num <= 0 || container->gameplay_tags.max <= 0) {
        return STR_LIT("[]");
      }

      int32_t     count  = MIN_VAL(container->gameplay_tags.num, 2);
      str_t       result = STR_LIT("[]");
      tmp_arena_t tmp    = scratch_begin(arena);
      {
        str_list_t list = {0};
        for (int32_t i = 0; i < count; ++i) {
          str_t name = unreal_fname_to_str(container->gameplay_tags.data[i].tag_name, tmp.arena);
          str_list_push(tmp.arena, &list, str_is_empty(name) ? STR_LIT("None") : name);
        }

        str_t suffix = container->gameplay_tags.num > count
                       ? str_push_fmt(tmp.arena, "] +%d", container->gameplay_tags.num - count)
                       : STR_LIT("]");
        result = str_list_join(arena, list, STR_LIT("["), STR_LIT(", "), suffix);
      }
      scratch_end(tmp);
      return result;
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.guid)) {
    if (value_size >= (int32_t)sizeof(fguid_t)) {
      const fguid_t *g = (const fguid_t *)value;
      return str_push_fmt(arena, "{%08X-%04X-%04X-%04X-%04X%08X}",
                          g->a,
                         (g->b >> 16) & 0xFFFF, g->b & 0xFFFF,
                         (g->c >> 16) & 0xFFFF, g->c & 0xFFFF,
                         g->d);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.int_point)) {
    if (value_size >= (int32_t)sizeof(unreal_prop_int_point_t)) {
      const unreal_prop_int_point_t *p = (const unreal_prop_int_point_t *)value;
      return str_push_fmt(arena, "{X=%d Y=%d}", p->x, p->y);
    }
  } else if (unreal_fprop_struct_is(prop, globals.unreal.int_vector)) {
    if (value_size >= (int32_t)sizeof(unreal_prop_int_vector_t)) {
      const unreal_prop_int_vector_t *v = (const unreal_prop_int_vector_t *)value;
      return str_push_fmt(arena, "{X=%d Y=%d Z=%d}", v->x, v->y, v->z);
    }
  }

  fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
  if (struct_prop->script_struct && struct_prop->script_struct->child_props) {
    return STR_LIT("{...}");
  }

  str_t hex = unreal_fprop_push_hex_summary(value, value_size, arena);
  return str_push_fmt(arena, "<no reflected fields> %.*s", STR_ARG(hex));
}

static str_t
unreal_fprop_push_enum_summary(fprop_enum_t *prop, const void *value, arena_t *arena)
{
  if (!prop->underlying_prop) {
    return STR_LIT("<enum has no underlying property>");
  }

  unreal_prop_integer_t integer = {0};
  if (!unreal_fprop_read_integer(&prop->underlying_prop->base, value, &integer)) {
    return STR_NULL;
  }

  int64_t signed_value = (int64_t)integer.value;
  str_t   name         = STR_NULL;
  if (prop->uenum) {
    for (int32_t i = 0; i < prop->uenum->names.num; ++i) {
      if (prop->uenum->names.data[i].value == signed_value) {
        name = unreal_fname_to_str(prop->uenum->names.data[i].key, arena);
        break;
      }
    }
  }

  if (!str_is_empty(name)) {
    return str_push_fmt(arena, "%.*s (%lld)", STR_ARG(name), (long long)signed_value);
  }

  return integer.is_signed ? str_push_fmt(arena, "%lld", (long long)signed_value)
                           : str_push_fmt(arena, "%llu", (unsigned long long)integer.value);
}

str_t
unreal_fprop_push_value_summary(fprop_t *prop, const void *value, int32_t value_size, arena_t *arena)
{
  if (!prop || !value || prop->elem_size <= 0 || value_size < prop->elem_size || !arena) {
    return STR_NULL;
  }

  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_BOOL: {
      bool result = false;
      return unreal_fprop_read_bool(prop, value, &result) ? (result ? STR_LIT("true") : STR_LIT("false")) : STR_NULL;
    }

    case UNREAL_PROP_KIND_BYTE:
    case UNREAL_PROP_KIND_INT8:
    case UNREAL_PROP_KIND_INT16:
    case UNREAL_PROP_KIND_INT32:
    case UNREAL_PROP_KIND_INT64:
    case UNREAL_PROP_KIND_UINT16:
    case UNREAL_PROP_KIND_UINT32:
    case UNREAL_PROP_KIND_UINT64: {
      unreal_prop_integer_t integer = {0};
      if (!unreal_fprop_read_integer(prop, value, &integer)) {
        return STR_NULL;
      }
      return integer.is_signed ? str_push_fmt(arena, "%lld", (long long)(int64_t)integer.value)
                               : str_push_fmt(arena, "%llu", (unsigned long long)integer.value);
    }

    case UNREAL_PROP_KIND_FLOAT:
    case UNREAL_PROP_KIND_DOUBLE: {
      double result = 0.0;
      return unreal_fprop_read_real(prop, value, &result) ? str_push_fmt(arena, "%.3f", result) : STR_NULL;
    }

    case UNREAL_PROP_KIND_NAME: {
      fname_t name_value = {0};
      if (value_size < (int32_t)sizeof(name_value) || !unreal_fprop_read_name(prop, value, &name_value)) {
        return STR_NULL;
      }
      str_t name = unreal_fname_to_str(name_value, arena);
      return str_push_fmt(arena, "\"%.*s\"", STR_ARG(name));
    }

    case UNREAL_PROP_KIND_STRING: {
      return value_size >= (int32_t)sizeof(fstring_t)
             ? unreal_fstring_push_quoted((const fstring_t *)value, arena)
             : STR_NULL;
    }

    case UNREAL_PROP_KIND_TEXT: {
      return value_size >= (int32_t)sizeof(ftext_t)
             ? unreal_ftext_push_summary((const ftext_t *)value, arena)
             : STR_NULL;
    }

    case UNREAL_PROP_KIND_OBJECT:
    case UNREAL_PROP_KIND_CLASS: {
      return value_size >= (int32_t)sizeof(uobject_t *)
             ? unreal_uobject_push_value_summary(unreal_fprop_get_object((fprop_obj_base_t *)prop, value), arena)
             : STR_NULL;
    }

    case UNREAL_PROP_KIND_SOFT_OBJECT:
    case UNREAL_PROP_KIND_SOFT_CLASS: {
      return unreal_fsoft_object_push_summary(value, value_size, arena);
    }

    case UNREAL_PROP_KIND_WEAK_OBJECT: {
      return value_size >= (int32_t)sizeof(fweak_object_ptr_t)
             ? unreal_fweak_object_push_summary(*(const fweak_object_ptr_t *)value, arena)
             : STR_NULL;
    }

    case UNREAL_PROP_KIND_LAZY_OBJECT: {
      if (value_size < (int32_t)sizeof(unreal_prop_lazy_object_ptr_t)) {
        return STR_NULL;
      }

      const unreal_prop_lazy_object_ptr_t *lazy = (const unreal_prop_lazy_object_ptr_t *)value;
      uobject_t *object = unreal_fweak_object_resolve(lazy->weak);
      if (object) {
        return unreal_uobject_push_full_name(object, arena);
      }

      const fguid_t *guid = &lazy->object_id;
      if (guid->a == 0 && guid->b == 0 && guid->c == 0 && guid->d == 0) {
        return STR_LIT("null");
      }
      return str_push_fmt(arena, "{%08X-%08X-%08X-%08X}", guid->a, guid->b, guid->c, guid->d);
    }

    case UNREAL_PROP_KIND_INTERFACE: {
      if (value_size < (int32_t)sizeof(unreal_prop_script_interface_t)) {
        return STR_NULL;
      }

      const unreal_prop_script_interface_t *interface = (const unreal_prop_script_interface_t *)value;
      str_t object = unreal_uobject_push_value_summary(interface->object, arena);
      return str_push_fmt(arena, "%.*s Interface=%p", STR_ARG(object), interface->interface_ptr);
    }

    case UNREAL_PROP_KIND_DELEGATE: {
      return value_size >= (int32_t)sizeof(fscript_delegate_t)
             ? unreal_fscript_delegate_push_summary((const fscript_delegate_t *)value, arena)
             : STR_NULL;
    }

    case UNREAL_PROP_KIND_MULTICAST_DELEGATE:
    case UNREAL_PROP_KIND_MULTICAST_INLINE_DELEGATE: {
      if (value_size < (int32_t)sizeof(fmulticast_script_delegate_t)) {
        return STR_NULL;
      }
      const fmulticast_script_delegate_t *list = (const fmulticast_script_delegate_t *)value;
      return str_push_fmt(arena, "Num=%d Max=%d Data=%p", list->num, list->max, list->data);
    }

    case UNREAL_PROP_KIND_MULTICAST_SPARSE_DELEGATE: {
      return str_push_fmt(arena, "SparseDelegate 0x%02X", (uint32_t)*(const uint8_t *)value);
    }

    case UNREAL_PROP_KIND_STRUCT: {
      return unreal_fprop_push_struct_summary(prop, value, value_size, arena);
    }

    case UNREAL_PROP_KIND_ARRAY: {
      int32_t num = unreal_array_num(value, (fprop_array_t *)prop);
      if (num < 0) {
        return STR_LIT("<invalid array>");
      }

      const fscript_array_t *array = (const fscript_array_t *)value;
      return str_push_fmt(arena, "Num=%d Max=%d Data=%p", num, array->max, array->data);
    }

    case UNREAL_PROP_KIND_SET: {
      int32_t num       = unreal_set_num(value, (fprop_set_t *)prop);
      int32_t max_index = unreal_set_max_index(value, (fprop_set_t *)prop);
      return num >= 0 && max_index >= 0
             ? str_push_fmt(arena, "Num=%d Slots=%d", num, max_index)
             : STR_LIT("<invalid set>");
    }

    case UNREAL_PROP_KIND_MAP: {
      int32_t num       = unreal_map_num(value, (fprop_map_t *)prop);
      int32_t max_index = unreal_map_max_index(value, (fprop_map_t *)prop);
      return num >= 0 && max_index >= 0
             ? str_push_fmt(arena, "Num=%d Slots=%d", num, max_index)
             : STR_LIT("<invalid map>");
    }

    case UNREAL_PROP_KIND_ENUM: {
      return unreal_fprop_push_enum_summary((fprop_enum_t *)prop, value, arena);
    }

    default: {
      return unreal_fprop_push_hex_summary(value, value_size, arena);
    }
  }
}

static bool
unreal_fprop_has_valid_layout(fprop_t *prop)
{
  return prop && prop->elem_size > 0 && prop->array_dim > 0;
}

uint64_t
unreal_fprop_complete_size(fprop_t *prop)
{
  if (!unreal_fprop_has_valid_layout(prop)) {
    return 0;
  }
  return (uint64_t)prop->elem_size * (uint64_t)prop->array_dim;
}

int32_t
unreal_fprop_min_alignment(fprop_t *prop)
{
  if (!unreal_fprop_has_valid_layout(prop) || !prop->vtable || !prop->vtable->get_min_alignment) {
    return 0;
  }

  int32_t alignment = prop->vtable->get_min_alignment(prop);
  return alignment > 0 ? alignment : 0;
}

void *
unreal_fprop_value_at(fprop_t *prop, void *value, int32_t array_idx)
{
  if (!unreal_fprop_has_valid_layout(prop) || !value || array_idx < 0 || array_idx >= prop->array_dim) {
    return NULL;
  }
  return (uint8_t *)value + (uint64_t)array_idx * (uint64_t)prop->elem_size;
}

void *
unreal_fprop_value_in_container(fprop_t *prop, void *container, int32_t array_idx)
{
  if (!prop || !container || prop->offset_internal < 0) {
    return NULL;
  }
  return unreal_fprop_value_at(prop, (uint8_t *)container + prop->offset_internal, array_idx);
}

bool
unreal_fprop_initialize_value(fprop_t *prop, void *value)
{
  uint64_t size = unreal_fprop_complete_size(prop);
  if (size == 0 || !value) {
    return false;
  }

  if (prop->prop_flags & CPF_ZERO_CONSTRUCTOR) {
    mem_zero(value, size);
    return true;
  }

  if (!prop->vtable || !prop->vtable->initialize_value_internal) {
    return false;
  }

  prop->vtable->initialize_value_internal(prop, value);
  return true;
}

bool
unreal_fprop_destroy_value(fprop_t *prop, void *value)
{
  if (!unreal_fprop_has_valid_layout(prop) || !value) {
    return false;
  }

  if (prop->prop_flags & CPF_NO_DESTRUCTOR) {
    return true;
  }

  if (!prop->vtable || !prop->vtable->destroy_value_internal) {
    return false;
  }

  prop->vtable->destroy_value_internal(prop, value);
  return true;
}

bool
unreal_fprop_clear_single_value(fprop_t *prop, void *value)
{
  if (!unreal_fprop_has_valid_layout(prop) || !value) {
    return false;
  }

  if ((prop->prop_flags & (CPF_NO_DESTRUCTOR | CPF_ZERO_CONSTRUCTOR)) ==
      (CPF_NO_DESTRUCTOR | CPF_ZERO_CONSTRUCTOR)) {
    mem_zero(value, (uint64_t)prop->elem_size);
    return true;
  }

  if (!prop->vtable || !prop->vtable->clear_value_internal) {
    return false;
  }

  prop->vtable->clear_value_internal(prop, value);
  return true;
}

bool
unreal_fprop_clear_complete_value(fprop_t *prop, void *value)
{
  if (!unreal_fprop_has_valid_layout(prop) || !value) {
    return false;
  }

  for (int32_t i = 0; i < prop->array_dim; ++i) {
    void *elem = unreal_fprop_value_at(prop, value, i);
    if (!unreal_fprop_clear_single_value(prop, elem)) {
      return false;
    }
  }
  return true;
}

bool
unreal_fprop_copy_single_value(fprop_t *prop, void *dst, const void *src)
{
  if (!unreal_fprop_has_valid_layout(prop) || !dst || !src) {
    return false;
  }

  if (dst == src) {
    return true;
  }

  if (prop->prop_flags & CPF_IS_PLAIN_OLD_DATA) {
    mem_copy(dst, (void *)src, (uint64_t)prop->elem_size);
    return true;
  }

  if (!prop->vtable || !prop->vtable->copy_values_internal) {
    return false;
  }

  prop->vtable->copy_values_internal(prop, dst, src, 1);
  return true;
}

bool
unreal_fprop_copy_complete_value(fprop_t *prop, void *dst, const void *src)
{
  uint64_t size = unreal_fprop_complete_size(prop);
  if (size == 0 || !dst || !src) {
    return false;
  }

  if (dst == src) {
    return true;
  }

  if (prop->prop_flags & CPF_IS_PLAIN_OLD_DATA) {
    mem_copy(dst, (void *)src, size);
    return true;
  }

  if (!prop->vtable || !prop->vtable->copy_values_internal) {
    return false;
  }

  prop->vtable->copy_values_internal(prop, dst, src, prop->array_dim);
  return true;
}

bool
unreal_fprop_copy_single_value_to_script_vm(fprop_t *prop, void *dst, const void *src)
{
  if (!unreal_fprop_has_valid_layout(prop) || !dst || !src ||
      !prop->vtable || !prop->vtable->copy_single_value_to_script_vm) {
    return false;
  }

  prop->vtable->copy_single_value_to_script_vm(prop, dst, src);
  return true;
}

bool
unreal_fprop_copy_complete_value_to_script_vm(fprop_t *prop, void *dst, const void *src)
{
  if (!unreal_fprop_has_valid_layout(prop) || !dst || !src ||
      !prop->vtable || !prop->vtable->copy_complete_value_to_script_vm) {
    return false;
  }

  prop->vtable->copy_complete_value_to_script_vm(prop, dst, src);
  return true;
}

bool
unreal_fprop_copy_single_value_from_script_vm(fprop_t *prop, void *dst, const void *src)
{
  if (!unreal_fprop_has_valid_layout(prop) || !dst || !src ||
      !prop->vtable || !prop->vtable->copy_single_value_from_script_vm) {
    return false;
  }

  prop->vtable->copy_single_value_from_script_vm(prop, dst, src);
  return true;
}

bool
unreal_fprop_copy_complete_value_from_script_vm(fprop_t *prop, void *dst, const void *src)
{
  if (!unreal_fprop_has_valid_layout(prop) || !dst || !src ||
      !prop->vtable || !prop->vtable->copy_complete_value_from_script_vm) {
    return false;
  }

  prop->vtable->copy_complete_value_from_script_vm(prop, dst, src);
  return true;
}

bool
unreal_fprop_single_values_identical(fprop_t *prop, const void *a, const void *b, uint32_t port_flags)
{
  if (!unreal_fprop_has_valid_layout(prop) || !a || !prop->vtable || !prop->vtable->identical) {
    return false;
  }
  return prop->vtable->identical(prop, a, b, port_flags);
}

bool
unreal_fprop_complete_values_identical(fprop_t *prop, const void *a, const void *b, uint32_t port_flags)
{
  if (!unreal_fprop_has_valid_layout(prop) || !a || !prop->vtable || !prop->vtable->identical) {
    return false;
  }

  for (int32_t i = 0; i < prop->array_dim; ++i) {
    const void *a_elem = unreal_fprop_value_at(prop, (void *)a, i);
    const void *b_elem = b ? unreal_fprop_value_at(prop, (void *)b, i) : NULL;
    if (!prop->vtable->identical(prop, a_elem, b_elem, port_flags)) {
      return false;
    }
  }
  return true;
}

bool
unreal_fprop_value_hash(fprop_t *prop, const void *value, uint32_t *out_hash)
{
  if (!unreal_fprop_has_valid_layout(prop) || !value || !out_hash ||
      !(prop->prop_flags & CPF_HAS_GET_VALUE_TYPE_HASH) ||
      !prop->vtable || !prop->vtable->get_value_type_hash_internal) {
    return false;
  }

  *out_hash = prop->vtable->get_value_type_hash_internal(prop, value);
  return true;
}

bool
unreal_fprop_same_type(fprop_t *prop, fprop_t *other)
{
  return prop && other && prop->vtable && prop->vtable->same_type && prop->vtable->same_type(prop, other);
}

static bool
unreal_fprop_kind_is_object_reference(unreal_prop_kind_t kind)
{
  return kind == UNREAL_PROP_KIND_OBJECT      || kind == UNREAL_PROP_KIND_CLASS       ||
         kind == UNREAL_PROP_KIND_SOFT_OBJECT || kind == UNREAL_PROP_KIND_SOFT_CLASS  ||
         kind == UNREAL_PROP_KIND_WEAK_OBJECT || kind == UNREAL_PROP_KIND_LAZY_OBJECT;
}

uobject_t *
unreal_fprop_get_object(fprop_obj_base_t *prop, const void *value)
{
  if (!prop || !value || !prop->vtable || !unreal_fprop_kind_is_object_reference(unreal_fprop_get_kind((fprop_t *)prop))) {
    return NULL;
  }

  return prop->vtable->get_object_prop_value ? prop->vtable->get_object_prop_value(prop, value) : NULL;
}

uobject_t *
unreal_fprop_get_referenced_object(fprop_t *prop, const void *value)
{
  if (!prop || !value) {
    return NULL;
  }

  unreal_prop_kind_t kind = unreal_fprop_get_kind(prop);
  switch (kind) {
    case UNREAL_PROP_KIND_OBJECT:
    case UNREAL_PROP_KIND_CLASS: {
      return prop->elem_size >= (int32_t)sizeof(uobject_t *)
             ? unreal_fprop_get_object((fprop_obj_base_t *)prop, value)
             : NULL;
    }

    case UNREAL_PROP_KIND_WEAK_OBJECT: {
      return prop->elem_size >= (int32_t)sizeof(fweak_object_ptr_t)
             ? unreal_fprop_get_object((fprop_obj_base_t *)prop, value)
             : NULL;
    }

    case UNREAL_PROP_KIND_LAZY_OBJECT: {
      return prop->elem_size >= (int32_t)sizeof(unreal_prop_lazy_object_ptr_t)
             ? unreal_fprop_get_object((fprop_obj_base_t *)prop, value)
             : NULL;
    }

    case UNREAL_PROP_KIND_SOFT_OBJECT:
    case UNREAL_PROP_KIND_SOFT_CLASS: {
      return prop->elem_size >= (int32_t)sizeof(unreal_prop_soft_object_ptr_t)
             ? unreal_fprop_get_object((fprop_obj_base_t *)prop, value)
             : NULL;
    }

    case UNREAL_PROP_KIND_INTERFACE: {
      return prop->elem_size >= (int32_t)sizeof(unreal_prop_script_interface_t)
             ? ((const unreal_prop_script_interface_t *)value)->object
             : NULL;
    }

    default: {
      return NULL;
    }
  }
}

bool
unreal_fprop_object_is_compatible(fprop_obj_base_t *prop, uobject_t *object)
{
  unreal_prop_kind_t kind = unreal_fprop_get_kind((fprop_t *)prop);
  if (!unreal_fprop_kind_is_object_reference(kind) || (object && (!prop->prop_class || !unreal_uobject_is_a(object, prop->prop_class)))) {
    return false;
  }

  if (!object) {
    return true;
  }

  uclass_t *meta_class = NULL;
  if (kind == UNREAL_PROP_KIND_CLASS) {
    meta_class = ((fprop_class_t *)prop)->meta_class;
  } else if (kind == UNREAL_PROP_KIND_SOFT_CLASS) {
    meta_class = ((fprop_class_soft_t *)prop)->meta_class;
  }

  if (meta_class && !unreal_uclass_is_child_of((uclass_t *)object, meta_class)) {
    return false;
  }
  return true;
}

bool
unreal_fprop_set_object(fprop_obj_base_t *prop, void *value, uobject_t *object)
{
  if (!prop || !value || !prop->vtable || !unreal_fprop_object_is_compatible(prop, object)) {
    return false;
  }

  if (!prop->vtable->set_object_prop_value) {
    return false;
  }

  prop->vtable->set_object_prop_value(prop, value, object);
  return true;
}

void
unreal_fprop_initialize_in_container(fprop_t *prop, void *container)
{
  void *value = unreal_fprop_value_in_container(prop, container, 0);
  if (value) {
    unreal_fprop_initialize_value(prop, value);
  }
}

void
unreal_fprop_destroy_in_container(fprop_t *prop, void *container)
{
  void *value = unreal_fprop_value_in_container(prop, container, 0);
  if (value) {
    unreal_fprop_destroy_value(prop, value);
  }
}

bool
unreal_ustruct_initialize_struct(ustruct_t *struct_type, void *memory, int32_t array_dim)
{
  if (!struct_type || !memory || array_dim <= 0 || struct_type->props_size < 0 ||
      !struct_type->vtable || !struct_type->vtable->init_struct) {
    return false;
  }

  struct_type->vtable->init_struct(struct_type, memory, array_dim);
  return true;
}

bool
unreal_ustruct_destroy_struct(ustruct_t *struct_type, void *memory, int32_t array_dim)
{
  if (!struct_type || !memory || array_dim <= 0 || struct_type->props_size < 0 ||
      !struct_type->vtable || !struct_type->vtable->destroy_struct) {
    return false;
  }

  struct_type->vtable->destroy_struct(struct_type, memory, array_dim);
  return true;
}

const wchar_t *
unreal_fprop_import_text_direct(fprop_t *prop, const wchar_t *text, void *value, uobject_t *owner)
{
  if (!prop || !prop->vtable || !prop->vtable->import_text_internal || !text || !value) {
    return NULL;
  }
  return prop->vtable->import_text_internal(prop, text, value, 0, owner, NULL);
}

bool
unreal_fprop_import_text(fprop_t *prop, void *value, uobject_t *owner, str_t text, arena_t *arena)
{
  uint64_t size      = unreal_fprop_complete_size(prop);
  int32_t  alignment = unreal_fprop_min_alignment(prop);
  if (size == 0 || alignment <= 0 || !value || !text.data || !arena) {
    return false;
  }

  bool        result = false;
  tmp_arena_t tmp    = scratch_begin(arena);
  {
    str16_t wide = str16_from_str(tmp.arena, text);
    void   *copy = arena_push_aligned(tmp.arena, size, (uint64_t)alignment);
    if (wide.data && copy && unreal_fprop_initialize_value(prop, copy)) {
      if (unreal_fprop_copy_single_value(prop, copy, value)) {
        const wchar_t *end = unreal_fprop_import_text_direct(prop, (const wchar_t *)wide.data, copy, owner);
        if (end) {
          while (*end == L' ' || *end == L'\t' || *end == L'\r' || *end == L'\n') {
            end += 1;
          }

          if (*end == L'\0') {
            result = unreal_fprop_copy_single_value(prop, value, copy);
          }
        }
      }
      unreal_fprop_destroy_value(prop, copy);
    }
  }
  scratch_end(tmp);
  return result;
}

static bool
unreal_array_prop_is_valid(fprop_array_t *prop)
{
  return prop && unreal_fprop_has_valid_layout(&prop->base) &&
         prop->base.elem_size >= (int32_t)sizeof(fscript_array_t) &&
         prop->inner && unreal_fprop_has_valid_layout(prop->inner);
}

static bool
unreal_array_view_is_valid(const void *array, fprop_array_t *prop)
{
  if (!array || !unreal_array_prop_is_valid(prop) || (prop->array_flags & EAPF_USES_MEM_IMAGE_ALLOCATOR)) {
    return false;
  }

  const fscript_array_t *script_array = (const fscript_array_t *)array;
  return script_array->num >= 0 && script_array->max >= script_array->num && (script_array->num == 0 || script_array->data != NULL);
}

int32_t
unreal_array_num(const void *array, fprop_array_t *prop)
{
  if (!unreal_array_view_is_valid(array, prop)) {
    return -1;
  }
  return ((const fscript_array_t *)array)->num;
}

void *
unreal_array_get(void *array, fprop_array_t *prop, int32_t idx)
{
  if (!unreal_array_view_is_valid(array, prop)) {
    return NULL;
  }

  fscript_array_t *script_array = (fscript_array_t *)array;
  if (idx < 0 || idx >= script_array->num) {
    return NULL;
  }

  return (uint8_t *)script_array->data + (uint64_t)idx * (uint64_t)prop->inner->elem_size;
}

int32_t
unreal_array_add(void *array, fprop_array_t *prop, const void *value)
{
  if (!array || !unreal_array_prop_is_valid(prop) || !value || !generic_array_add) {
    return -1;
  }
  return generic_array_add(array, prop, value);
}

void
unreal_array_insert(void *array, fprop_array_t *prop, int32_t idx, const void *value)
{
  if (array && unreal_array_prop_is_valid(prop) && idx >= 0 && value && generic_array_insert) {
    generic_array_insert(array, prop, value, idx);
  }
}

void
unreal_array_remove(void *array, fprop_array_t *prop, int32_t idx)
{
  if (array && unreal_array_prop_is_valid(prop) && idx >= 0 && generic_array_remove) {
    generic_array_remove(array, prop, idx);
  }
}

void
unreal_array_resize(void *array, fprop_array_t *prop, int32_t size)
{
  if (array && unreal_array_prop_is_valid(prop) && size >= 0 && generic_array_resize) {
    generic_array_resize(array, prop, size);
  }
}

void
unreal_array_set(void *array, fprop_array_t *prop, int32_t idx, const void *value, bool size_to_fit)
{
  if (array && unreal_array_prop_is_valid(prop) && idx >= 0 && value && generic_array_set) {
    generic_array_set(array, prop, idx, value, size_to_fit);
  }
}

bool
unreal_array_clear(void *array, fprop_array_t *prop)
{
  return array && unreal_array_prop_is_valid(prop) && unreal_fprop_clear_single_value(&prop->base, array);
}

static bool
unreal_script_set_view_is_valid(const fscript_set_t *set, const fscript_set_layout_t *layout)
{
  if (!set || !layout || layout->size <= 0 || layout->sparse_array_layout.size < layout->size ||
      layout->sparse_array_layout.alignment <= 0) {
    return false;
  }

  const fscript_sparse_array_t *sparse = &set->elems;
  return sparse->data.num             >= 0 &&
         sparse->data.max             >= sparse->data.num &&
         sparse->num_free_idx         >= 0 &&
         sparse->num_free_idx         <= sparse->data.num &&
         sparse->alloc_flags.num_bits >= sparse->data.num &&
         sparse->alloc_flags.max_bits >= sparse->alloc_flags.num_bits &&
         (sparse->data.num == 0 || sparse->data.data != NULL);
}

static int32_t
unreal_script_set_num(const fscript_set_t *set, const fscript_set_layout_t *layout)
{
  if (!unreal_script_set_view_is_valid(set, layout)) {
    return -1;
  }
  return set->elems.data.num - set->elems.num_free_idx;
}

static int32_t
unreal_script_set_max_index(const fscript_set_t *set, const fscript_set_layout_t *layout)
{
  if (!unreal_script_set_view_is_valid(set, layout)) {
    return -1;
  }
  return set->elems.data.num;
}

static bool
unreal_script_set_is_valid_index(const fscript_set_t *set, const fscript_set_layout_t *layout, int32_t idx)
{
  return unreal_script_set_view_is_valid(set, layout) && idx >= 0 && idx < set->elems.data.num &&
         unreal_tbit_array_is_set((tbit_array_t *)&set->elems.alloc_flags, idx);
}

static void *
unreal_script_set_get(void *set, const fscript_set_layout_t *layout, int32_t index)
{
  fscript_set_t *script_set = (fscript_set_t *)set;
  if (!unreal_script_set_is_valid_index(script_set, layout, index)) {
    return NULL;
  }

  return (uint8_t *)script_set->elems.data.data + (uint64_t)index * (uint64_t)layout->sparse_array_layout.size;
}

static bool
unreal_set_prop_is_valid(fprop_set_t *prop)
{
  return prop && unreal_fprop_has_valid_layout(&prop->base) &&
         prop->base.elem_size >= (int32_t)sizeof(fscript_set_t) && prop->elem_prop &&
         unreal_fprop_has_valid_layout(prop->elem_prop) && prop->set_layout.size > 0 &&
         prop->set_layout.sparse_array_layout.size >= prop->set_layout.size;
}

int32_t
unreal_set_num(const void *set, fprop_set_t *prop)
{
  if (!unreal_set_prop_is_valid(prop)) {
    return -1;
  }
  return unreal_script_set_num((const fscript_set_t *)set, &prop->set_layout);
}

int32_t
unreal_set_max_index(const void *set, fprop_set_t *prop)
{
  if (!unreal_set_prop_is_valid(prop)) {
    return -1;
  }
  return unreal_script_set_max_index((const fscript_set_t *)set, &prop->set_layout);
}

bool
unreal_set_is_valid_index(const void *set, fprop_set_t *prop, int32_t idx)
{
  return unreal_set_prop_is_valid(prop) && unreal_script_set_is_valid_index((const fscript_set_t *)set, &prop->set_layout, idx);
}

const void *
unreal_set_get(const void *set, fprop_set_t *prop, int32_t idx)
{
  if (!unreal_set_prop_is_valid(prop)) {
    return NULL;
  }
  return unreal_script_set_get((void *)set, &prop->set_layout, idx);
}

int32_t
unreal_set_find_index(const void *set, fprop_set_t *prop, const void *value)
{
  int32_t max_index = unreal_set_max_index(set, prop);
  if (max_index < 0 || !value) {
    return TSET_INVALID_ID;
  }

  for (int32_t index = 0; index < max_index; ++index) {
    const void *elem = unreal_set_get(set, prop, index);
    if (elem && unreal_fprop_single_values_identical(prop->elem_prop, elem, value, 0)) {
      return index;
    }
  }
  return TSET_INVALID_ID;
}

bool
unreal_set_contains(const void *set, fprop_set_t *prop, const void *value)
{
  return unreal_set_find_index(set, prop, value) != TSET_INVALID_ID;
}

void
unreal_set_add(void *set, fprop_set_t *prop, const void *value)
{
  if (!set || !unreal_set_prop_is_valid(prop) || !value || !fscript_set_add_elem) {
    return;
  }

  fscript_set_helper_t helper = {
    .elem_prop = prop->elem_prop,
    .set       = set,
    .layout    = prop->set_layout,
  };
  fscript_set_add_elem(&helper, value);
}

bool
unreal_set_remove(void *set, fprop_set_t *prop, const void *value)
{
  if (!set || !unreal_set_prop_is_valid(prop) || !value || !generic_set_remove) {
    return false;
  }
  return generic_set_remove(set, prop, value);
}

bool
unreal_set_clear(void *set, fprop_set_t *prop)
{
  return set && unreal_set_prop_is_valid(prop) && unreal_fprop_clear_single_value(&prop->base, set);
}

static bool
unreal_map_prop_is_valid(fprop_map_t *prop)
{
  if (!prop || !unreal_fprop_has_valid_layout(&prop->base) || !prop->key_prop || !prop->val_prop ||
      prop->base.elem_size < (int32_t)sizeof(fscript_map_t) ||
      !unreal_fprop_has_valid_layout(prop->key_prop) || !unreal_fprop_has_valid_layout(prop->val_prop)) {
    return false;
  }

  const fscript_map_layout_t *layout = &prop->map_layout;
  return layout->set_layout.size > 0 &&
         layout->set_layout.sparse_array_layout.size >= layout->set_layout.size &&
         layout->value_offset >= prop->key_prop->elem_size &&
         (uint64_t)layout->value_offset + (uint64_t)prop->val_prop->elem_size <= (uint64_t)layout->set_layout.size;
}

static bool
unreal_map_view_prop_is_valid(fprop_map_t *prop)
{
  return unreal_map_prop_is_valid(prop) && !(prop->map_flags & EMPF_USES_MEM_IMAGE_ALLOCATOR);
}

int32_t
unreal_map_num(const void *map, fprop_map_t *prop)
{
  if (!map || !unreal_map_view_prop_is_valid(prop)) {
    return -1;
  }
  return unreal_script_set_num(&((const fscript_map_t *)map)->pairs, &prop->map_layout.set_layout);
}

int32_t
unreal_map_max_index(const void *map, fprop_map_t *prop)
{
  if (!map || !unreal_map_view_prop_is_valid(prop)) {
    return -1;
  }
  return unreal_script_set_max_index(&((const fscript_map_t *)map)->pairs, &prop->map_layout.set_layout);
}

bool
unreal_map_is_valid_index(const void *map, fprop_map_t *prop, int32_t idx)
{
  return unreal_map_view_prop_is_valid(prop) && map &&
         unreal_script_set_is_valid_index(&((const fscript_map_t *)map)->pairs, &prop->map_layout.set_layout, idx);
}

const void *
unreal_map_get_key(const void *map, fprop_map_t *prop, int32_t idx)
{
  if (!unreal_map_view_prop_is_valid(prop) || !map) {
    return NULL;
  }
  return unreal_script_set_get(&((fscript_map_t *)map)->pairs, &prop->map_layout.set_layout, idx);
}

void *
unreal_map_get_value(void *map, fprop_map_t *prop, int32_t idx)
{
  uint8_t *key = (uint8_t *)unreal_map_get_key(map, prop, idx);
  return key ? key + prop->map_layout.value_offset : NULL;
}

int32_t
unreal_map_find_index(const void *map, fprop_map_t *prop, const void *key)
{
  int32_t max_idx = unreal_map_max_index(map, prop);
  if (max_idx < 0 || !key) {
    return TSET_INVALID_ID;
  }

  for (int32_t idx = 0; idx < max_idx; ++idx) {
    const void *candidate = unreal_map_get_key(map, prop, idx);
    if (candidate && unreal_fprop_single_values_identical(prop->key_prop, candidate, key, 0)) {
      return idx;
    }
  }
  return TSET_INVALID_ID;
}

void *
unreal_map_find_value_ptr(void *map, fprop_map_t *prop, const void *key)
{
  int32_t idx = unreal_map_find_index(map, prop, key);
  return idx != TSET_INVALID_ID ? unreal_map_get_value(map, prop, idx) : NULL;
}

bool
unreal_map_clear(void *map, fprop_map_t *prop)
{
  return map && unreal_map_prop_is_valid(prop) && unreal_fprop_clear_single_value(&prop->base, map);
}

void
unreal_map_add(void *map, fprop_map_t *prop, const void *key, const void *val)
{
  if (!map || !prop || !prop->key_prop || !prop->val_prop || !key || !val || !fscript_map_add_pair) {
    return;
  }

  fscript_map_helper_t helper = {
    .key_prop = prop->key_prop,
    .val_prop = prop->val_prop,
    .map      = map,
    .layout   = prop->map_layout,
    .flags    = prop->map_flags,
  };

  fscript_map_add_ctx_t ctx = {
    .helper   = &helper,
    .key      = key,
    .value    = val,
    .key_prop = prop->key_prop,
    .val_prop = prop->val_prop,
  };

  fscript_map_add_pair(&helper, &ctx);
}

bool
unreal_map_remove(void *map, fprop_map_t *prop, const void *key)
{
  if (!map || !prop || !prop->key_prop || !key || !fscript_map_remove_pair) {
    return false;
  }

  fscript_map_helper_t helper = {
    .key_prop = prop->key_prop,
    .val_prop = prop->val_prop,
    .map      = map,
    .layout   = prop->map_layout,
    .flags    = prop->map_flags,
  };

  fscript_map_remove_ctx_t ctx = {
    .helper   = &helper,
    .key      = key,
    .key_prop = prop->key_prop,
  };

  return fscript_map_remove_pair(&helper, &ctx);
}

bool
unreal_map_find(void *map, fprop_map_t *prop, const void *key, void *out_val)
{
  if (!map || !prop || !key || !generic_map_find) {
    return false;
  }
  return generic_map_find(map, prop, key, out_val);
}
