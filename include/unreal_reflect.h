#ifndef UNREAL_REFLECT_H
#define UNREAL_REFLECT_H

#include "unreal_prop.h"

MOD_EXTERN_C_BEGIN

typedef struct unreal_class_def_s          unreal_class_def_t;
typedef struct unreal_func_call_s          unreal_func_call_t;
typedef struct unreal_func_def_s           unreal_func_def_t;
typedef struct unreal_prop_array_default_s unreal_prop_array_default_t;
typedef struct unreal_prop_map_default_s   unreal_prop_map_default_t;
typedef struct unreal_prop_def_s           unreal_prop_def_t;
typedef struct unreal_prop_type_s          unreal_prop_type_t;
typedef uint64_t                           unreal_reflect_owner_t;

#define UNREAL_REFLECT_OWNER_PERSISTENT ((unreal_reflect_owner_t)0)

typedef void (MOD_CALL *unreal_func_impl_fn_t)(unreal_func_call_t *call);

/* NOTE: 'kind' comes from the property codec */
struct unreal_prop_type_s {
  unreal_prop_kind_t        type;
  uclass_t                 *cls;
  uscript_struct_t         *struct_type;
  uenum_t                  *enum_type;
  const unreal_prop_type_t *inner;
  const unreal_prop_type_t *value;
};

struct unreal_prop_array_default_s {
  const void *items;
  uint32_t    count;
};

struct unreal_prop_map_default_s {
  const void *keys;
  const void *values;
  uint32_t    count;
};

struct unreal_prop_def_s {
  str_t               name;
  unreal_prop_kind_t  type;
  eprop_flags_t       flags;
  uclass_t           *cls;
  uscript_struct_t   *struct_type;
  uenum_t            *enum_type;
  unreal_prop_type_t  inner;
  unreal_prop_type_t  value;

  union {
    struct {
      bool default_val;
    } boolean;

    struct {
      int64_t default_val;
    } integer;

    struct {
      uint64_t default_val;
    } unsigned_integer;

    struct {
      double default_val;
    } real;

    struct {
      uobject_t *default_val;
    } object;

    struct {
      str_t default_val;
    } fname;

    struct {
      str_t default_val;
    } string;

    struct {
      str_t default_val;
    } soft;

    struct {
      str_t default_val;
    } structure;

    struct {
      int64_t default_val;
    } enumeration;

    struct {
      unreal_prop_array_default_t default_val;
    } array;

    struct {
      unreal_prop_array_default_t default_val;
    } set;

    struct {
      unreal_prop_map_default_t default_val;
    } map;
  };
};

struct unreal_func_call_s {
  uobject_t *object;
  ufunc_t   *function;
  void      *params;
  void      *return_value;
  void      *user;
};

struct unreal_func_def_s {
  str_t                    name;
  efunc_flags_t            flags;
  const unreal_prop_def_t *params;
  uint32_t                 num_params;
  unreal_prop_type_t       ret;
  unreal_func_impl_fn_t    impl;
  void                    *user;
};

struct unreal_class_def_s {
  uint32_t                 struct_size;
  uclass_t                *parent;
  const unreal_prop_def_t *props;
  uint32_t                 num_props;
  const unreal_func_def_t *funcs;
  uint32_t                 num_funcs;
};

uclass_t *
unreal_reflect_define_class(str_t scope, str_t name, const unreal_class_def_t *def);

uclass_t *
unreal_reflect_define_class_owned(unreal_reflect_owner_t owner, str_t scope, str_t name, const unreal_class_def_t *def);

void
unreal_reflect_disable_owner(unreal_reflect_owner_t owner);

MOD_EXTERN_C_END

#endif /* UNREAL_REFLECT_H */
