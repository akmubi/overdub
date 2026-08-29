#include "common.h"
#include "uobject_search_internal.h"

#include "arena.h"
#include "globals.h"
#include "scratch.h"
#include "str.h"
#include "types.h"
#include "ui_nuklear.h"
#include "unreal.h"
#include "unreal_prop.h"

#include "vendor_stb.h"

#define DETAIL_C_ROW_COLLAPSIBLE_BG     nk_rgba(34, 43, 48, 210)
#define DETAIL_C_ROW_COLLAPSIBLE_HOVER  nk_rgba(95, 178, 145, 255)
#define DETAIL_C_ROW_COLLAPSIBLE_ACTIVE nk_rgba(50, 65, 72, 245)

#define DETAIL_C_PROP_TYPE_TEXT       UI_C_PROP_TYPE_TEXT
#define DETAIL_C_PROP_VALUE_TEXT      UI_C_PROP_VALUE_TEXT
#define DETAIL_C_PROP_LINK_VALUE_TEXT UI_C_PROP_LINK_TEXT
#define DETAIL_C_PROP_NAME_TEXT       UI_C_TEXT
#define DETAIL_C_PROP_OFFSET_TEXT     UI_C_TEXT
#define DETAIL_C_PROP_SIZE_TEXT       UI_C_TEXT
#define DETAIL_C_PROP_FLAGS_TEXT      DETAIL_C_PROP_VALUE_TEXT
#define DETAIL_C_ENUM_VALUE_TEXT      DETAIL_C_PROP_VALUE_TEXT

#define DETAIL_C_ROW_EXPAND nk_rgba(44, 35, 30, 235)
#define DETAIL_C_ROW_ACCENT UI_C_ACCENT

#define DETAIL_PROP_COL_COUNT       (5)
#define DETAIL_PROP_COL_TEXT_OFFSET "OFFSET"
#define DETAIL_PROP_COL_TEXT_SIZE   "SIZE"
#define DETAIL_PROP_COL_TEXT_NAME   "NAME"
#define DETAIL_PROP_COL_TEXT_TYPE   "TYPE"
#define DETAIL_PROP_COL_TEXT_VALUE  "VALUE"
#define DETAIL_PROP_COL_TEXT_FLAGS  "FLAGS"

#define DETAIL_ENUM_COL_COUNT      (2)
#define DETAIL_ENUM_COL_TEXT_NAME  "NAME"
#define DETAIL_ENUM_COL_TEXT_VALUE "VALUE"

typedef fscript_delegate_t           detail_fscript_delegate_t;
typedef fmulticast_script_delegate_t detail_fscript_delegate_array_t;

static void
detail_copy_text(struct nk_context *ctx, str_t text)
{
  if (!ctx || str_is_empty(text) || !ctx->clip.copy) {
    return;
  }
  ctx->clip.copy(ctx->clip.userdata, (const char *)text.data, (int)text.len);
}

static str_t
obj_flags_push_text(arena_t *arena, eobj_flags_t flags)
{
  str_t       decoded = {0};
  tmp_arena_t tmp     = scratch_begin(arena);
  {
    str_list_t list = {0};

    if (flags & RF_PUBLIC)                    str_list_push(tmp.arena, &list, STR_LIT("Public"));
    if (flags & RF_STANDALONE)                str_list_push(tmp.arena, &list, STR_LIT("Standalone"));
    if (flags & RF_MARK_AS_NATIVE)            str_list_push(tmp.arena, &list, STR_LIT("Native"));
    if (flags & RF_TRANSACTIONAL)             str_list_push(tmp.arena, &list, STR_LIT("Transactional"));
    if (flags & RF_CLASS_DEFAULT_OBJECT)      str_list_push(tmp.arena, &list, STR_LIT("ClassDefaultObject"));
    if (flags & RF_ARCHETYPE_OBJECT)          str_list_push(tmp.arena, &list, STR_LIT("ArchetypeObject"));
    if (flags & RF_TRANSIENT)                 str_list_push(tmp.arena, &list, STR_LIT("Transient"));
    if (flags & RF_MARK_AS_ROOT_SET)          str_list_push(tmp.arena, &list, STR_LIT("RootSet"));
    if (flags & RF_TAG_GARBAGE_TEMP)          str_list_push(tmp.arena, &list, STR_LIT("TagGarbageTemp"));
    if (flags & RF_NEED_INITIALIZATION)       str_list_push(tmp.arena, &list, STR_LIT("NeedInitialization"));
    if (flags & RF_NEED_LOAD)                 str_list_push(tmp.arena, &list, STR_LIT("NeedLoad"));
    if (flags & RF_NEED_POST_LOAD)            str_list_push(tmp.arena, &list, STR_LIT("NeedPostLoad"));
    if (flags & RF_NEED_POST_LOAD_SUBOBJECTS) str_list_push(tmp.arena, &list, STR_LIT("NeedPostLoadSubobjects"));
    if (flags & RF_BEGIN_DESTROYED)           str_list_push(tmp.arena, &list, STR_LIT("BeginDestroyed"));
    if (flags & RF_FINISH_DESTROYED)          str_list_push(tmp.arena, &list, STR_LIT("FinishDestroyed"));
    if (flags & RF_DEFAULT_SUB_OBJECT)        str_list_push(tmp.arena, &list, STR_LIT("DefaultSubObject"));
    if (flags & RF_WAS_LOADED)                str_list_push(tmp.arena, &list, STR_LIT("WasLoaded"));
    if (flags & RF_LOAD_COMPLETED)            str_list_push(tmp.arena, &list, STR_LIT("LoadCompleted"));
    if (flags & RF_DYNAMIC)                   str_list_push(tmp.arena, &list, STR_LIT("Dynamic"));

    if (list.count == 0) {
      decoded = str_push_fmt(arena, "0x%08X", flags);
    } else {
      decoded = str_list_join(arena, list, str_push_fmt(tmp.arena, "0x%08X (", flags), STR_LIT(" | "), STR_LIT(")"));
    }
  }
  scratch_end(tmp);

  return decoded;
}

static str_t
func_flags_push_text(arena_t *arena, uint32_t flags)
{
  str_t       decoded = STR_NULL;
  tmp_arena_t tmp     = scratch_begin(arena);
  {
    str_list_t list = {0};

    if (flags & FUNC_FLAG_FINAL)              str_list_push(tmp.arena, &list, STR_LIT("Final"));
    if (flags & FUNC_FLAG_NATIVE)             str_list_push(tmp.arena, &list, STR_LIT("Native"));
    if (flags & FUNC_FLAG_EVENT)              str_list_push(tmp.arena, &list, STR_LIT("Event"));
    if (flags & FUNC_FLAG_STATIC)             str_list_push(tmp.arena, &list, STR_LIT("Static"));
    if (flags & FUNC_FLAG_PUBLIC)             str_list_push(tmp.arena, &list, STR_LIT("Public"));
    if (flags & FUNC_FLAG_PRIVATE)            str_list_push(tmp.arena, &list, STR_LIT("Private"));
    if (flags & FUNC_FLAG_PROTECTED)          str_list_push(tmp.arena, &list, STR_LIT("Protected"));
    if (flags & FUNC_FLAG_NET)                str_list_push(tmp.arena, &list, STR_LIT("Net"));
    if (flags & FUNC_FLAG_NET_RELIABLE)       str_list_push(tmp.arena, &list, STR_LIT("Reliable"));
    if (flags & FUNC_FLAG_NET_SERVER)         str_list_push(tmp.arena, &list, STR_LIT("Server"));
    if (flags & FUNC_FLAG_NET_CLIENT)         str_list_push(tmp.arena, &list, STR_LIT("Client"));
    if (flags & FUNC_FLAG_NET_MULTICAST)      str_list_push(tmp.arena, &list, STR_LIT("Multicast"));
    if (flags & FUNC_FLAG_BLUEPRINT_CALLABLE) str_list_push(tmp.arena, &list, STR_LIT("BlueprintCallable"));
    if (flags & FUNC_FLAG_BLUEPRINT_EVENT)    str_list_push(tmp.arena, &list, STR_LIT("BlueprintEvent"));
    if (flags & FUNC_FLAG_BLUEPRINT_PURE)     str_list_push(tmp.arena, &list, STR_LIT("BlueprintPure"));
    if (flags & FUNC_FLAG_CONST)              str_list_push(tmp.arena, &list, STR_LIT("Const"));
    if (flags & FUNC_FLAG_EXEC)               str_list_push(tmp.arena, &list, STR_LIT("Exec"));

    if (list.count == 0) {
      decoded = str_push_fmt(arena, "0x%08X", flags);
    } else {
      decoded = str_list_join(arena, list, str_push_fmt(tmp.arena, "0x%08X (", flags), STR_LIT(" | "), STR_LIT(")"));
    }
  }
  scratch_end(tmp);
  return decoded;
}

static str_t
param_flags_push_text(arena_t *arena, eprop_flags_t flags)
{
  str_t       decoded = STR_NULL;
  tmp_arena_t tmp     = scratch_begin(arena);
  {
    str_list_t list = {0};

    if (flags & CPF_PARM)           str_list_push(tmp.arena, &list, STR_LIT("Parm"));
    if (flags & CPF_RETURN_PARM)    str_list_push(tmp.arena, &list, STR_LIT("Return"));
    if (flags & CPF_OUT_PARM)       str_list_push(tmp.arena, &list, STR_LIT("Out"));
    if (flags & CPF_CONST_PARM)     str_list_push(tmp.arena, &list, STR_LIT("Const"));
    if (flags & CPF_REFERENCE_PARM) str_list_push(tmp.arena, &list, STR_LIT("Ref"));

    if (list.count == 0) {
      decoded = str_push_fmt(arena, "0x%llX", flags);
    } else {
      decoded = str_list_join(arena, list, str_push_fmt(tmp.arena, "0x%llX (", flags), STR_LIT(" | "), STR_LIT(")"));
    }
  }
  scratch_end(tmp);
  return decoded;
}

static ui_text_span_t
detail_tab_label(detail_tab_t *tab)
{
  ASSERT(tab != NULL);
  ASSERT(tab->record != NULL);

  return tab->record->name;
}

static detail_tab_t *
detail_tab_find(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  for (detail_tab_t *tab = tool->details.first; tab; tab = tab->next) {
    if (tab->record == record) {
      return tab;
    }
  }

  return NULL;
}

static detail_prop_t *
detail_prop_alloc_runtime(detail_tab_t *tab)
{
  ASSERT(tab != NULL);

  detail_prop_t *node = tab->free_prop;
  if (node) {
    tab->free_prop = node->free_next;
    mem_zero(node, sizeof(*node));
  } else {
    node = ARENA_PUSH_ZERO(tab->info_arena, detail_prop_t);
  }

  if (node) {
    node->runtime_node = true;
  }
  return node;
}

static void
detail_prop_free_runtime_list(detail_tab_t *tab, detail_prop_t *first)
{
  ASSERT(tab != NULL);

  detail_prop_t *node = first;
  while (node) {
    detail_prop_t *next = node->next;

    if (node->first_child) {
      detail_prop_free_runtime_list(tab, node->first_child);
    }

    if (node->runtime_node) {
      mem_zero(node, sizeof(*node));
      node->free_next = tab->free_prop;
      tab->free_prop  = node;
    }

    node = next;
  }
}

static void
detail_prop_clear_runtime_children(detail_tab_t *tab, detail_prop_t *node)
{
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (node->first_child) {
    detail_prop_free_runtime_list(tab, node->first_child);
  }

  node->first_child      = NULL;
  node->last_child       = NULL;
  node->live_child_count = 0;
  node->live_slot_count  = 0;
}

static detail_prop_t *
detail_prop_push_pseudo(search_tool_t *tool, detail_tab_t *tab, fprop_t *prop, str_t name, uint32_t depth, bool permanent, bool runtime);
static void
detail_prop_expand_complex(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_t *prop, uint32_t depth, bool permanent, bool runtime);
static detail_prop_t *
detail_prop_push(search_tool_t *tool, detail_tab_t *tab, fprop_t *prop, bool is_param, uint32_t depth, bool permanent, bool runtime);
static detail_prop_t *
detail_prop_push_array_elem(search_tool_t *tool, detail_tab_t *tab, fprop_t *elem, uint32_t elem_idx, uint8_t *elem_addr, int32_t elem_offset, uint32_t depth, bool permanent, bool runtime);
static detail_prop_t *
detail_prop_push_set_elem(search_tool_t *tool, detail_tab_t *tab, fprop_t *elem, uint32_t elem_idx, uint8_t *elem_addr, int32_t elem_offset, uint32_t depth, bool permanent, bool runtime);
static detail_prop_t *
detail_prop_push_map_pair(
  search_tool_t *tool, detail_tab_t *tab, fprop_map_t *map, uint32_t pair_idx, uint8_t *key_addr, uint8_t *val_addr, int32_t pair_offset, uint32_t depth, bool permanent, bool runtime);
static detail_func_t *
detail_func_push(search_tool_t *tool, detail_tab_t *tab, record_t *func_record, int32_t index);
static detail_enum_t *
detail_enum_push(search_tool_t *tool, detail_tab_t *tab, record_t *enum_record);

detail_prop_t *
detail_prop_push_pseudo(search_tool_t *tool, detail_tab_t *tab, fprop_t *prop, str_t name, uint32_t depth, bool permanent, bool runtime)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  arena_t       *arena = (permanent) ? tab->info_arena : tab->value_arena;
  detail_prop_t *node  = (runtime) ? detail_prop_alloc_runtime(tab) : ARENA_PUSH_ZERO(arena, detail_prop_t);
  if (node) {
    node->prop        = prop;
    node->name        = ui_text_span_make(tool->ctx, str_push_copy(arena, name));
    node->type        = ui_text_span_make(tool->ctx, unreal_fprop_push_type_name(prop, arena));
    node->offset      = 0;
    node->size        = 0;
    node->array_dim   = 0;
    node->offset_text = ui_text_span_make(tool->ctx, STR_NULL);
    node->size_text   = ui_text_span_make(tool->ctx, STR_NULL);

    if (prop) {
      node->offset      = prop->offset_internal;
      node->size        = prop->elem_size;
      node->array_dim   = prop->array_dim;
      node->offset_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->offset));
      node->size_text   = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->size));

      detail_prop_t *expanded = detail_prop_push(tool, tab, prop, false, depth + 1, permanent, runtime);
      if (expanded) {
        node->first_child = expanded->first_child;
        node->last_child  = expanded->last_child;
      }
    }
  }
  return node;
}

void
detail_prop_expand_complex(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_t *prop, uint32_t depth, bool permanent, bool runtime)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!prop) {
    return;
  }

  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_STRUCT: {
      fprop_struct_t *sp = (fprop_struct_t *)prop;
      if (sp->script_struct) {
        ustruct_t *st = (ustruct_t *)sp->script_struct;
        for (ffield_t *f = st->child_props; f; f = f->next) {
          fprop_t       *child_prop = (fprop_t *)f;
          detail_prop_t *child_node = detail_prop_push(tool, tab, child_prop, false, depth + 1, permanent, runtime);
          if (child_node) {
            QUEUE_PUSH(node->first_child, node->last_child, child_node);
          }
        }
      }
      break;
    }

    case UNREAL_PROP_KIND_ARRAY: {
      fprop_array_t *ap         = (fprop_array_t *)prop;
      detail_prop_t *child_node = detail_prop_push_pseudo(tool, tab, ap->inner, STR_LIT("Inner"), depth + 1, permanent, runtime);
      if (child_node) {
        QUEUE_PUSH(node->first_child, node->last_child, child_node);
      }
      break;
    }

    case UNREAL_PROP_KIND_SET: {
      fprop_set_t   *sp         = (fprop_set_t *)prop;
      detail_prop_t *child_node = detail_prop_push_pseudo(tool, tab, sp->elem_prop, STR_LIT("Element"), depth + 1, permanent, runtime);
      if (child_node) {
        QUEUE_PUSH(node->first_child, node->last_child, child_node);
      }
      break;
    }

    case UNREAL_PROP_KIND_MAP: {
      fprop_map_t *mp = (fprop_map_t *)prop;

      detail_prop_t *key_child_node = detail_prop_push_pseudo(tool, tab, mp->key_prop, STR_LIT("Key"), depth + 1, permanent, runtime);
      if (key_child_node) {
        QUEUE_PUSH(node->first_child, node->last_child, key_child_node);
      }

      detail_prop_t *val_child_node = detail_prop_push_pseudo(tool, tab, mp->val_prop, STR_LIT("Value"), depth + 1, permanent, runtime);
      if (val_child_node) {
        QUEUE_PUSH(node->first_child, node->last_child, val_child_node);
      }
      break;
    }

    default:
      break;
  }
}

detail_prop_t *
detail_prop_push(search_tool_t *tool, detail_tab_t *tab, fprop_t *prop, bool is_param, uint32_t depth, bool permanent, bool runtime)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  if (!prop) {
    return NULL;
  }

  arena_t       *arena = (permanent) ? tab->info_arena : tab->value_arena;
  detail_prop_t *node  = (runtime) ? detail_prop_alloc_runtime(tab) : ARENA_PUSH_ZERO(arena, detail_prop_t);
  if (node) {
    node->prop        = prop;
    node->name        = ui_text_span_make(tool->ctx, unreal_fname_to_str(prop->name, arena));
    node->type        = ui_text_span_make(tool->ctx, unreal_fprop_push_type_name(prop, arena));
    node->offset      = prop->offset_internal;
    node->size        = prop->elem_size;
    node->array_dim   = prop->array_dim;
    node->is_param    = is_param;
    node->offset_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->offset));
    node->size_text   = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->size));

    if (is_param) {
      node->summary = ui_text_span_make(tool->ctx, param_flags_push_text(arena, prop->prop_flags));
    }

    detail_prop_expand_complex(tool, tab, node, prop, depth, permanent, runtime);
  }
  return node;
}

detail_prop_t *
detail_prop_push_array_elem(
  search_tool_t *tool, detail_tab_t *tab, fprop_t *elem, uint32_t elem_idx, uint8_t *elem_addr, int32_t elem_offset, uint32_t depth, bool permanent, bool runtime)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  if (!elem) {
    return NULL;
  }

  arena_t       *arena = (permanent) ? tab->info_arena : tab->value_arena;
  detail_prop_t *node  = (runtime) ? detail_prop_alloc_runtime(tab) : ARENA_PUSH_ZERO(arena, detail_prop_t);
  if (node) {
    node->prop           = elem;
    node->name           = ui_text_span_make(tool->ctx, str_push_fmt(arena, "[%u]", elem_idx));
    node->type           = ui_text_span_make(tool->ctx, unreal_fprop_push_type_name(elem, arena));
    node->offset         = elem_offset;
    node->size           = elem->elem_size;
    node->array_dim      = elem->array_dim;
    node->has_value_addr = elem_addr != NULL;
    node->value_addr     = elem_addr;
    node->live_slot_idx  = elem_idx;
    node->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->offset));
    node->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->size));

    detail_prop_expand_complex(tool, tab, node, elem, depth + 1, permanent, runtime);
  }
  return node;
}

detail_prop_t *
detail_prop_push_set_elem(
  search_tool_t *tool, detail_tab_t *tab, fprop_t *elem, uint32_t elem_idx, uint8_t *elem_addr, int32_t elem_offset, uint32_t depth, bool permanent, bool runtime)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  if (!elem) {
    return NULL;
  }

  arena_t       *arena = (permanent) ? tab->info_arena : tab->value_arena;
  detail_prop_t *node  = (runtime) ? detail_prop_alloc_runtime(tab) : ARENA_PUSH_ZERO(arena, detail_prop_t);
  if (node) {
    node->prop           = elem;
    node->name           = ui_text_span_make(tool->ctx, str_push_fmt(arena, "[%u]", elem_idx));
    node->type           = ui_text_span_make(tool->ctx, unreal_fprop_push_type_name(elem, arena));
    node->offset         = elem_offset;
    node->size           = elem->elem_size;
    node->array_dim      = elem->array_dim;
    node->has_value_addr = (elem_addr != NULL);
    node->value_addr     = elem_addr;
    node->live_slot_idx  = elem_idx;
    node->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->offset));
    node->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->size));

    detail_prop_expand_complex(tool, tab, node, elem, depth + 1, permanent, runtime);
  }
  return node;
}

detail_prop_t *
detail_prop_push_map_pair(
  search_tool_t *tool, detail_tab_t *tab, fprop_map_t *map, uint32_t pair_idx, uint8_t *key_addr, uint8_t *val_addr, int32_t pair_offset, uint32_t depth, bool permanent, bool runtime)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  if (!map || !map->key_prop || !map->val_prop) {
    return NULL;
  }

  arena_t *arena = (permanent) ? tab->info_arena : tab->value_arena;

  detail_prop_t *pair = NULL;
  detail_prop_t *key  = NULL;
  detail_prop_t *val  = NULL;

  if (runtime) {
    pair = detail_prop_alloc_runtime(tab);
    key  = detail_prop_alloc_runtime(tab);
    val  = detail_prop_alloc_runtime(tab);

    if (!pair || !key || !val) {
      return NULL;
    }
  } else {
    detail_prop_t *nodes = ARENA_PUSH_ARRAY_ZERO(arena, detail_prop_t, 3);
    if (!nodes) {
      return NULL;
    }

    pair = &nodes[0];
    key  = &nodes[1];
    val  = &nodes[2];
  }

  str_t key_type_name  = unreal_fprop_push_type_name(map->key_prop, arena);
  str_t val_type_name  = unreal_fprop_push_type_name(map->val_prop, arena);
  str_t pair_type_name = str_push_fmt(arena, "TPair<%.*s, %.*s>", STR_ARG(key_type_name), STR_ARG(val_type_name));

  pair->name           = ui_text_span_make(tool->ctx, str_push_fmt(arena, "[%u]", pair_idx));
  pair->type           = ui_text_span_make(tool->ctx, pair_type_name);
  pair->offset         = pair_offset;
  pair->size           = map->map_layout.set_layout.size;
  pair->has_value_addr = key_addr != NULL && val_addr != NULL;
  pair->value_addr     = key_addr;
  pair->live_slot_idx  = pair_idx;
  pair->children_kind  = DETAIL_PROP_CHILDREN_SCHEMA;
  pair->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)pair->offset));
  pair->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)pair->size));

  key->prop           = map->key_prop;
  key->name           = ui_text_span_make(tool->ctx, STR_LIT("Key"));
  key->type           = ui_text_span_make(tool->ctx, key_type_name);
  key->offset         = 0;
  key->size           = map->key_prop->elem_size;
  key->array_dim      = map->key_prop->array_dim;
  key->has_value_addr = key_addr != NULL;
  key->value_addr     = key_addr;
  key->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)key->offset));
  key->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)key->size));

  detail_prop_expand_complex(tool, tab, key, map->key_prop, depth + 1, permanent, runtime);
  QUEUE_PUSH(pair->first_child, pair->last_child, key);

  val->prop           = map->val_prop;
  val->name           = ui_text_span_make(tool->ctx, STR_LIT("Value"));
  val->type           = ui_text_span_make(tool->ctx, val_type_name);
  val->offset         = map->map_layout.value_offset;
  val->size           = map->val_prop->elem_size;
  val->array_dim      = map->val_prop->array_dim;
  val->has_value_addr = val_addr != NULL;
  val->value_addr     = val_addr;
  val->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)val->offset));
  val->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)val->size));

  detail_prop_expand_complex(tool, tab, val, map->val_prop, depth + 1, permanent, runtime);
  QUEUE_PUSH(pair->first_child, pair->last_child, val);
  return pair;
}

detail_func_t *
detail_func_push(search_tool_t *tool, detail_tab_t *tab, record_t *func_record, int32_t index)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(func_record != NULL);
  ASSERT(func_record->kind == UOBJECT_KIND_FUNC);

  arena_t       *arena = tab->info_arena;
  ufunc_t       *ufunc = (ufunc_t *)func_record->obj;
  detail_func_t *node  = ARENA_PUSH_ZERO(arena, detail_func_t);
  if (node) {
    node->record           = func_record;
    node->flags            = ufunc->func_flags;
    node->params_size      = ufunc->params_size;
    node->index            = index;
    node->flags_text       = ui_text_span_make(tool->ctx, func_flags_push_text(arena, node->flags));
    node->params_size_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%X", node->params_size));

    node->native_func_text             = ui_text_span_make(tool->ctx, str_push_fmt(arena, "%p", ufunc->func));
    node->event_graph_func_text        = ui_text_span_make(tool->ctx, str_push_fmt(arena, "%p", ufunc->event_graph_func));
    node->event_graph_call_offset_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "%p", ufunc->event_graph_call_offset));

    for (ffield_t *f = ufunc->child_props; f; f = f->next) {
      fprop_t *prop = (fprop_t *)f;

      if (!(prop->prop_flags & CPF_PARM)) {
        continue;
      }

      detail_prop_t *param = detail_prop_push(tool, tab, prop, true, 0, true, false);
      if (param) {
        QUEUE_PUSH(node->first_param, node->last_param, param);
      }
    }
  }
  return node;
}

detail_enum_t *
detail_enum_push(search_tool_t *tool, detail_tab_t *tab, record_t *enum_record)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(enum_record != NULL);
  ASSERT(enum_record->kind == UOBJECT_KIND_ENUM);

  arena_t       *arena = tab->info_arena;
  uenum_t       *uenum = (uenum_t *)enum_record->obj;
  detail_enum_t *node  = ARENA_PUSH_ZERO(arena, detail_enum_t);
  if (node) {
    node->record   = enum_record;
    node->cpp_type = ui_text_span_make(tool->ctx, unreal_fstring_to_str(uenum->cpp_type, arena));

    for (int i = 0; i < uenum->names.num; ++i) {
      detail_enum_entry_t *entry = ARENA_PUSH_ZERO(arena, detail_enum_entry_t);
      if (entry) {
        entry->name       = ui_text_span_make(tool->ctx, unreal_fname_to_str(uenum->names.data[i].key, arena));
        entry->value      = uenum->names.data[i].value;
        entry->value_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "%lld", (long long)entry->value));

        QUEUE_PUSH(node->first_entry, node->last_entry, entry);
      }
    }
  }
  return node;
}

static void
detail_owner_gather_props(search_tool_t *tool, detail_tab_t *tab, detail_owner_t *owner)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  if (!owner || !owner->record) {
    return;
  }

  ustruct_t *super = (ustruct_t *)owner->record->obj;
  for (ffield_t *f = super->child_props; f; f = f->next) {
    fprop_t *prop = (fprop_t *)f;

    if (prop->prop_flags & CPF_PARM) {
      continue;
    }

    detail_prop_t *detail = detail_prop_push(tool, tab, prop, false, 0, true, false);
    if (detail) {
      QUEUE_PUSH(owner->first_prop, owner->last_prop, detail);
    }
  }
}

static void
detail_owner_gather_funcs(search_tool_t *tool, detail_tab_t *tab, detail_owner_t *owner, uint32_t *func_index_counter)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(func_index_counter != NULL);

  if (!owner || !owner->record) {
    return;
  }

  ustruct_t *super = (ustruct_t *)owner->record->obj;
  for (ufield_t *f = super->children; f; f = f->next) {
    uobject_t *func_obj  = (uobject_t *)f;
    uint32_t   func_slot = (uint32_t)func_obj->internal_idx;

    ASSERT(func_slot < tool->cache.record_cap);

    record_t *func_record = record_from_slot(tool, func_slot);
    ASSERT(func_record);
    ASSERT(record_has_flag(func_record, RECORD_FLAG_LIVE));
    ASSERT(func_record->obj == func_obj);

    if (func_record->kind != UOBJECT_KIND_FUNC) {
      continue;
    }

    detail_func_t *func = detail_func_push(tool, tab, func_record, *func_index_counter);
    if (func) {
      ASSERT(cache_name(tool, func->record));

      *func_index_counter += 1;
      QUEUE_PUSH(owner->first_func, owner->last_func, func);
    }
  }
}

static void
detail_tab_build(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(cache_names(tool, tab->record));

  tab->flags        = ui_text_span_make(tool->ctx, obj_flags_push_text(tab->info_arena, tab->record->obj->obj_flags));
  tab->internal_idx = ui_text_span_make(tool->ctx, str_push_fmt(tab->info_arena, "%d", tab->record->obj->internal_idx));
  tab->addr         = ui_text_span_make(tool->ctx, str_push_fmt(tab->info_arena, "%p", tab->record->obj));
  tab->offset_col   = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_PROP_COL_TEXT_OFFSET));
  tab->size_col     = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_PROP_COL_TEXT_SIZE));
  tab->type_col     = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_PROP_COL_TEXT_TYPE));
  tab->name_col     = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_PROP_COL_TEXT_NAME));
  tab->val_col      = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_PROP_COL_TEXT_VALUE));
  tab->flags_col    = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_PROP_COL_TEXT_FLAGS));
  tab->no_instances = ui_text_span_make(tool->ctx, STR_LIT("No live instances"));
  tab->no_props     = ui_text_span_make(tool->ctx, STR_LIT("No properties"));
  tab->no_funcs     = ui_text_span_make(tool->ctx, STR_LIT("No functions"));
  tab->no_entries   = ui_text_span_make(tool->ctx, STR_LIT("No entries"));
  tab->no_params    = ui_text_span_make(tool->ctx, STR_LIT("No parameters"));
  tab->owner_hbar   = ui_text_span_make(tool->ctx, STR_LIT("==="));
  tab->value_base   = NULL;

  if (tab->record->obj->outer) {
    tab->outer_name = tab->record->outer_name;
  }

  if (tab->record->kind == UOBJECT_KIND_PACKAGE) {
    uint64_t state_size   = ((uint64_t)tool->cache.record_cap + 7) / 8;
    tab->package_expanded = ARENA_PUSH_ARRAY_ZERO(tab->info_arena, uint8_t, state_size);
    ASSERT(tab->package_expanded != NULL);
    return;
  }

  if (tab->record->kind == UOBJECT_KIND_ENUM) {
    tab->enumeration            = detail_enum_push(tool, tab, tab->record);
    tab->enumeration->maximized = true;

    tab->enumeration->enum_name_col  = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_ENUM_COL_TEXT_NAME));
    tab->enumeration->enum_value_col = ui_text_span_make(tool->ctx, STR_LIT(DETAIL_ENUM_COL_TEXT_VALUE));
    return;
  }

  if (tab->record->kind == UOBJECT_KIND_FUNC) {
    tab->function            = detail_func_push(tool, tab, tab->record, 0);
    tab->function->maximized = true;
    return;
  }

  ustruct_t *layout_type = (ustruct_t *)tab->record->obj->cls;
  uint8_t   *value_base  = (uint8_t *)tab->record->obj;

  if (tab->record->kind == UOBJECT_KIND_CLASS || tab->record->kind == UOBJECT_KIND_STRUCT) {
    layout_type = (ustruct_t *)tab->record->obj;
    value_base  = NULL;
  }

  tab->value_base = value_base;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    uint32_t depth = 0;
    for (ustruct_t *s = layout_type; s; s = s->super_struct) {
      depth += 1;
    }

    record_t **chain = ARENA_PUSH_ARRAY_ZERO(tmp.arena, record_t *, depth);
    if (chain) {
      uint32_t i = depth;
      for (ustruct_t *s = layout_type; s; s = s->super_struct) {
        uobject_t *super_obj  = (uobject_t *)s;
        uint32_t   super_slot = (uint32_t)super_obj->internal_idx;

        ASSERT(super_slot < tool->cache.record_cap);

        record_t *super_record = record_from_slot(tool, super_slot);
        ASSERT(super_record != NULL);
        ASSERT(record_has_flag(super_record, RECORD_FLAG_LIVE));
        ASSERT(super_record->obj == super_obj);

        chain[--i] = super_record;
      }

      uint32_t func_index_counter = 0;
      for (i = 0; i < depth; ++i) {
        detail_owner_t *owner = ARENA_PUSH_ZERO(tab->info_arena, detail_owner_t);
        if (owner) {
          owner->record = chain[i];
          ASSERT(cache_name(tool, owner->record));
          QUEUE_PUSH(tab->first_owner, tab->last_owner, owner);

          detail_owner_gather_props(tool, tab, owner);
          detail_owner_gather_funcs(tool, tab, owner, &func_index_counter);
        }
      }
    }
  }
  scratch_end(tmp);
}

static void
detail_tab_reset_storage(detail_tab_t *tab)
{
  ASSERT(tab != NULL);

  arena_t *info_arena  = tab->info_arena;
  arena_t *value_arena = tab->value_arena;

  arena_pop_to(info_arena, 0);
  arena_pop_to(value_arena, 0);

  mem_zero(tab, sizeof(*tab));

  tab->info_arena  = info_arena;
  tab->value_arena = value_arena;
}

static void
detail_tab_reset_keep_links(detail_tab_t *tab)
{
  ASSERT(tab != NULL);

  detail_tab_t *prev = tab->prev;
  detail_tab_t *next = tab->next;

  detail_tab_reset_storage(tab);

  tab->prev = prev;
  tab->next = next;
}

static detail_tab_t *
detail_tab_alloc(search_tool_t *tool)
{
  ASSERT(tool != NULL);

  if (tool->details.count >= COUNTOF(tool->details.tabs)) {
    return NULL;
  }

  detail_tab_t *tab = tool->details.free;
  if (!tab) {
    return NULL;
  }

  tool->details.free = tab->next;

  detail_tab_reset_storage(tab);

  tab->prev = tool->details.last;
  tab->next = NULL;

  if (tool->details.last) {
    tool->details.last->next = tab;
  } else {
    tool->details.first = tab;
  }

  tool->details.last   = tab;
  tool->details.count += 1;

  return tab;
}

static void
detail_tab_set_record(search_tool_t *tool, detail_tab_t *tab, record_t *record, bool pinned)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));

  detail_tab_reset_keep_links(tab);

  tab->record = record;
  tab->pinned = pinned;

  detail_tab_build(tool, tab);
}

void
detail_tab_pin(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);

  if (!tab) {
    return;
  }

  tab->pinned = true;

  if (tool->details.preview_tab == tab) {
    tool->details.preview_tab = NULL;
  }
}

void
detail_tab_close(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(tool->details.count > 0);

  detail_tab_t *select_after = tab->prev ? tab->prev : tab->next;

  if (tool->details.preview_tab == tab) {
    tool->details.preview_tab = NULL;
  }

  if (tab->prev) {
    tab->prev->next = tab->next;
  }

  if (tab->next) {
    tab->next->prev = tab->prev;
  }

  if (tool->details.last == tab) {
    tool->details.last = tab->prev;
  }

  if (tool->details.first == tab) {
    tool->details.first = tab->next;
  }

  tool->details.count -= 1;

  if (tool->details.selected_tab == tab) {
    tool->details.selected_tab = tool->details.count > 0 ? select_after : NULL;
  }

  detail_tab_reset_storage(tab);

  tab->prev = NULL;
  tab->next = tool->details.free;

  tool->details.free = tab;
}

void
detail_tab_open(search_tool_t *tool, record_t *record, bool pinned)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));

  detail_tab_t *tab = detail_tab_find(tool, record);
  if (tab) {
    if (pinned) {
      detail_tab_pin(tool, tab);
    }

    tool->details.selected_tab = tab;
    return;
  }

  if (!pinned) {
    tab = tool->details.preview_tab;

    if (tab && tab->pinned) {
      tool->details.preview_tab = NULL;
      tab                       = NULL;
    }

    if (!tab) {
      tab = detail_tab_alloc(tool);
    }

    if (!tab) {
      return;
    }

    detail_tab_set_record(tool, tab, record, false);

    tool->details.preview_tab  = tab;
    tool->details.selected_tab = tab;
    return;
  }

  if (tool->details.count >= COUNTOF(tool->details.tabs) && tool->details.preview_tab) {
    detail_tab_close(tool, tool->details.preview_tab);
  }

  tab = detail_tab_alloc(tool);
  if (!tab) {
    return;
  }

  detail_tab_set_record(tool, tab, record, true);

  tool->details.selected_tab = tab;
}

void
detail_tab_next(search_tool_t *tool)
{
  ASSERT(tool);

  if (!tool->details.selected_tab) {
    return;
  }

  if (!tool->details.selected_tab->next) {
    tool->details.selected_tab = tool->details.first;
  } else {
    tool->details.selected_tab = tool->details.selected_tab->next;
  }
}

void
detail_tab_prev(search_tool_t *tool)
{
  ASSERT(tool);

  if (!tool->details.selected_tab) {
    return;
  }

  if (!tool->details.selected_tab->prev) {
    tool->details.selected_tab = tool->details.last;
  } else {
    tool->details.selected_tab = tool->details.selected_tab->prev;
  }
}

void
details_clear_stale(search_tool_t *tool)
{
  detail_tab_t *tab  = tool->details.first;
  detail_tab_t *next = NULL;

  while (tab) {
    next = tab->next;

    if (!tab->record || !record_has_flag(tab->record, RECORD_FLAG_LIVE)) {
      detail_tab_close(tool, tab);
    }

    tab = next;
  }
}


static record_t *
detail_record_from_uobject(search_tool_t *tool, uobject_t *obj)
{
  ASSERT(tool != NULL);

  if (!obj) {
    return NULL;
  }

  if (!unreal_uobject_is_valid(obj)) {
    return NULL;
  }

  if (obj->internal_idx < 0) {
    return NULL;
  }

  uint32_t slot = (uint32_t)obj->internal_idx;
  if (slot >= tool->cache.record_cap) {
    return NULL;
  }

  record_t *record = &tool->cache.records[slot];
  if (!record_has_flag(record, RECORD_FLAG_LIVE) || record->obj != obj) {
    return NULL;
  }

  return record;
}

static record_t *
detail_record_from_function_name(search_tool_t *tool, uobject_t *owner, fname_t func_name)
{
  ASSERT(tool != NULL);

  if (!owner || !unreal_uobject_is_valid(owner) || !owner->cls) {
    return NULL;
  }

  for (ustruct_t *s = (ustruct_t *)owner->cls; s; s = s->super_struct) {
    for (ufield_t *field = s->children; field; field = field->next) {
      uobject_t *field_obj = (uobject_t *)field;

      if (!unreal_fname_equal(field_obj->name, func_name, false)) {
        continue;
      }

      record_t *record = detail_record_from_uobject(tool, field_obj);
      if (record && record->kind == UOBJECT_KIND_FUNC) {
        return record;
      }
    }
  }

  return NULL;
}

static record_t *
detail_script_delegate_open_record(search_tool_t *tool, detail_fscript_delegate_t *delegate)
{
  ASSERT(tool != NULL);

  if (!delegate) {
    return NULL;
  }

  uobject_t *obj = unreal_fweak_object_resolve(delegate->object);
  if (!obj) {
    return NULL;
  }

  record_t *func_record = detail_record_from_function_name(tool, obj, delegate->function_name);
  if (func_record) {
    return func_record;
  }

  return detail_record_from_uobject(tool, obj);
}


static bool
tarray_children_match(detail_prop_t *node, int32_t num)
{
  if (!node) {
    return false;
  }

  return node->children_kind == DETAIL_PROP_CHILDREN_ARRAY_ELEMS && node->live_child_count == num && node->live_slot_count == num;
}

static bool
tset_children_match(detail_prop_t *node, const void *set, fprop_set_t *prop, int32_t live_count, int32_t slot_count)
{
  if (!node || node->children_kind != DETAIL_PROP_CHILDREN_SET_ELEMS ||
      node->live_child_count != live_count || node->live_slot_count != slot_count) {
    return false;
  }

  detail_prop_t *child = node->first_child;
  for (int32_t idx = 0; idx < slot_count; ++idx) {
    if (!unreal_set_is_valid_index(set, prop, idx)) {
      continue;
    }

    if (!child || child->live_slot_idx != idx) {
      return false;
    }

    child = child->next;
  }

  return child == NULL;
}

static bool
tmap_children_match(detail_prop_t *node, const void *map, fprop_map_t *prop, int32_t live_count, int32_t slot_count)
{
  if (!node || node->children_kind != DETAIL_PROP_CHILDREN_MAP_PAIRS ||
      node->live_child_count != live_count || node->live_slot_count != slot_count) {
    return false;
  }

  detail_prop_t *child = node->first_child;
  for (int32_t idx = 0; idx < slot_count; ++idx) {
    if (!unreal_map_is_valid_index(map, prop, idx)) {
      continue;
    }

    if (!child || child->live_slot_idx != idx) {
      return false;
    }

    child = child->next;
  }

  return child == NULL;
}

static bool
mcast_delegate_children_match(detail_prop_t *node, int32_t num)
{
  if (!node) {
    return false;
  }

  return node->children_kind == DETAIL_PROP_CHILDREN_MCAST_DELEGATE && node->live_child_count == num && node->live_slot_count == num;
}

static void
tarray_rebuild_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_array_t *array_prop, void *array, int32_t num, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);
  ASSERT(array_prop != NULL);
  ASSERT(array_prop->inner != NULL);

  detail_prop_clear_runtime_children(tab, node);

  node->children_kind    = DETAIL_PROP_CHILDREN_ARRAY_ELEMS;
  node->live_child_count = num;
  node->live_slot_count  = num;

  if (num <= 0) {
    return;
  }

  fprop_t *inner = array_prop->inner;

  int32_t elem_size = inner->elem_size;
  ASSERT(elem_size > 0);

  for (int32_t i = 0; i < num; ++i) {
    int32_t  elem_offset = i * elem_size;
    uint8_t *elem_addr   = (uint8_t *)unreal_array_get(array, array_prop, i);

    detail_prop_t *elem = detail_prop_push_array_elem(tool, tab, inner, i, elem_addr, elem_offset, depth + 1, false, true);
    if (elem) {
      QUEUE_PUSH(node->first_child, node->last_child, elem);
    }
  }
}

static void
tset_rebuild_children(
  search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_set_t *set_prop, void *set, int32_t live_count, int32_t slot_count, int32_t depth)
{
  detail_prop_clear_runtime_children(tab, node);
  node->children_kind    = DETAIL_PROP_CHILDREN_SET_ELEMS;
  node->live_child_count = live_count;
  node->live_slot_count  = slot_count;

  if (slot_count <= 0 || live_count <= 0) {
    return;
  }

  fprop_t *set_elem = set_prop->elem_prop;

  int32_t slot_size = set_prop->set_layout.sparse_array_layout.size;
  ASSERT(slot_size > 0);

  for (int32_t i = 0; i < slot_count; ++i) {
    if (!unreal_set_is_valid_index(set, set_prop, i)) {
      continue;
    }

    int32_t  elem_offset = i * slot_size;
    uint8_t *elem_addr   = (uint8_t *)unreal_set_get(set, set_prop, i);

    detail_prop_t *elem = detail_prop_push_set_elem(tool, tab, set_elem, i, elem_addr, elem_offset, depth + 1, false, true);
    if (elem) {
      elem->live_slot_idx = i;
      QUEUE_PUSH(node->first_child, node->last_child, elem);
    }
  }
}

static void
tmap_rebuild_children(
  search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_map_t *map_prop, void *map, int32_t live_count, int32_t slot_count, int32_t depth)
{
  detail_prop_clear_runtime_children(tab, node);
  node->children_kind    = DETAIL_PROP_CHILDREN_MAP_PAIRS;
  node->live_child_count = live_count;
  node->live_slot_count  = slot_count;

  if (slot_count <= 0 || live_count <= 0) {
    return;
  }

  int32_t slot_size = map_prop->map_layout.set_layout.sparse_array_layout.size;
  ASSERT(slot_size > 0);

  for (int32_t i = 0; i < slot_count; ++i) {
    if (!unreal_map_is_valid_index(map, map_prop, i)) {
      continue;
    }

    int32_t  pair_offset = i * slot_size;
    uint8_t *key_addr    = (uint8_t *)unreal_map_get_key(map, map_prop, i);
    uint8_t *val_addr    = (uint8_t *)unreal_map_get_value(map, map_prop, i);

    detail_prop_t *pair = detail_prop_push_map_pair(tool, tab, map_prop, i, key_addr, val_addr, pair_offset, depth + 1, false, true);
    if (pair) {
      pair->live_slot_idx = i;
      QUEUE_PUSH(node->first_child, node->last_child, pair);
    }
  }
}

static void
mcast_delegate_rebuild_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, detail_fscript_delegate_array_t *list, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);
  ASSERT(list != NULL);

  detail_prop_clear_runtime_children(tab, node);

  node->children_kind    = DETAIL_PROP_CHILDREN_MCAST_DELEGATE;
  node->live_child_count = list->num;
  node->live_slot_count  = list->num;

  for (int32_t i = 0; i < list->num; ++i) {
    detail_prop_t *elem = detail_prop_alloc_runtime(tab);
    if (!elem) {
      break;
    }

    elem->prop          = NULL;
    elem->children_kind = DETAIL_PROP_CHILDREN_NONE;
    elem->live_slot_idx = i;
    elem->offset        = i * (int32_t)sizeof(detail_fscript_delegate_t);
    elem->size          = (int32_t)sizeof(detail_fscript_delegate_t);
    elem->array_dim     = 1;

    QUEUE_PUSH(node->first_child, node->last_child, elem);
  }

  UNUSED_VAR(depth);
}

static record_t *
detail_prop_open_record(search_tool_t *tool, detail_prop_t *node)
{
  ASSERT(tool != NULL);

  if (!node || !node->prop || !node->has_value_addr || !node->value_addr) {
    return NULL;
  }

  fprop_t *prop = node->prop;
  void    *addr = node->value_addr;

  switch (unreal_fprop_get_kind(prop)) {
    case UNREAL_PROP_KIND_OBJECT:
    case UNREAL_PROP_KIND_CLASS:
    case UNREAL_PROP_KIND_WEAK_OBJECT:
    case UNREAL_PROP_KIND_LAZY_OBJECT:
    case UNREAL_PROP_KIND_SOFT_OBJECT:
    case UNREAL_PROP_KIND_SOFT_CLASS:
    case UNREAL_PROP_KIND_INTERFACE: {
      return node->size >= prop->elem_size && prop->elem_size > 0
             ? detail_record_from_uobject(tool, unreal_fprop_get_referenced_object(prop, addr))
             : NULL;
    }

    case UNREAL_PROP_KIND_DELEGATE: {
      return node->size >= (int32_t)sizeof(detail_fscript_delegate_t)
             ? detail_script_delegate_open_record(tool, (detail_fscript_delegate_t *)addr)
             : NULL;
    }

    default: {
      return NULL;
    }
  }
}

static void
detail_prop_fill_value_tarray_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_array_t *array_prop, int32_t depth);
static void
detail_prop_fill_value_tset_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_set_t *set_prop, int32_t depth);
static void
detail_prop_fill_value_tmap_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_map_t *map_prop, int32_t depth);
static void
detail_prop_fill_value_mcast_delegate_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t*node, int32_t depth);
static void
detail_prop_fill_value_sparse_mcast_delegate_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, int32_t depth);
static void
detail_prop_fill_value_direct(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, int32_t depth);
static void
detail_prop_fill_value_from_base(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *first, uint8_t *base, int32_t depth);

static void
detail_prop_refresh_runtime_text(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, str_t name)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->runtime_node) {
    return;
  }

  arena_t *arena = tab->value_arena;

  if (!str_is_empty(name)) {
    node->name = ui_text_span_make(tool->ctx, name);
  }

  if (node->prop) {
    node->type = ui_text_span_make(tool->ctx, unreal_fprop_push_type_name(node->prop, arena));
  }

  node->offset_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->offset));
  node->size_text   = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)node->size));
}

static void
detail_prop_invalidate_value_tree(detail_prop_t *node)
{
  if (!node) {
    return;
  }

  node->has_value_addr = false;
  node->value_addr     = NULL;
  node->record_to_open = NULL;

  for (detail_prop_t *child = node->first_child; child; child = child->next) {
    detail_prop_invalidate_value_tree(child);
  }
}

void
detail_prop_fill_value_tarray_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_array_t *array_prop, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->has_value_addr) {
    return;
  }

  arena_t *arena = tab->value_arena;

  ASSERT(node->value_addr != NULL);
  ASSERT(array_prop != NULL);
  ASSERT(array_prop->inner != NULL);

  void   *array = node->value_addr;
  int32_t num   = unreal_array_num(array, array_prop);
  node->summary = ui_text_span_make(tool->ctx, unreal_fprop_push_value_summary(&array_prop->base, array, node->size, arena));

  if (num < 0) {
    detail_prop_clear_runtime_children(tab, node);
    node->children_kind    = DETAIL_PROP_CHILDREN_ARRAY_ELEMS;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->maximized        = false;
    return;
  }

  if (num == 0) {
    detail_prop_clear_runtime_children(tab, node);
    node->children_kind    = DETAIL_PROP_CHILDREN_ARRAY_ELEMS;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->maximized        = false;
    return;
  }

  if (!node->maximized && !tab->force_expand_values) {
    node->children_kind    = DETAIL_PROP_CHILDREN_ARRAY_ELEMS;
    node->live_child_count = num;
    return;
  }

  if (!tarray_children_match(node, num)) {
    tarray_rebuild_children(tool, tab, node, array_prop, array, num, depth);
  }

  int elem_size = array_prop->inner->elem_size;
  ASSERT(elem_size > 0);

  int elem_idx = 0;
  for (detail_prop_t *elem = node->first_child; elem; elem = elem->next) {
    if (!elem->prop) {
      continue;
    }

    if (elem_idx >= num) {
      break;
    }

    int elem_offset = elem_idx * elem_size;

    elem->offset         = elem_offset;
    elem->size           = elem_size;
    elem->value_addr     = unreal_array_get(array, array_prop, elem_idx);
    elem->has_value_addr = elem->value_addr != NULL;

    if (!elem->has_value_addr) {
      detail_prop_invalidate_value_tree(elem);
      elem->summary = ui_text_span_make(tool->ctx, STR_LIT("<stale>"));
      elem_idx += 1;
      continue;
    }

    detail_prop_refresh_runtime_text(tool, tab, elem, str_push_fmt(tab->value_arena, "[%d]", elem_idx));
    detail_prop_fill_value_direct(tool, tab, elem, depth + 1);

    elem_idx += 1;
  }
}

void
detail_prop_fill_value_tset_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_set_t *set_prop, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->has_value_addr) {
    return;
  }

  arena_t *arena = tab->value_arena;

  ASSERT(node->value_addr != NULL);
  ASSERT(set_prop != NULL);
  ASSERT(set_prop->elem_prop != NULL);

  void   *set        = node->value_addr;
  int32_t live_count = unreal_set_num(set, set_prop);
  int32_t slot_count = unreal_set_max_index(set, set_prop);
  node->summary      = ui_text_span_make(tool->ctx, unreal_fprop_push_value_summary(&set_prop->base, set, node->size, arena));

  if (live_count < 0 || slot_count < 0) {
    detail_prop_clear_runtime_children(tab, node);
    node->children_kind    = DETAIL_PROP_CHILDREN_SET_ELEMS;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->maximized        = false;
    return;
  }

  if (slot_count == 0 || live_count == 0) {
    detail_prop_clear_runtime_children(tab, node);
    node->children_kind    = DETAIL_PROP_CHILDREN_SET_ELEMS;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->maximized        = false;
    return;
  }

  if (!node->maximized && !tab->force_expand_values) {
    node->children_kind    = DETAIL_PROP_CHILDREN_SET_ELEMS;
    node->live_child_count = live_count;
    return;
  }

  if (!tset_children_match(node, set, set_prop, live_count, slot_count)) {
    tset_rebuild_children(tool, tab, node, set_prop, set, live_count, slot_count, depth);
  }

  int elem_size = set_prop->elem_prop->elem_size;
  int slot_size = set_prop->set_layout.sparse_array_layout.size;
  ASSERT(elem_size > 0);
  ASSERT(slot_size > 0);

  for (detail_prop_t *elem = node->first_child; elem; elem = elem->next) {
    if (!elem->prop) {
      continue;
    }

    int slot_idx = elem->live_slot_idx;
    ASSERT(slot_idx >= 0);
    ASSERT(slot_idx < slot_count);

    elem->value_addr     = (void *)unreal_set_get(set, set_prop, slot_idx);
    elem->has_value_addr = elem->value_addr != NULL;
    if (!elem->has_value_addr) {
      detail_prop_invalidate_value_tree(elem);
      elem->summary = ui_text_span_make(tool->ctx, STR_LIT("<stale>"));
      continue;
    }

    int elem_offset = slot_idx * slot_size;

    elem->offset         = elem_offset;
    elem->size           = elem_size;

    detail_prop_refresh_runtime_text(tool, tab, elem, str_push_fmt(tab->value_arena, "[%d]", slot_idx));
    detail_prop_fill_value_direct(tool, tab, elem, depth + 1);
  }
}

void
detail_prop_fill_value_tmap_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, fprop_map_t *map_prop, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->has_value_addr) {
    return;
  }

  arena_t *arena = tab->value_arena;

  ASSERT(node->value_addr != NULL);
  ASSERT(map_prop != NULL);

  void   *map        = node->value_addr;
  int32_t live_count = unreal_map_num(map, map_prop);
  int32_t slot_count = unreal_map_max_index(map, map_prop);
  node->summary      = ui_text_span_make(tool->ctx, unreal_fprop_push_value_summary(&map_prop->base, map, node->size, arena));

  if (live_count < 0 || slot_count < 0) {
    detail_prop_clear_runtime_children(tab, node);
    node->children_kind    = DETAIL_PROP_CHILDREN_MAP_PAIRS;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->maximized        = false;
    return;
  }

  if (slot_count == 0 || live_count == 0) {
    detail_prop_clear_runtime_children(tab, node);
    node->children_kind    = DETAIL_PROP_CHILDREN_MAP_PAIRS;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->maximized        = false;
    return;
  }

  if (!node->maximized && !tab->force_expand_values) {
    node->children_kind    = DETAIL_PROP_CHILDREN_MAP_PAIRS;
    node->live_child_count = live_count;
    return;
  }

  if (!tmap_children_match(node, map, map_prop, live_count, slot_count)) {
    tmap_rebuild_children(tool, tab, node, map_prop, map, live_count, slot_count, depth);
  }

  int pair_size = map_prop->map_layout.set_layout.size;
  int slot_size = map_prop->map_layout.set_layout.sparse_array_layout.size;
  ASSERT(pair_size > 0);
  ASSERT(slot_size > 0);

  for (detail_prop_t *pair = node->first_child; pair; pair = pair->next) {
    if (!pair->first_child) {
      continue;
    }

    int slot_idx = pair->live_slot_idx;
    ASSERT(slot_idx >= 0);
    ASSERT(slot_idx < slot_count);

    uint8_t *key_addr = (uint8_t *)unreal_map_get_key(map, map_prop, slot_idx);
    uint8_t *val_addr = (uint8_t *)unreal_map_get_value(map, map_prop, slot_idx);
    if (!key_addr || !val_addr) {
      detail_prop_invalidate_value_tree(pair);
      pair->summary = ui_text_span_make(tool->ctx, STR_LIT("<stale>"));
      continue;
    }

    int pair_offset = slot_idx * slot_size;

    pair->offset         = pair_offset;
    pair->size           = pair_size;
    pair->has_value_addr = true;
    pair->value_addr     = key_addr;
    pair->summary        = ui_text_span_make(tool->ctx, STR_LIT("{...}"));

    {
      str_t key_type_name  = unreal_fprop_push_type_name(map_prop->key_prop, arena);
      str_t val_type_name  = unreal_fprop_push_type_name(map_prop->val_prop, arena);
      str_t pair_type_name = str_push_fmt(arena, "TPair<%.*s, %.*s>", STR_ARG(key_type_name), STR_ARG(val_type_name));

      pair->name        = ui_text_span_make(tool->ctx, str_push_fmt(arena, "[%d]", slot_idx));
      pair->type        = ui_text_span_make(tool->ctx, pair_type_name);
      pair->offset_text = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)pair->offset));
      pair->size_text   = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)pair->size));
    }

    if (pair->first_child && pair->first_child->prop) {
      detail_prop_t *key = pair->first_child;

      key->offset         = 0;
      key->size           = map_prop->key_prop->elem_size;
      key->has_value_addr = true;
      key->value_addr     = key_addr;

      detail_prop_refresh_runtime_text(tool, tab, key, STR_LIT("Key"));
      detail_prop_fill_value_direct(tool, tab, key, depth + 2);

      if (key->next && key->next->prop) {
        detail_prop_t *val = key->next;

        val->offset         = map_prop->map_layout.value_offset;
        val->size           = map_prop->val_prop->elem_size;
        val->has_value_addr = true;
        val->value_addr     = val_addr;

        detail_prop_refresh_runtime_text(tool, tab, val, STR_LIT("Value"));
        detail_prop_fill_value_direct(tool, tab, val, depth + 2);
      }
    }
  }
}

void
detail_prop_fill_value_mcast_delegate_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->has_value_addr) {
    return;
  }

  arena_t *arena = tab->value_arena;

  detail_fscript_delegate_array_t *list = (detail_fscript_delegate_array_t *)node->value_addr;

  int32_t num = list->num;
  int32_t max = list->max;

  ASSERT(num <= max);

  node->summary = ui_text_span_make(tool->ctx, str_push_fmt(arena, "Num=%d Max=%d Data=%p", num, max, list->data));

  if (!list->data || num <= 0 || max <= 0) {
    detail_prop_clear_runtime_children(tab, node);

    node->children_kind    = DETAIL_PROP_CHILDREN_MCAST_DELEGATE;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->summary          = ui_text_span_make(tool->ctx, str_push_fmt(arena, "Num=%d Max=%d Data=%p", num, max, list->data));
    node->maximized        = false;
    return;
  }

  if (!node->maximized && !tab->force_expand_values) {
    node->children_kind    = DETAIL_PROP_CHILDREN_MCAST_DELEGATE;
    node->live_child_count = num;
    return;
  }

  if (!mcast_delegate_children_match(node, num)) {
    mcast_delegate_rebuild_children(tool, tab, node, list, depth);
  }

  for (detail_prop_t *elem = node->first_child; elem; elem = elem->next) {
    int32_t idx = elem->live_slot_idx;

    if (idx < 0 || idx >= num) {
      continue;
    }

    detail_fscript_delegate_t *d = list->data + idx;

    elem->offset         = idx * (int32_t)sizeof(*d);
    elem->size           = (int32_t)sizeof(*d);
    elem->has_value_addr = true;
    elem->value_addr     = (uint8_t *)d;

    elem->name           = ui_text_span_make(tool->ctx, str_push_fmt(arena, "[%d]", idx));
    elem->type           = ui_text_span_make(tool->ctx, STR_LIT("DelegateBinding"));
    elem->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)elem->offset));
    elem->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)elem->size));
    elem->summary        = ui_text_span_make(tool->ctx, unreal_fscript_delegate_push_summary((fscript_delegate_t *)d, arena));
    elem->record_to_open = detail_script_delegate_open_record(tool, d);
  }

  UNUSED_VAR(depth);
}

void
detail_prop_fill_value_sparse_mcast_delegate_children(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->has_value_addr || !node->prop) {
    return;
  }

  arena_t                         *arena       = tab->value_arena;
  uobject_t                       *value_owner = tab->record ? tab->record->obj : NULL;
  detail_fscript_delegate_array_t *list        = unreal_get_mcast_sparse_delegate(value_owner, node->prop->name);

  if (!list) {
    detail_prop_clear_runtime_children(tab, node);

    node->children_kind    = DETAIL_PROP_CHILDREN_MCAST_DELEGATE;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->summary          = ui_text_span_make(tool->ctx, str_push_fmt(arena, "<unbound>"));
    node->maximized        = false;
    return;
  }

  int32_t num = list->num;
  int32_t max = list->max;

  if (!list->data || num <= 0 || max <= 0) {
    detail_prop_clear_runtime_children(tab, node);

    node->children_kind    = DETAIL_PROP_CHILDREN_MCAST_DELEGATE;
    node->live_child_count = 0;
    node->live_slot_count  = 0;
    node->summary          = ui_text_span_make(tool->ctx, str_push_fmt(arena, "Num=%d Max=%d Data=%p", num, max, list->data));
    node->maximized        = false;
    return;
  }

  ASSERT(num <= max);

  node->summary = ui_text_span_make(tool->ctx, str_push_fmt(arena, "Num=%d Max=%d Data=%p", num, max, list->data));

  if (!node->maximized && !tab->force_expand_values) {
    node->children_kind    = DETAIL_PROP_CHILDREN_MCAST_DELEGATE;
    node->live_child_count = num;
    return;
  }

  if (!mcast_delegate_children_match(node, num)) {
    mcast_delegate_rebuild_children(tool, tab, node, list, depth);
  }

  for (detail_prop_t *elem = node->first_child; elem; elem = elem->next) {
    int32_t idx = elem->live_slot_idx;

    if (idx < 0 || idx >= num) {
      continue;
    }

    detail_fscript_delegate_t *d = &list->data[idx];

    elem->offset         = idx * (int32_t)sizeof(*d);
    elem->size           = (int32_t)sizeof(*d);
    elem->has_value_addr = true;
    elem->value_addr     = (uint8_t *)d;

    elem->name           = ui_text_span_make(tool->ctx, str_push_fmt(arena, "[%d]", idx));
    elem->type           = ui_text_span_make(tool->ctx, STR_LIT("DelegateBinding"));
    elem->offset_text    = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)elem->offset));
    elem->size_text      = ui_text_span_make(tool->ctx, str_push_fmt(arena, "0x%04X", (uint32_t)elem->size));
    elem->summary        = ui_text_span_make(tool->ctx, unreal_fscript_delegate_push_summary((fscript_delegate_t *)d, arena));
    elem->record_to_open = detail_script_delegate_open_record(tool, d);
  }
}

void
detail_prop_fill_value_direct(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *node, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(node != NULL);

  if (!node->has_value_addr || node->is_param) {
    return;
  }

  node->record_to_open = NULL;
  node->summary        = ui_text_span_make(tool->ctx, STR_NULL);

  switch (unreal_fprop_get_kind(node->prop)) {
    case UNREAL_PROP_KIND_ARRAY: {
      detail_prop_fill_value_tarray_children(tool, tab, node, (fprop_array_t *)node->prop, depth);
      break;
    }

    case UNREAL_PROP_KIND_SET: {
      detail_prop_fill_value_tset_children(tool, tab, node, (fprop_set_t *)node->prop, depth);
      break;
    }

    case UNREAL_PROP_KIND_MAP: {
      detail_prop_fill_value_tmap_children(tool, tab, node, (fprop_map_t *)node->prop, depth);
      break;
    }

    case UNREAL_PROP_KIND_MULTICAST_DELEGATE:
    case UNREAL_PROP_KIND_MULTICAST_INLINE_DELEGATE: {
      detail_prop_fill_value_mcast_delegate_children(tool, tab, node, depth);
      break;
    }

    case UNREAL_PROP_KIND_MULTICAST_SPARSE_DELEGATE: {
      detail_prop_fill_value_sparse_mcast_delegate_children(tool, tab, node, depth);
      break;
    }

    default: {
      node->summary        = ui_text_span_make(tool->ctx, unreal_fprop_push_value_summary(node->prop, node->value_addr, node->size, tab->value_arena));
      node->record_to_open = detail_prop_open_record(tool, node);

      if (node->first_child && (node->maximized || tab->force_expand_values)) {
        detail_prop_fill_value_from_base(tool, tab, node->first_child, node->value_addr, depth + 1);
      }
      break;
    }
  }

  if (node->first_child && str_is_empty(node->summary.str)) {
    node->summary = ui_text_span_make(tool->ctx, STR_LIT("{...}"));
  }
}

void
detail_prop_fill_value_from_base(search_tool_t *tool, detail_tab_t *tab, detail_prop_t *first, uint8_t *base, int32_t depth)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);

  if (!first || !base) {
    return;
  }

  for (detail_prop_t *prop = first; prop; prop = prop->next) {
    if (!prop->prop) {
      continue;
    }

    if (prop->is_param) {
      continue;
    }

    prop->value_addr     = unreal_fprop_value_in_container(prop->prop, base, 0);
    prop->has_value_addr = prop->value_addr != NULL;

    if (!prop->has_value_addr) {
      detail_prop_invalidate_value_tree(prop);
      prop->summary = ui_text_span_make(tool->ctx, STR_LIT("<invalid layout>"));
      continue;
    }

    if (prop->runtime_node) {
      detail_prop_refresh_runtime_text(tool, tab, prop, unreal_fname_to_str(prop->prop->name, tab->value_arena));
    }

    detail_prop_fill_value_direct(tool, tab, prop, depth);
  }
}

static void
details_fill_owner_values(search_tool_t *tool, detail_tab_t *tab)
{
  for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
    if (!owner->first_prop) {
      continue;
    }

    detail_prop_fill_value_from_base(tool, tab, owner->first_prop, tab->value_base, 0);
  }
}

static void
details_refresh_values(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tab != NULL);

  arena_reset(tab->value_arena);
  details_fill_owner_values(tool, tab);
}

static void
details_refresh_all_values(search_tool_t *tool, detail_tab_t *tab)
{
  bool old_force_expand    = tab->force_expand_values;
  tab->force_expand_values = true;
  details_refresh_values(tool, tab);
  tab->force_expand_values = old_force_expand;
}

static void
detail_dump_push_line(arena_t *arena, str_list_t *lines, int depth, str_t text)
{
  ASSERT(arena != NULL);
  ASSERT(lines != NULL);

  if (str_is_empty(text)) {
    str_list_push(arena, lines, STR_NULL);
    return;
  }

  uint64_t indent_len = (uint64_t)MAX_VAL(depth, 0) * 2;
  uint64_t size       = indent_len + text.len;
  uint8_t *buf        = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, size + 1);
  if (buf) {
    if (indent_len > 0) {
      mem_set(&buf[0], ' ', indent_len);
    }
    mem_copy(&buf[indent_len], text.data, text.len);
    buf[size] = '\0';

    str_list_push(arena, lines, str_make(buf, size));
  }
}

static void
detail_dump_push_vfmt(arena_t *arena, str_list_t *lines, int depth, const char *fmt, va_list args)
{
  va_list args2;
  va_copy(args2, args);
  int needed = stbsp_vsnprintf(NULL, 0, fmt, args2);
  va_end(args2);

  str_t line = STR_NULL;
  if (needed > 0) {
    uint64_t indent_len = (uint64_t)MAX_VAL(depth, 0) * 2;
    uint64_t cap        = indent_len + (uint64_t)needed + 1; // vsnprintf expects a room for nul-terminator
    uint8_t *data       = ARENA_PUSH_ARRAY(arena, uint8_t, cap);
    if (data) {
      stbsp_vsnprintf((char *)&data[indent_len], needed + 1, fmt, args);

      if (indent_len > 0) {
        mem_set(&data[0], ' ', indent_len);
      }

      line = str_make(data, indent_len + (uint64_t)needed);
    }
  }

  str_list_push(arena, lines, line);
}

static void
detail_dump_push_fmt(arena_t *arena, str_list_t *lines, int depth, const char *fmt, ...)
{
  ASSERT(arena != NULL);
  ASSERT(lines != NULL);
  ASSERT(fmt != NULL);

  va_list args;
  va_start(args, fmt);
  detail_dump_push_vfmt(arena, lines, depth, fmt, args);
  va_end(args);
}

static void
detail_dump_prop_list(arena_t *arena, str_list_t *lines, detail_prop_t *first, int depth)
{
  ASSERT(arena != NULL);
  ASSERT(lines != NULL);

  for (detail_prop_t *prop = first; prop; prop = prop->next) {
    detail_dump_push_fmt(arena,
                         lines,
                         depth,
                         "%.*s  %.*s  %.*s  %.*s  %.*s",
                         STR_ARG(prop->offset_text.str),
                         STR_ARG(prop->size_text.str),
                         STR_ARG(prop->type.str),
                         STR_ARG(prop->name.str),
                         STR_ARG(prop->summary.str));

    if (prop->first_child) {
      detail_dump_prop_list(arena, lines, prop->first_child, depth + 1);
    }
  }
}

static str_t
detail_props_push_dump(search_tool_t *tool, detail_tab_t *tab, arena_t *arena)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(arena != NULL);

  details_refresh_all_values(tool, tab);

  str_list_t lines = {0};

  detail_dump_push_fmt(arena,
                       &lines,
                       0,
                       "%.*s  %.*s  %.*s  %.*s  %.*s",
                       STR_ARG(tab->offset_col.str),
                       STR_ARG(tab->size_col.str),
                       STR_ARG(tab->type_col.str),
                       STR_ARG(tab->name_col.str),
                       STR_ARG(tab->val_col.str));

  for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
    int prop_count = 0;
    for (detail_prop_t *prop = owner->first_prop; prop; prop = prop->next) {
      prop_count += 1;
    }

    detail_dump_push_fmt(arena, &lines, 0, "%.*s (%d):", STR_ARG(owner->record->name.str), prop_count);

    if (owner->first_prop) {
      detail_dump_prop_list(arena, &lines, owner->first_prop, 1);
    } else {
      detail_dump_push_line(arena, &lines, 1, tab->no_props.str);
    }
  }

  return str_list_join(arena, lines, STR_NULL, STR_LIT("\r\n"), STR_NULL);
}

static str_t
detail_push_single_func_dump(search_tool_t *tool, detail_tab_t *tab, arena_t *arena)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(tab->function != NULL);
  ASSERT(arena != NULL);

  str_list_t lines = {0};
  detail_dump_push_fmt(arena,
                       &lines,
                       0,
                       "%.*s  %.*s  %.*s  %.*s  %.*s",
                       STR_ARG(tab->offset_col.str),
                       STR_ARG(tab->size_col.str),
                       STR_ARG(tab->type_col.str),
                       STR_ARG(tab->name_col.str),
                       STR_ARG(tab->flags_col.str));

  if (tab->function->first_param) {
    detail_dump_prop_list(arena, &lines, tab->function->first_param, 1);
  } else {
    detail_dump_push_line(arena, &lines, 1, tab->no_params.str);
  }

  return str_list_join(arena, lines, STR_NULL, STR_LIT("\r\n"), STR_NULL);
}

static str_t
detail_push_funcs_dump(arena_t *arena, detail_tab_t *tab)
{
  ASSERT(arena != NULL);
  ASSERT(tab != NULL);

  str_list_t lines = {0};

  detail_dump_push_fmt(arena,
                       &lines,
                       0,
                       "%.*s  %.*s  %.*s  %.*s  %.*s",
                       STR_ARG(tab->offset_col.str),
                       STR_ARG(tab->size_col.str),
                       STR_ARG(tab->type_col.str),
                       STR_ARG(tab->name_col.str),
                       STR_ARG(tab->flags_col.str));

  for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
    detail_dump_push_fmt(arena, &lines, 0, "%.*s  %.*s  %.*s", STR_ARG(tab->owner_hbar.str), STR_ARG(owner->record->name.str), STR_ARG(tab->owner_hbar.str));

    if (!owner->first_func) {
      detail_dump_push_line(arena, &lines, 1, tab->no_funcs.str);
      continue;
    }

    for (detail_func_t *func = owner->first_func; func; func = func->next) {
      detail_dump_push_line(arena, &lines, 1, func->record->name.str);

      detail_dump_push_fmt(arena,
                           &lines,
                           2,
                           "%.*s  %.*s  %.*s  %.*s  %.*s",
                           STR_ARG(tab->offset_col.str),
                           STR_ARG(tab->size_col.str),
                           STR_ARG(tab->type_col.str),
                           STR_ARG(tab->name_col.str),
                           STR_ARG(tab->flags_col.str));

      if (func->first_param) {
        detail_dump_prop_list(arena, &lines, func->first_param, 2);
      } else {
        detail_dump_push_line(arena, &lines, 2, tab->no_params.str);
      }
    }
  }

  return str_list_join(arena, lines, STR_NULL, STR_LIT("\r\n"), STR_NULL);
}

static str_t
detail_push_instances_dump(arena_t *arena, search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(arena != NULL);
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(tab->record != NULL);

  str_list_t lines = {0};

  detail_dump_push_fmt(arena, &lines, 0, "%.*s  %.*s", STR_ARG(tab->type_col.str), STR_ARG(tab->name_col.str));

  bool any = false;
  for (record_t *instance = record_from_slot(tool, tab->record->first_instance_slot); instance; instance = record_from_slot(tool, instance->next_instance_slot)) {
    if (!record_has_flag(instance, RECORD_FLAG_LIVE)) {
      continue;
    }

    if (!cache_names(tool, instance)) {
      continue;
    }

    record_t *type_record = record_from_slot(tool, instance->type_slot);
    if (!type_record || !record_has_flag(type_record, RECORD_FLAG_LIVE)) {
      continue;
    }

    if (!cache_name(tool, type_record)) {
      continue;
    }

    detail_dump_push_fmt(arena, &lines, 0, "%.*s  %.*s", STR_ARG(type_record->name.str), STR_ARG(instance->name.str));
    any = true;
  }

  if (!any) {
    detail_dump_push_line(arena, &lines, 0, tab->no_instances.str);
  }

  return str_list_join(arena, lines, STR_NULL, STR_LIT("\r\n"), STR_NULL);
}

static void
detail_push_package_children_dump(arena_t *arena, search_tool_t *tool, str_list_t *lines, record_t *record, int depth)
{
  ASSERT(arena != NULL);
  ASSERT(tool != NULL);
  ASSERT(lines != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));

  for (record_t *child = record_from_slot(tool, record->first_child_slot); child; child = record_from_slot(tool, child->next_sibling_slot)) {
    if (!record_has_flag(child, RECORD_FLAG_LIVE)) {
      continue;
    }

    if (!cache_names(tool, child)) {
      continue;
    }

    record_t *type_record = record_from_slot(tool, child->type_slot);
    if (!type_record || !record_has_flag(type_record, RECORD_FLAG_LIVE)) {
      continue;
    }

    if (!cache_name(tool, type_record)) {
      continue;
    }

    detail_dump_push_fmt(arena, lines, depth, "%.*s  %.*s", STR_ARG(type_record->name.str), STR_ARG(child->name.str));

    if (child->first_child_slot != RECORD_SLOT_INVALID) {
      detail_push_package_children_dump(arena, tool, lines, child, depth + 1);
    }
  }
}

static str_t
detail_push_package_dump(arena_t *arena, search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(arena != NULL);
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(tab->record != NULL);

  str_list_t lines = {0};

  detail_dump_push_fmt(arena, &lines, 0, "%.*s  %.*s", STR_ARG(tab->type_col.str), STR_ARG(tab->name_col.str));

  if (tab->record->first_child_slot != RECORD_SLOT_INVALID) {
    detail_push_package_children_dump(arena, tool, &lines, tab->record, 0);
  } else {
    detail_dump_push_line(arena, &lines, 0, STR_LIT("No children"));
  }

  return str_list_join(arena, lines, STR_NULL, STR_LIT("\r\n"), STR_NULL);
}

static str_t
detail_push_enum_dump(arena_t *arena, search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(arena != NULL);
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);
  ASSERT(tab->enumeration != NULL);

  detail_enum_t *e     = tab->enumeration;
  str_list_t     lines = {0};

  detail_dump_push_fmt(arena, &lines, 0, "%.*s  %.*s", STR_ARG(e->enum_name_col.str), STR_ARG(e->enum_value_col.str));
  if (!e->first_entry) {
    detail_dump_push_fmt(arena, &lines, 0, "%.*s", STR_ARG(tab->no_entries.str));
  } else {
    for (detail_enum_entry_t *entry = e->first_entry; entry; entry = entry->next) {
      detail_dump_push_fmt(arena, &lines, 0, "%.*s  %.*s", STR_ARG(entry->name.str), STR_ARG(entry->value_text.str));
    }
  }

  return str_list_join(arena, lines, STR_NULL, STR_LIT("\r\n"), STR_NULL);
}

static void
draw_details_header_line(search_tool_t *tool, struct nk_grid_state *grid_state,
                         ui_text_cell_t key, ui_text_cell_t val, record_t *record_to_open)
{
  static const struct nk_grid_column columns[] = {
    {.sizing = NK_GRID_COLUMN_CONTENT},
    {.sizing = NK_GRID_COLUMN_CONTENT},
  };
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(grid_state != NULL);

  struct nk_context     *ctx          = tool->ctx;
  struct nk_rect         row_bounds   = {0};
  struct nk_grid_options grid_options = {
    .row_height = 20.0f,
    .column_gap = ctx->style.window.spacing.x,
  };

  struct nk_text_options key_options = {
    .alignment = key.align,
    .flags     = NK_TEXT_OPTION_SELECTABLE,
    .color     = key.fg,
  };

  struct nk_text_options val_options = {
    .alignment = val.align,
    .flags     = NK_TEXT_OPTION_SELECTABLE,
    .color     = val.fg,
  };

  struct nk_grid grid;

  nk_grid_begin(ctx, &grid, grid_state, columns, 2, &grid_options);
  if (nk_grid_row_begin(&grid)) {
    struct nk_rect key_bounds = nk_grid_cell_bounds(&grid, 0, nk_vec2(0.0f, 0.0f));
    struct nk_rect val_bounds = nk_grid_cell_bounds(&grid, 1, nk_vec2(0.0f, 0.0f));

    row_bounds.x = key_bounds.x;
    row_bounds.y = key_bounds.y;
    row_bounds.w = val_bounds.x + val_bounds.w - key_bounds.x;
    row_bounds.h = NK_MAX(key_bounds.h, val_bounds.h);

  }
  ui_grid_str(&grid, 0, key.text.str, &key_options);
  ui_grid_str(&grid, 1, val.text.str, &val_options);
  nk_grid_row_end(&grid);
  nk_grid_end(&grid);

  if (nk_contextual_begin(ctx, NK_WINDOW_BORDER, nk_vec2(190.0f, 92.0f), row_bounds)) {
    if (!str_is_empty(val.text.str)) {
      nk_layout_row_dynamic(ctx, 20.0f, 1);
      if (nk_contextual_item_label(ctx, "Copy value", NK_TEXT_LEFT)) {
        detail_copy_text(ctx, val.text.str);
      }
    }

    nk_layout_row_dynamic(ctx, 20.0f, 1);
    if (nk_contextual_item_label(ctx, "Copy row", NK_TEXT_LEFT)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t line = str_push_fmt(tmp.arena, "%.*s %.*s", STR_ARG(key.text.str), STR_ARG(val.text.str));
        detail_copy_text(ctx, line);
      }
      scratch_end(tmp);
    }

    if (record_to_open && record_has_flag(record_to_open, RECORD_FLAG_LIVE)) {
      nk_layout_row_dynamic(ctx, 20.0f, 1);
      if (nk_contextual_item_label(ctx, "Open in new tab", NK_TEXT_LEFT)) {
        detail_tab_open(tool, record_to_open, true);
      }
    }

    nk_contextual_end(ctx);
  }
}

static void
draw_details_header(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(tab->record != NULL);
  ASSERT(record_has_flag(tab->record, RECORD_FLAG_LIVE));
  ASSERT(tab->record->obj != NULL);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    ui_text_span_t name_key                         = ui_text_span_make(tool->ctx, STR_LIT("Name:"));
    ui_text_span_t full_name_key                    = ui_text_span_make(tool->ctx, STR_LIT("Full Name:"));
    ui_text_span_t class_name_key                   = ui_text_span_make(tool->ctx, STR_LIT("Class:"));
    ui_text_span_t outer_name_key                   = ui_text_span_make(tool->ctx, STR_LIT("Outer:"));
    ui_text_span_t flags_key                        = ui_text_span_make(tool->ctx, STR_LIT("Flags:"));
    ui_text_span_t internal_idx_key                 = ui_text_span_make(tool->ctx, STR_LIT("Internal index:"));
    ui_text_span_t addr_key                         = ui_text_span_make(tool->ctx, STR_LIT("Address:"));
    ui_text_span_t func_flags_key                   = ui_text_span_make(tool->ctx, STR_LIT("Function flags:"));
    ui_text_span_t func_params_size_key             = ui_text_span_make(tool->ctx, STR_LIT("Parameters size:"));
    ui_text_span_t func_native_func_key             = ui_text_span_make(tool->ctx, STR_LIT("Native function:"));
    ui_text_span_t func_event_graph_func_key        = ui_text_span_make(tool->ctx, STR_LIT("Event graph function:"));
    ui_text_span_t func_event_graph_call_offset_key = ui_text_span_make(tool->ctx, STR_LIT("Event graph call offset:"));
    ui_text_span_t cpp_type_key                     = ui_text_span_make(tool->ctx, STR_LIT("C++ type:"));
    ui_text_span_t null_val                         = ui_text_span_make(tool->ctx, STR_LIT("<null>"));

    ASSERT(cache_names(tool, tab->record));

    ui_text_span_t name_val                         = tab->record->name;
    ui_text_span_t full_name_val                    = tab->record->full_name;
    ui_text_span_t class_name_val                   = null_val;
    ui_text_span_t outer_name_val                   = null_val;
    ui_text_span_t flags_val                        = tab->flags;
    ui_text_span_t internal_idx_val                 = tab->internal_idx;
    ui_text_span_t addr_val                         = tab->addr;
    ui_text_span_t func_flags_val                   = null_val;
    ui_text_span_t func_params_size_val             = null_val;
    ui_text_span_t func_native_func_val             = null_val;
    ui_text_span_t func_event_graph_func_val        = null_val;
    ui_text_span_t func_event_graph_call_offset_val = null_val;
    ui_text_span_t cpp_type_val                     = null_val;

    bool has_class = false;
    bool has_outer = false;
    bool is_func   = false;
    bool is_enum   = false;

    record_t *type_record = record_from_slot(tool, tab->record->type_slot);
    if (type_record) {
      ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));
      ASSERT(cache_name(tool, type_record));

      has_class      = true;
      class_name_val = type_record->name;
    }

    record_t *parent_record = record_from_slot(tool, tab->record->parent_slot);
    if (parent_record) {
      ASSERT(record_has_flag(parent_record, RECORD_FLAG_LIVE));

      has_outer      = true;
      outer_name_val = tab->outer_name;
    }

    if (tab->record->kind == UOBJECT_KIND_FUNC && tab->function) {
      is_func                          = true;
      func_flags_val                   = tab->function->flags_text;
      func_params_size_val             = tab->function->params_size_text;
      func_native_func_val             = tab->function->native_func_text;
      func_event_graph_func_val        = tab->function->event_graph_func_text;
      func_event_graph_call_offset_val = tab->function->event_graph_call_offset_text;
    }

    if (tab->record->kind == UOBJECT_KIND_ENUM && tab->enumeration && !str_is_empty(tab->enumeration->cpp_type.str)) {
      is_enum      = true;
      cpp_type_val = tab->enumeration->cpp_type;
    }

    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(name_key, UI_C_TEXT), UI_TEXT_CELL(name_val, UI_C_TEXT), NULL);
    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(full_name_key, UI_C_TEXT), UI_TEXT_CELL(full_name_val, UI_C_TEXT), NULL);

    struct nk_color class_color = has_class ? uobject_kind_color(tab->record->kind) : UI_C_TEXT;
    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(class_name_key, UI_C_TEXT), UI_TEXT_CELL(class_name_val, class_color), type_record);

    struct nk_color outer_color = has_outer ? uobject_kind_color(parent_record->kind) : UI_C_TEXT;
    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(outer_name_key, UI_C_TEXT), UI_TEXT_CELL(outer_name_val, outer_color), parent_record);

    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(flags_key, UI_C_TEXT), UI_TEXT_CELL(flags_val, UI_C_TEXT), NULL);
    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(internal_idx_key, UI_C_TEXT), UI_TEXT_CELL(internal_idx_val, UI_C_TEXT), NULL);
    draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(addr_key, UI_C_TEXT), UI_TEXT_CELL(addr_val, UI_C_TEXT), NULL);

    if (is_func) {
      draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(func_flags_key, UI_C_TEXT), UI_TEXT_CELL(func_flags_val, UI_C_TEXT), NULL);
      draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(func_params_size_key, UI_C_TEXT), UI_TEXT_CELL(func_params_size_val, UI_C_TEXT), NULL);

      draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(func_native_func_key, UI_C_TEXT), UI_TEXT_CELL(func_native_func_val, UI_C_TEXT), NULL);
      draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(func_event_graph_func_key, UI_C_TEXT), UI_TEXT_CELL(func_event_graph_func_val, UI_C_TEXT), NULL);
      draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(func_event_graph_call_offset_key, UI_C_TEXT), UI_TEXT_CELL(func_event_graph_call_offset_val, UI_C_TEXT), NULL);
    }

    if (is_enum) {
      draw_details_header_line(tool, &tab->header_grid, UI_TEXT_CELL(cpp_type_key, UI_C_TEXT), UI_TEXT_CELL(cpp_type_val, UI_C_TEXT), NULL);
    }
  }
  scratch_end(tmp);
}

typedef enum detail_tree_row_view_e {
  DETAIL_TREE_ROW_TEXT,
  DETAIL_TREE_ROW_CLOSED,
  DETAIL_TREE_ROW_OPEN,
} detail_tree_row_view_t;

typedef struct ui_detail_row_s ui_detail_row_t;
struct ui_detail_row_s {
  ui_text_cell_t *parts;
  int             count;
  bool            has_children;
  int             depth;
  str_t           copy_text;
  record_t       *record_to_open;
};

static str_t
detail_row_push_line(arena_t *arena, ui_detail_row_t row)
{
  ASSERT(arena != NULL);

  str_list_t list = {0};
  for (int i = 0; i < row.count; ++i) {
    if (!str_is_empty(row.parts[i].text.str)) {
      str_list_push(arena, &list, row.parts[i].text.str);
    }
  }
  return str_list_join(arena, list, STR_NULL, STR_LIT("  "), STR_NULL);
}

static bool
ui_detail_row(search_tool_t *tool, struct nk_grid_state *grid_state, ui_detail_row_t row, bool *maximized)
{
  struct nk_grid_column  columns[NK_GRID_MAX_COLUMNS] = {0};
  struct nk_grid_options grid_options = {0};
  struct nk_context     *ctx;
  struct nk_style       *style;
  struct nk_panel       *layout;
  struct nk_grid         grid;
  struct nk_rect         row_bounds = {0};
  float                  indent_w;
  float                  symbol_w;
  float                  gap_w;
  bool                   expanded;
  detail_tree_row_view_t view;
  int                    i;

  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(grid_state != NULL);
  ASSERT(row.parts != NULL);
  ASSERT(row.count > 0 && row.count + 1 <= NK_GRID_MAX_COLUMNS);
  if (row.has_children) {
    ASSERT(maximized != NULL);
  }

  ctx = tool->ctx;
  if (!ctx->current || !ctx->current->layout) {
    return false;
  }

  style     = &ctx->style;
  layout    = ctx->current->layout;
  gap_w     = ui_text_width(ctx, STR_LIT(" "));
  indent_w  = style->tab.indent;
  symbol_w  = NK_MAX(style->font_size, 12.0f);
  row.depth = NK_MAX(row.depth, 0);

  if (indent_w <= 0.0f) {
    indent_w = ui_text_width(ctx, STR_LIT("  "));
  }
  if (indent_w <= 0.0f) {
    indent_w = 12.0f;
  }

  columns[0].sizing = NK_GRID_COLUMN_FIXED;
  columns[0].width  = style->tab.padding.x + (float)row.depth * indent_w + symbol_w + gap_w;
  for (i = 0; i < row.count; ++i) {
    columns[i + 1].sizing = NK_GRID_COLUMN_CONTENT;
  }

  grid_options.row_height = style->font_size + 2.0f * style->tab.padding.y;
  grid_options.column_gap = gap_w;
  nk_grid_begin(ctx, &grid, grid_state, columns, row.count + 1, &grid_options);

  expanded = row.has_children ? *maximized : false;
  view     = row.has_children ? (*maximized ? DETAIL_TREE_ROW_OPEN : DETAIL_TREE_ROW_CLOSED) : DETAIL_TREE_ROW_TEXT;

  nk_layout_set_min_row_height(ctx, grid_options.row_height);
  if (nk_grid_row_begin(&grid)) {
    struct nk_rect               bounds;
    enum nk_widget_layout_states layout_state;

    nk_grid_push_row(&grid);
    layout_state = nk_widget(&bounds, ctx);
    if (layout_state != NK_WIDGET_INVALID) {
      struct nk_input          *in        = NULL;
      nk_flags                  row_state = 0;
      struct nk_command_buffer *out       = nk_window_get_canvas(ctx);
      struct nk_color           bg        = row.has_children ? DETAIL_C_ROW_COLLAPSIBLE_BG : UI_C_TRANSPARENT;
      struct nk_rect            sym;

      row_bounds   = bounds;
      row_bounds.w = grid.offsets[row.count] + grid.widths[row.count];

      if (layout_state != NK_WIDGET_ROM && layout_state != NK_WIDGET_DISABLED &&
          !(layout->flags & NK_WINDOW_ROM) && !(layout->flags & NK_WINDOW_NO_INPUT) &&
          ui_nk_current_panel_accepts_input(ctx)) {
        in = &ctx->input;
      }

      if (in && row.has_children) {
        if (nk_button_behavior(&row_state, row_bounds, in, NK_BUTTON_DEFAULT)) {
          *maximized = !*maximized;
          expanded   = *maximized;
          view       = *maximized ? DETAIL_TREE_ROW_OPEN : DETAIL_TREE_ROW_CLOSED;
        }
        ctx->last_widget_state = row_state;
      }

      if (row.has_children && row_state & NK_WIDGET_STATE_HOVER) {
        bg = DETAIL_C_ROW_COLLAPSIBLE_HOVER;
      }

      if (row.has_children && row_state & NK_WIDGET_STATE_ACTIVE) {
        bg = DETAIL_C_ROW_COLLAPSIBLE_ACTIVE;
      }

      if (bg.a > 0) {
        nk_fill_rect(out, row_bounds, style->selectable.rounding, bg);
      }

      sym = nk_rect(bounds.x + style->tab.padding.x + (float)row.depth * indent_w,
                    bounds.y + (bounds.h - symbol_w) * 0.5f, symbol_w, symbol_w);
      if (view != DETAIL_TREE_ROW_TEXT) {
        enum nk_symbol_type           symbol;
        const struct nk_style_button *button;
        nk_flags                      draw_state = 0;

        if (view == DETAIL_TREE_ROW_OPEN) {
          symbol = style->tab.sym_maximize;
          button = &style->tab.node_maximize_button;
        } else {
          symbol = style->tab.sym_minimize;
          button = &style->tab.node_minimize_button;
        }
        nk_do_button_symbol(&draw_state, out, sym, symbol, NK_BUTTON_DEFAULT, button, NULL, style->font);
      }

    }
  }
  for (i = 0; i < row.count; ++i) {
    struct nk_text_options options = {
      .alignment = row.parts[i].align,
      .flags     = NK_TEXT_OPTION_SELECTABLE,
      .color     = row.parts[i].fg,
    };
    ui_grid_str(&grid, i + 1, row.parts[i].text.str, &options);
  }
  nk_grid_row_end(&grid);
  nk_grid_end(&grid);
  nk_layout_reset_min_row_height(ctx);

  if (nk_contextual_begin(ctx, NK_WINDOW_BORDER, nk_vec2(190.0f, 92.0f), row_bounds)) {
    if (!str_is_empty(row.copy_text)) {
      nk_layout_row_dynamic(ctx, 20.0f, 1);
      if (nk_contextual_item_label(ctx, "Copy value", NK_TEXT_LEFT)) {
        detail_copy_text(ctx, row.copy_text);
      }
    }

    nk_layout_row_dynamic(ctx, 20.0f, 1);
    if (nk_contextual_item_label(ctx, "Copy row", NK_TEXT_LEFT)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t line = detail_row_push_line(tmp.arena, row);
        detail_copy_text(ctx, line);
      }
      scratch_end(tmp);
    }

    if (row.record_to_open && record_has_flag(row.record_to_open, RECORD_FLAG_LIVE)) {
      nk_layout_row_dynamic(ctx, 20.0f, 1);
      if (nk_contextual_item_label(ctx, "Open in new tab", NK_TEXT_LEFT)) {
        detail_tab_open(tool, row.record_to_open, true);
      }
    }

    nk_contextual_end(ctx);
  }
  return expanded;
}

typedef struct detail_tree_state_s {
  bool has_children;
  bool has_collapsed;
} detail_tree_state_t;

static inline bool
detail_prop_has_children(detail_prop_t *prop)
{
  switch (prop->children_kind) {
    case DETAIL_PROP_CHILDREN_ARRAY_ELEMS:
    case DETAIL_PROP_CHILDREN_SET_ELEMS:
    case DETAIL_PROP_CHILDREN_MAP_PAIRS:
    case DETAIL_PROP_CHILDREN_MCAST_DELEGATE: {
      return prop->live_child_count > 0;
    }

    default: {
      return prop->first_child != NULL;
    }
  }
}

static void
detail_props_collect_tree_state(detail_prop_t *first, detail_tree_state_t *state)
{
  for (detail_prop_t *prop = first; prop; prop = prop->next) {
    if (detail_prop_has_children(prop)) {
      state->has_children = true;

      if (!prop->maximized) {
        state->has_collapsed = true;
      }

      detail_props_collect_tree_state(prop->first_child, state);
    }
  }
}

static void
detail_props_set_expanded(detail_prop_t *first, bool expanded)
{
  for (detail_prop_t *prop = first; prop; prop = prop->next) {
    prop->maximized = expanded && detail_prop_has_children(prop);
    detail_props_set_expanded(prop->first_child, expanded);
  }
}

static detail_tree_state_t
detail_funcs_tree_state(detail_tab_t *tab)
{
  detail_tree_state_t state = {0};
  for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
    for (detail_func_t *func = owner->first_func; func; func = func->next) {
      state.has_children = true;

      if (!func->maximized) {
        state.has_collapsed = true;
      }
      detail_props_collect_tree_state(func->first_param, &state);
    }
  }
  return state;
}

static void
detail_funcs_set_expanded(detail_tab_t *tab, bool expanded)
{
  for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
    for (detail_func_t *func = owner->first_func; func; func = func->next) {
      func->maximized = expanded;
      detail_props_set_expanded(func->first_param, expanded);
    }
  }
}

static bool
detail_tree_toggle_button(struct nk_context *ctx, detail_tree_state_t state)
{
  str_t label = STR_LIT("Expand");
  if (state.has_children && !state.has_collapsed) {
    label = STR_LIT("Collapse");
  }

  if (!state.has_children) {
    nk_widget_disable_begin(ctx);
  }

  bool clicked = ui_button_str(ctx, label);

  if (!state.has_children) {
    nk_widget_disable_end(ctx);
  }

  return clicked;
}

static inline bool
detail_package_is_expanded(search_tool_t *tool, detail_tab_t *tab, record_t *record)
{
  uint32_t slot = record_to_slot(tool, record);
  return (tab->package_expanded[slot / 8] & (1u << (slot % 8))) != 0;
}

static inline void
detail_package_set_expanded(search_tool_t *tool, detail_tab_t *tab, record_t *record, bool expanded)
{
  uint32_t slot = record_to_slot(tool, record);
  uint8_t  mask = (uint8_t)(1u << (slot % 8));

  if (expanded) {
    tab->package_expanded[slot / 8] |= mask;
  } else {
    tab->package_expanded[slot / 8] &= (uint8_t)~mask;
  }
}

static void
detail_package_collect_tree_state(search_tool_t *tool, detail_tab_t *tab, record_t *record, detail_tree_state_t *state)
{
  for (record_t *child = record_from_slot(tool, record->first_child_slot); child; child = record_from_slot(tool, child->next_sibling_slot)) {
    if (record_slot_valid(child->first_child_slot)) {
      state->has_children = true;

      if (!detail_package_is_expanded(tool, tab, child)) {
        state->has_collapsed = true;
      }

      detail_package_collect_tree_state(tool, tab, child, state);
    }
  }
}

static void
detail_package_set_children_expanded(search_tool_t *tool, detail_tab_t *tab, record_t *record, bool expanded)
{
  for (record_t *child = record_from_slot(tool, record->first_child_slot); child; child = record_from_slot(tool, child->next_sibling_slot)) {
    detail_package_set_expanded(tool, tab, child, expanded);
    detail_package_set_children_expanded(tool, tab, child, expanded);
  }
}

static void
package_draw_children(search_tool_t *tool, detail_tab_t *tab, record_t *record, int depth)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));

  for (record_t *child_record = record_from_slot(tool, record->first_child_slot); child_record; child_record = record_from_slot(tool, child_record->next_sibling_slot)) {
    ASSERT(record_has_flag(child_record, RECORD_FLAG_LIVE));

    if (!cache_names(tool, child_record)) {
      continue;
    }

    record_t *child_type_record = record_from_slot(tool, child_record->type_slot);
    ASSERT(child_type_record);
    ASSERT(record_has_flag(child_type_record, RECORD_FLAG_LIVE));

    ui_text_cell_t parts[] = {
      UI_TEXT_CELL(child_type_record->name, uobject_kind_color(child_record->kind)),
      UI_TEXT_CELL(child_record->name, UI_C_TEXT),
    };

    ui_detail_row_t row = {
      .parts          = parts,
      .count          = COUNTOF(parts),
      .depth          = depth,
      .has_children   = (child_record->first_child_slot != RECORD_SLOT_INVALID),
      .copy_text      = child_record->full_name.str,
      .record_to_open = child_record,
    };

    bool maximized = detail_package_is_expanded(tool, tab, child_record);
    if (ui_detail_row(tool, &tab->package_grid, row, &maximized)) {
      if (child_record->first_child_slot != RECORD_SLOT_INVALID) {
        package_draw_children(tool, tab, child_record, depth + 1);
      }
    }
    detail_package_set_expanded(tool, tab, child_record, maximized);
  }
}

static void
draw_package_section(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    if (ui_tree_push(tool->ctx, NK_TREE_TAB, STR_LIT("Package Content"), true)) {
      nk_layout_row_dynamic(tool->ctx, 22.0f, 2);
      if (ui_button_str(tool->ctx, STR_LIT("Copy"))) {
        str_t dump = detail_push_package_dump(tmp.arena, tool, tab);
        detail_copy_text(tool->ctx, dump);
      }

      detail_tree_state_t state = {0};
      detail_package_collect_tree_state(tool, tab, tab->record, &state);

      if (detail_tree_toggle_button(tool->ctx, state)) {
        detail_package_set_children_expanded(tool, tab, tab->record, state.has_collapsed);
      }

      if (tab->record->first_child_slot != RECORD_SLOT_INVALID) {
        package_draw_children(tool, tab, tab->record, 0);
      } else {
        nk_layout_row_dynamic(tool->ctx, 20.0f, 1);
        nk_label(tool->ctx, "No children", NK_TEXT_LEFT);
      }
      nk_tree_pop(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static void
draw_detail_enum_entry(search_tool_t *tool, struct nk_grid_state *grid_state, detail_enum_entry_t *entry)
{
  ASSERT(entry != NULL);

  ui_text_cell_t parts[] = {
    UI_TEXT_CELL(entry->name, UI_C_TEXT),
    UI_TEXT_CELL(entry->value_text, DETAIL_C_ENUM_VALUE_TEXT),
  };

  ui_detail_row_t row = {
    .parts        = parts,
    .count        = COUNTOF(parts),
    .depth        = 0,
    .has_children = false,
    .copy_text    = entry->value_text.str,
  };

  ui_detail_row(tool, grid_state, row, NULL);
}

static void
draw_detail_enum_section(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tab != NULL);
  if (!tab->enumeration) {
    return;
  }

  detail_enum_t *e = tab->enumeration;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    if (ui_tree_push(tool->ctx, NK_TREE_TAB, STR_LIT("Entries"), NK_MAXIMIZED)) {
      nk_style_push_vec2(tool->ctx, &tool->ctx->style.tab.padding, nk_vec2(2.0f, 2.0f));
      nk_style_push_style_item(tool->ctx, &tool->ctx->style.window.fixed_background, UI_ITEM(UI_C_TRANSPARENT));

      nk_layout_row_dynamic(tool->ctx, 22.0f, 1);
      if (ui_button_str(tool->ctx, STR_LIT("Copy"))) {
        str_t dump = detail_push_enum_dump(tmp.arena, tool, tab);
        detail_copy_text(tool->ctx, dump);
      }

      ui_text_cell_t header_parts[] = {
        UI_TEXT_CELL(e->enum_name_col, UI_C_TEXT),
        UI_TEXT_CELL(e->enum_value_col, DETAIL_C_ENUM_VALUE_TEXT),
      };

      ui_detail_row_t header_row = {
        .parts          = header_parts,
        .count          = COUNTOF(header_parts),
        .depth          = 0,
        .has_children   = false,
        .record_to_open = NULL,
      };
      ui_detail_row(tool, &e->grid, header_row, NULL);

      if (!e->first_entry) {
        ui_text_cell_t parts[] = {
          UI_TEXT_CELL(tab->no_entries, UI_C_TEXT),
        };

        ui_detail_row_t row = {
          .parts          = parts,
          .count          = COUNTOF(parts),
          .depth          = 1,
          .has_children   = false,
          .record_to_open = NULL,
        };
        ui_detail_row(tool, &tab->single_grid, row, NULL);
      } else {
        for (detail_enum_entry_t *entry = e->first_entry; entry; entry = entry->next) {
          draw_detail_enum_entry(tool, &e->grid, entry);
        }
      }

      nk_style_pop_style_item(tool->ctx);
      nk_style_pop_vec2(tool->ctx);

      nk_tree_pop(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static void
draw_detail_prop(search_tool_t *tool, detail_tab_t *tab, struct nk_grid_state *grid_state, detail_prop_t *prop, int depth)
{
  ASSERT(tool != NULL);
  ASSERT(prop != NULL);
  ASSERT(grid_state != NULL);

  struct nk_color summary_color = DETAIL_C_PROP_VALUE_TEXT;
  if (prop->is_param) {
    summary_color = DETAIL_C_PROP_FLAGS_TEXT;
  } else if (prop->record_to_open) {
    summary_color = uobject_kind_color(prop->record_to_open->kind);
  }

  ui_text_cell_t parts[] = {
    UI_TEXT_CELL(prop->offset_text, DETAIL_C_PROP_OFFSET_TEXT),
    UI_TEXT_CELL(prop->size_text, DETAIL_C_PROP_SIZE_TEXT),
    UI_TEXT_CELL(prop->type, DETAIL_C_PROP_TYPE_TEXT),
    UI_TEXT_CELL(prop->name, DETAIL_C_PROP_NAME_TEXT),
    UI_TEXT_CELL(prop->summary, summary_color),
  };

  bool has_children = detail_prop_has_children(prop);

  ui_detail_row_t row = {
    .parts          = parts,
    .count          = COUNTOF(parts),
    .depth          = depth,
    .has_children   = has_children,
    .copy_text      = (!prop->is_param) ? prop->summary.str : STR_NULL,
    .record_to_open = prop->record_to_open,
  };

  bool was_maximized = prop->maximized;
  if (ui_detail_row(tool, grid_state, row, &prop->maximized)) {
    if (!was_maximized && tab->value_base && prop->prop) {
      detail_prop_fill_value_direct(tool, tab, prop, depth);
    }

    for (detail_prop_t *child = prop->first_child; child; child = child->next) {
      draw_detail_prop(tool, tab, grid_state, child, depth + 1);
    }
  }
}

static void
draw_detail_disasm_section(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);

  ASSERT(tab->record != NULL);
  ASSERT(tab->record->obj != NULL);

  struct nk_context *ctx = tool->ctx;

  ustruct_t *ustruct = (ustruct_t *)tab->record->obj;
  if (!ustruct || ustruct->script.num <= 0 || !ustruct->script.data) {
    return;
  }

  if (ui_tree_state_push(ctx, NK_TREE_TAB, STR_LIT("Disassembly"), &tab->disasm_maximized)) {
    if (!tab->disasm_built) {
      tab->disasm_built = true;
      tab->disasm_lines = unreal_disassemble_structure(ustruct, tab->info_arena);
    }

    nk_layout_row_dynamic(ctx, 22.0f, 1);
    if (ui_button_str(ctx, STR_LIT("Copy"))) {
      str_t text = str_list_join(tab->info_arena, tab->disasm_lines, STR_NULL, STR_LIT("\n"), STR_NULL);
      if (!str_is_empty(text)) {
        detail_copy_text(ctx, text);
      }
    }

    nk_style_push_vec2(ctx, &ctx->style.window.group_padding, nk_vec2(4.0f, 4.0f));
    nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0.0f, 0.0f));

    nk_layout_row_dynamic(ctx, 260.0f, 1);
    if (nk_group_begin(ctx, "uobject_search.function.disasm", NK_WINDOW_BORDER)) {
      for (str_node_t *node = tab->disasm_lines.first; node; node = node->next) {
        struct nk_text_options options = {
          .alignment = NK_TEXT_LEFT,
          .flags     = NK_TEXT_OPTION_SELECTABLE,
          .color     = UI_C_TEXT,
        };
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        ui_str_ex(ctx, node->str, &options);
      }

      nk_group_end(ctx);
    }

    nk_style_pop_vec2(ctx);
    nk_style_pop_vec2(ctx);

    nk_tree_pop(ctx);
  }
}

static void
draw_detail_func_section(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tab != NULL);

  if (!tab->function) {
    return;
  }

  nk_layout_row_dynamic(tool->ctx, 22.0f, 1);
  if (ui_button_str(tool->ctx, STR_LIT("Call..."))) {
    ufunc_call_open(tool, tab->function->record, NULL);
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    detail_func_t *f = tab->function;

    uint32_t param_count = 0;
    for (detail_prop_t *param = f->first_param; param; param = param->next) {
      param_count += 1;
    }

    str_t title = str_push_fmt(tmp.arena, "Parameters (%u)", param_count);
    if (ui_tree_state_push(tool->ctx, NK_TREE_TAB, title, &tab->layout_func_maximized)) {
      nk_style_push_vec2(tool->ctx, &tool->ctx->style.tab.padding, nk_vec2(2.0f, 2.0f));
      nk_style_push_style_item(tool->ctx, &tool->ctx->style.window.fixed_background, UI_ITEM(UI_C_TRANSPARENT));

      nk_layout_row_dynamic(tool->ctx, 22.0f, 2);
      if (ui_button_str(tool->ctx, STR_LIT("Copy"))) {
        str_t dump = detail_push_single_func_dump(tool, tab, tmp.arena);
        detail_copy_text(tool->ctx, dump);
      }

      detail_tree_state_t state = {0};
      detail_props_collect_tree_state(f->first_param, &state);

      if (detail_tree_toggle_button(tool->ctx, state)) {
        detail_props_set_expanded(f->first_param, state.has_collapsed);
      }

      ui_text_cell_t header_parts[] = {
        UI_TEXT_CELL(tab->offset_col, UI_C_TEXT),
        UI_TEXT_CELL(tab->size_col, UI_C_TEXT),
        UI_TEXT_CELL(tab->type_col, UI_C_TEXT),
        UI_TEXT_CELL(tab->name_col, UI_C_TEXT),
        UI_TEXT_CELL(tab->flags_col, UI_C_TEXT),
      };

      ui_detail_row_t header_row = {
        .parts          = header_parts,
        .count          = COUNTOF(header_parts),
        .depth          = 0,
        .has_children   = false,
        .record_to_open = NULL,
      };
      ui_detail_row(tool, &tab->param_grid, header_row, NULL);

      if (!f->first_param) {
        ui_text_cell_t parts[] = {
          UI_TEXT_CELL(tab->no_params, UI_C_TEXT),
        };

        ui_detail_row_t header_row = {
          .parts          = parts,
          .count          = COUNTOF(parts),
          .depth          = 0,
          .has_children   = false,
          .record_to_open = NULL,
        };
        ui_detail_row(tool, &tab->single_grid, header_row, NULL);
      } else {
        for (detail_prop_t *param = f->first_param; param; param = param->next) {
          draw_detail_prop(tool, tab, &tab->param_grid, param, 0);
        }
      }

      nk_style_pop_style_item(tool->ctx);
      nk_style_pop_vec2(tool->ctx);

      nk_tree_pop(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static void
draw_detail_layout_section_instances(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);
  ASSERT(tab->record != NULL);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t title = str_push_fmt(tmp.arena, "Instances (%u)", tab->record->instance_count);
    if (ui_tree_state_push(tool->ctx, NK_TREE_TAB, title, &tab->layout_instances_maximized)) {
      nk_style_push_vec2(tool->ctx, &tool->ctx->style.tab.padding, nk_vec2(2.0f, 2.0f));
      nk_style_push_style_item(tool->ctx, &tool->ctx->style.window.fixed_background, UI_ITEM(UI_C_TRANSPARENT));

      nk_layout_row_dynamic(tool->ctx, 22.0f, 1);
      if (ui_button_str(tool->ctx, STR_LIT("Copy"))) {
        str_t dump = detail_push_instances_dump(tmp.arena, tool, tab);
        detail_copy_text(tool->ctx, dump);
      }

      if (tab->record->first_instance_slot == RECORD_SLOT_INVALID) {
        ui_text_cell_t parts[] = {
          UI_TEXT_CELL(tab->no_instances, UI_C_TEXT),
        };

        ui_detail_row_t row = {
          .parts          = parts,
          .count          = COUNTOF(parts),
          .depth          = 0,
          .has_children   = false,
          .record_to_open = NULL,
        };
        ui_detail_row(tool, &tab->single_grid, row, NULL);
      } else {
        ui_text_cell_t header_parts[] = {
          UI_TEXT_CELL(tab->type_col, UI_C_TEXT),
          UI_TEXT_CELL(tab->name_col, UI_C_TEXT),
        };

        ui_detail_row_t header_row = {
          .parts          = header_parts,
          .count          = COUNTOF(header_parts),
          .depth          = 0,
          .has_children   = false,
          .record_to_open = NULL,
        };
        ui_detail_row(tool, &tab->instance_grid, header_row, NULL);

        for (record_t *instance = record_from_slot(tool, tab->record->first_instance_slot); instance; instance = record_from_slot(tool, instance->next_instance_slot)) {
          if (!record_has_flag(instance, RECORD_FLAG_LIVE)) {
            continue;
          }

          record_t *type_record = record_from_slot(tool, instance->type_slot);
          ASSERT(type_record != NULL);
          ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));
          ASSERT(cache_names(tool, instance));
          ASSERT(cache_names(tool, type_record));

          ui_text_cell_t instance_parts[] = {
            UI_TEXT_CELL(type_record->name, uobject_kind_color(instance->kind)),
            UI_TEXT_CELL(instance->name, UI_C_TEXT),
          };

          ui_detail_row_t row = {
            .parts          = instance_parts,
            .count          = COUNTOF(instance_parts),
            .depth          = 0,
            .has_children   = false,
            .copy_text      = instance->full_name.str,
            .record_to_open = instance,
          };
          ui_detail_row(tool, &tab->instance_grid, row, NULL);
        }
      }

      nk_style_pop_style_item(tool->ctx);
      nk_style_pop_vec2(tool->ctx);

      nk_tree_pop(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static int
detail_visible_prop_count(detail_prop_t *first)
{
  int count = 0;

  for (detail_prop_t *prop = first; prop; prop = prop->next) {
    count += 1;

    if (prop->first_child && prop->maximized) {
      count += detail_visible_prop_count(prop->first_child);
    }
  }

  return count;
}

static int
detail_visible_prop_row_count(detail_tab_t *tab)
{
  int count = 1; // table header

  for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
    count += 1; // owner header
    if (owner->first_prop) {
      count += detail_visible_prop_count(owner->first_prop);
    } else {
      count += 1;
    }
  }

  return count;
}

static void
draw_detail_layout_section_props(search_tool_t *tool, detail_tab_t *tab)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(tab != NULL);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    uint32_t prop_count = 0;
    for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
      for (detail_prop_t *prop = owner->first_prop; prop; prop = prop->next) {
        prop_count += 1;
      }
    }

    str_t prop_tab_title = str_push_fmt(tmp.arena, "Properties (%u)", prop_count);
    if (ui_tree_state_push(tool->ctx, NK_TREE_TAB, prop_tab_title, &tab->layout_prop_maximized)) {
      nk_style_push_vec2(tool->ctx, &tool->ctx->style.tab.padding, nk_vec2(2.0f, 2.0f));
      nk_style_push_style_item(tool->ctx, &tool->ctx->style.window.fixed_background, UI_ITEM(UI_C_TRANSPARENT));

      details_refresh_values(tool, tab);

      nk_layout_row_dynamic(tool->ctx, 22.0f, 2);
      if (ui_button_str(tool->ctx, STR_LIT("Copy"))) {
        str_t dump = detail_props_push_dump(tool, tab, tmp.arena);
        detail_copy_text(tool->ctx, dump);
      }

      detail_tree_state_t state = {0};
      for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
        detail_props_collect_tree_state(owner->first_prop, &state);
      }

      if (detail_tree_toggle_button(tool->ctx, state)) {
        if (state.has_collapsed) {
          details_refresh_all_values(tool, tab);
        }

        for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
          detail_props_set_expanded(owner->first_prop, state.has_collapsed);
        }
      }

      float group_pad_y = tool->ctx->style.window.group_padding.y;
      float scroll_h    = tool->ctx->style.window.scrollbar_size.y;
      float pad_y       = tool->ctx->style.window.spacing.y;
      float row_h       = tool->ctx->style.font_size + 2.0f * tool->ctx->style.tab.padding.y + pad_y;
      int   row_count   = detail_visible_prop_row_count(tab);
      float group_h     = group_pad_y + (float)row_count * row_h + scroll_h + group_pad_y;

      nk_layout_row_dynamic(tool->ctx, group_h, 1);
      if (nk_group_begin(tool->ctx, "uobject_search.details.properties.table", NK_WINDOW_NO_SCROLLBAR_V)) {
        ui_text_cell_t header_parts[] = {
          UI_TEXT_CELL(tab->offset_col, UI_C_TEXT),
          UI_TEXT_CELL(tab->size_col, UI_C_TEXT),
          UI_TEXT_CELL(tab->type_col, UI_C_TEXT),
          UI_TEXT_CELL(tab->name_col, UI_C_TEXT),
          UI_TEXT_CELL(tab->val_col, UI_C_TEXT),
        };

        ui_detail_row_t header_row = {
          .parts          = header_parts,
          .count          = COUNTOF(header_parts),
          .depth          = 1,
          .has_children   = false,
          .record_to_open = NULL,
        };
        ui_detail_row(tool, &tab->prop_grid, header_row, NULL);

        for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
          int prop_count = 0;
          for (detail_prop_t *prop = owner->first_prop; prop; prop = prop->next) {
            prop_count += 1;
          }

          str_t          owner_header  = str_push_fmt(tmp.arena, "%.*s (%d): ", STR_ARG(owner->record->name.str), prop_count);
          ui_text_cell_t owner_parts[] = {
            UI_TEXT_CELL(ui_text_span_make(tool->ctx, owner_header), uobject_kind_color(owner->record->kind)),
          };

          ui_detail_row_t owner_row = {
            .parts          = owner_parts,
            .count          = COUNTOF(owner_parts),
            .depth          = 0,
            .has_children   = false,
            .record_to_open = owner->record,
          };

          ui_detail_row(tool, &tab->single_grid, owner_row, NULL);

          if (!owner->first_prop) {
            ui_text_cell_t parts[] = {
              UI_TEXT_CELL(tab->no_props, UI_C_TEXT),
            };

            ui_detail_row_t row = {
              .parts          = parts,
              .count          = COUNTOF(parts),
              .depth          = 1,
              .has_children   = false,
              .record_to_open = NULL,
            };
            ui_detail_row(tool, &tab->single_grid, row, NULL);
          } else {
            for (detail_prop_t *prop = owner->first_prop; prop; prop = prop->next) {
              draw_detail_prop(tool, tab, &tab->prop_grid, prop, 1);
            }
          }
        }
        nk_group_end(tool->ctx);
      }

      nk_style_pop_style_item(tool->ctx);
      nk_style_pop_vec2(tool->ctx);

      nk_tree_pop(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static void
draw_detail_layout_section_funcs(search_tool_t *tool, detail_tab_t *tab)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    uint32_t func_count = 0;
    for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
      for (detail_func_t *func = owner->first_func; func; func = func->next) {
        func_count += 1;
      }
    }

    str_t func_tab_title = str_push_fmt(tmp.arena, "Functions (%u)", func_count);
    if (ui_tree_state_push(tool->ctx, NK_TREE_TAB, func_tab_title, &tab->layout_func_maximized)) {
      nk_style_push_vec2(tool->ctx, &tool->ctx->style.tab.padding, nk_vec2(2.0f, 2.0f));
      nk_style_push_style_item(tool->ctx, &tool->ctx->style.window.fixed_background, UI_ITEM(UI_C_TRANSPARENT));

      nk_layout_row_dynamic(tool->ctx, 22.0f, 2);
      if (ui_button_str(tool->ctx, STR_LIT("Copy"))) {
        str_t dump = detail_push_funcs_dump(tmp.arena, tab);
        detail_copy_text(tool->ctx, dump);
      }

      detail_tree_state_t state = detail_funcs_tree_state(tab);
      if (detail_tree_toggle_button(tool->ctx, state)) {
        detail_funcs_set_expanded(tab, state.has_collapsed);
      }

      for (detail_owner_t *owner = tab->first_owner; owner; owner = owner->next) {
        ui_text_cell_t owner_parts[] = {
          UI_TEXT_CELL(tab->owner_hbar, UI_C_TEXT),
          UI_TEXT_CELL(owner->record->name, uobject_kind_color(owner->record->kind)),
          UI_TEXT_CELL(tab->owner_hbar, UI_C_TEXT),
        };

        ui_detail_row_t owner_row = {
          .parts          = owner_parts,
          .count          = COUNTOF(owner_parts),
          .depth          = 0,
          .has_children   = false,
          .record_to_open = owner->record,
        };
        ui_detail_row(tool, &tab->func_owner_grid, owner_row, NULL);

        if (!owner->first_func) {
          ui_text_cell_t parts[] = {
            UI_TEXT_CELL(tab->no_funcs, UI_C_TEXT),
          };

          ui_detail_row_t row = {
            .parts          = parts,
            .count          = COUNTOF(parts),
            .depth          = 1,
            .has_children   = false,
            .record_to_open = NULL,
          };
          ui_detail_row(tool, &tab->single_grid, row, NULL);
        } else {
          for (detail_func_t *func = owner->first_func; func; func = func->next) {
            ui_text_cell_t func_parts[] = {
              UI_TEXT_CELL(func->record->name, uobject_kind_color(func->record->kind)),
            };

            ui_detail_row_t row = {
              .parts          = func_parts,
              .count          = COUNTOF(func_parts),
              .depth          = 1,
              .has_children   = true,
              .record_to_open = func->record,
            };

            if (ui_detail_row(tool, &tab->func_name_grid, row, &func->maximized)) {
              nk_layout_row_dynamic(tool->ctx, 22.0f, 1);
              if (ui_button_str(tool->ctx, STR_LIT("Call..."))) {
                ufunc_call_open(tool, func->record, NULL);
              }

              ui_text_cell_t header_parts[] = {
                UI_TEXT_CELL(tab->offset_col, UI_C_TEXT),
                UI_TEXT_CELL(tab->size_col, UI_C_TEXT),
                UI_TEXT_CELL(tab->type_col, UI_C_TEXT),
                UI_TEXT_CELL(tab->name_col, UI_C_TEXT),
                UI_TEXT_CELL(tab->flags_col, UI_C_TEXT),
              };

              ui_detail_row_t header_row = {
                .parts          = header_parts,
                .count          = COUNTOF(header_parts),
                .depth          = 2,
                .has_children   = false,
                .record_to_open = NULL,
              };
              ui_detail_row(tool, &tab->param_grid, header_row, NULL);

              if (!func->first_param) {
                ui_text_cell_t parts[] = {
                  UI_TEXT_CELL(tab->no_params, UI_C_TEXT),
                };

                ui_detail_row_t row = {
                  .parts          = parts,
                  .count          = COUNTOF(parts),
                  .depth          = 2,
                  .has_children   = false,
                  .record_to_open = NULL,
                };
                ui_detail_row(tool, &tab->single_grid, row, NULL);
              } else {
                for (detail_prop_t *prop = func->first_param; prop; prop = prop->next) {
                  draw_detail_prop(tool, tab, &tab->param_grid, prop, 2);
                }
              }
            }
          }
        }
      }

      nk_style_pop_style_item(tool->ctx);
      nk_style_pop_vec2(tool->ctx);

      nk_tree_pop(tool->ctx);
    }
  }
  scratch_end(tmp);
}

static void
draw_details_body(search_tool_t *tool, detail_tab_t *tab)
{
  draw_details_header(tool, tab);

  switch (tab->record->kind) {
  case UOBJECT_KIND_PACKAGE: {
    draw_package_section(tool, tab);
    break;
  }

  case UOBJECT_KIND_ENUM: {
    draw_detail_enum_section(tool, tab);
    break;
  }

  case UOBJECT_KIND_FUNC: {
    draw_detail_func_section(tool, tab);
    draw_detail_disasm_section(tool, tab);
    break;
  }

  case UOBJECT_KIND_CLASS:
  case UOBJECT_KIND_STRUCT: {
    draw_detail_layout_section_props(tool, tab);
    draw_detail_layout_section_funcs(tool, tab);
    draw_detail_layout_section_instances(tool, tab);
    draw_detail_disasm_section(tool, tab);
    break;
  }

  case UOBJECT_KIND_UNKNOWN:
  case UOBJECT_KIND_CDO:
  case UOBJECT_KIND_WORLD:
  case UOBJECT_KIND_LEVEL:
  case UOBJECT_KIND_ACTOR:
  case UOBJECT_KIND_ACTOR_COMPONENT:
  case UOBJECT_KIND_GAME_INSTANCE:
  case UOBJECT_KIND_LOCAL_PLAYER:
  case UOBJECT_KIND_PLAYER_CONTROLLER:
  case UOBJECT_KIND_PAWN:
  case UOBJECT_KIND_HUD:
  case UOBJECT_KIND_WIDGET:
  case UOBJECT_KIND_BLUEPRINT:
  case UOBJECT_KIND_DATA_ASSET:
  case UOBJECT_KIND_DATA_TABLE: {
    draw_detail_layout_section_props(tool, tab);
    break;
  }
  }
}

void
draw_details_panel(search_tool_t *tool, float panel_h)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);

  struct nk_context *ctx     = tool->ctx;
  float              total_h = 0.0f;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    struct nk_input *in = NULL;
    if (!(ctx->current->layout->flags & NK_WINDOW_ROM) && !(ctx->current->layout->flags & NK_WINDOW_NO_INPUT) && ui_nk_current_panel_accepts_input(ctx)) {
      in = &ctx->input;
    }

    if (in) {
      if (tool->details.count > 0) {
        if (keybind_str_is_pressed(STR_LIT("Ctrl+Tab"))) {
          detail_tab_prev(tool);
        } else if (keybind_str_is_pressed(STR_LIT("Tab"))) {
          detail_tab_next(tool);
        } else if (keybind_str_is_pressed(STR_LIT("Ctrl+W"))) {
          if (tool->details.selected_tab) {
            detail_tab_close(tool, tool->details.selected_tab);
          }
        }
      }
    }

    if (tool->details.count > 0) {
      nk_style_push_vec2(ctx, &ctx->style.scrollh.padding, nk_vec2(0.0f, 0.0f));
      nk_style_push_vec2(ctx, &ctx->style.window.scrollbar_size, nk_vec2(6.0f, 6.0f));
      nk_style_push_vec2(ctx, &ctx->style.window.group_padding, nk_vec2(0.0f, 0.0f));
      nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0.0f, 0.0f));
      nk_style_push_float(ctx, &ctx->style.button.rounding, 0.0f);
      nk_style_push_style_item(ctx, &ctx->style.button.normal, ctx->style.property.normal);
      nk_style_push_style_item(ctx, &ctx->style.button.hover, ctx->style.property.hover);
      nk_style_push_style_item(ctx, &ctx->style.button.active, ctx->style.property.active);
      nk_style_push_color(ctx, &ctx->style.button.border_color, ctx->style.property.border_color);

      float tab_h       = 18.0f;
      float text_pad    = 4.0f;
      float group_pad_y = ctx->style.window.group_padding.y;
      float group_pad_x = ctx->style.window.group_padding.x;
      float scroll_h    = ctx->style.window.scrollbar_size.y;

      float button_pad = ctx->style.button.padding.x + ctx->style.button.border + ctx->style.button.rounding;
      float x_width    = ui_text_width(ctx, STR_LIT("X")) + 2.0f * button_pad;

      float total_width = 0.0f;

      for (detail_tab_t *tab = tool->details.first; tab; tab = tab->next) {
        ASSERT(record_has_flag(tab->record, RECORD_FLAG_LIVE));
        ASSERT(!str_is_empty(tab->record->name.str));

        ui_text_span_t label = detail_tab_label(tab);
        total_width         += label.width + 2.0f * (text_pad + button_pad) + x_width;
      }

      float max_content_width   = nk_window_get_content_region_size(ctx).x;
      float total_content_width = total_width + 2.0f * group_pad_x;

      bool has_scrollbar = (total_content_width > max_content_width);

      total_h = group_pad_y + tab_h + group_pad_y + (has_scrollbar)*scroll_h;

      nk_layout_row_dynamic(ctx, total_h, 1);

      nk_flags flags = (has_scrollbar) ? NK_WINDOW_NO_SCROLLBAR_V : NK_WINDOW_NO_SCROLLBAR;
      if (nk_group_begin(ctx, "uobject_search.details.tabs", flags)) {
        nk_layout_row_begin(ctx, NK_STATIC, tab_h, tool->details.count * 2);
        {
          detail_tab_t *tab_to_close = NULL;
          detail_tab_t *tab          = tool->details.first;
          detail_tab_t *next         = NULL;

          while (tab) {
            next = tab->next;

            ui_text_span_t label = detail_tab_label(tab);

            nk_layout_row_push(ctx, label.width + 2.0f * (text_pad + button_pad));
            bool is_selected = (tool->details.selected_tab == tab);
            bool is_pinned   = (tab->pinned);

            if (is_selected) {
              nk_style_push_style_item(ctx, &ctx->style.button.normal, UI_ITEM(UI_C_ACCENT));
              nk_style_push_style_item(ctx, &ctx->style.button.hover, UI_ITEM(UI_C_ACCENT_HOVER));
              nk_style_push_style_item(ctx, &ctx->style.button.active, UI_ITEM(UI_C_ACCENT_ACTIVE));
              nk_style_push_color(ctx, &ctx->style.button.border_color, UI_C_BORDER_FOCUS);
            }

            if (is_pinned) {
              nk_style_push_color(ctx, &ctx->style.button.text_normal, UI_C_TEXT_MUTED);
              nk_style_push_color(ctx, &ctx->style.button.text_hover,  UI_C_TEXT_MUTED);
              nk_style_push_color(ctx, &ctx->style.button.text_active, UI_C_TEXT_MUTED);
            }

            if (ui_button_str(ctx, label.str)) {
              tool->details.selected_tab = tab;
              detail_tab_pin(tool, tab);
            }

            nk_layout_row_push(ctx, x_width);
            if (nk_button_symbol(ctx, NK_SYMBOL_X)) {
              tab_to_close = tab;
            }

            if (is_pinned) {
              nk_style_pop_color(ctx);
              nk_style_pop_color(ctx);
              nk_style_pop_color(ctx);
            }

            if (is_selected) {
              nk_style_pop_color(ctx);
              nk_style_pop_style_item(ctx);
              nk_style_pop_style_item(ctx);
              nk_style_pop_style_item(ctx);
            }

            tab  = next;
          }

          if (tab_to_close) {
            detail_tab_close(tool, tab_to_close);
          }
        }
        nk_layout_row_end(ctx);
        nk_group_end(ctx);
      }

      nk_style_pop_color(ctx);
      nk_style_pop_style_item(ctx);
      nk_style_pop_style_item(ctx);
      nk_style_pop_style_item(ctx);
      nk_style_pop_float(ctx);
      nk_style_pop_vec2(ctx);
      nk_style_pop_vec2(ctx);
      nk_style_pop_vec2(ctx);
      nk_style_pop_vec2(ctx);
    }

    float body_h = NK_MAX(panel_h - total_h, 0.0f);
    nk_layout_row_dynamic(ctx, body_h, 1);

    detail_tab_t *tab = tool->details.selected_tab;
    if (tab) {
      if (nk_group_scrolled_begin(ctx, &tab->body_scroll, "uobject_search.details.body", NK_WINDOW_BORDER)) {
        draw_details_body(tool, tab);
        nk_group_scrolled_end(ctx);
      }
    } else {
      if (nk_group_begin(ctx, "uobject_search.details.body.empty", NK_WINDOW_BORDER)) {
        nk_group_end(ctx);
      }
    }
  }
  scratch_end(tmp);
}
