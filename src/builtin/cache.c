#include "common.h"
#include "uobject_search_internal.h"

#include "globals.h"
#include "ui_nuklear.h"

bool
build_in_progress(search_tool_t *tool)
{
  ASSERT(tool != NULL);
  return tool->cache.build_cursor < tool->cache.build_total;
}

bool
record_slot_valid(uint32_t slot)
{
  return slot != RECORD_SLOT_INVALID;
}

record_t *
record_from_slot(search_tool_t *tool, uint32_t slot)
{
  if (!tool || slot == RECORD_SLOT_INVALID || slot >= tool->cache.record_cap) {
    return NULL;
  }

  return &tool->cache.records[slot];
}

uint32_t
record_to_slot(search_tool_t *tool, record_t *record)
{
  if (!tool || !record || !tool->cache.records) {
    return RECORD_SLOT_INVALID;
  }

  if (record < tool->cache.records || record >= tool->cache.records + tool->cache.record_cap) {
    return RECORD_SLOT_INVALID;
  }

  return (uint32_t)(record - tool->cache.records);
}

bool
record_is_type_record(record_t *record)
{
  if (!record) {
    return false;
  }
  return record->kind == UOBJECT_KIND_CLASS || record->kind == UOBJECT_KIND_STRUCT || record->kind == UOBJECT_KIND_FUNC;
}

bool
record_is_instance_record(record_t *record)
{
  if (!record) {
    return false;
  }
  return record->kind == UOBJECT_KIND_CDO || record->kind == UOBJECT_KIND_UNKNOWN;
}

bool
record_has_flag(record_t *record, record_flags_t flag)
{
  return record && ((record->flags & flag) != 0);
}

void
record_set_flag(record_t *record, record_flags_t flag, bool enabled)
{
  if (!record) {
    return;
  }

  if (enabled) {
    record->flags |= flag;
  } else {
    record->flags &= (uint8_t)~flag;
  }
}

bool
slot_list_init(arena_t *arena, slot_list_t *list, uint32_t cap)
{
  ASSERT(arena != NULL);
  ASSERT(list != NULL);

  list->cap   = cap;
  list->count = 0;
  list->slots = ARENA_PUSH_ARRAY_ZERO(arena, uint32_t, cap);

  return (list->slots != NULL);
}

void
slot_list_reset(slot_list_t *list)
{
  ASSERT(list != NULL);
  list->count = 0;
}

bool
push_back_pending(pending_queue_t *queue, uint32_t slot)
{
  ASSERT(queue != NULL);
  ASSERT(slot < queue->cap);

  if (queue->set[slot]) {
    return false;
  }

  if (queue->count == queue->cap) {
    return false;
  }

  queue->slots[queue->head++ % queue->cap]  = slot;
  queue->count                             += 1;
  queue->set[slot]                          = true;
  return true;
}

bool
pop_front_pending(pending_queue_t *queue, uint32_t *out_slot)
{
  ASSERT(queue != NULL);

  if (queue->count == 0) {
    return false;
  }

  uint32_t slot = queue->slots[queue->tail++ % queue->cap];

  queue->count     -= 1;
  queue->set[slot]  = false;

  if (out_slot) {
    *out_slot = slot;
  }
  return true;
}

bool
push_front_pending(pending_queue_t *queue, uint32_t slot)
{
  ASSERT(queue != NULL);
  ASSERT(slot < queue->cap);

  if (queue->set[slot]) {
    return false;
  }

  if (queue->count == queue->cap) {
    return false;
  }

  queue->tail = (queue->tail == 0) ? queue->cap - 1 : queue->tail - 1;

  queue->slots[queue->tail]  = slot;
  queue->count              += 1;
  queue->set[slot]           = true;
  return true;
}

void
clear_pending(pending_queue_t *queue)
{
  ASSERT(queue != NULL);

  queue->count = 0;
  queue->head  = 0;
  queue->tail  = 0;
  mem_zero(queue->set, queue->cap * sizeof(queue->set[0]));
}

void
add_live(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);
  ASSERT(!record_has_flag(record, RECORD_FLAG_LIVE));
  ASSERT(tool->cache.live_slots.count < tool->cache.live_slots.cap);

  record_set_flag(record, RECORD_FLAG_LIVE, true);
  record->live_list_idx                                      = tool->cache.live_slots.count;
  tool->cache.live_slots.slots[tool->cache.live_slots.count] = record_to_slot(tool, record);
  tool->cache.live_slots.count++;
}

void
remove_live(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  if (!record_has_flag(record, RECORD_FLAG_LIVE)) {
    return;
  }

  ASSERT(tool->cache.live_slots.slots != NULL);
  ASSERT(record->live_list_idx < tool->cache.live_slots.count);

  uint32_t last_idx   = tool->cache.live_slots.count - 1;
  uint32_t idx        = record->live_list_idx;
  uint32_t moved_slot = tool->cache.live_slots.slots[last_idx];
  if (idx != last_idx) {
    tool->cache.live_slots.slots[idx]             = moved_slot;
    tool->cache.records[moved_slot].live_list_idx = idx;
  }
  tool->cache.live_slots.count = last_idx;
  record->live_list_idx        = 0;
  record_set_flag(record, RECORD_FLAG_LIVE, false);
}

void
add_match(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));

  if (record_has_flag(record, RECORD_FLAG_MATCHED)) {
    return;
  }

  ASSERT(tool->search.matches.count < tool->search.matches.cap);

  record_set_flag(record, RECORD_FLAG_MATCHED, true);
  record->match_idx                                       = tool->search.matches.count;
  tool->search.matches.slots[tool->search.matches.count]  = record_to_slot(tool, record);
  tool->search.matches.count                             += 1;
}

void
remove_match(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  if (!record_has_flag(record, RECORD_FLAG_MATCHED)) {
    return;
  }

  ASSERT(tool->search.matches.slots != NULL);
  ASSERT(record->match_idx < tool->search.matches.count);

  uint32_t last_idx   = tool->search.matches.count - 1;
  uint32_t idx        = record->match_idx;
  uint32_t moved_slot = tool->search.matches.slots[last_idx];
  if (idx != last_idx) {
    tool->search.matches.slots[idx]           = moved_slot;
    tool->cache.records[moved_slot].match_idx = idx;
  }
  tool->search.matches.count = last_idx;
  record->match_idx          = 0;
  record_set_flag(record, RECORD_FLAG_MATCHED, false);
}

void
clear_match(search_tool_t *tool)
{
  ASSERT(tool != NULL);

  for (uint32_t i = 0; i < tool->search.matches.count; ++i) {
    uint32_t slot = tool->search.matches.slots[i];
    if (slot >= tool->cache.record_cap) {
      continue;
    }

    record_t *record  = &tool->cache.records[slot];
    record->match_idx = 0;
    record_set_flag(record, RECORD_FLAG_MATCHED, false);
  }

  tool->search.matches.count = 0;
}

void
add_visible(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));
  ASSERT(tool->search.visible.count < tool->search.visible.cap);

  if (!record_has_flag(record, RECORD_FLAG_VISIBLE)) {
    record_set_flag(record, RECORD_FLAG_VISIBLE, true);
    record->visible_idx                                    = tool->search.visible.count;
    tool->search.visible.slots[tool->search.visible.count] = record_to_slot(tool, record);
    tool->search.visible.count++;
  }
}

void
remove_visible(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);

  if (!record_has_flag(record, RECORD_FLAG_VISIBLE)) {
    return;
  }

  ASSERT(tool->search.visible.slots != NULL);
  ASSERT(record->visible_idx < tool->search.visible.count);

  uint32_t last_idx   = tool->search.visible.count - 1;
  uint32_t idx        = record->visible_idx;
  uint32_t moved_slot = tool->search.visible.slots[last_idx];
  if (idx != last_idx) {
    tool->search.visible.slots[idx]             = moved_slot;
    tool->cache.records[moved_slot].visible_idx = idx;
  }

  tool->search.visible.count = last_idx;
  record->visible_idx        = 0;
  record_set_flag(record, RECORD_FLAG_VISIBLE, false);
}

void
clear_visible(search_tool_t *tool)
{
  ASSERT(tool != NULL);

  for (uint32_t i = 0; i < tool->search.visible.count; ++i) {
    uint32_t slot = tool->search.visible.slots[i];
    if (slot >= tool->cache.record_cap) {
      continue;
    }

    record_t *record    = &tool->cache.records[slot];
    record->visible_idx = 0;
    record_set_flag(record, RECORD_FLAG_VISIBLE, false);
  }

  tool->search.visible.count = 0;
}

void
rebuild_matches(search_tool_t *tool)
{
  ASSERT(tool != NULL);

  clear_visible(tool);
  clear_pending(&tool->search.pending_select);

  clear_match(tool);
  clear_pending(&tool->search.pending_search_types);
  clear_pending(&tool->search.pending_search_instances);

  for (uint32_t i = 0; i < tool->cache.live_slots.count; ++i) {
    uint32_t  slot   = tool->cache.live_slots.slots[i];
    record_t *record = record_from_slot(tool, slot);
    ASSERT(record != NULL);
    if (record_has_flag(record, RECORD_FLAG_LIVE)) {
      if (record_is_type_record(record)) {
        push_back_pending(&tool->search.pending_search_types, slot);
      } else {
        push_back_pending(&tool->search.pending_search_instances, slot);
      }
    }
  }
  tool->search.search_cursor  = 0;
  tool->search.search_total   = tool->search.pending_search_types.count + tool->search.pending_search_instances.count;
  tool->search.gen           += 1;
}

void
rebuild_visible(search_tool_t *tool)
{
  ASSERT(tool != NULL);

  clear_visible(tool);
  clear_pending(&tool->search.pending_select);

  for (uint32_t i = 0; i < tool->search.matches.count; ++i) {
    uint32_t  slot   = tool->search.matches.slots[i];
    record_t *record = record_from_slot(tool, slot);
    ASSERT(record != NULL);
    ASSERT(record_has_flag(record, RECORD_FLAG_MATCHED));

    if (record_has_flag(record, RECORD_FLAG_LIVE)) {
      push_back_pending(&tool->search.pending_select, slot);
    }
  }
}

bool
pending_queue_init(arena_t *arena, pending_queue_t *list, uint32_t cap)
{
  ASSERT(arena != NULL);
  ASSERT(list != NULL);

  list->cap   = cap;
  list->count = 0;
  list->head  = 0;
  list->tail  = 0;
  list->set   = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, cap);
  list->slots = ARENA_PUSH_ARRAY_ZERO(arena, uint32_t, cap);

  return (list->set != NULL && list->slots != NULL);
}

bool
cache_name(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(record != NULL);
  ASSERT(record->obj != NULL);

  if (!str_is_empty(record->name.str)) {
    return true;
  }

  if (str_is_empty(record->full_name.str)) {
    uint64_t full_name_len = unreal_uobject_get_full_name_len(record->obj);
    record->full_name.str  = string_pool_realloc(&tool->cache.strings, record->full_name.str, (uint32_t)full_name_len);
    if (str_is_empty(record->full_name.str)) {
      return false;
    }
  }

  str_t    full_name = record->full_name.str;
  uint64_t name_len  = unreal_uobject_get_name_len(record->obj);
  str_t    name      = str_make(&full_name.data[full_name.len - name_len], name_len);

  unreal_uobject_write_name(record->obj, name.data, name.len);
  record->name = ui_text_span_make(tool->ctx, name);
  return true;
}

bool
cache_outer_name(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(record != NULL);
  ASSERT(record->obj != NULL);

  if (!str_is_empty(record->outer_name.str)) {
    return true;
  }

  if (str_is_empty(record->full_name.str)) {
    uint64_t full_name_len = unreal_uobject_get_full_name_len(record->obj);
    record->full_name.str  = string_pool_realloc(&tool->cache.strings, record->full_name.str, (uint32_t)full_name_len);
    if (str_is_empty(record->full_name.str)) {
      return false;
    }
  }

  if (record->parent_slot == RECORD_SLOT_INVALID) {
    record->outer_name = ui_text_span_make(tool->ctx, STR_NULL);
    return true;
  }

  str_t    full_name = record->full_name.str;
  uint64_t name_len  = record->name.str.len;

  if (str_is_empty(record->name.str)) {
    name_len = unreal_uobject_get_name_len(record->obj);
  }

  uint64_t outer_name_len = full_name.len - (name_len + 1);
  str_t    outer_name     = str_make(&full_name.data[0], outer_name_len);

  unreal_uobject_write_full_name(record->obj, outer_name.data, outer_name.len + 1 /* '.' */);
  record->outer_name = ui_text_span_make(tool->ctx, outer_name);
  return true;
}

bool
cache_names(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(record != NULL);
  ASSERT(record->obj != NULL);

  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));
  ASSERT(record->type_slot != RECORD_SLOT_INVALID);

  record_t *type_record = record_from_slot(tool, record->type_slot);
  ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));

  if (str_is_empty(type_record->name.str)) {
    if (!cache_name(tool, type_record)) {
      return false;
    }
  }

  if (str_is_empty(record->name.str)) {
    if (!cache_name(tool, record)) {
      return false;
    }
  }

  if (record->parent_slot != RECORD_SLOT_INVALID && str_is_empty(record->outer_name.str)) {
    if (!cache_outer_name(tool, record)) {
      return false;
    }
  }

  if (record->parent_slot != RECORD_SLOT_INVALID) {
    record->full_name.width = record->outer_name.width + ui_text_width(tool->ctx, STR_LIT(".")) + record->name.width;
  } else {
    record->full_name.width = record->name.width;
  }
  return true;
}

void
release_names(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  string_pool_release(&tool->cache.strings, record->full_name.str);

  record->full_name  = (ui_text_span_t){0};
  record->outer_name = (ui_text_span_t){0};
  record->name       = (ui_text_span_t){0};
}

static void
link_record(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));
  ASSERT(record->obj);
  ASSERT(record->type_slot == RECORD_SLOT_INVALID);
  ASSERT(record->parent_slot == RECORD_SLOT_INVALID);
  ASSERT(record->prev_sibling_slot == RECORD_SLOT_INVALID);
  ASSERT(record->next_sibling_slot == RECORD_SLOT_INVALID);
  ASSERT(record->prev_instance_slot == RECORD_SLOT_INVALID);
  ASSERT(record->next_instance_slot == RECORD_SLOT_INVALID);

  if (record->obj->outer) {
    int32_t outer_slot = record->obj->outer->internal_idx;
    MASSERT(outer_slot >= 0 && (uint32_t)outer_slot < tool->cache.record_cap, "invalid slot: %d", outer_slot);

    record_t *outer_record = record_from_slot(tool, outer_slot);
    ASSERT(outer_record);
    ASSERT(record_has_flag(outer_record, RECORD_FLAG_LIVE));
    ASSERT(record->obj->outer == outer_record->obj);

    record->parent_slot       = outer_slot;
    record->prev_sibling_slot = outer_record->last_child_slot;

    uint32_t record_slot = record_to_slot(tool, record);

    record_t *last_child_record = record_from_slot(tool, outer_record->last_child_slot);
    if (last_child_record) {
      last_child_record->next_sibling_slot = record_slot;
    } else {
      outer_record->first_child_slot = record_slot;
    }

    outer_record->last_child_slot = record_slot;
  }

  uobject_t *type_obj  = (uobject_t *)record->obj->cls;
  int32_t    type_slot = type_obj->internal_idx;
  ASSERT((uint32_t)type_slot < tool->cache.record_cap);

  record_t *type_record = record_from_slot(tool, type_slot);
  ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));
  ASSERT(type_record->obj == type_obj);

  record->type_slot = type_slot;

  uint32_t record_slot = record_to_slot(tool, record);

  record->prev_instance_slot = type_record->last_instance_slot;

  record_t *last_instance_record = record_from_slot(tool, type_record->last_instance_slot);
  if (last_instance_record) {
    last_instance_record->next_instance_slot = record_slot;
  } else {
    type_record->first_instance_slot = record_slot;
  }

  type_record->last_instance_slot  = record_slot;
  type_record->instance_count     += 1;
}

static void
detach_instance_list(search_tool_t *tool, record_t *type_record)
{
  ASSERT(tool != NULL);
  ASSERT(type_record != NULL);

  uint32_t type_slot = record_to_slot(tool, type_record);

  uint32_t instance_slot = type_record->first_instance_slot;
  while (record_slot_valid(instance_slot)) {
    record_t *instance_record = record_from_slot(tool, instance_slot);
    ASSERT(instance_record != NULL);

    uint32_t next_slot = instance_record->next_instance_slot;

    if (instance_record->type_slot == type_slot) {
      instance_record->type_slot          = RECORD_SLOT_INVALID;
      instance_record->prev_instance_slot = RECORD_SLOT_INVALID;
      instance_record->next_instance_slot = RECORD_SLOT_INVALID;
    }

    instance_slot = next_slot;
  }

  type_record->first_instance_slot = RECORD_SLOT_INVALID;
  type_record->last_instance_slot  = RECORD_SLOT_INVALID;
  type_record->instance_count      = 0;
}

static void
unlink_record(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);
  ASSERT(record_has_flag(record, RECORD_FLAG_LIVE));
  ASSERT(record->obj);

  record_t *parent_record = record_from_slot(tool, record->parent_slot);
  if (parent_record && record_has_flag(parent_record, RECORD_FLAG_LIVE) && parent_record->obj == record->obj->outer) {
    record_t *prev_sibling_record = record_from_slot(tool, record->prev_sibling_slot);
    if (prev_sibling_record) {
      prev_sibling_record->next_sibling_slot = record->next_sibling_slot;
    } else {
      parent_record->first_child_slot = record->next_sibling_slot;
    }

    record_t *next_sibling_record = record_from_slot(tool, record->next_sibling_slot);
    if (next_sibling_record) {
      next_sibling_record->prev_sibling_slot = record->prev_sibling_slot;
    } else {
      parent_record->last_child_slot = record->prev_sibling_slot;
    }
  }

  /* unlink this record from its class/type instance list */
  record_t *type_record = record_from_slot(tool, record->type_slot);
  if (type_record && record_has_flag(type_record, RECORD_FLAG_LIVE) && type_record->obj == (uobject_t *)record->obj->cls) {
    record_t *prev_instance_record = record_from_slot(tool, record->prev_instance_slot);
    if (prev_instance_record) {
      prev_instance_record->next_instance_slot = record->next_instance_slot;
    } else {
      type_record->first_instance_slot = record->next_instance_slot;
    }

    record_t *next_instance_record = record_from_slot(tool, record->next_instance_slot);
    if (next_instance_record) {
      next_instance_record->prev_instance_slot = record->prev_instance_slot;
    } else {
      type_record->last_instance_slot = record->prev_instance_slot;
    }

    ASSERT(type_record->instance_count > 0);
    type_record->instance_count -= 1;
  }

  /* unlink instances whose type is this record */
  if (record_slot_valid(record->first_instance_slot) || record->instance_count > 0) {
    detach_instance_list(tool, record);
  }

  /* detach outer children */
  uint32_t child_slot = record->first_child_slot;
  while (record_slot_valid(child_slot)) {
    record_t *child_record = record_from_slot(tool, child_slot);
    ASSERT(child_record != NULL);

    uint32_t next_slot = child_record->next_sibling_slot;

    child_record->parent_slot       = RECORD_SLOT_INVALID;
    child_record->prev_sibling_slot = RECORD_SLOT_INVALID;
    child_record->next_sibling_slot = RECORD_SLOT_INVALID;

    child_slot = next_slot;
  }

  record->type_slot           = RECORD_SLOT_INVALID;
  record->parent_slot         = RECORD_SLOT_INVALID;
  record->first_child_slot    = RECORD_SLOT_INVALID;
  record->last_child_slot     = RECORD_SLOT_INVALID;
  record->prev_sibling_slot   = RECORD_SLOT_INVALID;
  record->next_sibling_slot   = RECORD_SLOT_INVALID;
  record->first_instance_slot = RECORD_SLOT_INVALID;
  record->last_instance_slot  = RECORD_SLOT_INVALID;
  record->prev_instance_slot  = RECORD_SLOT_INVALID;
  record->next_instance_slot  = RECORD_SLOT_INVALID;
  record->instance_count      = 0;
}

bool
remove_slot(search_tool_t *tool, uint32_t slot)
{
  record_t *record = record_from_slot(tool, slot);
  if (!record || !record_has_flag(record, RECORD_FLAG_LIVE)) {
    return false;
  }

  remove_visible(tool, record);
  remove_match(tool, record);
  unlink_record(tool, record);
  release_names(tool, record);
  remove_live(tool, record);

  details_clear_stale(tool);

  mem_zero(record, sizeof(*record));

  record->type_slot           = RECORD_SLOT_INVALID;
  record->parent_slot         = RECORD_SLOT_INVALID;
  record->first_child_slot    = RECORD_SLOT_INVALID;
  record->last_child_slot     = RECORD_SLOT_INVALID;
  record->prev_sibling_slot   = RECORD_SLOT_INVALID;
  record->next_sibling_slot   = RECORD_SLOT_INVALID;
  record->first_instance_slot = RECORD_SLOT_INVALID;
  record->last_instance_slot  = RECORD_SLOT_INVALID;
  record->prev_instance_slot  = RECORD_SLOT_INVALID;
  record->next_instance_slot  = RECORD_SLOT_INVALID;
  record->instance_count      = 0;
  return true;
}

bool
cache_slot(search_tool_t *tool, uint32_t slot)
{
  record_t *record = record_from_slot(tool, slot);
  if (!record) {
    return false;
  }

  fuobject_item_t *item = unreal_uobject_array_get_item(slot);
  if (!item || !item->obj) {
    return false;
  }

  if (!unreal_uobject_array_item_is_valid(item)) {
    return false;
  }

  if (record_has_flag(record, RECORD_FLAG_LIVE)) {
    if (uobject_item_matches_record(item, record)) {
      add_to_search(tool, record);
      return true;
    }

    remove_slot(tool, slot);
  }

  record->obj                 = item->obj;
  record->kind                = uobject_kind(item->obj);
  record->fname               = item->obj->name;
  record->type_slot           = RECORD_SLOT_INVALID;
  record->parent_slot         = RECORD_SLOT_INVALID;
  record->first_child_slot    = RECORD_SLOT_INVALID;
  record->last_child_slot     = RECORD_SLOT_INVALID;
  record->prev_sibling_slot   = RECORD_SLOT_INVALID;
  record->next_sibling_slot   = RECORD_SLOT_INVALID;
  record->first_instance_slot = RECORD_SLOT_INVALID;
  record->last_instance_slot  = RECORD_SLOT_INVALID;
  record->prev_instance_slot  = RECORD_SLOT_INVALID;
  record->next_instance_slot  = RECORD_SLOT_INVALID;
  record->instance_count      = 0;

  add_live(tool, record);
  add_to_search(tool, record);
  push_back_pending(&tool->cache.pending_slots, slot);

  return true;
}

void
on_uobject_created(uobject_t *obj, int32_t idx, void *user)
{
  (void)obj;

  search_tool_t *tool = user;
  if (!tool || idx < 0) {
    return;
  }

  uint32_t slot = (uint32_t)idx;
  cache_slot(tool, slot);
}

void
on_uobject_deleted(uobject_t *obj, int32_t idx, void *user)
{
  (void)obj;

  search_tool_t *tool = user;
  if (!tool || idx < 0) {
    return;
  }

  uint32_t slot = (uint32_t)idx;

#if BUILD_DEBUG
  if (slot < tool->cache.record_cap) {
    record_t *record = record_from_slot(tool, slot);
    if (record_has_flag(record, RECORD_FLAG_LIVE)) {
      ASSERT(record->obj == obj);
    }
  }
#endif

  remove_slot(tool, slot);
}

static void
process_pending_slot(search_tool_t *tool, record_t *record)
{
  ASSERT(tool != NULL);
  ASSERT(record != NULL);

  link_record(tool, record);
}

bool
build_cache_tick(search_tool_t *tool, uint64_t start_us)
{
  while (tool->cache.build_cursor < tool->cache.build_total) {
    if (work_budget_exhausted(tool, start_us)) {
      return false;
    }

    cache_slot(tool, tool->cache.build_cursor++);
  }
  return true;
}

void
build_cache_all(search_tool_t *tool)
{
  while (tool->cache.build_cursor < tool->cache.build_total) {
    cache_slot(tool, tool->cache.build_cursor++);
  }
}

bool
process_pending_slots_tick(search_tool_t *tool, uint64_t start_us)
{
  while (tool->cache.pending_slots.count > 0) {
    if (work_budget_exhausted(tool, start_us)) {
      return false;
    }

    uint32_t slot = 0;
    if (!pop_front_pending(&tool->cache.pending_slots, &slot)) {
      break;
    }

    record_t *record = record_from_slot(tool, slot);
    if (record_has_flag(record, RECORD_FLAG_LIVE)) {
      process_pending_slot(tool, record);
    }
  }

  return true;
}
