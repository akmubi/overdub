#include "unreal.h"
#include "arena.h"
#include "file.h"
#include "globals.h"
#include "path.h"
#include "scratch.h"
#include "signatures.h"
#include "str.h"

#include <windows.h>

STATIC_ASSERT(sizeof(fcritical_section_t) == sizeof(CRITICAL_SECTION), "critical section size mismatch");
STATIC_ASSERT(_Alignof(fcritical_section_t) == _Alignof(CRITICAL_SECTION), "critical section alignment mismatch");

/* ===================================================== FNAME ====================================================== */

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
  MASSERT(globals.name_pool != NULL, "name pool is missing");

  uint32_t block  = cmp_idx >> FNAME_BLOCK_OFFSET_BITS;              // high bits
  uint32_t offset = cmp_idx & ((1u << FNAME_BLOCK_OFFSET_BITS) - 1); // low 16 bits

  if (block >= FNAME_MAX_BLOCKS) {
    return NULL;
  }

  uint8_t *base = (uint8_t *)globals.name_pool->entries.blocks[block];
  if (!base) {
    return NULL;
  }

  return (fname_entry_t *)(base + offset * 2);
}

uint64_t
unreal_fname_utf8_len(fname_t name)
{
  uint64_t       len   = 0;
  fname_entry_t *entry = unreal_fname_entry_get(name.cmp_idx);
  if (entry) {
    if (entry->header.is_wide) {
      len += str16_utf8_len(str16_make(entry->name.wide, (uint64_t)entry->header.len));
    } else {
      len += entry->header.len;
    }

    if (name.num > 0) {
      len += 1 + (uint64_t)u32_count_digits(FNAME_INTERNAL_TO_EXTERNAL(name.num));
    }
  }
  return len;
}

uint64_t
unreal_fname_utf8_write(uint8_t *buf, uint64_t max_len, fname_t name)
{
  uint64_t       len   = 0;
  fname_entry_t *entry = unreal_fname_entry_get(name.cmp_idx);
  if (buf && entry) {
    if (entry->header.is_wide) {
      len += str16_write_utf8(buf, max_len, str16_make(entry->name.wide, (uint64_t)entry->header.len));
    } else {
      len = MIN_VAL(entry->header.len, max_len);
      mem_copy(buf, entry->name.ansi, len);
    }

    if (len < max_len && name.num > 0) {
      uint8_t  suffix[10];
      uint64_t num = 0;
      uint32_t val = FNAME_INTERNAL_TO_EXTERNAL(name.num);

      do {
        suffix[num++] = (uint8_t)('0' + (val % 10));
        val /= 10;
      } while (val);

      if (len + 1 + num <= max_len) {
        buf[len++] = '_';

        while (num > 0) {
          buf[len++] = suffix[--num];
        }
      }
    }
  }
  return len;
}

str_t
unreal_fname_to_str(fname_t fname, arena_t *arena)
{
  str_t    result = STR_NULL;
  uint64_t len    = unreal_fname_utf8_len(fname);
  uint8_t *buf    = ARENA_PUSH_ARRAY(arena, uint8_t, len + 1);
  if (buf) {
    ASSERT(unreal_fname_utf8_write(buf, len, fname) == len);
    buf[len] = '\0';
    result   = str_make(buf, len);
  }

  return result;
}

fname_t
unreal_fname_from_str(str_t s, efind_name_t find_type)
{
  fname_t fname = {0};
#if !defined BUILD_TEST_UI
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str16_t s16 = str16_from_str(tmp.arena, s);
    fname_construct(&fname, (const wchar_t *)s16.data, find_type);
  }
  scratch_end(tmp);
#else
  (void)s;
  (void)find_type;
#endif
  return fname;
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
  ASSERT(globals.name_pool != NULL);

  if (!cb) {
    return;
  }

  fname_entry_allocator_t *alloc = &globals.name_pool->entries;

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

/* text matching is ASCII-oriented. UTF-8 multibyte queries are not decoded when matching wide FName entries  */
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
unreal_fstring_from_str(str_t s, arena_t *perm)
{
  return unreal_fstring_view_from_str16(str16_from_str(perm, s));
}

str_t
unreal_fstring_to_str(fstring_t fs, arena_t *perm)
{
  return str_from_str16(perm, unreal_fstring_view_to_str16(fs));
}

/* =================================================== FIOSTATUS ==================================================== */

str_t
unreal_fio_status_to_str(fio_status_t status, arena_t *arena)
{
  str_t       result = STR_NULL;
  tmp_arena_t tmp    = scratch_begin(arena);
  {
    str_t       err_msg_str  = str_from_str16(tmp.arena, str16_from_wstr_with_cap(status.err_msg, COUNTOF(status.err_msg)));
    const char *err_code_str = eio_err_code_to_str(status.err_code);

    result = str_push_fmt(arena, "%.*s (%d - %s)", STR_ARG(err_msg_str), status.err_code, err_code_str);
  }
  scratch_end(tmp);
  return result;
}

/* ==================================================== UOBJECT ===================================================== */
void
unreal_common_collect(unreal_common_t *common)
{
  ASSERT(common != NULL);

  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    tmp_arena_t tmp       = scratch_begin(NULL);
    str_t       full_name = unreal_uobject_push_full_name(obj, tmp.arena);

    if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Object"), 0)) {
      common->core_object = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Class"), 0)) {
      common->core_class = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.ScriptStruct"), 0)) {
      common->core_scriptstruct = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Function"), 0)) {
      common->core_func = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Enum"), 0)) {
      common->core_enum = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/CoreUObject.Package"), 0)) {
      common->core_package = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Actor"), 0)) {
      common->actor = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.PlayerController"), 0)) {
      common->player_ctrl = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.LocalPlayer"), 0)) {
      common->local_player = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Pawn"), 0)) {
      common->pawn = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.HUD"), 0)) {
      common->hud = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.World"), 0)) {
      common->world = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.GameInstance"), 0)) {
      common->game_instance = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Level"), 0)) {
      common->level = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.ActorComponent"), 0)) {
      common->actor_component = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/UMG.Widget"), 0)) {
      common->widget = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Blueprint"), 0)) {
      common->blueprint = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.DataAsset"), 0)) {
      common->data_asset = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.DataTable"), 0)) {
      common->data_table = (uclass_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Default__GameplayStatics"), 0)) {
      common->gameplay_statics_cdo = obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass"), 0)) {
      common->begin_spawn = (ufunc_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.GameplayStatics.FinishSpawningActor"), 0)) {
      common->finish_spawn = (ufunc_t *)obj;
    } else if (str_equal(full_name, STR_LIT("/Script/Engine.Actor.K2_DestroyActor"), 0)) {
      common->destroy_actor = (ufunc_t *)obj;
    }
    scratch_end(tmp);
  }

  fname_entry_allocator_t *alloc = &globals.name_pool->entries;
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

      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t name_str = unreal_fname_to_str(name, tmp.arena);
        if (str_equal(name_str, STR_LIT("BoolProperty"), 0)) {
          common->bool_prop = name;
        } else if (str_equal(name_str, STR_LIT("ByteProperty"), 0)) {
          common->byte_prop = name;
        } else if (str_equal(name_str, STR_LIT("Int8Property"), 0)) {
          common->int8_prop = name;
        } else if (str_equal(name_str, STR_LIT("Int16Property"), 0)) {
          common->int16_prop = name;
        } else if (str_equal(name_str, STR_LIT("IntProperty"), 0)) {
          common->int_prop = name;
        } else if (str_equal(name_str, STR_LIT("Int32Property"), 0)) {
          common->int32_prop = name;
        } else if (str_equal(name_str, STR_LIT("Int64Property"), 0)) {
          common->int64_prop = name;
        } else if (str_equal(name_str, STR_LIT("UInt16Property"), 0)) {
          common->uint16_prop = name;
        } else if (str_equal(name_str, STR_LIT("UInt32Property"), 0)) {
          common->uint32_prop = name;
        } else if (str_equal(name_str, STR_LIT("UInt64Property"), 0)) {
          common->uint64_prop = name;
        } else if (str_equal(name_str, STR_LIT("FloatProperty"), 0)) {
          common->float_prop = name;
        } else if (str_equal(name_str, STR_LIT("DoubleProperty"), 0)) {
          common->double_prop = name;
        } else if (str_equal(name_str, STR_LIT("NameProperty"), 0)) {
          common->name_prop = name;
        } else if (str_equal(name_str, STR_LIT("StrProperty"), 0)) {
          common->str_prop = name;
        } else if (str_equal(name_str, STR_LIT("TextProperty"), 0)) {
          common->text_prop = name;
        } else if (str_equal(name_str, STR_LIT("ObjectProperty"), 0)) {
          common->obj_prop = name;
        } else if (str_equal(name_str, STR_LIT("ClassProperty"), 0)) {
          common->class_prop = name;
        } else if (str_equal(name_str, STR_LIT("SoftObjectProperty"), 0)) {
          common->soft_obj_prop = name;
        } else if (str_equal(name_str, STR_LIT("SoftClassProperty"), 0)) {
          common->soft_class_prop = name;
        } else if (str_equal(name_str, STR_LIT("WeakObjectProperty"), 0)) {
          common->weak_obj_prop = name;
        } else if (str_equal(name_str, STR_LIT("LazyObjectProperty"), 0)) {
          common->lazy_obj_prop = name;
        } else if (str_equal(name_str, STR_LIT("InterfaceProperty"), 0)) {
          common->interface_prop = name;
        } else if (str_equal(name_str, STR_LIT("StructProperty"), 0)) {
          common->struct_prop = name;
        } else if (str_equal(name_str, STR_LIT("ArrayProperty"), 0)) {
          common->array_prop = name;
        } else if (str_equal(name_str, STR_LIT("SetProperty"), 0)) {
          common->set_prop = name;
        } else if (str_equal(name_str, STR_LIT("MapProperty"), 0)) {
          common->map_prop = name;
        } else if (str_equal(name_str, STR_LIT("EnumProperty"), 0)) {
          common->enum_prop = name;
        } else if (str_equal(name_str, STR_LIT("DelegateProperty"), 0)) {
          common->delegate_prop = name;
        } else if (str_equal(name_str, STR_LIT("MulticastDelegateProperty"), 0)) {
          common->mcast_delegate_prop = name;
        } else if (str_equal(name_str, STR_LIT("MulticastInlineDelegateProperty"), 0)) {
          common->mcast_inline_delegate_prop = name;
        } else if (str_equal(name_str, STR_LIT("MulticastSparseDelegateProperty"), 0)) {
          common->mcast_sparse_delegate_prop = name;
        } else if (str_equal(name_str, STR_LIT("Rotator"), 0)) {
          common->rotator = name;
        } else if (str_equal(name_str, STR_LIT("Color"), 0)) {
          common->color = name;
        } else if (str_equal(name_str, STR_LIT("Vector"), 0)) {
          common->vector = name;
        } else if (str_equal(name_str, STR_LIT("Vector_NetQuantize"), 0)) {
          common->vector_net_quantize = name;
        } else if (str_equal(name_str, STR_LIT("Vector_NetQuantize10"), 0)) {
          common->vector_net_quantize10 = name;
        } else if (str_equal(name_str, STR_LIT("Vector_NetQuantize100"), 0)) {
          common->vector_net_quantize100 = name;
        } else if (str_equal(name_str, STR_LIT("Vector_NetQuantizeNormal"), 0)) {
          common->vector_net_quantize_normal = name;
        } else if (str_equal(name_str, STR_LIT("Vector2D"), 0)) {
          common->vector2d = name;
        } else if (str_equal(name_str, STR_LIT("Vector4"), 0)) {
          common->vector4 = name;
        } else if (str_equal(name_str, STR_LIT("LinearColor"), 0)) {
          common->linear_color = name;
        } else if (str_equal(name_str, STR_LIT("Quat"), 0)) {
          common->quat = name;
        } else if (str_equal(name_str, STR_LIT("Transform"), 0)) {
          common->transform = name;
        } else if (str_equal(name_str, STR_LIT("Key"), 0)) {
          common->key = name;
        } else if (str_equal(name_str, STR_LIT("GameplayTag"), 0)) {
          common->gameplay_tag = name;
        } else if (str_equal(name_str, STR_LIT("GameplayTagContainer"), 0)) {
          common->gameplay_tag_container = name;
        } else if (str_equal(name_str, STR_LIT("Guid"), 0)) {
          common->guid = name;
        } else if (str_equal(name_str, STR_LIT("IntPoint"), 0)) {
          common->int_point = name;
        } else if (str_equal(name_str, STR_LIT("IntVector"), 0)) {
          common->int_vector = name;
        }
      }
      scratch_end(tmp);

      byte_offset += entry_size;
    }
  }
}

fuobject_item_t *
unreal_uobject_array_get_item(int idx)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");

  fchunked_fixed_uobject_array_t *a = &globals.uobjects->obj_objs;
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
#else
  (void)idx;
  return false;
#endif
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
unreal_uobject_push_name(uobject_t *obj, arena_t *arena)
{
  str_t result = STR_NULL;
  if (obj) {
    uint64_t len = unreal_uobject_get_name_len(obj);
    uint8_t *buf = ARENA_PUSH_ARRAY(arena, uint8_t, len + 1);
    if (buf) {
      ASSERT(unreal_uobject_write_name(obj, buf, len) == len);
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
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      uint64_t depth = 0;
      for (uobject_t *cur = obj; cur; cur = cur->outer) {
        depth += 1;
      }

      uobject_t **chain = ARENA_PUSH_ARRAY(tmp.arena, uobject_t *, depth);
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
    scratch_end(tmp);
  }
  return len;
}

str_t
unreal_uobject_push_full_name(uobject_t *obj, arena_t *arena)
{
  str_t result = STR_NULL;
  if (obj) {
    uint64_t len = unreal_uobject_get_full_name_len(obj);
    if (len > 0) {
      uint8_t *buf = ARENA_PUSH_ARRAY(arena, uint8_t, len + 1);
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
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->obj_objs.num_elems;
#else
  return 0;
#endif
}

int
unreal_uobject_array_capacity(void)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->obj_objs.max_elems;
#else
  return 0;
#endif
}

int
unreal_uobject_array_num_chunks(void)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->obj_objs.num_chunks;
#else
  return 0;
#endif
}

int
unreal_uobject_array_max_chunks(void)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->obj_objs.max_chunks;
#else
  return 0;
#endif
}

int
unreal_uobject_array_first_gc_index(void)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->obj_first_cg_idx;
#else
  return 0;
#endif
}

int
unreal_uobject_array_last_non_gc_index(void)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->obj_last_non_cg_idx;
#else
  return 0;
#endif
}

bool
unreal_uobject_array_is_open_for_disregard_for_gc(void)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  return globals.uobjects->open_for_disregard_for_gc;
#else
  return 0;
#endif
}

static void
add_uobject_create_listener(fuobject_listener_t *listener)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  int32_t idx = globals.uobjects->create_listeners.num++;
  if (globals.uobjects->create_listeners.num > globals.uobjects->create_listeners.max) {
    tarray_grow((void *)&globals.uobjects->create_listeners);
  }
  globals.uobjects->create_listeners.data[idx] = listener;
#else
  (void)listener;
#endif
}

static void
remove_uobject_create_listener(fuobject_listener_t *listener)
{
#if !defined BUILD_TEST_UI
  MASSERT(globals.uobjects != NULL, "uobject array is missing");
  tarray_fuobject_listener_t *arr = &globals.uobjects->create_listeners;
  int32_t                     num = arr->num;
  int                         idx = -1;

  for (int i = 0; i < num; ++i) {
    if (arr->data[i] == listener) {
      idx = i;
      break;
    }
  }

  if (idx >= 0) {
    if (idx != num - 1) {
      arr->data[idx] = arr->data[num - 1];
    }
    arr->num = num - 1;
    tarray_shrink((void *)arr);
  }
#else
  (void)listener;
#endif
}

static void
remove_uobject_delete_listener(fuobject_listener_t *listener)
{
#if !defined BUILD_TEST_UI
  EnterCriticalSection((LPCRITICAL_SECTION)&globals.uobjects->delete_listeners_critical);

  tarray_fuobject_listener_t *arr = &globals.uobjects->delete_listeners;
  int32_t                     num = arr->num;
  int                         idx = -1;

  for (int i = 0; i < num; ++i) {
    if (arr->data[i] == listener) {
      idx = i;
      break;
    }
  }

  if (idx >= 0) {
    if (idx != num - 1) {
      arr->data[idx] = arr->data[num - 1];
    }
    arr->num = num - 1;
    tarray_shrink((void *)arr);
  }

  LeaveCriticalSection((LPCRITICAL_SECTION)&globals.uobjects->delete_listeners_critical);
#else
  (void)listener;
#endif
}

void
uobject_listener_destroy_cb(fuobject_listener_t *self)
{
  (void)self;
}

void
uobject_listener_on_notify_cb(fuobject_listener_t *self, const uobject_t *obj, int32_t idx)
{
  uobject_listener_t *listener = (uobject_listener_t *)self;
  if (listener && listener->on_notify) {
    listener->on_notify((uobject_t *)obj, idx, listener->user);
  }
}

void
uobject_listener_on_shutdown_cb(fuobject_listener_t *self)
{
  uobject_listener_t *listener = (uobject_listener_t *)self;
  if (listener) {
    unreal_uobject_listener_del(listener);
  }
}

void
unreal_register_uobject_listener(uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user)
{
  int free_idx = -1;
  for (int i = 0; i < CONFIG_UOBJECT_ARRAY_MAX_LISTENERS; ++i) {
    uobject_listener_t *current = &globals.listeners[i];
    if (!current->occupied) {
      if (free_idx < 0) {
        free_idx = i;
      }
      continue;
    }

    if (current->kind == kind && current->on_notify == notify_cb && current->user == user) {
      return; // duplicate
    }
  }

  if (free_idx < 0) {
    return; // no available slots
  }

  uobject_listener_t *slot = &globals.listeners[free_idx];
  mem_zero(slot, sizeof(*slot));

  slot->vftable         = &slot->vftable_backing;
  slot->vftable_backing = (fuobject_listener_vftable_t){
    .destroy     = uobject_listener_destroy_cb,
    .on_notify   = uobject_listener_on_notify_cb,
    .on_shutdown = uobject_listener_on_shutdown_cb,
  };
  slot->kind      = kind;
  slot->on_notify = notify_cb;
  slot->user      = user;

  unreal_uobject_listener_add(slot);
}

void
unreal_deregister_object_listener(uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user)
{
  for (int i = 0; i < CONFIG_UOBJECT_ARRAY_MAX_LISTENERS; ++i) {
    uobject_listener_t *current = &globals.listeners[i];
    if (!current->occupied) {
      continue;
    }

    if (current->kind == kind && current->on_notify == notify_cb && current->user == user) {
      unreal_uobject_listener_del(current);
      return;
    }
  }
}

void
unreal_uobject_listener_add(uobject_listener_t *listener)
{
  ASSERT(listener != NULL);

  if (!listener->occupied) {
#if !defined BUILD_TEST_UI
    if (listener->kind == UOBJECT_LISTENER_KIND_CREATE) {
      add_uobject_create_listener((fuobject_listener_t *)listener);
    } else if (listener->kind == UOBJECT_LISTENER_KIND_DELETE) {
      ASSERT(add_uobject_delete_listener != NULL);
      add_uobject_delete_listener(globals.uobjects, (fuobject_listener_t *)listener);
    }
#endif
    listener->occupied = true;
  }
}

void
unreal_uobject_listener_del(uobject_listener_t *listener)
{
  ASSERT(listener != NULL);

  if (listener->occupied) {
    if (listener->kind == UOBJECT_LISTENER_KIND_CREATE) {
      remove_uobject_create_listener((fuobject_listener_t *)listener);
    } else if (listener->kind == UOBJECT_LISTENER_KIND_DELETE) {
      remove_uobject_delete_listener((fuobject_listener_t *)listener);
    }
    listener->occupied = false;
  }
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

  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (!unreal_uobject_is_a(obj, globals.unreal.core_class)) {
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
  for (int i = 0; i < unreal_uobject_array_count(); ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);
    if (!obj) {
      continue;
    }

    if (!unreal_uobject_is_a(obj, globals.unreal.core_class)) {
      continue;
    }

    tmp_arena_t tmp = scratch_begin(NULL);

    str_t obj_full_name = unreal_uobject_push_full_name(obj, tmp.arena);
    bool  match         = str_equal(obj_full_name, full_name, 0);

    scratch_end(tmp);

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

    tmp_arena_t tmp = scratch_begin(NULL);

    str_t obj_full_name = unreal_uobject_push_full_name(obj, tmp.arena);
    bool  match         = str_equal(obj_full_name, full_name, 0);

    scratch_end(tmp);

    if (match) {
      return obj;
    }
  }

  return NULL;
}

void
unreal_process_event(uobject_t *self, ufunc_t *func, void *params)
{
#if !defined BUILD_TEST_UI
  if (self && func) {
    process_event_real(self, func, params);
  }
#else
  (void)self;
  (void)func;
  (void)params;
#endif
}

uobject_t *
unreal_spawn_actor(uobject_t *world_ctx_obj, uclass_t *cls)
{
  uobject_t  *result = NULL;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    begin_actor_spawn_params_t  *p1 = arena_push_aligned(tmp.arena, sizeof(*p1), 16);
    finish_actor_spawn_params_t *p2 = arena_push_aligned(tmp.arena, sizeof(*p2), 16);

    MASSERT(p1 != NULL, "failed to allocate begin actor spawn params");
    MASSERT(p2 != NULL, "failed to allocate finish actor spawn params");

    if (p1 && p2) {
      ftransform_t xf = {
        .rotation = {.w = 1.0f},
        .scale3d  = {.x = 1.0f, .y = 1.0f, .z = 1.0f},
      };

      *p1 = (begin_actor_spawn_params_t){
        .world_ctx_obj               = world_ctx_obj,
        .actor_class                 = cls,
        .spawn_transform             = xf,
        .collision_handling_override = 3,
      };

      uobject_t *gameplay_statics_cdo = globals.unreal.gameplay_statics_cdo;
      ufunc_t   *begin_spawn          = globals.unreal.begin_spawn;
      ufunc_t   *finish_spawn         = globals.unreal.finish_spawn;

      MASSERT(gameplay_statics_cdo != NULL, "missing /Script/Engine.Default__GameplayStatics");
      MASSERT(begin_spawn != NULL, "missing /Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass");
      MASSERT(finish_spawn != NULL, "missing /Script/Engine.GameplayStatics.FinishSpawningActor");

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
  scratch_end(tmp);
  return result;
}

void
unreal_despawn_actor(uobject_t *actor)
{
  ufunc_t *destroy_actor = globals.unreal.destroy_actor;
  if (actor) {
    unreal_process_event(actor, destroy_actor, NULL);
  }
}

uclass_t *
unreal_load_class(uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags)
{
  uclass_t   *result = NULL;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str16_t name16     = str16_from_str(tmp.arena, name);
    str16_t filename16 = str16_from_str(tmp.arena, filename);

    // NOTE: str16_from_str allocates wchar string with a null-terminator in mind, so the following is OK
    const wchar_t *namew     = str16_is_empty(name16) ? NULL : name16.data;
    const wchar_t *filenamew = str16_is_empty(filename16) ? NULL : filename16.data;

#if !defined BUILD_TEST_UI
    result = static_load_class(base_cls, outer, namew, filenamew, load_flags);
#else
    (void)filenamew;
    (void)namew;
    (void)base_cls;
    (void)outer;
    (void)load_flags;
#endif

  }
  scratch_end(tmp);
  return result;
}

bool
unreal_mount_pak(str_t file_path, int order)
{
  if (!globals.pak_file) {
    return false;
  }

  bool        result = false;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str_t abs_file_path = file_path;
    if (!path_is_abs(file_path)) {
      abs_file_path = path_join(tmp.arena, globals.game_dir, file_path);
    }

    if (file_exists(abs_file_path)) {
      /* if absolute path can be reduced to relative path - do it */
      str_t mount_file_path = abs_file_path;
      str_t rel_file_path   = STR_NULL;
      if (path_make_relative(file_path, globals.game_dir, &rel_file_path)) {
        mount_file_path = rel_file_path;
      }

      str16_t mount_file_path16 = str16_from_str(tmp.arena, mount_file_path);
      if (!str16_is_empty(mount_file_path16)) {
#if !defined BUILD_TEST_UI
        result = pak_file_mount(globals.pak_file, mount_file_path16.data, order, NULL, true);
#else
        (void)order;
#endif
      }
    }
  }
  scratch_end(tmp);
  return result;
}

bool
unreal_mount_iostore(str_t file_path, int order)
{
  if (!globals.io_dispatcher) {
    return false;
  }

  str_t file_ext = path_get_ext(file_path);
  if (!str_equal(file_ext, STR_LIT("utoc"), STR_CMP_FLAG_IGNORE_CASE) && !str_equal(file_ext, STR_LIT("ucas"), STR_CMP_FLAG_IGNORE_CASE)) {
    /* invalid extension */
    return false;
  }

  bool        result = false;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    str_t abs_file_path = file_path;
    if (!path_is_abs(file_path)) {
      abs_file_path = path_join(tmp.arena, globals.game_dir, file_path);
    }

    if (file_exists(abs_file_path)) {
      /* if absolute path can be reduced to relative path - do it */
      str_t mount_file_path = abs_file_path;
      str_t rel_file_path   = STR_NULL;
      if (path_make_relative(file_path, globals.game_dir, &rel_file_path)) {
        mount_file_path = rel_file_path;
      }

      /* mount accepts file path without .utoc/.ucas extension */
      mount_file_path = path_trim_ext(mount_file_path);

      fstring_t path_fstring = unreal_fstring_from_str(mount_file_path, tmp.arena);
      if (path_fstring.data && path_fstring.len > 0) {
        fio_env_t env = {
          .order = order,
          .path  = path_fstring,
        };

        fio_status_t  status        = {0};
        fguid_t       enc_guid      = {0};
        faes_key_t    enc_key       = {0};
        fio_status_t *result_status = NULL;

#if !defined BUILD_TEST_UI
        result_status = io_dispatcher_mount(globals.io_dispatcher, &status, &env, &enc_guid, &enc_key);
#else
        (void)env;
        (void)status;
        (void)enc_guid;
        (void)enc_key;
#endif
        result = (result_status && result_status->err_code == IO_ERROR_OK);
      }
    }
  }
  scratch_end(tmp);
  return result;
}

fnative_func_ptr_t
unreal_get_native(uint8_t opcode)
{
  fnative_func_ptr_t func = NULL;
#if !defined BUILD_TEST_UI
  ASSERT(globals.natives != NULL);
  func = globals.natives[opcode];
#else
  (void)opcode;
#endif
  return func;
}

bool
unreal_fframe_step(fframe_t *stack, void *result)
{
  ASSERT(stack != NULL);
  ASSERT(stack->code != NULL);

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
    tmp_arena_t tmp = scratch_begin(NULL);

    str_t prop_name = unreal_fname_to_str(p->base.name, tmp.arena);
    bool  found     = str_equal(prop_name, name, 0);

    scratch_end(tmp);

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
      tmp_arena_t tmp = scratch_begin(NULL);

      str_t func_name = unreal_fname_to_str(f->base.name, tmp.arena);
      bool  found     = str_equal(func_name, name, 0);

      scratch_end(tmp);

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
#if !defined BUILD_TEST_UI
  if (get_mcast_sparse_delegate) {
    return get_mcast_sparse_delegate(delegate_owner, delegate_name);
  }
#else
  (void)delegate_owner;
  (void)delegate_name;
#endif
  return NULL;
}
