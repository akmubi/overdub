#include "mod_unreal.h"

#define TMAP_FNAME_UINT8PTR_KEY_EQUAL(A, B) unreal_fname_equal(A, B, false)
#define TMAP_FNAME_UINT8PTR_KEY_HASH(KEY)   unreal_fname_hash(KEY)

TMAP_DEFINE_FUNCS(unreal_tmap_fname_uint8ptr, tmap_fname_uint8ptr_t, fname_t, uint8_t *, TMAP_FNAME_UINT8PTR_KEY_EQUAL, TMAP_FNAME_UINT8PTR_KEY_HASH)

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

uint32_t
unreal_fname_hash(fname_t name)
{
  uint32_t block  = name.cmp_idx >> FNAME_BLOCK_OFFSET_BITS;
  uint32_t offset = name.cmp_idx & ((1u << FNAME_BLOCK_OFFSET_BITS) - 1);

  uint32_t hash = (block << (32 - FNAME_MAX_BLOCK_BITS)) +
                  (block) +
                  (offset << FNAME_BLOCK_OFFSET_BITS) +
                  (offset) +
                  (offset >> 4);

  return hash + name.num;
}

bool
unreal_fname_is_none(fname_t name)
{
  return name.cmp_idx == 0 && name.num == 0;
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
static uint32_t g_fcrc_table[256] = {
  0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
  0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
  0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
  0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
  0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
  0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
  0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
  0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
  0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
  0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
  0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
  0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
  0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
  0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
  0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
  0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
  0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
  0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
  0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
  0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
  0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
  0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
  0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
  0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
  0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
  0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
  0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
  0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
  0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
  0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
  0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
  0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4,
};


fstring_t
unreal_fstring_from_str(str_t s, mod_arena_t arena)
{
  str16_t s16 = str16_from_str(arena, s);
  MOD_ASSERT(s16.data[s16.len] == 0); // must be null-terminated

  return (fstring_t){
    .data = s16.data,
    .len  = (int32_t)s16.len + 1,
    .max  = (int32_t)s16.len + 1,
  };
}

str_t
unreal_fstring_to_str(fstring_t fs, mod_arena_t arena)
{
  return str_from_str16(arena, str16_from_wstr_with_cap(fs.data, fs.len));
}

static inline uint32_t
unreal_crc_byte(uint32_t hash, uint8_t byte)
{
  return ((hash >> 8) & 0x00FFFFFF) ^ g_fcrc_table[(hash ^ byte) & 0xFF];
}

static inline uint16_t
unreal_tchar_to_upper(uint16_t c)
{
  return (c >= 'a' && c <= 'z') ? (uint16_t)(c - ('a' - 'A')) : c;
}

uint32_t
unreal_fstring_hash(fstring_t str)
{
  if (!str.data || str.len <= 1) {
    return 0;
  }

  int32_t  len  = str.len - 1;
  uint32_t hash = 0;
  for (int32_t i = 0; i < len; ++i) {
    uint16_t ch = unreal_tchar_to_upper(str.data[i]);

    hash = unreal_crc_byte(hash, (uint8_t)(ch >> 0));
    hash = unreal_crc_byte(hash, (uint8_t)(ch >> 8));
  }

  return hash;
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
    for (uclass_t *c = obj->cls; c; c = (uclass_t *)c->super_struct) {
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
  for (ustruct_t *s = cls ? (ustruct_t *)cls : NULL; s; s = s->super_struct) {
    if (unreal_fname_match_text(s->name, str, ignore_case, exact_match)) {
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

uobject_t *
unreal_static_load_object(uclass_t *obj_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags, void *sandbox, bool allow_obj_reconcile, void *instancing_ctx)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (!MOD_HOST_API_HAS_FIELD(host, static_load_object) || !host->static_load_object) {
    return NULL;
  }
  return host->static_load_object(obj_cls, outer, name, filename, load_flags, sandbox, allow_obj_reconcile, instancing_ctx);
}

uclass_t *
unreal_static_load_class(uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags)
{
  const mod_host_api_t *host = mod_sdk_host();
  return (host) ? host->static_load_class(base_cls, outer, name, filename, load_flags) : NULL;
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
    if (unreal_fname_match_text(p->name, name, false, true)) {
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
      if (unreal_fname_match_text(f->name, name, false, true)) {
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
      if (unreal_fname_equal(f->name, name, ignore_num)) {
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

/* =============================================== CONTAINER UTILITIES ============================================== */

static uint32_t *
unreal_tbit_array_data(tbit_array_t *bits)
{
  if (!bits) {
    return NULL;
  }

  if (bits->allocator.secondary_data) {
    return (uint32_t *)bits->allocator.secondary_data;
  }

  if (bits->num_bits > COUNTOF(bits->allocator.inline_data) * 32) {
    MOD_ASSERT_MSG(false, "TBitArray has %d bits but no secondary allocation", bits->num_bits);
    return NULL;
  }

  return bits->allocator.inline_data;
}

bool
unreal_tbit_array_is_set(tbit_array_t *bits, int32_t idx)
{
  if (!bits || idx < 0 || idx >= bits->num_bits) {
    return false;
  }

  uint32_t *data = unreal_tbit_array_data(bits);
  if (!data) {
    return false;
  }

  uint32_t bit_idx = (uint32_t)idx;
  return (data[bit_idx >> 5] & (1u << (bit_idx & 31))) != 0;
}

int32_t
unreal_tbit_array_find_next_set(tbit_array_t *bits, int32_t start_idx)
{
  if (!bits || bits->num_bits <= 0) {
    return TSET_INVALID_ID;
  }

  if (start_idx < 0) {
    start_idx = 0;
  }

  uint32_t *data = unreal_tbit_array_data(bits);
  if (!data) {
    return TSET_INVALID_ID;
  }

  for (int32_t idx = start_idx; idx < bits->num_bits; ++idx) {
    uint32_t bit_idx = (uint32_t)idx;
    if (data[bit_idx >> 5] & (1u << (bit_idx & 31))) {
      return idx;
    }
  }

  return TSET_INVALID_ID;
}

int32_t
unreal_tset_hash_head(hash_allocator_t *hash, int32_t hash_size, uint32_t key_hash)
{
  if (!hash || hash_size <= 0) {
    return TSET_INVALID_ID;
  }

  if ((hash_size & (hash_size - 1)) != 0) {
    MOD_ASSERT_MSG(false, "TSet hash size must be a power of two: %d", hash_size);
    return TSET_INVALID_ID;
  }

  int32_t *data = hash->secondary_data ? (int32_t *)hash->secondary_data : hash->inline_data;
  if (!hash->secondary_data && hash_size > COUNTOF(hash->inline_data)) {
    MOD_ASSERT_MSG(false, "TSet hash has %d buckets but no secondary allocation", hash_size);
    return TSET_INVALID_ID;
  }

  return data[key_hash & (uint32_t)(hash_size - 1)];
}

bool
unreal_map_helpers_available(void)
{
  const mod_host_api_t *host = mod_sdk_host();
  return MOD_HOST_API_HAS_FIELD(host, map_find) && host->map_add && host->map_remove && host->map_find;
}

void
unreal_map_add(void *map, fprop_map_t *prop, const void *key, const void *val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (!map || !prop || !key || !val || !MOD_HOST_API_HAS_FIELD(host, map_add) || !host->map_add) {
    return;
  }
  host->map_add(map, prop, key, val);
}

bool
unreal_map_remove(void *map, fprop_map_t *prop, const void *key)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (!map || !prop || !key || !MOD_HOST_API_HAS_FIELD(host, map_remove) || !host->map_remove) {
    return false;
  }
  return host->map_remove(map, prop, key);
}

bool
unreal_map_find(void *map, fprop_map_t *prop, const void *key, void *out_val)
{
  const mod_host_api_t *host = mod_sdk_host();
  if (!map || !prop || !key || !out_val || !MOD_HOST_API_HAS_FIELD(host, map_find) || !host->map_find) {
    return false;
  }
  return host->map_find(map, prop, key, out_val);
}

/* =================================================== DATA TABLE =================================================== */

tmap_fname_uint8ptr_t *
unreal_udata_table_get_row_map(udata_table_t *table)
{
  return (table) ? &table->row_map : NULL;
}

void
unreal_udata_table_add_row(udata_table_t *table, fname_t row_name, const ftable_row_base_t *row_data)
{
  if (table && row_data) {
    table->vtable->add_row(table, row_name, row_data);
  }
}

void
unreal_udata_table_remove_row(udata_table_t *table, fname_t row_name)
{
  if (table) {
    table->vtable->remove_row(table, row_name);
  }
}

void
unreal_udata_table_empty(udata_table_t *table)
{
  if (table) {
    table->vtable->empty_table(table);
  }
}

uint8_t *
unreal_udata_table_find_row(udata_table_t *table, fname_t row_name)
{
  if (table && table->row_struct && !unreal_fname_is_none(row_name)) {
    uint8_t **row_data_ptr = unreal_tmap_fname_uint8ptr_find(&table->row_map, row_name);
    if (row_data_ptr) {
      return *row_data_ptr;
    }
  }
  return NULL;
}
