#include "mod_unreal.h"

#include <string.h>

unreal_cached_objects_t g_cached_objects = {0};

void
unreal_cache_objects(void)
{
  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    tmp_arena_t tmp       = mod_scratch_begin(MOD_ARENA_INVALID);
    str_t       full_name = unreal_uobject_push_full_name(obj, tmp.arena);

    if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Object"), 0)) {
      g_cached_objects.core_object = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Class"), 0)) {
      g_cached_objects.core_class = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.ScriptStruct"), 0)) {
      g_cached_objects.core_scriptstruct = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Function"), 0)) {
      g_cached_objects.core_func = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Enum"), 0)) {
      g_cached_objects.core_enum = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Package"), 0)) {
      g_cached_objects.core_package = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Actor"), 0)) {
      g_cached_objects.actor = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.PlayerController"), 0)) {
      g_cached_objects.player_ctrl = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.LocalPlayer"), 0)) {
      g_cached_objects.local_player = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Pawn"), 0)) {
      g_cached_objects.pawn = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.HUD"), 0)) {
      g_cached_objects.hud = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.World"), 0)) {
      g_cached_objects.world = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.GameInstance"), 0)) {
      g_cached_objects.game_instance = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Level"), 0)) {
      g_cached_objects.level = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.ActorComponent"), 0)) {
      g_cached_objects.actor_component = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/UMG.Widget"), 0)) {
      g_cached_objects.widget = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Blueprint"), 0)) {
      g_cached_objects.blueprint = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.DataAsset"), 0)) {
      g_cached_objects.data_asset = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.DataTable"), 0)) {
      g_cached_objects.data_table = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Default__GameplayStatics"), 0)) {
      g_cached_objects.gameplay_statics_cdo = obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass"), 0)) {
      g_cached_objects.begin_spawn = (ufunc_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.GameplayStatics.FinishSpawningActor"), 0)) {
      g_cached_objects.finish_spawn = (ufunc_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Actor.K2_DestroyActor"), 0)) {
      g_cached_objects.destroy_actor = (ufunc_t *)obj;
    }
    mod_scratch_end(tmp);
  }
}

static inline int
u32_count_digits(uint32_t v)
{
  int count = 1;
  while (v >= 10) {
    v     /= 10;
    count += 1;
  }
  return count;
}

fname_pool_t *
unreal_get_name_pool(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->get_name_pool() : NULL;
}

bool
unreal_fname_equal(fname_t a, fname_t b, bool ignore_num)
{
  if (ignore_num) {
    return a.cmp_idx == b.cmp_idx;
  }
  return a.cmp_idx == b.cmp_idx && a.num == b.num;
}

fname_entry_t *
unreal_fname_entry_get(uint32_t cmp_idx)
{
  fname_pool_t *name_pool = unreal_get_name_pool();
  if (!name_pool) {
    /* should not happen */
    return NULL;
  }

  uint32_t block  = cmp_idx >> FNAME_BLOCK_OFFSET_BITS;              // high bits
  uint32_t offset = cmp_idx & ((1u << FNAME_BLOCK_OFFSET_BITS) - 1); // low 16 bits

  if (block >= FNAME_MAX_BLOCKS) {
    return NULL;
  }

  uint8_t *base = (uint8_t *)name_pool->entries.blocks[block];
  if (!base) {
    return NULL;
  }

  return (fname_entry_t *)(base + offset * 2);
}

uint64_t
unreal_fname_utf8_len(fname_t name)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->fname_utf8_len(name) : 0ULL;
}

uint64_t
unreal_fname_utf8_write(uint8_t *buf, uint64_t max_len, fname_t name)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host && buf && max_len > 0) ? host->fname_utf8_write(name, buf, max_len) : 0ULL;
}

str_t
unreal_fname_to_str(fname_t fname, mod_arena_t arena)
{
  str_t    result = STR_NULL;
  uint64_t len    = unreal_fname_utf8_len(fname);
  uint8_t *buf    = mod_arena_push(arena, len + 1, 1);
  if (buf) {
    MOD_ASSERT(unreal_fname_utf8_write(buf, len, fname) == len);
    buf[len] = '\0';
    result   = str_make(buf, len);
  }

  return result;
}

fname_t
unreal_fname_from_str(str_t str, efind_name_t find_type)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->fname_from_str(str, find_type) : (fname_t){0};
}

#define FNAME_BLOCK_BYTES ((1u << FNAME_BLOCK_OFFSET_BITS) * 2u)

static uint32_t
unreal_fname_entry_size_bytes(fname_entry_t *entry)
{
  if (!entry) {
    return 0;
  }

  uint32_t len  = entry->header.len;
  uint32_t size = sizeof(fname_entry_header_t);

  if (entry->header.is_wide) {
    size += len * sizeof(uint16_t);
  } else {
    size += len;
  }

  return (uint32_t)ALIGN_UP(size, 2);
}

void
unreal_fname_pool_iterate(fname_pool_iter_cb_t cb, void *user)
{
  if (!cb) {
    return;
  }

  fname_pool_t *name_pool = unreal_get_name_pool();
  if (!name_pool) {
    return;
  }

  fname_entry_allocator_t *alloc = &name_pool->entries;

  for (uint32_t block_idx = 0; block_idx <= alloc->current_block && block_idx < FNAME_MAX_BLOCKS; ++block_idx) {
    uint8_t *block = alloc->blocks[block_idx];
    if (!block) {
      continue;
    }

    uint32_t block_limit = FNAME_BLOCK_BYTES;
    if (block_idx == alloc->current_block) {
      block_limit = alloc->current_byte_cursor;
    }

    for (uint32_t byte_offset = 0; byte_offset + sizeof(fname_entry_header_t) <= block_limit;) {
      fname_entry_t *entry = (fname_entry_t *)(block + byte_offset);

      uint32_t entry_size = unreal_fname_entry_size_bytes(entry);
      if (entry_size == 0 || byte_offset + entry_size > block_limit) {
        break;
      }

      fname_t name = {
        .cmp_idx = (block_idx << FNAME_BLOCK_OFFSET_BITS) | (byte_offset / 2),
        .num     = 0,
      };

      if (!cb(name, entry, user)) {
        return;
      }

      byte_offset += entry_size;
    }
  }
}

static bool
unreal_fname_entry_match_text_ansi(fname_entry_t *entry, str_t text, bool ignore_case)
{
  uint32_t last = (uint32_t)entry->header.len - (uint32_t)text.len;
  for (uint32_t i = 0; i <= last; ++i) {
    bool match = true;

    for (uint32_t j = 0; j < text.len; ++j) {
      uint8_t a = entry->name.ansi[i + j];
      uint8_t b = text.data[j];

      if (ignore_case) {
        a = ascii_to_lower(a);
        b = ascii_to_lower(b);
      }

      if (a != b) {
        match = false;
        break;
      }
    }

    if (match) {
      return true;
    }
  }

  return false;
}

static bool
unreal_fname_entry_match_text_wide(fname_entry_t *entry, str_t text, bool ignore_case)
{
  uint32_t last = (uint32_t)entry->header.len - (uint32_t)text.len;
  for (uint32_t i = 0; i <= last; ++i) {
    bool match = true;

    for (uint32_t j = 0; j < text.len; ++j) {
      uint16_t a = entry->name.wide[i + j];
      uint16_t b = text.data[j];

      if (ignore_case) {
        a = utf16_ascii_to_lower(a);
        b = utf16_ascii_to_lower(b);
      }

      if (a != b) {
        match = false;
        break;
      }
    }

    if (match) {
      return true;
    }
  }

  return false;
}

/* NOTE: text matching is ASCII-oriented. UTF-8 multibyte queries are not decoded when matching wide FName entries */
bool
unreal_fname_entry_match_text(fname_entry_t *entry, str_t text, bool ignore_case, bool exact_match)
{
  if (!entry || str_is_empty(text) || entry->header.len == 0) {
    return false;
  }

  if (exact_match && text.len != entry->header.len) {
    return false;
  }

  if (text.len > entry->header.len) {
    return false;
  }

  if (entry->header.is_wide) {
    return unreal_fname_entry_match_text_wide(entry, text, ignore_case);
  } else {
    return unreal_fname_entry_match_text_ansi(entry, text, ignore_case);
  }
}

static bool
unreal_fname_entry_match_text16_ansi(fname_entry_t *entry, str16_t text, bool ignore_case)
{
  uint32_t last = (uint32_t)entry->header.len - (uint32_t)text.len;
  for (uint32_t i = 0; i <= last; ++i) {
    bool match = true;

    for (uint32_t j = 0; j < text.len; ++j) {
      uint16_t a = entry->name.ansi[i + j];
      uint16_t b = text.data[j];

      if (ignore_case) {
        a = utf16_ascii_to_lower(a);
        b = utf16_ascii_to_lower(b);
      }

      if (a != b) {
        match = false;
        break;
      }
    }

    if (match) {
      return true;
    }
  }

  return false;
}

static bool
unreal_fname_entry_match_text16_wide(fname_entry_t *entry, str16_t text, bool ignore_case)
{
  uint32_t last = (uint32_t)entry->header.len - (uint32_t)text.len;
  for (uint32_t i = 0; i <= last; ++i) {
    bool match = true;

    for (uint32_t j = 0; j < text.len; ++j) {
      uint16_t a = entry->name.wide[i + j];
      uint16_t b = text.data[j];

      if (ignore_case) {
        a = utf16_ascii_to_lower(a);
        b = utf16_ascii_to_lower(b);
      }

      if (a != b) {
        match = false;
        break;
      }
    }

    if (match) {
      return true;
    }
  }

  return false;
}

bool
unreal_fname_entry_match_text16(fname_entry_t *entry, str16_t text, bool ignore_case, bool exact_match)
{
  if (!entry || str16_is_empty(text) || entry->header.len == 0) {
    return false;
  }

  if (exact_match && text.len != entry->header.len) {
    return false;
  }

  if (text.len > entry->header.len) {
    return false;
  }

  if (entry->header.is_wide) {
    return unreal_fname_entry_match_text16_wide(entry, text, ignore_case);
  } else {
    return unreal_fname_entry_match_text16_ansi(entry, text, ignore_case);
  }
}

bool
unreal_fname_match_text(fname_t name, str_t text, bool ignore_case, bool exact_match)
{
  if (str_is_empty(text)) {
    return false;
  }

  fname_entry_t *entry = unreal_fname_entry_get(name.cmp_idx);
  if (!entry || entry->header.len == 0) {
    return false;
  }

  return unreal_fname_entry_match_text(entry, text, ignore_case, exact_match);
}

bool
unreal_fname_match_text16(fname_t name, str16_t text, bool ignore_case, bool exact_match)
{
  if (str16_is_empty(text)) {
    return false;
  }

  fname_entry_t *entry = unreal_fname_entry_get(name.cmp_idx);
  if (!entry || entry->header.len == 0) {
    return false;
  }

  return unreal_fname_entry_match_text16(entry, text, ignore_case, exact_match);
}

/* ==================================================== FSTRING ===================================================== */

fstring_t
unreal_fstring_view_from_str16(str16_t s)
{
  return (fstring_t){
    .data = s.data,
    .len  = (int32_t)s.len,
    .max  = (int32_t)s.len,
  };
}

str16_t
unreal_fstring_view_to_str16(fstring_t fs)
{
  return str16_from_wstr_with_cap(fs.data, fs.len);
}

fstring_t
unreal_fstring_from_str(str_t s, mod_arena_t arena)
{
  return unreal_fstring_view_from_str16(str16_from_str(arena, s));
}

str_t
unreal_fstring_to_str(fstring_t fs, mod_arena_t arena)
{
  return str_from_str16(arena, unreal_fstring_view_to_str16(fs));
}

/* =================================================== FIOSTATUS ==================================================== */

str_t
unreal_fio_status_to_str(fio_status_t status, mod_arena_t arena)
{
  str_t       result = STR_NULL;
  tmp_arena_t tmp    = mod_scratch_begin(arena);
  {
    str_t       err_msg_str  = str_from_str16(tmp.arena, str16_from_wstr_with_cap(status.err_msg, COUNTOF(status.err_msg)));
    const char *err_code_str = eio_err_code_to_str(status.err_code);

    result = str_push_fmt(arena, "%.*s (%d - %s)", STR_ARG(err_msg_str), status.err_code, err_code_str);
  }
  mod_scratch_end(tmp);
  return result;
}

/* ==================================================== UOBJECT ===================================================== */

fuobject_array_t *
unreal_get_object_array(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->get_object_array() : NULL;
}

uworld_t *
unreal_get_current_world(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    uworld_t **gworld_ptr = host->get_gworld_ptr();
    if (gworld_ptr) {
      return *gworld_ptr;
    }
  }
  return NULL;
}

bool
unreal_register_uobject_listener(uobject_listener_kind_t kind, uobject_notify_cb_t notify_cb, void *user)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  return (host) ? host->register_uobject_listener(mod, kind, notify_cb, user) : false;
}

void
unreal_deregister_uobject_listener(uobject_listener_kind_t kind, uobject_notify_cb_t notify_cb, void *user)
{
  const mod_host_api_t *host = mod_sdk_host();
  mod_t                 mod  = mod_sdk_mod_handle();
  if (host) {
    host->deregister_uobject_listener(mod, kind, notify_cb, user);
  }
}

fuobject_item_t *
unreal_uobject_array_get_item(int idx)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return NULL;
  }

  fchunked_fixed_uobject_array_t *a = &uobjects->obj_objs;
  if (idx < 0 || idx >= a->num_elems) {
    return NULL;
  }

  int chunk_idx  = idx / UOBJECT_ARRAY_NUM_ELEMS_PER_CHUNK;
  int within_idx = idx % UOBJECT_ARRAY_NUM_ELEMS_PER_CHUNK;

  if (chunk_idx < 0 || chunk_idx >= a->num_chunks) {
    return NULL;
  }

  fuobject_item_t *chunk = a->objs[chunk_idx];
  if (!chunk) {
    return NULL;
  }

  return &chunk[within_idx];
}

bool
unreal_uobject_array_item_is_valid(fuobject_item_t *item)
{
  if (!item || !item->obj) {
    return false;
  }

  if (item->flags & (IOF_UNREACHABLE | IOF_PENDING_KILL)) {
    return false;
  }

  return true;
}

uobject_t *
unreal_uobject_array_get_obj(int idx)
{
  fuobject_item_t *item = unreal_uobject_array_get_item(idx);
  if (!item || !unreal_uobject_array_item_is_valid(item)) {
    return NULL;
  }
  return item->obj;
}

bool
unreal_uobject_is_a(uobject_t *obj, uclass_t *cls)
{
  if (obj && cls) {
    for (uclass_t *c = obj->cls; c; c = (uclass_t *)c->base.super_struct) {
      if (c == cls) {
        return true;
      }
    }
  }
  return false;
}

bool
unreal_uobject_is_default(uobject_t *obj)
{
  return (obj && (obj->obj_flags & (RF_CLASS_DEFAULT_OBJECT | RF_ARCHETYPE_OBJECT)));
}

bool
unreal_uobject_is_valid(uobject_t *obj)
{
  if (!obj) {
    return false;
  }

  fuobject_item_t *item = unreal_uobject_array_get_item(obj->internal_idx);
  if (!item || !unreal_uobject_array_item_is_valid(item)) {
    return false;
  }

  /* The item at this index must point back to our object.
   * If it points to something else, our object was destroyed
   * and the slot was reused. */
  if (item->obj != obj) {
    return false;
  }

  return true;
}

uint64_t
unreal_uobject_get_name_len(uobject_t *obj)
{
  uint64_t len = 0;
  if (obj) {
    len = unreal_fname_utf8_len(obj->name);
  }
  return len;
}

uint64_t
unreal_uobject_write_name(uobject_t *obj, uint8_t *buf, uint64_t max_len)
{
  uint64_t len = 0;
  if (obj && buf && max_len > 0) {
    len = unreal_fname_utf8_write(buf, max_len, obj->name);
  }
  return len;
}

str_t
unreal_uobject_push_name(uobject_t *obj, mod_arena_t arena)
{
  str_t result = STR_NULL;
  if (obj) {
    uint64_t len = unreal_uobject_get_name_len(obj);
    uint8_t *buf = mod_arena_push(arena, len + 1, 1);
    if (buf) {
      MOD_ASSERT(unreal_uobject_write_name(obj, buf, len) == len);
      buf[len] = '\0';
      result   = str_make(buf, len);
    }
  }

  return result;
}

bool
unreal_outer_chain_contains(uobject_t *obj, str_t str, bool ignore_case, bool exact_match)
{
  for (uobject_t *cur = obj; cur; cur = cur->outer) {
    if (unreal_fname_match_text(cur->name, str, ignore_case, exact_match)) {
      return true;
    }
  }
  return false;
}

bool
unreal_super_chain_contains(uclass_t *cls, str_t str, bool ignore_case, bool exact_match)
{
  for (ustruct_t *s = cls ? &cls->base : NULL; s; s = s->super_struct) {
    if (unreal_fname_match_text(s->base.base.name, str, ignore_case, exact_match)) {
      return true;
    }
  }
  return false;
}

uint64_t
unreal_uobject_get_full_name_len(uobject_t *obj)
{
  uint64_t len = 0;
  if (obj) {
    len += unreal_fname_utf8_len(obj->name);
    for (uobject_t *cur = obj->outer; cur; cur = cur->outer) {
      len += 1; // '.'
      len += unreal_fname_utf8_len(cur->name);
    }
  }
  return len;
}

uint64_t
unreal_uobject_write_full_name(uobject_t *obj, uint8_t *buf, uint64_t max_len)
{
  uint64_t len = 0;
  if (obj && buf && max_len > 0) {
    tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);
    {
      uint64_t depth = 0;
      for (uobject_t *cur = obj; cur; cur = cur->outer) {
        depth += 1;
      }

      uobject_t **chain = mod_arena_push(tmp.arena, depth * sizeof(uobject_t *), sizeof(uobject_t *));
      uint64_t    i     = depth;
      for (uobject_t *cur = obj; cur; cur = cur->outer) {
        chain[--i] = cur;
      }

      for (i = 0; i < depth && len < max_len; ++i) {
        len += unreal_fname_utf8_write(buf + len, max_len - len, chain[i]->name);
        if (i + 1 < depth && len + 1 <= max_len) {
          buf[len++] = '.';
        }
      }
    }
    mod_scratch_end(tmp);
  }
  return len;
}

str_t
unreal_uobject_push_full_name(uobject_t *obj, mod_arena_t arena)
{
  str_t result = STR_NULL;
  if (obj) {
    uint64_t len = unreal_uobject_get_full_name_len(obj);
    if (len > 0) {
      uint8_t *buf = mod_arena_push(arena, len + 1, 1);
      if (buf) {
        unreal_uobject_write_full_name(obj, buf, len);
        buf[len] = '\0';
        result   = str_make(buf, len);
      }
    }
  }
  return result;
}

int
unreal_uobject_array_count(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->obj_objs.num_elems;
}

int
unreal_uobject_array_capacity(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->obj_objs.max_elems;
}

int
unreal_uobject_array_num_chunks(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->obj_objs.num_chunks;
}

int
unreal_uobject_array_max_chunks(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->obj_objs.max_chunks;
}

int
unreal_uobject_array_first_gc_index(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->obj_first_cg_idx;
}

int
unreal_uobject_array_last_non_gc_index(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->obj_last_non_cg_idx;
}

bool
unreal_uobject_array_is_open_for_disregard_for_gc(void)
{
  fuobject_array_t *uobjects = unreal_get_object_array();
  if (!uobjects) {
    return 0;
  }
  return uobjects->open_for_disregard_for_gc;
}

uobject_t *
unreal_uobject_find(str_t name, bool ignore_case, bool exact_match)
{
  if (str_is_empty(name)) {
    return NULL;
  }

  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (unreal_fname_match_text(obj->name, name, ignore_case, exact_match)) {
      return obj;
    }
  }

  return NULL;
}

uclass_t *
unreal_uclass_find(str_t name, bool ignore_case, bool exact_match)
{
  if (str_is_empty(name)) {
    return NULL;
  }

  MOD_ASSERT_MSG(g_cached_objects.core_class != NULL, "/Script/CoreUObject.Class is not cached, make sure you call unreal_cache_objects()");

  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (!unreal_uobject_is_a(obj, g_cached_objects.core_class)) {
      continue;
    }

    if (unreal_fname_match_text(obj->name, name, ignore_case, exact_match)) {
      return (uclass_t *)obj;
    }
  }

  return NULL;
}

uobject_t *
unreal_uobject_find_first_of(uclass_t *cls)
{
  if (!cls) {
    return NULL;
  }

  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (unreal_uobject_is_a(obj, cls) && !unreal_uobject_is_default(obj)) {
      return obj;
    }
  }

  return NULL;
}

uclass_t *
unreal_uobject_find_class_by_full_name(str_t full_name)
{
  MOD_ASSERT_MSG(g_cached_objects.core_class != NULL, "/Script/CoreUObject.Class is not cached, make sure you call unreal_cache_objects()");

  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (!unreal_uobject_is_a(obj, g_cached_objects.core_class)) {
      continue;
    }

    tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);

    str_t obj_full_name = unreal_uobject_push_full_name(obj, tmp.arena);
    bool  match         = str_equal(obj_full_name, full_name, 0);

    mod_scratch_end(tmp);

    if (match) {
      return (uclass_t *)obj;
    }
  }

  return NULL;
}

uobject_t *
unreal_uobject_find_by_full_name(uclass_t *cls, str_t full_name)
{
  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (cls && !unreal_uobject_is_a(obj, cls)) {
      continue;
    }

    tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);

    str_t obj_full_name = unreal_uobject_push_full_name(obj, tmp.arena);
    bool  match         = str_equal(obj_full_name, full_name, 0);

    mod_scratch_end(tmp);

    if (match) {
      return obj;
    }
  }

  return NULL;
}

void
unreal_process_event(uobject_t *self, ufunc_t *func, void *params)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (host) {
    host->process_event(self, func, params);
  }
}

uobject_t *
unreal_spawn_actor(uobject_t *world_ctx_obj, uclass_t *cls)
{
  MOD_ASSERT_MSG(g_cached_objects.gameplay_statics_cdo != NULL, "/Script/Engine.Default__GameplayStatics is not cached, make sure you call unreal_cache_objects()");
  MOD_ASSERT_MSG(g_cached_objects.begin_spawn != NULL, "/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass is not cached, make sure you call unreal_cache_objects()");
  MOD_ASSERT_MSG(g_cached_objects.finish_spawn != NULL, "/Script/Engine.GameplayStatics.FinishSpawningActor is not cached, make sure you call unreal_cache_objects()");

  uobject_t  *result = NULL;
  tmp_arena_t tmp    = mod_scratch_begin(MOD_ARENA_INVALID);
  {
    begin_actor_spawn_params_t  *p1 = mod_arena_push(tmp.arena, sizeof(*p1), 16);
    finish_actor_spawn_params_t *p2 = mod_arena_push(tmp.arena, sizeof(*p2), 16);

    MOD_ASSERT_MSG(p1 != NULL, "failed to allocate begin actor spawn params");
    MOD_ASSERT_MSG(p2 != NULL, "failed to allocate finish actor spawn params");

    if (p1 && p2) {
      ftransform_t xf = {
        .rotation = {.w = 1.0f},
        .scale3d  = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
      };

      *p1 = (begin_actor_spawn_params_t){
        .world_ctx_obj               = (uworld_t *)world_ctx_obj,
        .actor_class                 = cls,
        .spawn_transform             = xf,
        .collision_handling_override = 3,
      };

      uobject_t *gameplay_statics_cdo = g_cached_objects.gameplay_statics_cdo;
      ufunc_t   *begin_spawn          = g_cached_objects.begin_spawn;
      ufunc_t   *finish_spawn         = g_cached_objects.finish_spawn;

      MOD_ASSERT_MSG(gameplay_statics_cdo != NULL, "missing /Script/Engine.Default__GameplayStatics");
      MOD_ASSERT_MSG(begin_spawn != NULL, "missing /Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass");
      MOD_ASSERT_MSG(finish_spawn != NULL, "missing /Script/Engine.GameplayStatics.FinishSpawningActor");

      unreal_process_event(gameplay_statics_cdo, begin_spawn, p1);
      if (p1->return_value) {
        *p2 = (finish_actor_spawn_params_t){
          .actor           = p1->return_value,
            .spawn_transform = xf,
        };

        unreal_process_event(gameplay_statics_cdo, finish_spawn, p2);
        if (p2->return_value) {
          result = (uobject_t *)p2->return_value;
        }
      }
    }
  }
  mod_scratch_end(tmp);
  return result;
}

void
unreal_despawn_actor(uobject_t *actor)
{
  MOD_ASSERT_MSG(g_cached_objects.destroy_actor != NULL, "/Script/Engine.Actor.K2_DestroyActor is not cached, make sure you call unreal_cache_objects()");

  ufunc_t *destroy_actor = g_cached_objects.destroy_actor;
  if (actor) {
    unreal_process_event(actor, destroy_actor, NULL);
  }
}

uclass_t *
unreal_load_class(uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->uobject_load_class(base_cls, outer, name, filename, load_flags) : NULL;
}

bool
unreal_mount_pak(str_t file_path, int order)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->mount_pak(file_path, order) : false;
}

bool
unreal_mount_iostore(str_t file_path, int order)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->mount_iostore(file_path, order) : false;
}

fnative_func_ptr_t
unreal_get_native(uint8_t opcode)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->get_native(opcode) : (fnative_func_ptr_t){0};
}

bool
unreal_fframe_step(fframe_t *stack, void *result)
{
  MOD_ASSERT(stack != NULL);
  MOD_ASSERT(stack->code != NULL);

  uint8_t            opcode = *stack->code++;
  fnative_func_ptr_t exec   = unreal_get_native(opcode);
  if (!exec) {
    return false;
  }

  exec(stack->obj, stack, result);
  return true;
}

/* ================================================ CLASS INTROSPECTION ============================================= */
fprop_t *
unreal_ustruct_find_prop(ustruct_t *s, str_t name)
{
  for (fprop_t *p = s->prop_link; p; p = p->prop_link_next) {
    tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);

    str_t prop_name = unreal_fname_to_str(p->base.name, tmp.arena);
    bool  found     = str_equal(prop_name, name, 0);

    mod_scratch_end(tmp);

    if (found) {
      return p;
    }
  }

  return NULL;
}

ufunc_t *
unreal_ustruct_find_func(ustruct_t *s, str_t name)
{
  for (ustruct_t *cur = s; cur; cur = cur->super_struct) {
    for (ufield_t *f = cur->children; f; f = f->next) {
      tmp_arena_t tmp = mod_scratch_begin(MOD_ARENA_INVALID);

      str_t func_name = unreal_fname_to_str(f->base.name, tmp.arena);
      bool  found     = str_equal(func_name, name, 0);

      mod_scratch_end(tmp);

      if (found) {
        return (ufunc_t *)f;
      }
    }
  }

  return NULL;
}

ufunc_t *
unreal_ustruct_find_func_fname(ustruct_t *s, fname_t name, bool ignore_num)
{
  for (ustruct_t *cur = s; cur; cur = cur->super_struct) {
    for (ufield_t *f = cur->children; f; f = f->next) {
      if (unreal_fname_equal(f->base.name, name, ignore_num)) {
        return (ufunc_t *)f;
      }
    }
  }

  return NULL;
}

void *
unreal_get_mcast_sparse_delegate(uobject_t *delegate_owner, fname_t delegate_name)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->get_mcast_sparse_delegate(delegate_owner, delegate_name) : NULL;
}
