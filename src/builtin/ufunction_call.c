#include "uobject_search_internal.h"

#include "arena.h"
#include "globals.h"
#include "log.h"
#include "mod_host.h"
#include "scratch.h"
#include "str.h"
#include "ui_nuklear.h"
#include "unreal.h"

#include "vendor_stb.h"

#include <stdlib.h>

#define CALL_TEXT_CAP            (512)
#define CALL_SEARCH_CAP          (128)
#define CALL_STATUS_CAP          (512)
#define CALL_MAX_CONTAINER_ELEMS (128)

#define CALL_C_TYPE_TEXT  nk_rgba(218, 156, 62, 255)
#define CALL_C_VALUE_TEXT nk_rgba(139, 191, 166, 255)
#define CALL_C_LINK_TEXT  nk_rgba(122, 184, 232, 255)

typedef uint8_t call_param_role_t;
enum {
  CALL_PARAM_INPUT = 0,
  CALL_PARAM_OUTPUT,
  CALL_PARAM_RETURN,
};

typedef uint8_t call_value_kind_t;
enum {
  CALL_VALUE_LEAF = 0,
  CALL_VALUE_STRUCT,
  CALL_VALUE_ARRAY,
  CALL_VALUE_FIXED_ARRAY,
};

typedef struct call_object_ref_s call_object_ref_t;
struct call_object_ref_s {
  uint32_t   slot;
  uobject_t *expected;
  bool       is_none;
};

typedef struct call_value_s call_value_t;
struct call_value_s {
  call_value_t *next;
  call_value_t *first_child;
  call_value_t *last_child;

  fprop_t          *prop;
  str_t             name;
  str_t             type;
  call_param_role_t role;
  call_value_kind_t kind;

  int32_t array_index;
  int32_t array_count;
  int32_t child_count;

  bool supported;
  bool expanded;
  bool auto_world;
  bool class_picker;
  bool soft_picker;

  call_object_ref_t object_ref;
  char              input[CALL_TEXT_CAP];
};

typedef struct call_result_s call_result_t;
struct call_result_s {
  call_result_t *next;
  call_result_t *first_child;
  call_result_t *last_child;

  str_t name;
  str_t type;
  char  value[CALL_TEXT_CAP];
  char  tooltip[CALL_TEXT_CAP];
  bool  expanded;
};

typedef struct call_picker_s call_picker_t;
struct call_picker_s {
  call_object_ref_t *ref;
  fprop_t           *soft_prop;
  char              *soft_input;
  uclass_t          *required_class;
  bool               class_values;
  bool               allow_none;

  uint64_t  generation;
  uint32_t  count;
  uint32_t *slots;
  char      search[CALL_SEARCH_CAP];
};

struct ufunc_call_dialog_s {
  arena_t *model_arena;
  arena_t *result_arena;

  bool           open;
  bool           bounds_inited;
  struct nk_rect bounds;

  struct nk_grid_state target_grid;
  struct nk_grid_state input_grid;
  struct nk_grid_state output_grid;

  uint32_t          func_slot;
  ufunc_t          *func_expected;
  uclass_t         *receiver_class;
  call_object_ref_t target;

  call_value_t  *first_param;
  call_value_t  *last_param;
  call_result_t *first_result;
  call_result_t *last_result;

  call_picker_t picker;

  char status[CALL_STATUS_CAP];
  bool status_is_error;
};

typedef TARRAY(void) call_tarray_view_t;

typedef struct call_fsoft_object_ptr_s call_fsoft_object_ptr_t;
struct call_fsoft_object_ptr_s {
  int32_t             object_idx;
  int32_t             serial_num;
  int32_t             tag_at_last_test;
  int32_t             pad0;
  fsoft_object_path_t path;
};

static bool
call_record_is_live_object(search_tool_t *tool, uint32_t slot, uobject_t *expected)
{
  record_t *record = record_from_slot(tool, slot);
  return record && expected && record_has_flag(record, RECORD_FLAG_LIVE) && record->obj == expected && unreal_uobject_is_valid(expected);
}

static void
call_status_set(ufunc_call_dialog_t *dialog, bool error, const char *fmt, ...)
{
  if (!dialog) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  str_write_vfmt(dialog->status, sizeof(dialog->status), fmt, args);
  va_end(args);

  dialog->status_is_error = error;
}

static void
call_object_ref_set_none(call_object_ref_t *ref)
{
  if (!ref) {
    return;
  }

  ref->slot     = RECORD_SLOT_INVALID;
  ref->expected = NULL;
  ref->is_none  = true;
}

static void
call_object_ref_set(search_tool_t *tool, call_object_ref_t *ref, uobject_t *obj)
{
  if (!ref) {
    return;
  }

  if (!obj || !unreal_uobject_is_valid(obj)) {
    call_object_ref_set_none(ref);
    return;
  }

  uint32_t  slot   = (obj->internal_idx >= 0) ? (uint32_t)obj->internal_idx : RECORD_SLOT_INVALID;
  record_t *record = record_from_slot(tool, slot);
  if (!record || !record_has_flag(record, RECORD_FLAG_LIVE) || record->obj != obj) {
    call_object_ref_set_none(ref);
    return;
  }

  ref->slot     = slot;
  ref->expected = obj;
  ref->is_none  = false;
}

static bool
call_name_is_world_context(fname_t name)
{
  return unreal_fname_match_text(name, STR_LIT("WorldContextObject"), true, true) ||
         unreal_fname_match_text(name, STR_LIT("WorldContext"),       true, true) ||
         unreal_fname_match_text(name, STR_LIT("__WorldContext"),     true, true);
}

static bool
call_prop_is_supported(fprop_t *prop)
{
  if (!prop || !prop->cls || prop->elem_size <= 0 || prop->array_dim <= 0) {
    return false;
  }

  fname_t supported[] = {
    globals.unreal.bool_prop,       globals.unreal.byte_prop,       globals.unreal.int8_prop,
    globals.unreal.int16_prop,      globals.unreal.int_prop,        globals.unreal.int32_prop,
    globals.unreal.int64_prop,      globals.unreal.uint16_prop,     globals.unreal.uint32_prop,
    globals.unreal.uint64_prop,     globals.unreal.float_prop,      globals.unreal.double_prop,
    globals.unreal.name_prop,       globals.unreal.str_prop,        globals.unreal.text_prop,
    globals.unreal.obj_prop,        globals.unreal.class_prop,      globals.unreal.soft_obj_prop,
    globals.unreal.soft_class_prop, globals.unreal.struct_prop,     globals.unreal.array_prop,
    globals.unreal.enum_prop,
  };

  for (int i = 0; i < COUNTOF(supported); ++i) {
    if (unreal_fprop_class_is(prop, supported[i])) {
      return true;
    }
  }
  return false;
}

static void
call_value_set_default(call_value_t *node)
{
  if (!node || !node->prop) {
    return;
  }

  fprop_t *prop = node->prop;
  if (unreal_fprop_class_is(prop, globals.unreal.bool_prop)) {
    str_write_fmt(node->input, sizeof(node->input), "false");
  } else if (unreal_fprop_class_is(prop, globals.unreal.float_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.double_prop)) {
    str_write_fmt(node->input, sizeof(node->input), "0.0");
  } else if (unreal_fprop_class_is(prop, globals.unreal.byte_prop)   ||
             unreal_fprop_class_is(prop, globals.unreal.int8_prop)   ||
             unreal_fprop_class_is(prop, globals.unreal.int16_prop)  ||
             unreal_fprop_class_is(prop, globals.unreal.int_prop)    ||
             unreal_fprop_class_is(prop, globals.unreal.int32_prop)  ||
             unreal_fprop_class_is(prop, globals.unreal.int64_prop)  ||
             unreal_fprop_class_is(prop, globals.unreal.uint16_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.uint32_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.uint64_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.enum_prop)) {
    str_write_fmt(node->input, sizeof(node->input), "0");
  } else {
    node->input[0] = '\0';
  }
}

static call_value_t *
call_value_build(ufunc_call_dialog_t *dialog, fprop_t *prop, str_t name, call_param_role_t role, bool allow_fixed_array);

static void
call_value_push_child(call_value_t *parent, call_value_t *child)
{
  if (!parent || !child) {
    return;
  }

  QUEUE_PUSH(parent->first_child, parent->last_child, child);
  parent->child_count += 1;
}

static call_value_t *
call_value_build_base(ufunc_call_dialog_t *dialog, fprop_t *prop, str_t name, call_param_role_t role)
{
  call_value_t *node = ARENA_PUSH_ZERO(dialog->model_arena, call_value_t);
  if (!node) {
    return NULL;
  }

  node->prop        = prop;
  node->name        = str_push_copy(dialog->model_arena, name);
  node->type        = unreal_fprop_push_type_name(prop, dialog->model_arena);
  node->role        = role;
  node->array_index = -1;
  node->supported   = call_prop_is_supported(prop);
  node->expanded    = true;
  node->object_ref  = (call_object_ref_t){
    .slot    = RECORD_SLOT_INVALID,
    .is_none = true,
  };

  if (unreal_fprop_class_is(prop, globals.unreal.class_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.soft_class_prop)) {
    node->class_picker = true;
  }

  if (unreal_fprop_class_is(prop, globals.unreal.soft_obj_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.soft_class_prop)) {
    node->soft_picker = true;
  }

  call_value_set_default(node);
  return node;
}

static call_value_t *
call_value_build(ufunc_call_dialog_t *dialog, fprop_t *prop, str_t name, call_param_role_t role, bool allow_fixed_array)
{
  call_value_t *node = call_value_build_base(dialog, prop, name, role);
  if (!node || !node->supported) {
    return node;
  }

  if (allow_fixed_array && prop->array_dim > 1) {
    node->kind = CALL_VALUE_FIXED_ARRAY;
    for (int32_t i = 0; i < prop->array_dim; ++i) {
      call_value_t *elem = call_value_build(dialog, prop, str_push_fmt(dialog->model_arena, "[%d]", i), role, false);
      if (!elem) {
        node->supported = false;
        break;
      }

      elem->array_index = i;
      call_value_push_child(node, elem);
      node->supported = node->supported && elem->supported;
    }
    return node;
  }

  if (unreal_fprop_class_is(prop, globals.unreal.struct_prop)) {
    node->kind = CALL_VALUE_STRUCT;
    fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
    if (!struct_prop->script_struct || !struct_prop->script_struct->child_props) {
      node->supported = false;
      return node;
    }

    for (ffield_t *field = struct_prop->script_struct->child_props; field; field = field->next) {
      fprop_t *child_prop = (fprop_t *)field;
      str_t    child_name = unreal_fname_to_str(child_prop->name, dialog->model_arena);

      call_value_t *child = call_value_build(dialog, child_prop, child_name, role, true);
      if (!child) {
        node->supported = false;
        break;
      }

      call_value_push_child(node, child);
      node->supported = node->supported && child->supported;
    }
  } else if (unreal_fprop_class_is(prop, globals.unreal.array_prop)) {
    fprop_array_t *array_prop = (fprop_array_t *)prop;

    node->kind        = CALL_VALUE_ARRAY;
    node->array_count = 0;
    str_write_fmt(node->input, sizeof(node->input), "0");

    if (!array_prop->inner || !call_prop_is_supported(array_prop->inner)) {
      node->supported = false;
    }
  } else {
    node->kind = CALL_VALUE_LEAF;
  }

  return node;
}

static bool
call_array_parse_count(call_value_t *node, int32_t *out_count)
{
  if (!node || node->kind != CALL_VALUE_ARRAY || !out_count || node->input[0] == '\0') {
    return false;
  }

  char *end   = NULL;
  long  value = strtol(node->input, &end, 10);

  if (end == node->input || *end != '\0' || value < 0 || value > CALL_MAX_CONTAINER_ELEMS) {
    return false;
  }

  *out_count = (int32_t)value;
  return true;
}

static bool
call_array_sync(ufunc_call_dialog_t *dialog, call_value_t *node)
{
  int32_t count = 0;
  if (!call_array_parse_count(node, &count)) {
    return false;
  }

  fprop_array_t *array_prop = (fprop_array_t *)node->prop;
  while (node->child_count < count) {
    int32_t idx = node->child_count;

    call_value_t *child = call_value_build(dialog, array_prop->inner, str_push_fmt(dialog->model_arena, "[%d]", idx), node->role, true);
    if (!child) {
      return false;
    }

    child->array_index = idx;
    call_value_push_child(node, child);
    node->supported = node->supported && child->supported;
  }

  node->array_count = count;
  return true;
}

static call_param_role_t
call_param_role(fprop_t *prop)
{
  if (prop->prop_flags & CPF_RETURN_PARM) {
    return CALL_PARAM_RETURN;
  }

  /* ReferenceParm describes reference passing, not a second pin direction.
   * UHT commonly emits OutParm | ReferenceParm for ordinary output arrays and
   * structs, so OutParm must classify the parameter as output-only. */
  if (prop->prop_flags & CPF_OUT_PARM) {
    return CALL_PARAM_OUTPUT;
  }

  return CALL_PARAM_INPUT;
}

static void
call_results_build_placeholders(ufunc_call_dialog_t *dialog);

static void
call_model_build(search_tool_t *tool, ufunc_call_dialog_t *dialog)
{
  arena_reset(dialog->model_arena);
  arena_reset(dialog->result_arena);

  dialog->first_param  = NULL;
  dialog->last_param   = NULL;
  dialog->first_result = NULL;
  dialog->last_result  = NULL;

  ufunc_t *func = dialog->func_expected;
  for (ffield_t *field = func ? func->child_props : NULL; field; field = field->next) {
    fprop_t *prop = (fprop_t *)field;
    if (!(prop->prop_flags & CPF_PARM)) {
      continue;
    }

    call_param_role_t role = call_param_role(prop);
    call_value_t     *node = call_value_build(dialog, prop, unreal_fname_to_str(prop->name, dialog->model_arena), role, true);
    if (!node) {
      call_status_set(dialog, true, "Failed to allocate the reflected parameter model");
      return;
    }

    if (role == CALL_PARAM_INPUT && call_name_is_world_context(prop->name) &&
        unreal_fprop_class_is(prop, globals.unreal.obj_prop)) {
      node->auto_world = true;
    }

    QUEUE_PUSH(dialog->first_param, dialog->last_param, node);
  }

  call_results_build_placeholders(dialog);

  call_status_set(dialog, false, "Ready. Blank values are zero/empty, not reflected C++ defaults.");
  UNUSED_VAR(tool);
}

static uclass_t *
call_node_required_class(call_value_t *node)
{
  if (!node || !node->prop) {
    return NULL;
  }

  if (unreal_fprop_class_is(node->prop, globals.unreal.class_prop)) {
    return ((fprop_class_t *)node->prop)->meta_class;
  }

  if (unreal_fprop_class_is(node->prop, globals.unreal.soft_class_prop)) {
    return ((fprop_class_soft_t *)node->prop)->meta_class;
  }

  if (unreal_fprop_class_is(node->prop, globals.unreal.obj_prop) ||
      unreal_fprop_class_is(node->prop, globals.unreal.soft_obj_prop)) {
    return ((fprop_obj_base_t *)node->prop)->prop_class;
  }

  return NULL;
}

static bool
call_candidate_matches(uobject_t *obj, uclass_t *required_class, bool class_values)
{
  if (!obj || !required_class) {
    return false;
  }

  if (class_values) {
    if (!unreal_uobject_is_a(obj, globals.unreal.core_class)) {
      return false;
    }
    return unreal_uclass_is_child_of((uclass_t *)obj, required_class);
  }

  return unreal_uobject_is_a(obj, required_class);
}

static void
call_picker_rebuild(search_tool_t *tool, ufunc_call_dialog_t *dialog)
{
  call_picker_t *picker = &dialog->picker;

  picker->count = 0;
  if (!picker->required_class || !picker->slots) {
    picker->generation = tool->cache.generation;
    return;
  }

  for (uint32_t i = 0; i < tool->cache.live_slots.count; ++i) {
    uint32_t  slot   = tool->cache.live_slots.slots[i];
    record_t *record = record_from_slot(tool, slot);
    if (!record || !record_has_flag(record, RECORD_FLAG_LIVE) || !record->obj) {
      continue;
    }

    if (call_candidate_matches(record->obj, picker->required_class, picker->class_values)) {
      picker->slots[picker->count++] = slot;
    }
  }
  picker->generation = tool->cache.generation;
}

static bool
call_append_escaped_quoted(arena_t *arena, str_t raw, str_t *out)
{
  uint64_t cap = raw.len * 2 + 3;
  uint8_t *buf = ARENA_PUSH_ARRAY(arena, uint8_t, cap);
  if (!buf) {
    return false;
  }

  uint64_t at = 0;
  buf[at++] = '"';
  for (uint64_t i = 0; i < raw.len; ++i) {
    uint8_t c = raw.data[i];
    if (c == '\\' || c == '"') {
      buf[at++] = '\\';
      buf[at++] = c;
    } else if (c == '\n') {
      buf[at++] = '\\';
      buf[at++] = 'n';
    } else if (c == '\r') {
      buf[at++] = '\\';
      buf[at++] = 'r';
    } else if (c == '\t') {
      buf[at++] = '\\';
      buf[at++] = 't';
    } else {
      buf[at++] = c;
    }
  }
  buf[at++] = '"';
  buf[at]   = '\0';

  *out = str_make(buf, at);
  return true;
}

static str_t
call_object_literal(search_tool_t *tool, call_object_ref_t ref, arena_t *arena)
{
  if (ref.is_none || !ref.expected) {
    return STR_LIT("None");
  }

  if (!call_record_is_live_object(tool, ref.slot, ref.expected)) {
    return STR_NULL;
  }

  str_t cls_name = unreal_uobject_push_name((uobject_t *)ref.expected->cls, arena);
  str_t path     = unreal_uobject_push_full_name(ref.expected, arena);
  return str_push_fmt(arena, "%.*s'%.*s'", STR_ARG(cls_name), STR_ARG(path));
}

static str_t
call_value_literal(search_tool_t *tool, ufunc_call_dialog_t *dialog, call_value_t *node, arena_t *arena)
{
  if (!node || !node->supported) {
    return STR_NULL;
  }

  if (node->kind == CALL_VALUE_STRUCT) {
    str_list_t parts = {0};
    for (call_value_t *child = node->first_child; child; child = child->next) {
      str_t value = call_value_literal(tool, dialog, child, arena);
      if (str_is_empty(value)) {
        return STR_NULL;
      }

      str_list_push(arena, &parts, str_push_fmt(arena, "%.*s=%.*s", STR_ARG(child->name), STR_ARG(value)));
    }

    return str_list_join(arena, parts, STR_LIT("("), STR_LIT(","), STR_LIT(")"));
  }

  if (node->kind == CALL_VALUE_ARRAY || node->kind == CALL_VALUE_FIXED_ARRAY) {
    if (node->kind == CALL_VALUE_ARRAY && !call_array_sync(dialog, node)) {
      return STR_NULL;
    }

    int32_t    count = node->kind == CALL_VALUE_ARRAY ? node->array_count : node->child_count;
    str_list_t parts = {0};
    int32_t    idx   = 0;
    for (call_value_t *child = node->first_child; child && idx < count; child = child->next, ++idx) {
      str_t value = call_value_literal(tool, dialog, child, arena);
      if (str_is_empty(value)) {
        return STR_NULL;
      }

      str_list_push(arena, &parts, value);
    }

    return str_list_join(arena, parts, STR_LIT("("), STR_LIT(","), STR_LIT(")"));
  }

  fprop_t *prop = node->prop;
  if (unreal_fprop_class_is(prop, globals.unreal.obj_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.class_prop)) {
    if (node->auto_world) {
      uworld_t         *world     = globals.gworld_ptr ? *globals.gworld_ptr : NULL;
      call_object_ref_t world_ref = {
        .slot     = RECORD_SLOT_INVALID,
        .expected = (uobject_t *)world,
        .is_none  = (world == NULL),
      };

      if (world) {
        world_ref.slot = (uint32_t)((uobject_t *)world)->internal_idx;
      }
      return call_object_literal(tool, world_ref, arena);
    }

    return call_object_literal(tool, node->object_ref, arena);
  }

  str_t raw = str_from_cstr_with_cap(node->input, sizeof(node->input));
  if (unreal_fprop_class_is(prop, globals.unreal.str_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.text_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.name_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.soft_obj_prop) ||
      unreal_fprop_class_is(prop, globals.unreal.soft_class_prop)) {
    str_t quoted = STR_NULL;
    if (!call_append_escaped_quoted(arena, raw, &quoted)) {
      return STR_NULL;
    }
    return quoted;
  }

  return str_push_copy(arena, raw);
}

static bool
call_import_literal(fprop_t *prop, void *value, uobject_t *owner, str_t literal, arena_t *arena)
{
  if (!prop || !value || str_is_empty(literal)) {
    return false;
  }

  str16_t wide = str16_from_str(arena, literal);
  if (!wide.data) {
    return false;
  }

  const wchar_t *end = unreal_fprop_import_text_direct(prop, (const wchar_t *)wide.data, value, owner);
  if (!end) {
    return false;
  }

  while (*end == L' ' || *end == L'\t' || *end == L'\r' || *end == L'\n') {
    end += 1;
  }

  return *end == L'\0';
}

static bool
call_apply_value(search_tool_t *tool, ufunc_call_dialog_t *dialog, call_value_t *node, uint8_t *value, uobject_t *owner, arena_t *arena)
{
  if (!node || !node->supported || !value) {
    return false;
  }

  if (node->kind == CALL_VALUE_FIXED_ARRAY) {
    int32_t idx = 0;
    for (call_value_t *child = node->first_child; child; child = child->next, ++idx) {
      if (!call_apply_value(tool, dialog, child, value + (uint64_t)idx * (uint64_t)node->prop->elem_size, owner, arena)) {
        return false;
      }
    }
    return true;
  }

  if (node->kind == CALL_VALUE_STRUCT) {
    for (call_value_t *child = node->first_child; child; child = child->next) {
      if (!call_apply_value(tool, dialog, child, value + child->prop->offset_internal, owner, arena)) {
        return false;
      }
    }
    return true;
  }

  if (node->kind == CALL_VALUE_ARRAY) {
    if (!call_array_sync(dialog, node)) {
      return false;
    }
  }

  if (node->kind == CALL_VALUE_LEAF &&
      (unreal_fprop_class_is(node->prop, globals.unreal.obj_prop) ||
       unreal_fprop_class_is(node->prop, globals.unreal.class_prop))) {
    uobject_t *selected = NULL;
    if (node->auto_world) {
      selected = globals.gworld_ptr ? (uobject_t *)*globals.gworld_ptr : NULL;
    } else if (!node->object_ref.is_none) {
      if (!call_record_is_live_object(tool, node->object_ref.slot, node->object_ref.expected)) {
        return false;
      }
      selected = node->object_ref.expected;
    }

    if (selected && !call_candidate_matches(selected, call_node_required_class(node), node->class_picker)) {
      return false;
    }

    *(uobject_t **)value = selected;
    return true;
  }

  str_t literal = call_value_literal(tool, dialog, node, arena);
  return call_import_literal(node->prop, value, owner, literal, arena);
}

static int64_t
call_read_integer(fprop_t *prop, void *value)
{
  if (unreal_fprop_class_is(prop, globals.unreal.byte_prop))   return *(uint8_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.int8_prop))   return *(int8_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.int16_prop))  return *(int16_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.int_prop))    return *(int32_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.int32_prop))  return *(int32_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.int64_prop))  return *(int64_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.uint16_prop)) return *(uint16_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.uint32_prop)) return *(uint32_t *)value;
  if (unreal_fprop_class_is(prop, globals.unreal.uint64_prop)) return (int64_t)*(uint64_t *)value;

  return 0;
}

static void
call_result_format_leaf(call_result_t *result, fprop_t *prop, void *value, arena_t *arena)
{
  if (unreal_fprop_class_is(prop, globals.unreal.bool_prop)) {
    fprop_bool_t *bp   = (fprop_bool_t *)prop;
    uint8_t       byte = *((uint8_t *)value + bp->byte_offset);
    str_write_fmt(result->value, sizeof(result->value), "%s", (byte & bp->field_mask) ? "true" : "false");
  } else if (unreal_fprop_class_is(prop, globals.unreal.float_prop)) {
    str_write_fmt(result->value, sizeof(result->value), "%.9g", (double)*(float *)value);
  } else if (unreal_fprop_class_is(prop, globals.unreal.double_prop)) {
    str_write_fmt(result->value, sizeof(result->value), "%.17g", *(double *)value);
  } else if (unreal_fprop_class_is(prop, globals.unreal.uint64_prop)) {
    str_write_fmt(result->value, sizeof(result->value), "%llu", (unsigned long long)*(uint64_t *)value);
  } else if (unreal_fprop_class_is(prop, globals.unreal.byte_prop)   ||
             unreal_fprop_class_is(prop, globals.unreal.int8_prop)   ||
             unreal_fprop_class_is(prop, globals.unreal.int16_prop)  ||
             unreal_fprop_class_is(prop, globals.unreal.int_prop)    ||
             unreal_fprop_class_is(prop, globals.unreal.int32_prop)  ||
             unreal_fprop_class_is(prop, globals.unreal.int64_prop)  ||
             unreal_fprop_class_is(prop, globals.unreal.uint16_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.uint32_prop)) {
    str_write_fmt(result->value, sizeof(result->value), "%lld", (long long)call_read_integer(prop, value));
  } else if (unreal_fprop_class_is(prop, globals.unreal.enum_prop)) {
    fprop_enum_t *enum_prop = (fprop_enum_t *)prop;
    int64_t       integer   = enum_prop->underlying_prop ? call_read_integer(&enum_prop->underlying_prop->base, value) : 0;
    str_t         name      = STR_NULL;
    if (enum_prop->uenum) {
      for (int32_t i = 0; i < enum_prop->uenum->names.num; ++i) {
        if (enum_prop->uenum->names.data[i].value == integer) {
          name = unreal_fname_to_str(enum_prop->uenum->names.data[i].key, arena);
          break;
        }
      }
    }

    if (!str_is_empty(name)) {
      str_write_fmt(result->value, sizeof(result->value), "%.*s", STR_ARG(name));
    } else {
      str_write_fmt(result->value, sizeof(result->value), "%lld", (long long)integer);
    }
  } else if (unreal_fprop_class_is(prop, globals.unreal.name_prop)) {
    str_t text = unreal_fname_to_str(*(fname_t *)value, arena);
    str_write_fmt(result->value, sizeof(result->value), "%.*s", STR_ARG(text));
  } else if (unreal_fprop_class_is(prop, globals.unreal.str_prop)) {
    str_t text = unreal_fstring_to_str(*(fstring_t *)value, arena);
    str_write_fmt(result->value, sizeof(result->value), "%.*s", STR_ARG(text));
  } else if (unreal_fprop_class_is(prop, globals.unreal.text_prop)) {
    ftext_t   *text         = (ftext_t *)value;
    fstring_t *display      = (text->text_data.obj && text->text_data.obj->vtable) ? text->text_data.obj->vtable->get_display_string(text->text_data.obj) : NULL;
    str_t      display_text = display ? unreal_fstring_to_str(*display, arena) : STR_NULL;
    str_write_fmt(result->value, sizeof(result->value), "%.*s", STR_ARG(display_text));
  } else if (unreal_fprop_class_is(prop, globals.unreal.obj_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.class_prop)) {
    uobject_t *obj = *(uobject_t **)value;
    if (!obj) {
      str_write_fmt(result->value, sizeof(result->value), "None");
    } else if (!unreal_uobject_is_valid(obj)) {
      str_write_fmt(result->value, sizeof(result->value), "<invalid>");
    } else {
      str_t name      = unreal_uobject_push_name(obj, arena);
      str_t full_name = unreal_uobject_push_full_name(obj, arena);
      str_write_fmt(result->value, sizeof(result->value), "%.*s", STR_ARG(name));
      str_write_fmt(result->tooltip, sizeof(result->tooltip), "%.*s", STR_ARG(full_name));
    }
  } else if (unreal_fprop_class_is(prop, globals.unreal.soft_obj_prop) ||
             unreal_fprop_class_is(prop, globals.unreal.soft_class_prop)) {
    call_fsoft_object_ptr_t *soft  = (call_fsoft_object_ptr_t *)value;
    str_t                    asset = unreal_fname_to_str(soft->path.asset_path_name, arena);
    str_t                    sub   = unreal_fstring_to_str(soft->path.sub_path_string, arena);
    if (!str_is_empty(sub)) {
      str_write_fmt(result->value, sizeof(result->value), "%.*s:%.*s", STR_ARG(asset), STR_ARG(sub));
    } else {
      str_write_fmt(result->value, sizeof(result->value), "%.*s", STR_ARG(asset));
    }
  } else {
    str_write_fmt(result->value, sizeof(result->value), "<unavailable>");
  }
}

static call_result_t *
call_result_snapshot(ufunc_call_dialog_t *dialog, fprop_t *prop, str_t name, uint8_t *value, bool allow_fixed_array)
{
  call_result_t *result = ARENA_PUSH_ZERO(dialog->result_arena, call_result_t);
  if (!result) {
    return NULL;
  }

  result->name     = str_push_copy(dialog->result_arena, name);
  result->type     = unreal_fprop_push_type_name(prop, dialog->result_arena);
  result->expanded = true;

  if (allow_fixed_array && prop->array_dim > 1) {
    str_write_fmt(result->value, sizeof(result->value), "Num=%d", prop->array_dim);
    for (int32_t i = 0; i < prop->array_dim; ++i) {
      call_result_t *child = call_result_snapshot(dialog, prop, str_push_fmt(dialog->result_arena, "[%d]", i), value + (uint64_t)i * (uint64_t)prop->elem_size, false);
      if (child) {
        QUEUE_PUSH(result->first_child, result->last_child, child);
      }
    }
    return result;
  }

  if (unreal_fprop_class_is(prop, globals.unreal.struct_prop)) {
    fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
    str_write_fmt(result->value, sizeof(result->value), "{...}");
    if (struct_prop->script_struct) {
      for (ffield_t *field = struct_prop->script_struct->child_props; field; field = field->next) {
        fprop_t       *child_prop = (fprop_t *)field;
        call_result_t *child      = call_result_snapshot(dialog, child_prop, unreal_fname_to_str(child_prop->name, dialog->result_arena), value + child_prop->offset_internal, true);
        if (child) {
          QUEUE_PUSH(result->first_child, result->last_child, child);
        }
      }
    }
  } else if (unreal_fprop_class_is(prop, globals.unreal.array_prop)) {
    fprop_array_t      *array_prop = (fprop_array_t *)prop;
    call_tarray_view_t *array      = (call_tarray_view_t *)value;
    int32_t             count      = (array->num >= 0 && array->num <= array->max && array->data) ? array->num : 0;

    str_write_fmt(result->value, sizeof(result->value), "Num=%d", count);

    int32_t shown = MIN_VAL(count, CALL_MAX_CONTAINER_ELEMS);
    for (int32_t i = 0; i < shown; ++i) {
      call_result_t *child = call_result_snapshot(dialog, array_prop->inner, str_push_fmt(dialog->result_arena, "[%d]", i), (uint8_t *)array->data + (uint64_t)i * (uint64_t)array_prop->inner->elem_size, true);
      if (child) {
        QUEUE_PUSH(result->first_child, result->last_child, child);
      }
    }
  } else {
    call_result_format_leaf(result, prop, value, dialog->result_arena);
  }
  return result;
}

static call_result_t *
call_result_placeholder(ufunc_call_dialog_t *dialog, fprop_t *prop, str_t name, bool allow_fixed_array)
{
  call_result_t *result = ARENA_PUSH_ZERO(dialog->result_arena, call_result_t);
  if (!result) {
    return NULL;
  }

  result->name     = str_push_copy(dialog->result_arena, name);
  result->type     = unreal_fprop_push_type_name(prop, dialog->result_arena);
  result->expanded = true;

  if (allow_fixed_array && prop->array_dim > 1) {
    for (int32_t i = 0; i < prop->array_dim; ++i) {
      call_result_t *child = call_result_placeholder(dialog, prop, str_push_fmt(dialog->result_arena, "[%d]", i), false);
      if (child) {
        QUEUE_PUSH(result->first_child, result->last_child, child);
      }
    }
  } else if (unreal_fprop_class_is(prop, globals.unreal.struct_prop)) {
    fprop_struct_t *struct_prop = (fprop_struct_t *)prop;
    if (struct_prop->script_struct) {
      for (ffield_t *field = struct_prop->script_struct->child_props; field; field = field->next) {
        fprop_t       *child_prop = (fprop_t *)field;
        call_result_t *child      = call_result_placeholder(dialog, child_prop, unreal_fname_to_str(child_prop->name, dialog->result_arena), true);
        if (child) {
          QUEUE_PUSH(result->first_child, result->last_child, child);
        }
      }
    }
  }
  return result;
}

static void
call_results_build_placeholders(ufunc_call_dialog_t *dialog)
{
  for (call_value_t *node = dialog->first_param; node; node = node->next) {
    if (node->role != CALL_PARAM_OUTPUT && node->role != CALL_PARAM_RETURN) {
      continue;
    }

    call_result_t *result = call_result_placeholder(dialog, node->prop, node->name, true);
    if (result) {
      QUEUE_PUSH(dialog->first_result, dialog->last_result, result);
    }
  }
}

static bool
call_validate_schema(ufunc_call_dialog_t *dialog, ufunc_t *func)
{
  uint32_t reflected_count = 0;
  for (call_value_t *node = dialog->first_param; node; node = node->next) {
    fprop_t *prop = node->prop;

    reflected_count += 1;
    if (!node->supported) {
      call_status_set(dialog, true, "Unsupported parameter type: %.*s %.*s", STR_ARG(node->type), STR_ARG(node->name));
      return false;
    }

    if (prop->offset_internal < 0 || prop->elem_size <= 0 || prop->array_dim <= 0) {
      call_status_set(dialog, true, "Invalid reflected layout for %.*s", STR_ARG(node->name));
      return false;
    }

    uint64_t end = (uint64_t)prop->offset_internal + (uint64_t)prop->elem_size * (uint64_t)prop->array_dim;
    if (end > func->params_size) {
      call_status_set(dialog, true, "Parameter %.*s extends past ParmsSize", STR_ARG(node->name));
      return false;
    }

    if (node->kind == CALL_VALUE_ARRAY && !call_array_sync(dialog, node)) {
      call_status_set(dialog, true, "Array %.*s Num must be between 0 and %d", STR_ARG(node->name), CALL_MAX_CONTAINER_ELEMS);
      return false;
    }
  }

  if (reflected_count != func->num_params) {
    call_status_set(dialog, true, "Reflected parameter count mismatch (%u != %u)", reflected_count, (uint32_t)func->num_params);
    return false;
  }

  return true;
}

static bool
call_validate_target(search_tool_t *tool, ufunc_call_dialog_t *dialog, uobject_t **out_target)
{
  if (!dialog->receiver_class || !unreal_uobject_is_valid((uobject_t *)dialog->receiver_class)) {
    call_status_set(dialog, true, "The function's owning class is no longer valid");
    return false;
  }

  if (dialog->target.is_none || !call_record_is_live_object(tool, dialog->target.slot, dialog->target.expected)) {
    call_status_set(dialog, true, "Select a live target object");
    return false;
  }

  if (!unreal_uobject_is_a(dialog->target.expected, dialog->receiver_class)) {
    call_status_set(dialog, true, "The selected target is not an instance of the function's owning class");
    return false;
  }

  *out_target = dialog->target.expected;
  return true;
}

static void
call_execute(search_tool_t *tool, ufunc_call_dialog_t *dialog)
{
#if defined BUILD_TEST_UI
  (void)tool;
  call_status_set(dialog, true, "Unavailable");
  return;
#else

  if (!call_record_is_live_object(tool, dialog->func_slot, (uobject_t *)dialog->func_expected)) {
    call_status_set(dialog, true, "The function was unloaded or replaced");
    return;
  }

  ufunc_t   *func   = dialog->func_expected;
  uobject_t *target = NULL;
  if (!call_validate_target(tool, dialog, &target) || !call_validate_schema(dialog, func)) {
    return;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    bool     initialized = false;
    bool     imported    = true;
    uint64_t params_size = MAX_VAL((uint64_t)func->params_size, 1ULL);
    uint8_t *params      = arena_push_zero_aligned(tmp.arena, params_size, 16);
    if (!params) {
      call_status_set(dialog, true, "Failed to allocate 0x%X bytes for parameters", func->params_size);
    } else {
      for (call_value_t *node = dialog->first_param; node; node = node->next) {
        unreal_fprop_initialize_in_container(node->prop, params);
      }
      initialized = true;

      for (call_value_t *node = dialog->first_param; node; node = node->next) {
        if (node->role == CALL_PARAM_OUTPUT || node->role == CALL_PARAM_RETURN) {
          continue;
        }

        uint8_t *value = params + node->prop->offset_internal;
        if (!call_apply_value(tool, dialog, node, value, target, tmp.arena)) {
          call_status_set(dialog, true, "Could not parse %.*s (%.*s)", STR_ARG(node->name), STR_ARG(node->type));
          imported = false;
          break;
        }
      }

      if (imported) {
        unreal_process_event_observed(target, func, func->params_size ? params : NULL);

        arena_reset(dialog->result_arena);
        dialog->first_result = NULL;
        dialog->last_result  = NULL;
        for (call_value_t *node = dialog->first_param; node; node = node->next) {
          if (node->role != CALL_PARAM_OUTPUT && node->role != CALL_PARAM_RETURN) {
            continue;
          }

          call_result_t *result = call_result_snapshot(dialog, node->prop, node->name, params + node->prop->offset_internal, true);
          if (result) {
            QUEUE_PUSH(dialog->first_result, dialog->last_result, result);
          }
        }
        call_status_set(dialog, false, "Call completed");
      }
    }

    if (initialized && params) {
      for (call_value_t *node = dialog->first_param; node; node = node->next) {
        unreal_fprop_destroy_in_container(node->prop, params);
      }
    }
  }
  scratch_end(tmp);
#endif
}

bool
ufunc_call_init(search_tool_t *tool, const mod_host_api_t *host, mod_handle_t h)
{
  if (!tool || !host) {
    return false;
  }

  ufunc_call_dialog_t *dialog = ARENA_PUSH_ZERO(tool->perm, ufunc_call_dialog_t);
  if (!dialog) {
    return false;
  }

  dialog->model_arena  = mod_arena_handle_resolve(host->arena_create(h, 32 * MB, 64 * KB));
  dialog->result_arena = mod_arena_handle_resolve(host->arena_create(h, 32 * MB, 64 * KB));
  dialog->picker.slots = ARENA_PUSH_ARRAY(tool->perm, uint32_t, tool->cache.record_cap);
  dialog->func_slot    = RECORD_SLOT_INVALID;
  call_object_ref_set_none(&dialog->target);

  if (!dialog->model_arena || !dialog->result_arena || (tool->cache.record_cap > 0 && !dialog->picker.slots)) {
    return false;
  }

  tool->call_dialog = dialog;
  return true;
}

void
ufunc_call_open(search_tool_t *tool, record_t *func_record, record_t *suggested_target)
{
  if (!tool || !tool->call_dialog || !func_record || func_record->kind != UOBJECT_KIND_FUNC ||
      !record_has_flag(func_record, RECORD_FLAG_LIVE)) {
    return;
  }

  ufunc_call_dialog_t *dialog = tool->call_dialog;
  ufunc_t             *func   = (ufunc_t *)func_record->obj;
  uobject_t           *outer  = func ? func->outer : NULL;
  if (!func || !outer || !unreal_uobject_is_a(outer, globals.unreal.core_class)) {
    call_status_set(dialog, true, "UFunction has no valid owning UClass");
    return;
  }

  dialog->open              = true;
  dialog->func_slot         = record_to_slot(tool, func_record);
  dialog->func_expected     = func;
  dialog->receiver_class    = (uclass_t *)outer;
  dialog->picker.ref        = NULL;
  dialog->picker.soft_input = NULL;
  dialog->picker.generation = UINT64_MAX;

  nk_grid_state_reset(&dialog->target_grid);
  nk_grid_state_reset(&dialog->input_grid);
  nk_grid_state_reset(&dialog->output_grid);

  call_object_ref_set_none(&dialog->target);
  if (suggested_target && record_has_flag(suggested_target, RECORD_FLAG_LIVE) &&
      unreal_uobject_is_a(suggested_target->obj, dialog->receiver_class)) {
    call_object_ref_set(tool, &dialog->target, suggested_target->obj);
  } else if ((func->func_flags & FUNC_FLAG_STATIC) && dialog->receiver_class->cdo) {
    call_object_ref_set(tool, &dialog->target, dialog->receiver_class->cdo);
  }

  call_model_build(tool, dialog);
}

void
ufunc_call_close(search_tool_t *tool)
{
  if (!tool || !tool->call_dialog) {
    return;
  }

  tool->call_dialog->open              = false;
  tool->call_dialog->picker.ref        = NULL;
  tool->call_dialog->picker.soft_input = NULL;
}

void
ufunc_call_on_uobject_removed(search_tool_t *tool, uobject_t *obj)
{
  if (!tool || !tool->call_dialog || !obj) {
    return;
  }

  ufunc_call_dialog_t *dialog = tool->call_dialog;
  if ((uobject_t *)dialog->func_expected == obj || (uobject_t *)dialog->receiver_class == obj) {
    ufunc_call_close(tool);
  }
}

static str_t
call_object_ref_label(search_tool_t *tool, call_object_ref_t *ref, arena_t *arena)
{
  if (!ref || ref->is_none) {
    return STR_LIT("None");
  }

  if (!call_record_is_live_object(tool, ref->slot, ref->expected)) {
    return STR_LIT("<destroyed>");
  }

  return unreal_uobject_push_name(ref->expected, arena);
}

static void
call_picker_activate(search_tool_t       *tool,
                     ufunc_call_dialog_t *dialog,
                     call_object_ref_t   *ref,
                     fprop_t             *soft_prop,
                     char                *soft_input,
                     uclass_t            *required_class,
                     bool                 class_values,
                     bool                 allow_none)
{
  call_picker_t *picker = &dialog->picker;

  if (picker->ref            != ref            ||
      picker->soft_input     != soft_input     ||
      picker->required_class != required_class ||
      picker->class_values   != class_values   ||
      picker->allow_none     != allow_none) {
    picker->ref            = ref;
    picker->soft_prop      = soft_prop;
    picker->soft_input     = soft_input;
    picker->required_class = required_class;
    picker->class_values   = class_values;
    picker->allow_none     = allow_none;
    picker->search[0]      = '\0';
    picker->generation     = UINT64_MAX;
  }

  if (picker->generation != tool->cache.generation) {
    call_picker_rebuild(tool, dialog);
  }
}

static void
call_draw_popup_tooltip(struct nk_context *ctx, struct nk_rect owner_bounds, struct nk_rect anchor, str_t text)
{
  if (!ctx || str_is_empty(text)) {
    return;
  }

  struct {
    int   offset;
    int   len;
    float width;
  } lines[8] = {0};

  struct nk_command_buffer *out            = nk_window_get_canvas(ctx);
  struct nk_rect            old_clip       = out->clip;
  float                     padding        = 6.0f;
  float                     max_text_width = NK_MAX(40.0f, owner_bounds.w - 8.0f - 2.0f * padding);
  float                     text_width     = 0.0f;
  int                       line_count     = 0;
  int                       offset         = 0;

  while (offset < (int)text.len && line_count < COUNTOF(lines)) {
    int   end        = offset + 1;
    int   last_break = -1;
    float line_width = 0.0f;

    for (; end <= (int)text.len; ++end) {
      uint8_t c = text.data[end - 1];
      if (c == '/' || c == '.' || c == ':' || c == ' ') {
        last_break = end;
      }

      line_width = nk_text_width(ctx, ctx->style.font, ctx->style.font_size, (const char *)text.data + offset, end - offset);
      if (line_width > max_text_width) {
        end = (last_break > offset) ? last_break : NK_MAX(offset + 1, end - 1);
        line_width = nk_text_width(ctx, ctx->style.font, ctx->style.font_size, (const char *)text.data + offset, end - offset);
        break;
      }
    }

    end = NK_MIN(end, (int)text.len);
    lines[line_count].offset = offset;
    lines[line_count].len    = end - offset;
    lines[line_count].width  = line_width;
    text_width               = NK_MAX(text_width, line_width);
    line_count              += 1;
    offset                   = end;
  }

  float line_height = ctx->style.font_size + 2.0f;
  float width       = NK_MAX(80.0f, text_width + 2.0f * padding);
  float height      = (float)line_count * line_height + 2.0f * padding;
  float x           = NK_CLAMP(owner_bounds.x + 4.0f, anchor.x, owner_bounds.x + owner_bounds.w - width - 4.0f);
  float y           = anchor.y + anchor.h + 2.0f;

  if (y + height > owner_bounds.y + owner_bounds.h - 4.0f) {
    y = anchor.y - height - 2.0f;
  }
  y = NK_CLAMP(owner_bounds.y + 4.0f, y, owner_bounds.y + owner_bounds.h - height - 4.0f);

  struct nk_rect bounds = nk_rect(x, y, width, height);

  nk_push_scissor(out, owner_bounds);
  nk_fill_rect(out, bounds, ctx->style.window.rounding, UI_C_BG_PANEL);
  nk_stroke_rect(out, bounds, ctx->style.window.rounding, 1.0f, UI_C_BORDER_FOCUS);
  for (int i = 0; i < line_count; ++i) {
    struct nk_rect label = nk_rect(x + padding, y + padding + (float)i * line_height, width - 2.0f * padding, ctx->style.font_size);
    nk_draw_text(out, label, (const char *)text.data + lines[i].offset, lines[i].len, ctx->style.font, UI_C_BG_PANEL, UI_C_TEXT);
  }
  nk_push_scissor(out, old_clip);
}

static bool
call_picker_draw_contents(search_tool_t *tool, ufunc_call_dialog_t *dialog)
{
  struct nk_context *ctx    = tool->ctx;
  call_picker_t     *picker = &dialog->picker;

  bool           selected                         = false;
  char           hovered_full_name[CALL_TEXT_CAP] = {0};
  struct nk_rect hovered_bounds                   = {0};

  nk_layout_row_dynamic(ctx, 24.0f, 1);
  nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, picker->search, sizeof(picker->search), nk_filter_default);

  if (picker->allow_none) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    if (nk_button_label(ctx, "None")) {
      if (picker->soft_input) {
        picker->soft_input[0] = '\0';
      } else {
        call_object_ref_set_none(picker->ref);
      }
      selected = true;
    }
  }

  str_t query = str_from_cstr_with_cap(picker->search, sizeof(picker->search));
  int   shown = 0;

  for (uint32_t i = 0; i < picker->count && shown < 256; ++i) {
    uint32_t  slot   = picker->slots[i];
    record_t *record = record_from_slot(tool, slot);
    if (!record || !record_has_flag(record, RECORD_FLAG_LIVE) || !record->obj) {
      continue;
    }

    tmp_arena_t tmp = scratch_begin(NULL);
    {
      str_t full_name = unreal_uobject_push_full_name(record->obj, tmp.arena);
      str_t name      = unreal_uobject_push_name(record->obj, tmp.arena);
      bool  matches   = str_is_empty(query) || str_find(full_name, query, STR_CMP_FLAG_IGNORE_CASE, NULL);
      if (matches) {
        struct nk_color object_color = uobject_kind_color(record->kind);

        nk_layout_row_dynamic(ctx, 22.0f, 1);
        struct nk_rect item_bounds = nk_widget_bounds(ctx);
        bool           hovered     = nk_widget_is_hovered(ctx);

        nk_style_push_color(ctx, &ctx->style.button.text_normal, object_color);
        nk_style_push_color(ctx, &ctx->style.button.text_hover, object_color);
        nk_style_push_color(ctx, &ctx->style.button.text_active, object_color);

        bool pressed = nk_button_text(ctx, (const char *)name.data, (int)name.len);

        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);
        nk_style_pop_color(ctx);

        if (hovered) {
          str_write_fmt(hovered_full_name, sizeof(hovered_full_name), "%.*s", STR_ARG(full_name));
          hovered_bounds = item_bounds;
        }

        if (pressed) {
          if (picker->soft_input) {
            str_write_fmt(picker->soft_input, CALL_TEXT_CAP, "%.*s", STR_ARG(full_name));
          } else {
            call_object_ref_set(tool, picker->ref, record->obj);
          }
          selected = true;
        }
        shown += 1;
      }
    }
    scratch_end(tmp);
  }

  if (shown >= 256) {
    nk_layout_row_dynamic(ctx, 20.0f, 1);
    nk_label_colored(ctx, "More matches exist; narrow the search.", NK_TEXT_CENTERED, UI_C_TEXT_MUTED);
  } else if (shown == 0 && !(picker->allow_none && str_is_empty(query))) {
    nk_layout_row_dynamic(ctx, 20.0f, 1);
    nk_label_colored(ctx, "No loaded matches", NK_TEXT_CENTERED, UI_C_TEXT_MUTED);
  }

  if (hovered_full_name[0] != '\0') {
    call_draw_popup_tooltip(ctx, dialog->bounds, hovered_bounds, str_from_cstr_with_cap(hovered_full_name, sizeof(hovered_full_name)));
  }
  return selected;
}

static void
call_draw_object_combo(search_tool_t       *tool,
                       ufunc_call_dialog_t *dialog,
                       call_object_ref_t   *ref,
                       uclass_t            *required_class,
                       bool                 class_values,
                       bool                 allow_none)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t label = call_object_ref_label(tool, ref, tmp.arena);
    float width = NK_MAX(260.0f, nk_widget_width(tool->ctx));
    struct nk_color label_color = ref && !ref->is_none && call_record_is_live_object(tool, ref->slot, ref->expected)
                                  ? uobject_kind_color(uobject_kind(ref->expected))
                                  : UI_C_TEXT_DIM;

    nk_style_push_color(tool->ctx, &tool->ctx->style.combo.label_normal, label_color);
    nk_style_push_color(tool->ctx, &tool->ctx->style.combo.label_hover, label_color);
    nk_style_push_color(tool->ctx, &tool->ctx->style.combo.label_active, label_color);

    bool open = nk_combo_begin_text(tool->ctx, (const char *)label.data, (int)label.len, nk_vec2(width, 360.0f));

    nk_style_pop_color(tool->ctx);
    nk_style_pop_color(tool->ctx);
    nk_style_pop_color(tool->ctx);

    if (!open && nk_widget_is_hovered(tool->ctx) && ref && !ref->is_none &&
        call_record_is_live_object(tool, ref->slot, ref->expected)) {
      str_t full_name = unreal_uobject_push_full_name(ref->expected, tmp.arena);
      nk_tooltip_text(tool->ctx, (const char *)full_name.data, (int)full_name.len);
    }

    if (open) {
      call_picker_activate(tool, dialog, ref, NULL, NULL, required_class, class_values, allow_none);
      if (call_picker_draw_contents(tool, dialog)) {
        nk_combo_close(tool->ctx);
      }
      nk_combo_end(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static void
call_draw_soft_value(search_tool_t *tool, ufunc_call_dialog_t *dialog, call_value_t *node)
{
  struct nk_context *ctx = tool->ctx;

  char group_name[64];
  str_write_fmt(group_name, sizeof(group_name), "ufunction_call.soft.%p", (void *)node);

  if (nk_group_begin(ctx, group_name, NK_WINDOW_NO_SCROLLBAR)) {
    float picker_w = 30.0f;
    float gap      = ctx->style.window.spacing.x;
    float input_w  = NK_MAX(60.0f, nk_window_get_content_region(ctx).w - picker_w - gap);

    nk_layout_row_begin(ctx, NK_STATIC, 22.0f, 2);
    {
      nk_layout_row_push(ctx, input_w);
      nk_style_push_color(ctx, &ctx->style.edit.text_normal, CALL_C_LINK_TEXT);
      nk_style_push_color(ctx, &ctx->style.edit.text_hover,  CALL_C_LINK_TEXT);
      nk_style_push_color(ctx, &ctx->style.edit.text_active, CALL_C_LINK_TEXT);

      nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, node->input, sizeof(node->input), nk_filter_default);

      nk_style_pop_color(ctx);
      nk_style_pop_color(ctx);
      nk_style_pop_color(ctx);

      nk_layout_row_push(ctx, picker_w);
      if (nk_combo_begin_label(ctx, "...", nk_vec2(440.0f, 360.0f))) {
        call_picker_activate(tool, dialog, &node->object_ref, node->prop, node->input, call_node_required_class(node), node->class_picker, true);
        if (call_picker_draw_contents(tool, dialog)) {
          nk_combo_close(ctx);
        }
        nk_combo_end(ctx);
      }
    }
    nk_layout_row_end(ctx);
    nk_group_end(ctx);
  }
}

static void
call_grid_begin(struct nk_context *ctx, struct nk_grid *grid, struct nk_grid_state *state, float row_height)
{
  static const struct nk_grid_column columns[] = {
    {.sizing = NK_GRID_COLUMN_CONTENT, .min_width = 80.0f},
    {.sizing = NK_GRID_COLUMN_CONTENT, .min_width = 72.0f},
    {.sizing = NK_GRID_COLUMN_FLEX,    .min_width = 160.0f},
  };

  struct nk_grid_options options = {
    .row_height = row_height,
    .column_gap = ctx->style.window.spacing.x,
  };
  nk_grid_begin(ctx, grid, state, columns, COUNTOF(columns), &options);
}

static void
call_target_grid_begin(struct nk_context *ctx, struct nk_grid *grid, struct nk_grid_state *state, float row_height)
{
  static const struct nk_grid_column columns[] = {
    {.sizing = NK_GRID_COLUMN_CONTENT, .min_width = 80.0f},
    {.sizing = NK_GRID_COLUMN_FLEX,    .min_width = 160.0f},
  };

  struct nk_grid_options options = {
    .row_height = row_height,
    .column_gap = ctx->style.window.spacing.x,
  };

  nk_grid_begin(ctx, grid, state, columns, COUNTOF(columns), &options);
}

static void
call_grid_text(struct nk_grid *grid, int column, str_t text, struct nk_color color)
{
  struct nk_text_options options = {
    .alignment = NK_TEXT_LEFT,
    .flags     = NK_TEXT_OPTION_SELECTABLE,
    .color     = color,
  };
  ui_grid_str(grid, column, text, &options);
}

static void
call_edit_colored(struct nk_context *ctx, nk_flags flags, char *text, int cap, nk_plugin_filter filter, struct nk_color color)
{
  nk_style_push_color(ctx, &ctx->style.edit.text_normal, color);
  nk_style_push_color(ctx, &ctx->style.edit.text_hover,  color);
  nk_style_push_color(ctx, &ctx->style.edit.text_active, color);

  nk_edit_string_zero_terminated(ctx, flags, text, cap, filter);

  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
}

static bool
call_tree_button(struct nk_context *ctx, str_t title)
{
  nk_style_push_color(ctx, &ctx->style.button.text_normal, CALL_C_TYPE_TEXT);
  nk_style_push_color(ctx, &ctx->style.button.text_hover,  CALL_C_TYPE_TEXT);
  nk_style_push_color(ctx, &ctx->style.button.text_active, CALL_C_TYPE_TEXT);

  bool pressed = nk_button_text(ctx, (const char *)title.data, (int)title.len);

  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  nk_style_pop_color(ctx);
  return pressed;
}

static void
call_draw_value_row(search_tool_t       *tool,
                    ufunc_call_dialog_t *dialog,
                    struct nk_grid_state *grid_state,
                    call_value_t         *node,
                    int32_t               depth)
{
  struct nk_context *ctx = tool->ctx;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t indented_name = str_push_fmt(tmp.arena, "%*s%.*s", depth * 2, "", STR_ARG(node->name));
    struct nk_grid grid;
    call_grid_begin(ctx, &grid, grid_state, 24.0f);
    nk_grid_row_begin(&grid);

    if (node->kind != CALL_VALUE_LEAF) {
      str_t title = str_push_fmt(tmp.arena, "%s %.*s", node->expanded ? "-" : "+", STR_ARG(node->type));
      float width = ui_text_width(ctx, title) + 2.0f * ctx->style.button.padding.x;
      nk_grid_push(&grid, 0, nk_vec2(width, ctx->style.font_size));
      if (call_tree_button(ctx, title)) {
        node->expanded = !node->expanded;
      }
    } else {
      call_grid_text(&grid, 0, node->type, CALL_C_TYPE_TEXT);
    }

    call_grid_text(&grid, 1, indented_name, UI_C_TEXT);

    nk_grid_push(&grid, 2, nk_vec2(0.0f, 22.0f));
    if (!node->supported) {
      nk_label_colored(ctx, "<unsupported>", NK_TEXT_LEFT, UI_C_RED);
    } else if (node->kind == CALL_VALUE_ARRAY) {
      nk_spacer(ctx);
    } else if (node->kind == CALL_VALUE_FIXED_ARRAY) {
      nk_labelf_selectable_colored(ctx, NK_TEXT_LEFT, CALL_C_VALUE_TEXT, "Num=%d", node->child_count);
    } else if (node->kind == CALL_VALUE_STRUCT) {
      nk_label_selectable_colored(ctx, "{...}", NK_TEXT_LEFT, CALL_C_VALUE_TEXT);
    } else if (node->auto_world) {
      uworld_t *world = globals.gworld_ptr ? *globals.gworld_ptr : NULL;
      if (world && unreal_uobject_is_valid((uobject_t *)world)) {
        str_t name      = unreal_uobject_push_name((uobject_t *)world, tmp.arena);
        str_t full_name = unreal_uobject_push_full_name((uobject_t *)world, tmp.arena);
        nk_text_selectable_colored(ctx, (const char *)name.data, (int)name.len, NK_TEXT_LEFT, CALL_C_LINK_TEXT);
        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip_text(ctx, (const char *)full_name.data, (int)full_name.len);
        }
      } else {
        nk_label_colored(ctx, "<no current world>", NK_TEXT_LEFT, UI_C_RED);
      }
    } else if (unreal_fprop_class_is(node->prop, globals.unreal.obj_prop) ||
               unreal_fprop_class_is(node->prop, globals.unreal.class_prop)) {
      call_draw_object_combo(tool, dialog, &node->object_ref, call_node_required_class(node), node->class_picker, true);
    } else if (node->soft_picker) {
      call_draw_soft_value(tool, dialog, node);
    } else {
      call_edit_colored(ctx, NK_EDIT_FIELD, node->input, sizeof(node->input), nk_filter_default, CALL_C_VALUE_TEXT);
    }

    nk_grid_row_end(&grid);
    nk_grid_end(&grid);
  }
  scratch_end(tmp);

  if (!node->expanded || !node->supported) {
    return;
  }

  if (node->kind == CALL_VALUE_ARRAY) {
    tmp_arena_t label_tmp = scratch_begin(NULL);
    {
      struct nk_grid grid;
      call_grid_begin(ctx, &grid, grid_state, 24.0f);
      {
        nk_grid_row_begin(&grid);
        call_grid_text(&grid, 0, STR_LIT("int32_t"), CALL_C_TYPE_TEXT);
        call_grid_text(&grid, 1, str_push_fmt(label_tmp.arena, "%*sNum", (depth + 1) * 2, ""), UI_C_TEXT);
        nk_grid_push(&grid, 2, nk_vec2(0.0f, 22.0f));
        call_edit_colored(ctx, NK_EDIT_FIELD, node->input, sizeof(node->input), nk_filter_decimal, CALL_C_VALUE_TEXT);
        nk_grid_row_end(&grid);
      }
      nk_grid_end(&grid);
    }
    scratch_end(label_tmp);

    call_array_sync(dialog, node);

    int32_t idx = 0;
    for (call_value_t *child = node->first_child; child && idx < node->array_count; child = child->next, ++idx) {
      call_draw_value_row(tool, dialog, grid_state, child, depth + 1);
    }
  } else {
    for (call_value_t *child = node->first_child; child; child = child->next) {
      call_draw_value_row(tool, dialog, grid_state, child, depth + 1);
    }
  }
}

static void
call_draw_result_row(search_tool_t         *tool,
                     struct nk_grid_state *grid_state,
                     call_result_t         *result,
                     int32_t                depth)
{
  struct nk_context *ctx = tool->ctx;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t indented_name = str_push_fmt(tmp.arena, "%*s%.*s", depth * 2, "", STR_ARG(result->name));
    struct nk_grid grid;
    call_grid_begin(ctx, &grid, grid_state, 24.0f);
    nk_grid_row_begin(&grid);

    if (result->first_child) {
      str_t title = str_push_fmt(tmp.arena, "%s %.*s", result->expanded ? "-" : "+", STR_ARG(result->type));
      float width = ui_text_width(ctx, title) + 2.0f * ctx->style.button.padding.x;
      nk_grid_push(&grid, 0, nk_vec2(width, ctx->style.font_size));
      if (call_tree_button(ctx, title)) {
        result->expanded = !result->expanded;
      }
    } else {
      call_grid_text(&grid, 0, result->type, CALL_C_TYPE_TEXT);
    }

    call_grid_text(&grid, 1, indented_name, UI_C_TEXT);

    nk_grid_push(&grid, 2, nk_vec2(0.0f, 22.0f));
    call_edit_colored(ctx,
                      (nk_flags)NK_EDIT_FIELD | (nk_flags)NK_EDIT_READ_ONLY,
                      result->value,
                      sizeof(result->value),
                      nk_filter_default,
                      CALL_C_VALUE_TEXT);
    if (result->tooltip[0] != '\0' && nk_widget_is_hovered(ctx)) {
      str_t tooltip = str_from_cstr_with_cap(result->tooltip, sizeof(result->tooltip));
      nk_tooltip_text(ctx, (const char *)tooltip.data, (int)tooltip.len);
    }
    nk_grid_row_end(&grid);
    nk_grid_end(&grid);
  }
  scratch_end(tmp);

  if (result->expanded) {
    for (call_result_t *child = result->first_child; child; child = child->next) {
      call_draw_result_row(tool, grid_state, child, depth + 1);
    }
  }
}

static bool
call_has_input_params(ufunc_call_dialog_t *dialog)
{
  for (call_value_t *node = dialog->first_param; node; node = node->next) {
    if (node->role == CALL_PARAM_INPUT) {
      return true;
    }
  }
  return false;
}

static bool
call_has_output_params(ufunc_call_dialog_t *dialog)
{
  for (call_value_t *node = dialog->first_param; node; node = node->next) {
    if (node->role == CALL_PARAM_OUTPUT || node->role == CALL_PARAM_RETURN) {
      return true;
    }
  }
  return false;
}

static bool
call_has_latent_param(ufunc_call_dialog_t *dialog)
{
  for (call_value_t *node = dialog->first_param; node; node = node->next) {
    if (unreal_fname_match_text(node->prop->name, STR_LIT("LatentInfo"), true, true) ||
        unreal_fname_match_text(node->prop->name, STR_LIT("LatentActionInfo"), true, true)) {
      return true;
    }
  }
  return false;
}

static bool
call_can_invoke(search_tool_t *tool, ufunc_call_dialog_t *dialog)
{
  if (!call_record_is_live_object(tool, dialog->func_slot, (uobject_t *)dialog->func_expected) ||
      dialog->target.is_none ||
      !call_record_is_live_object(tool, dialog->target.slot, dialog->target.expected) ||
      !dialog->receiver_class ||
      !unreal_uobject_is_a(dialog->target.expected, dialog->receiver_class)) {
    return false;
  }

  for (call_value_t *node = dialog->first_param; node; node = node->next) {
    if (!node->supported) {
      return false;
    }

    if (node->auto_world && (!globals.gworld_ptr || !*globals.gworld_ptr)) {
      return false;
    }
  }

  return true;
}

static void
call_draw_table_header(struct nk_context *ctx, struct nk_grid_state *grid_state)
{
  struct nk_grid grid;
  call_grid_begin(ctx, &grid, grid_state, 22.0f);
  nk_grid_row_begin(&grid);
  {
    call_grid_text(&grid, 0, STR_LIT("TYPE"), UI_C_TEXT);
    call_grid_text(&grid, 1, STR_LIT("NAME"), UI_C_TEXT);
    call_grid_text(&grid, 2, STR_LIT("VALUE"), UI_C_TEXT);
  }
  nk_grid_row_end(&grid);
  nk_grid_end(&grid);
}

void
ufunc_call_draw(search_tool_t *tool, unsigned int vw, unsigned int vh)
{
  if (!tool || !tool->ctx || !tool->call_dialog || !tool->call_dialog->open) {
    return;
  }

  ufunc_call_dialog_t *dialog      = tool->call_dialog;
  struct nk_context   *ctx         = tool->ctx;
  const char          *window_name = "uobject_search.ufunction_call";

  if (nk_window_is_active(ctx, "uobject_search")) {
    nk_window_set_focus(ctx, window_name);
  }

  if (!dialog->bounds_inited) {
    float w = NK_MIN(900.0f, (float)vw - 40.0f);
    float h = NK_MIN(720.0f, (float)vh - 40.0f);

    dialog->bounds        = nk_rect(NK_MAX(20.0f, ((float)vw - w) * 0.5f), NK_MAX(20.0f, ((float)vh - h) * 0.5f), w, h);
    dialog->bounds_inited = true;
  }

  tmp_arena_t title_tmp = scratch_begin(NULL);
  str_t func_name = (dialog->func_expected && unreal_uobject_is_valid((uobject_t *)dialog->func_expected))
                    ? unreal_uobject_push_full_name((uobject_t *)dialog->func_expected, title_tmp.arena)
                    : STR_LIT("<stale UFunction>");

  str_t    title = str_push_fmt(title_tmp.arena, "Call %.*s", STR_ARG(func_name));
  nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE | NK_WINDOW_NO_SCROLLBAR;
  if (nk_begin_titled(ctx, window_name, (const char *)title.data, dialog->bounds, flags)) {
    dialog->bounds = nk_window_get_bounds(ctx);
    ui_win_clamp_bounds(ctx, window_name, nk_vec2(620.0f, 360.0f), nk_vec2((float)vw, (float)vh));

    struct nk_rect content   = nk_window_get_content_region(ctx);
    float          status_h  = 20.0f;
    float          buttons_h = 28.0f;
    float          bottom_h  = 4.0f;
    float          body_h    = NK_MAX(120.0f, content.h - status_h - buttons_h - bottom_h - 3.0f * ctx->style.window.spacing.y - 2.0f);

    nk_layout_row_dynamic(ctx, body_h, 1);
    if (nk_group_begin(ctx, "ufunction_call.body", 0)) {
      nk_layout_row_dynamic(ctx, 20.0f, 1);
      nk_label(ctx, "TARGET", NK_TEXT_LEFT);

      tmp_arena_t target_tmp = scratch_begin(NULL);
      {
        struct nk_grid grid;
        str_t target_type = dialog->receiver_class
                            ? str_push_fmt(target_tmp.arena, "Object<%.*s>", STR_ARG(unreal_uobject_push_name((uobject_t *)dialog->receiver_class, target_tmp.arena)))
                            : STR_LIT("Object<?>" );
        call_target_grid_begin(ctx, &grid, &dialog->target_grid, 26.0f);
        {
          nk_grid_row_begin(&grid);
          {
            call_grid_text(&grid, 0, target_type, CALL_C_TYPE_TEXT);
            nk_grid_push(&grid, 1, nk_vec2(0.0f, 24.0f));
            call_draw_object_combo(tool, dialog, &dialog->target, dialog->receiver_class, false, false);
          }
          nk_grid_row_end(&grid);
        }
        nk_grid_end(&grid);
      }
      scratch_end(target_tmp);

      if (dialog->func_expected && (dialog->func_expected->func_flags & FUNC_FLAG_NET)) {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label_colored(ctx, "Warning: this is a network/RPC function and may send traffic or require authority.", NK_TEXT_CENTERED, UI_C_ORANGE);
      }

      if (call_has_latent_param(dialog)) {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label_colored(ctx, "Warning: this appears to be a latent function; a zero/default LatentInfo is usually not meaningful.", NK_TEXT_CENTERED, UI_C_ORANGE);
      }

      nk_layout_row_dynamic(ctx, 8.0f, 1);
      nk_spacer(ctx);

      nk_layout_row_dynamic(ctx, 20.0f, 1);
      nk_label(ctx, "INPUTS", NK_TEXT_LEFT);
      call_draw_table_header(ctx, &dialog->input_grid);

      if (call_has_input_params(dialog)) {
        for (call_value_t *node = dialog->first_param; node; node = node->next) {
          if (node->role == CALL_PARAM_INPUT) {
            call_draw_value_row(tool, dialog, &dialog->input_grid, node, 0);
          }
        }
      } else {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label_colored(ctx, "No input parameters", NK_TEXT_CENTERED, UI_C_TEXT_MUTED);
      }

      nk_layout_row_dynamic(ctx, 8.0f, 1);
      nk_spacer(ctx);

      nk_layout_row_dynamic(ctx, 20.0f, 1);
      nk_label(ctx, "OUTPUTS", NK_TEXT_LEFT);

      call_draw_table_header(ctx, &dialog->output_grid);
      if (dialog->first_result) {
        for (call_result_t *result = dialog->first_result; result; result = result->next) {
          call_draw_result_row(tool, &dialog->output_grid, result, 0);
        }
      } else {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label_colored(ctx,
                         call_has_output_params(dialog) ? "Call the function to populate output values" : "No return/out parameters",
                         NK_TEXT_CENTERED,
                         UI_C_TEXT_MUTED);
      }
      nk_group_end(ctx);
    }

    nk_layout_row_dynamic(ctx, status_h, 1);
    nk_label_colored(ctx, dialog->status, NK_TEXT_CENTERED, dialog->status_is_error ? UI_C_RED : CALL_C_VALUE_TEXT);

    nk_layout_row_dynamic(ctx, buttons_h, 2);

    bool can_invoke = call_can_invoke(tool, dialog);
    if (!can_invoke) {
      nk_widget_disable_begin(ctx);
    }

    nk_style_push_style_item(ctx, &ctx->style.button.normal, UI_ITEM(UI_C_ACCENT));
    nk_style_push_style_item(ctx, &ctx->style.button.hover, UI_ITEM(UI_C_ACCENT_HOVER));
    nk_style_push_style_item(ctx, &ctx->style.button.active, UI_ITEM(UI_C_ACCENT_ACTIVE));
    nk_style_push_color(ctx, &ctx->style.button.border_color, UI_C_BORDER_FOCUS);

    bool call_pressed = ui_button_str(ctx, STR_LIT("Call"));

    nk_style_pop_color(ctx);
    nk_style_pop_style_item(ctx);
    nk_style_pop_style_item(ctx);
    nk_style_pop_style_item(ctx);

    if (call_pressed) {
      call_execute(tool, dialog);
    }

    if (!can_invoke) {
      nk_widget_disable_end(ctx);
    }

    if (ui_button_str(ctx, STR_LIT("Close"))) {
      ufunc_call_close(tool);
    }

    nk_layout_row_dynamic(ctx, bottom_h, 1);
    nk_spacer(ctx);
  }
  nk_end(ctx);
  scratch_end(title_tmp);

  if (nk_window_is_closed(ctx, window_name)) {
    ufunc_call_close(tool);
  }
}
