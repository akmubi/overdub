#ifndef UNREAL_PROP_H
#define UNREAL_PROP_H

#include "unreal.h"

MOD_EXTERN_C_BEGIN

/* FProperty metadata and lookup. */
typedef enum unreal_prop_kind_e {
  UNREAL_PROP_KIND_UNKNOWN,
  UNREAL_PROP_KIND_BOOL,
  UNREAL_PROP_KIND_BYTE,
  UNREAL_PROP_KIND_INT8,
  UNREAL_PROP_KIND_INT16,
  UNREAL_PROP_KIND_INT32,
  UNREAL_PROP_KIND_INT64,
  UNREAL_PROP_KIND_UINT16,
  UNREAL_PROP_KIND_UINT32,
  UNREAL_PROP_KIND_UINT64,
  UNREAL_PROP_KIND_FLOAT,
  UNREAL_PROP_KIND_DOUBLE,
  UNREAL_PROP_KIND_NAME,
  UNREAL_PROP_KIND_STRING,
  UNREAL_PROP_KIND_TEXT,
  UNREAL_PROP_KIND_OBJECT,
  UNREAL_PROP_KIND_CLASS,
  UNREAL_PROP_KIND_SOFT_OBJECT,
  UNREAL_PROP_KIND_SOFT_CLASS,
  UNREAL_PROP_KIND_WEAK_OBJECT,
  UNREAL_PROP_KIND_LAZY_OBJECT,
  UNREAL_PROP_KIND_INTERFACE,
  UNREAL_PROP_KIND_STRUCT,
  UNREAL_PROP_KIND_ARRAY,
  UNREAL_PROP_KIND_SET,
  UNREAL_PROP_KIND_MAP,
  UNREAL_PROP_KIND_ENUM,
  UNREAL_PROP_KIND_DELEGATE,
  UNREAL_PROP_KIND_MULTICAST_DELEGATE,
  UNREAL_PROP_KIND_MULTICAST_INLINE_DELEGATE,
  UNREAL_PROP_KIND_MULTICAST_SPARSE_DELEGATE,
} unreal_prop_kind_t;

typedef uint8_t unreal_func_param_role_t;
enum {
  UNREAL_FUNC_PARAM_INPUT = 0,
  UNREAL_FUNC_PARAM_OUTPUT,
  UNREAL_FUNC_PARAM_RETURN,
};

typedef struct unreal_prop_integer_s unreal_prop_integer_t;
struct unreal_prop_integer_s {
  uint64_t value;
  bool     is_signed;
};

typedef struct unreal_prop_snapshot_s unreal_prop_snapshot_t;
struct unreal_prop_snapshot_s {
  unreal_prop_snapshot_t *next;
  unreal_prop_snapshot_t *first_child;
  unreal_prop_snapshot_t *last_child;

  str_t         name;
  str_t         type;
  str_t         value;
  str_t         tooltip;
  eprop_flags_t prop_flags;
  bool          expanded;
  bool          truncated;
};

typedef struct unreal_prop_snapshot_builder_s unreal_prop_snapshot_builder_t;
struct unreal_prop_snapshot_builder_s {
  arena_t *arena;

  uint32_t max_depth;
  uint32_t max_container_elements;
  uint32_t max_nodes;
  uint32_t max_text_length;

  uint32_t node_count;
  bool     truncated;
};

MOD_API fprop_t *
unreal_ustruct_find_prop(ustruct_t *s, str_t name);

MOD_API unreal_prop_kind_t
unreal_fprop_get_kind(fprop_t *prop);
MOD_API unreal_func_param_role_t
unreal_fprop_get_param_role(fprop_t *prop);
MOD_API bool
unreal_fprop_struct_is(fprop_t *prop, fname_t struct_name);
MOD_API bool
unreal_fprop_class_is(fprop_t *prop, fname_t name);
MOD_API bool
unreal_fprop_class_is_a(fprop_t *prop, fname_t name);
MOD_API str_t
unreal_fprop_push_type_name(fprop_t *prop, arena_t *arena);

MOD_API bool
unreal_fprop_read_bool(fprop_t *prop, const void *value, bool *out);
MOD_API bool
unreal_fprop_read_integer(fprop_t *prop, const void *value, unreal_prop_integer_t *out);
MOD_API bool
unreal_fprop_read_real(fprop_t *prop, const void *value, double *out);
MOD_API bool
unreal_fprop_read_name(fprop_t *prop, const void *value, fname_t *out);
MOD_API bool
unreal_fprop_read_string(fprop_t *prop, const void *value, arena_t *arena, str_t *out);
MOD_API bool
unreal_fprop_read_soft_path(fprop_t *prop, const void *value, arena_t *arena, str_t *out);

MOD_API bool
unreal_fprop_write_bool(fprop_t *prop, void *value, bool input);
MOD_API bool
unreal_fprop_write_integer(fprop_t *prop, void *value, unreal_prop_integer_t input);
MOD_API bool
unreal_fprop_write_real(fprop_t *prop, void *value, double input);
MOD_API bool
unreal_fprop_write_name(fprop_t *prop, void *value, fname_t input);

MOD_API uobject_t *
unreal_fprop_get_object(fprop_obj_base_t *prop, const void *value);
MOD_API uobject_t *
unreal_fprop_get_referenced_object(fprop_t *prop, const void *value);
MOD_API uclass_t *
unreal_fprop_get_reference_class(fprop_t *prop);
MOD_API bool
unreal_fprop_object_is_compatible(fprop_obj_base_t *prop, uobject_t *object);
MOD_API bool
unreal_fprop_set_object(fprop_obj_base_t *prop, void *value, uobject_t *object);
MOD_API bool
unreal_fprop_interface_is_compatible(fprop_iface_t *prop, uobject_t *object);
MOD_API bool
unreal_fprop_set_interface(fprop_iface_t *prop, void *value, uobject_t *object);

MOD_API uobject_t *
unreal_fweak_object_resolve(fweak_object_ptr_t weak);
MOD_API bool
unreal_fweak_object_from_object(uobject_t *object, fweak_object_ptr_t *out);
MOD_API bool
unreal_ufunction_signature_compatible(ufunc_t *signature, ufunc_t *function);
MOD_API ufunc_t *
unreal_fprop_delegate_signature(fprop_t *prop);
MOD_API bool
unreal_fscript_delegate_make(fscript_delegate_t *out, uobject_t *target, ufunc_t *function);
MOD_API bool
unreal_fprop_delegate_contains(uobject_t *owner, fprop_t *prop, const fscript_delegate_t *binding);
MOD_API bool
unreal_fprop_delegate_bind(uobject_t *owner, fprop_t *prop, const fscript_delegate_t *binding);
MOD_API bool
unreal_fprop_delegate_unbind(uobject_t *owner, fprop_t *prop, const fscript_delegate_t *binding);
MOD_API str_t
unreal_fscript_delegate_push_summary(const fscript_delegate_t *delegate, arena_t *arena);
/* NOTE: value_size is the readable size of one property element, not its complete fixed-array size */
MOD_API str_t
unreal_fprop_push_value_summary(fprop_t *prop, const void *value, int32_t value_size, arena_t *arena);

MOD_API void
unreal_prop_snapshot_builder_init(unreal_prop_snapshot_builder_t *builder, arena_t *arena);
MOD_API unreal_prop_snapshot_t *
unreal_fprop_snapshot_build(unreal_prop_snapshot_builder_t *builder, fprop_t *prop, str_t name, const void *value);

static inline void *
unreal_uprop_ptr(fprop_t *prop, void *container)
{
  return (uint8_t *)container + prop->offset_internal;
}

MOD_API uint64_t
unreal_fprop_complete_size(fprop_t *prop);
MOD_API int32_t
unreal_fprop_min_alignment(fprop_t *prop);
MOD_API void *
unreal_fprop_value_at(fprop_t *prop, void *value, int32_t array_idx);
MOD_API void *
unreal_fprop_value_in_container(fprop_t *prop, void *container, int32_t array_idx);

MOD_API bool
unreal_fprop_initialize_value(fprop_t *prop, void *value);
MOD_API bool
unreal_fprop_destroy_value(fprop_t *prop, void *value);
MOD_API bool
unreal_fprop_clear_single_value(fprop_t *prop, void *value);
MOD_API bool
unreal_fprop_clear_complete_value(fprop_t *prop, void *value);
MOD_API bool
unreal_fprop_copy_single_value(fprop_t *prop, void *dst, const void *src);
MOD_API bool
unreal_fprop_copy_complete_value(fprop_t *prop, void *dst, const void *src);

MOD_API bool
unreal_fprop_copy_single_value_to_script_vm(fprop_t *prop, void *dst, const void *src);
MOD_API bool
unreal_fprop_copy_complete_value_to_script_vm(fprop_t *prop, void *dst, const void *src);
MOD_API bool
unreal_fprop_copy_single_value_from_script_vm(fprop_t *prop, void *dst, const void *src);
MOD_API bool
unreal_fprop_copy_complete_value_from_script_vm(fprop_t *prop, void *dst, const void *src);
MOD_API bool
unreal_fprop_single_values_identical(fprop_t *prop, const void *a, const void *b, uint32_t port_flags);
MOD_API bool
unreal_fprop_complete_values_identical(fprop_t *prop, const void *a, const void *b, uint32_t port_flags);
MOD_API bool
unreal_fprop_value_hash(fprop_t *prop, const void *value, uint32_t *out_hash);
MOD_API bool
unreal_fprop_same_type(fprop_t *prop, fprop_t *other);

MOD_API void
unreal_fprop_initialize_in_container(fprop_t *prop, void *container);
MOD_API void
unreal_fprop_destroy_in_container(fprop_t *prop, void *container);

MOD_API bool
unreal_ustruct_initialize_struct(ustruct_t *struct_type, void *memory, int32_t array_dim);
MOD_API bool
unreal_ustruct_destroy_struct(ustruct_t *struct_type, void *memory, int32_t array_dim);

MOD_API const wchar_t *
unreal_fprop_import_text_direct(fprop_t *prop, const wchar_t *text, void *value, uobject_t *owner);

/* NOTE: imports one initialized property element transactionally. text uses engine's ImportText literal syntax */
MOD_API bool
unreal_fprop_import_text(fprop_t *prop, void *value, uobject_t *owner, str_t text, arena_t *arena);

/* Reflected dynamic containers. Indices are Unreal's zero-based sparse/array indices. */
MOD_API int32_t
unreal_array_num(const void *array, fprop_array_t *prop);
MOD_API void *
unreal_array_get(void *array, fprop_array_t *prop, int32_t idx);
MOD_API int32_t
unreal_array_add(void *array, fprop_array_t *prop, const void *value);
MOD_API void
unreal_array_insert(void *array, fprop_array_t *prop, int32_t idx, const void *value);
MOD_API void
unreal_array_remove(void *array, fprop_array_t *prop, int32_t idx);
MOD_API void
unreal_array_resize(void *array, fprop_array_t *prop, int32_t size);
MOD_API void
unreal_array_set(void *array, fprop_array_t *prop, int32_t idx, const void *value, bool size_to_fit);
MOD_API bool
unreal_array_clear(void *array, fprop_array_t *prop);

MOD_API int32_t
unreal_set_num(const void *set, fprop_set_t *prop);
MOD_API int32_t
unreal_set_max_index(const void *set, fprop_set_t *prop);
MOD_API bool
unreal_set_is_valid_index(const void *set, fprop_set_t *prop, int32_t idx);
MOD_API const void *
unreal_set_get(const void *set, fprop_set_t *prop, int32_t idx);
MOD_API int32_t
unreal_set_find_index(const void *set, fprop_set_t *prop, const void *value);
MOD_API bool
unreal_set_contains(const void *set, fprop_set_t *prop, const void *value);
MOD_API void
unreal_set_add(void *set, fprop_set_t *prop, const void *value);
MOD_API bool
unreal_set_remove(void *set, fprop_set_t *prop, const void *value);
MOD_API bool
unreal_set_clear(void *set, fprop_set_t *prop);

MOD_API int32_t
unreal_map_num(const void *map, fprop_map_t *prop);
MOD_API int32_t
unreal_map_max_index(const void *map, fprop_map_t *prop);
MOD_API bool
unreal_map_is_valid_index(const void *map, fprop_map_t *prop, int32_t idx);
MOD_API const void *
unreal_map_get_key(const void *map, fprop_map_t *prop, int32_t idx);
MOD_API void *
unreal_map_get_value(void *map, fprop_map_t *prop, int32_t idx);
MOD_API int32_t
unreal_map_find_index(const void *map, fprop_map_t *prop, const void *key);
MOD_API void *
unreal_map_find_value_ptr(void *map, fprop_map_t *prop, const void *key);
MOD_API bool
unreal_map_clear(void *map, fprop_map_t *prop);
MOD_API void
unreal_map_add(void *map, fprop_map_t *prop, const void *key, const void *val);
MOD_API bool
unreal_map_remove(void *map, fprop_map_t *prop, const void *key);
MOD_API bool
unreal_map_find(void *map, fprop_map_t *prop, const void *key, void *out_val);

MOD_EXTERN_C_END

#endif /* UNREAL_PROP_H */
