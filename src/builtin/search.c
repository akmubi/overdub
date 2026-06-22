#include "scratch.h"
#include "uobject_search_internal.h"

#include "globals.h"

bool
uobject_item_matches_record(fuobject_item_t *item, record_t *record)
{
  if (!item || !item->obj || !record) {
    return false;
  }

  return item->obj == record->obj && unreal_fname_equal(item->obj->name, record->fname, false);
}

bool
record_matches_query(search_tool_t *tool, record_t *record)
{
  if (query_is_empty(&tool->search.query) || !record || !record_has_flag(record, RECORD_FLAG_LIVE) || !record->obj) {
    return false;
  }

  fname_t        name  = record->obj->name;
  fname_entry_t *entry = unreal_fname_entry_get(name.cmp_idx);
  if (!entry) {
    return false;
  }

  if (entry->header.len == 0) {
    return false;
  }

  return query_matches_fname_entry(entry, &tool->search.query, tool->search.ignore_case, tool->search.exact_match);
}

static inline bool
fprop_class_is(fprop_t *prop, fname_t name)
{
  return prop && prop->base.cls && unreal_fname_equal(prop->base.cls->name, name, false);
}

static bool
query_matches_uobject_name(search_tool_t *tool, uobject_t *obj)
{
  if (!obj) {
    return false;
  }

  return query_matches_fname(obj->name, &tool->search.query, tool->search.ignore_case, tool->search.exact_match);
}

static bool
prop_matches_query(search_tool_t *tool, fprop_t *prop)
{
  if (!prop) {
    return false;
  }

  /* property name itself: Conditions, AchievementName, StatName, etc. */
  if (query_matches_fname(prop->base.name, &tool->search.query, tool->search.ignore_case, tool->search.exact_match)) {
    return true;
  }

  /* property class/type name: ArrayProperty, StructProperty, ObjectProperty, etc. */
  if (prop->base.cls && query_matches_fname(prop->base.cls->name, &tool->search.query, tool->search.ignore_case, tool->search.exact_match)) {
    return true;
  }

  if (fprop_class_is(prop, globals.unreal.struct_prop)) {
    fprop_struct_t *p = (fprop_struct_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->script_struct)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.array_prop)) {
    fprop_array_t *p = (fprop_array_t *)prop;

    if (prop_matches_query(tool, p->inner)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.set_prop)) {
    fprop_set_t *p = (fprop_set_t *)prop;

    if (prop_matches_query(tool, p->elem_prop)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.map_prop)) {
    fprop_map_t *p = (fprop_map_t *)prop;

    if (prop_matches_query(tool, p->key_prop) || prop_matches_query(tool, p->val_prop)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.obj_prop) || fprop_class_is(prop, globals.unreal.weak_obj_prop) || fprop_class_is(prop, globals.unreal.lazy_obj_prop) ||
             fprop_class_is(prop, globals.unreal.soft_obj_prop)) {
    fprop_obj_base_t *p = (fprop_obj_base_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->prop_class)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.class_prop)) {
    fprop_class_t *p = (fprop_class_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->base.base.prop_class) || query_matches_uobject_name(tool, (uobject_t *)p->meta_class)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.soft_class_prop)) {
    fprop_class_soft_t *p = (fprop_class_soft_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->base.base.prop_class) || query_matches_uobject_name(tool, (uobject_t *)p->meta_class)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.interface_prop)) {
    fprop_iface_t *p = (fprop_iface_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->iface_class)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.byte_prop)) {
    fprop_byte_t *p = (fprop_byte_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->uenum)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.enum_prop)) {
    fprop_enum_t *p = (fprop_enum_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->uenum) || prop_matches_query(tool, (fprop_t *)p->underlying_prop)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.delegate_prop)) {
    fprop_delegate_t *p = (fprop_delegate_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->signature_func)) {
      return true;
    }
  } else if (fprop_class_is(prop, globals.unreal.mcast_delegate_prop) || fprop_class_is(prop, globals.unreal.mcast_inline_delegate_prop) ||
             fprop_class_is(prop, globals.unreal.mcast_sparse_delegate_prop)) {
    fprop_mcast_delegate_t *p = (fprop_mcast_delegate_t *)prop;

    if (query_matches_uobject_name(tool, (uobject_t *)p->signature_func)) {
      return true;
    }
  }

  return false;
}

static ustruct_t *
record_get_property_owner(record_t *record)
{
  if (!record || !record->obj) {
    return NULL;
  }

  uobject_t *obj = record->obj;

  if (unreal_uobject_is_a(obj, globals.unreal.core_class) || unreal_uobject_is_a(obj, globals.unreal.core_scriptstruct) || unreal_uobject_is_a(obj, globals.unreal.core_func)) {
    return (ustruct_t *)obj;
  }

  if (obj->cls) {
    return (ustruct_t *)obj->cls;
  }

  return NULL;
}

bool
record_props_match_query(search_tool_t *tool, record_t *record)
{
  if (query_is_empty(&tool->search.query) || !record || !record_has_flag(record, RECORD_FLAG_LIVE)) {
    return false;
  }

  ustruct_t *owner = record_get_property_owner(record);
  if (!owner) {
    return false;
  }

  for (ffield_t *field = owner->child_props; field; field = field->next) {
    fprop_t *prop = (fprop_t *)field;

    if (prop_matches_query(tool, prop)) {
      return true;
    }
  }

  return false;
}

bool
record_passes_visible_filter(search_tool_t *tool, record_t *record)
{
  if (!record_has_flag(record, RECORD_FLAG_LIVE) || !record_has_flag(record, RECORD_FLAG_MATCHED)) {
    return false;
  }

  if (tool->search.has_byte_code) {
    if (!record->obj) {
      return false;
    }

    if (!unreal_uobject_is_a(record->obj, globals.unreal.core_func) && !unreal_uobject_is_a(record->obj, globals.unreal.core_scriptstruct) &&
        !unreal_uobject_is_a(record->obj, globals.unreal.core_class)) {
      return false;
    }

    ustruct_t *ustruct = (ustruct_t *)record->obj;
    if (!ustruct->script.data || ustruct->script.num <= 0) {
      return false;
    }
  }

  return AS_BOOL(tool->search.enabled_kind_mask & FLAG(record->kind));
}

void
update_visible(search_tool_t *tool, record_t *record)
{
  if (record_has_flag(record, RECORD_FLAG_LIVE)) {
    if (record_passes_visible_filter(tool, record)) {
      add_visible(tool, record);
      ASSERT(cache_names(tool, record));
    } else {
      remove_visible(tool, record);
    }
  }
}

static void
add_type_match_children(search_tool_t *tool, record_t *owner)
{
  ASSERT(tool != NULL);
  ASSERT(owner != NULL);
  ASSERT(owner->obj != NULL);
  ASSERT(record_has_flag(owner, RECORD_FLAG_LIVE));

  ustruct_t *type = (ustruct_t *)owner->obj;
  for (ufield_t *field = type->children; field; field = field->next) {
    uobject_t *child_obj = (uobject_t *)field;
    int32_t    slot      = child_obj->internal_idx;

    if (slot < 0 || (uint32_t)slot >= tool->cache.record_cap) {
      continue;
    }

    record_t *child_record = record_from_slot(tool, slot);
    if (!child_record || !record_has_flag(child_record, RECORD_FLAG_LIVE) || child_record->obj != child_obj) {
      continue;
    }

    child_record->checked_gen = tool->search.gen;
    add_match(tool, child_record);

    push_back_pending(&tool->search.pending_select, slot);
  }
}

static bool
has_type_matched(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  record_t *type_record = record_from_slot(tool, record->type_slot);
  ASSERT(type_record != NULL);
  ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));

  return type_record->checked_gen == tool->search.gen && record_has_flag(type_record, RECORD_FLAG_MATCHED);
}

bool
update_match(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  if (!record_has_flag(record, RECORD_FLAG_LIVE)) {
    return false;
  }

  if (record->checked_gen != tool->search.gen) {
    record->checked_gen = tool->search.gen;

    bool matched = false;

    if (!matched && tool->search.match_outer) {
      record_t *parent_record = record_from_slot(tool, record->parent_slot);
      if (parent_record) {
        ASSERT(record_has_flag(parent_record, RECORD_FLAG_LIVE));

        if (update_match(tool, parent_record)) {
          matched = true;
        }
      }
    }

    if (!matched && tool->search.match_type_instances && record_is_instance_record(record) && has_type_matched(tool, record)) {
      matched = true;
    }

    if (!matched && record_matches_query(tool, record)) {
      matched = true;
    }

    if (!matched && tool->search.match_props && record_props_match_query(tool, record)) {
      matched = true;
    }

    if (matched) {
      add_match(tool, record);

      if (tool->search.match_type_children && (record->kind == UOBJECT_KIND_CLASS || record->kind == UOBJECT_KIND_STRUCT)) {
        add_type_match_children(tool, record);
      }
    } else {
      remove_visible(tool, record);
      remove_match(tool, record);
    }

    push_back_pending(&tool->search.pending_select, record_to_slot(tool, record));
  }
  return record_has_flag(record, RECORD_FLAG_MATCHED);
}

bool
add_to_search(search_tool_t *tool, record_t *record)
{
  if (query_is_empty(&tool->search.query)) {
    return true;
  }

  uint32_t slot = record_to_slot(tool, record);

  bool ok = false;
  if (record_is_type_record(record)) {
    ok = push_back_pending(&tool->search.pending_search_types, slot);
  } else {
    ok = push_back_pending(&tool->search.pending_search_instances, slot);
  }

  if (ok) {
    tool->search.search_total += 1;
  }
  return ok;
}

bool
search_matching_tick(search_tool_t *tool, uint64_t start_us)
{
  while (tool->search.pending_search_types.count > 0) {
    if (work_budget_exhausted(tool, start_us)) {
      return false;
    }

    uint32_t slot = 0;
    if (!pop_front_pending(&tool->search.pending_search_types, &slot)) {
      break;
    }

    record_t *record = record_from_slot(tool, slot);
    if (record_has_flag(record, RECORD_FLAG_LIVE)) {
      update_match(tool, record);
    }
    tool->search.search_cursor += 1;
  }

  while (tool->search.pending_search_instances.count > 0) {
    if (work_budget_exhausted(tool, start_us)) {
      return false;
    }

    uint32_t slot = 0;
    if (!pop_front_pending(&tool->search.pending_search_instances, &slot)) {
      break;
    }

    record_t *record = record_from_slot(tool, slot);
    if (record_has_flag(record, RECORD_FLAG_LIVE)) {
      update_match(tool, record);
    }
    tool->search.search_cursor += 1;
  }

  return true;
}

bool
select_visible_tick(search_tool_t *tool, uint64_t start_us)
{
  while (tool->search.pending_select.count > 0) {
    if (work_budget_exhausted(tool, start_us)) {
      return false;
    }

    uint32_t slot = 0;
    if (!pop_front_pending(&tool->search.pending_select, &slot)) {
      break;
    }

    record_t *record = record_from_slot(tool, slot);
    if (record_has_flag(record, RECORD_FLAG_LIVE) && record_has_flag(record, RECORD_FLAG_MATCHED)) {
      update_visible(tool, record);
    }
  }
  return true;
}

void
start_search(search_tool_t *tool, str_t query)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    query_set(&tool->search.query, query);

    if (query_is_empty(&tool->search.query)) {
      clear_visible(tool);
      clear_pending(&tool->search.pending_select);

      clear_match(tool);
      clear_pending(&tool->search.pending_search_types);
      clear_pending(&tool->search.pending_search_instances);
      tool->search.search_cursor = 0;
      tool->search.search_total  = 0;
    } else {
      rebuild_matches(tool);
    }
  }
  scratch_end(tmp);
}
