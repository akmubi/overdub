#include "mod_manager.h"

#include "arena.h"
#include "file.h"
#include "globals.h"
#include "ini.h"
#include "input.h"
#include "log.h"
#include "mod.h"
#include "path.h"
#include "scratch.h"
#include "sigscan.h"
#include "str.h"
#include "types.h"
#include "unreal.h"

#include "ui_console.h"

#include "version.h"

#include <windows.h>

static str_t
mod_color_to_str(arena_t *arena, mod_color_t c)
{
  return str_push_fmt(arena, "%u.%u.%u.%u", c.r, c.g, c.b, c.a);
}

static mod_t *
mod_alloc_slot(mod_manager_t *manager, str_t id)
{
  if (manager->mod_count >= CONFIG_MOD_MANAGER_MAX_MODS) {
    return NULL;
  }

  for (int i = 0; i < manager->mod_count; ++i) {
    mod_t *m = &manager->mods[i];
    MASSERT(!str_equal(m->manifest.info.id, id, 0), "duplicate mod id: '%.*s'", STR_ARG(id));
  }

  uint16_t idx      = (uint16_t)manager->mod_count++;
  mod_t   *new_slot = &manager->mods[idx];
  mem_zero(new_slot, sizeof(*new_slot));
  new_slot->idx = idx;
  return new_slot;
}

bool
mod_handle_is_valid(mod_manager_t *manager, mod_handle_t h)
{
  uint16_t num = (uint16_t)(h & 0xFFFF);
  int      idx = (int)(num - 1);

  return (num > 0 && idx >= 0 && idx < manager->mod_count);
}

mod_handle_t
mod_handle_make(mod_t *m)
{
  if (!m) {
    return MOD_HANDLE_INVALID;
  }
  return m->idx + 1;
}

mod_t *
mod_handle_resolve(mod_manager_t *manager, mod_handle_t h)
{
  if (!mod_handle_is_valid(manager, h)) {
    return NULL;
  }

  uint16_t num = (uint16_t)(h & 0xFFFF);
  int      idx = (int)num - 1;

  mod_t *m = &manager->mods[idx];
  ASSERT(m->idx == idx);
  return m;
}

bool
mod_cfg_handle_is_valid(mod_manager_t *manager, mod_cfg_handle_t h)
{
  uint16_t mod_num = (uint16_t)((h >> 0) & 0xFFFF);
  uint16_t cfg_num = (uint16_t)((h >> 16) & 0xFFFF);
  int      mod_idx = (int)(mod_num - 1);
  int      cfg_idx = (int)(cfg_num - 1);

  return (mod_num > 0) && (cfg_num > 0) && (mod_idx >= 0 && mod_idx < manager->mod_count) && (cfg_idx >= 0 && cfg_idx < manager->mods[mod_idx].option_count);
}

mod_cfg_handle_t
mod_cfg_handle_make(mod_option_runtime_t *rt)
{
  if (!rt) {
    return MOD_CFG_HANDLE_INVALID;
  }

  uint16_t cfg_num = rt->idx + 1;
  uint16_t mod_num = rt->mod_idx + 1;

  return ((uint64_t)cfg_num << 16) | ((uint64_t)mod_num);
}

mod_option_runtime_t *
mod_cfg_handle_resolve(mod_manager_t *manager, mod_cfg_handle_t h)
{
  if (!mod_cfg_handle_is_valid(manager, h)) {
    return NULL;
  }

  uint64_t mod_num = (uint64_t)((h >> 0) & 0xFFFF);
  uint64_t cfg_num = (uint64_t)((h >> 16) & 0xFFFF);
  int      mod_idx = (int)mod_num - 1;
  int      cfg_idx = (int)cfg_num - 1;

  mod_t *m = &manager->mods[mod_idx];
  ASSERT(m->idx == mod_idx);

  mod_option_runtime_t *rt = &m->options[cfg_idx];
  ASSERT(rt->idx == cfg_idx);

  return rt;
}

static mod_option_info_t *
mod_option_info_get(mod_manager_t *manager, mod_cfg_handle_t h)
{
  if (!mod_cfg_handle_is_valid(manager, h)) {
    return NULL;
  }

  uint16_t mod_num = (uint16_t)((h >> 0) & 0xFFFF);
  uint16_t cfg_num = (uint16_t)((h >> 16) & 0xFFFF);
  int      mod_idx = (int)mod_num - 1;
  int      cfg_idx = (int)cfg_num - 1;

  return &manager->mods[mod_idx].manifest.options[cfg_idx];
}

static mod_t *
mod_cfg_handle_resolve_mod(mod_manager_t *manager, mod_cfg_handle_t h)
{
  if (!mod_cfg_handle_is_valid(manager, h)) {
    return NULL;
  }

  uint16_t mod_num = (uint16_t)((h >> 0) & 0xFFFF);
  int      mod_idx = (int)mod_num - 1;

  return &manager->mods[mod_idx];
}

mod_handle_t
mod_manager_find_mod(mod_manager_t *manager, str_t mod_id)
{
  for (int i = 0; i < manager->mod_count; ++i) {
    str_t id = manager->mods[i].manifest.info.id;
    if (str_equal(id, mod_id, 0)) {
      return mod_handle_make(&manager->mods[i]);
    }
  }
  return MOD_HANDLE_INVALID;
}

static mod_t *
find_mod(mod_manager_t *manager, str_t mod_id)
{
  for (int i = 0; i < manager->mod_count; ++i) {
    str_t id = manager->mods[i].manifest.info.id;
    if (str_equal(id, mod_id, 0)) {
      return &manager->mods[i];
    }
  }
  return NULL;
}

void
mod_manager_apply_order_from_config(mod_manager_t *manager, str_array_t mod_order_ids)
{
  bool has_order[CONFIG_MOD_MANAGER_MAX_MODS] = {0};

  manager->mod_order.count = 0;
  mem_zero(manager->mod_order.want, sizeof(manager->mod_order.want));

  /* add known mods in the order given by the ID list */
  for (uint64_t i = 0; i < mod_order_ids.count; ++i) {
    str_t  id = mod_order_ids.items[i];
    mod_t *m  = find_mod(manager, id);
    if (m) {
      MASSERT(m->idx < CONFIG_MOD_MANAGER_MAX_MODS, "invalid mod index: %d", m->idx);

      /* filter out duplicates */
      uint32_t slot = m->idx;
      if (!has_order[slot]) {
        manager->mod_order.want[manager->mod_order.count++] = mod_handle_make(m);
        has_order[slot]                                     = true;
      }
    }
  }

  /* append mods missing from the ID list */
  for (int i = 0; i < manager->mod_count; ++i) {
    mod_t *m = &manager->mods[i];
    MASSERT(m->idx < CONFIG_MOD_MANAGER_MAX_MODS, "invalid mod index: %d", m->idx);

    uint32_t slot = m->idx;
    if (!has_order[slot]) {
      manager->mod_order.want[manager->mod_order.count++] = mod_handle_make(m);
      has_order[slot]                                     = true;
    }
  }

  memcpy(manager->mod_order.runtime, manager->mod_order.want, sizeof(manager->mod_order.runtime));
}

static str_list_t
mod_manager_get_mod_order_id_list(mod_manager_t *manager, arena_t *arena)
{
  str_list_t id_list = {0};
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t h = manager->mod_order.want[i]; // NOTE: not runtime mod order
    mod_t       *m = mod_handle_resolve(manager, h);
    if (m) {
      str_list_push(arena, &id_list, m->manifest.info.id);
    }
  }
  return id_list;
}

static str_list_t
mod_manager_get_disabled_mods_list(mod_manager_t *manager, arena_t *arena)
{
  str_list_t id_list = {0};
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t h = manager->mod_order.want[i];
    mod_t       *m = mod_handle_resolve(manager, h);
    if (m && !m->enabled) {
      str_list_push(arena, &id_list, m->manifest.info.id);
    }
  }
  return id_list;
}

static void
mod_manager_apply_disabled_mods(mod_manager_t *manager, str_array_t mod_ids)
{
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t h = manager->mod_order.want[i];
    mod_t       *m = mod_handle_resolve(manager, h);
    if (m) {
      if (str_array_contains(mod_ids, m->manifest.info.id, 0)) {
        m->enabled = false;
      } else {
        m->enabled = true;
      }
    }
  }
}

static bool
mod_parse_kind(str_t s, mod_kind_t *out)
{
  static str_t mod_kind_names[MOD_KIND_MAX] = {
    [MOD_KIND_MOD]  = STR_CLIT("mod"),
    [MOD_KIND_TOOL] = STR_CLIT("tool"),
  };

  for (int i = 0; i < COUNTOF(mod_kind_names); ++i) {
    str_t name = mod_kind_names[i];
    if (str_equal_icase(s, name)) {
      *out = (mod_kind_t)i;
      return true;
    }
  }
  return false;
}

static bool
mod_parse_attach_to(str_t s, mod_spawn_context_kind_t *out)
{
  static str_t context_spawn_names[MOD_SPAWN_CONTEXT_MAX] = {
    [MOD_SPAWN_CONTEXT_NONE]              = STR_CLIT("none"),
    [MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER] = STR_CLIT("player_controller"),
    [MOD_SPAWN_CONTEXT_LOCAL_PLAYER]      = STR_CLIT("local_player"),
    [MOD_SPAWN_CONTEXT_PAWN]              = STR_CLIT("pawn"),
    [MOD_SPAWN_CONTEXT_HUD]               = STR_CLIT("hud"),
    [MOD_SPAWN_CONTEXT_WORLD]             = STR_CLIT("world"),
    [MOD_SPAWN_CONTEXT_GAME_INSTANCE]     = STR_CLIT("game_instance"),
    [MOD_SPAWN_CONTEXT_CUSTOM]            = STR_CLIT("custom_class_path"),
  };

  for (int i = 0; i < COUNTOF(context_spawn_names); ++i) {
    str_t name = context_spawn_names[i];
    if (str_equal_icase(s, name)) {
      *out = (mod_spawn_context_kind_t)i;
      return true;
    }
  }
  return false;
}

static bool
parse_version(str_t s, version_t *out_version)
{
  bool result = false;
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    mem_zero(out_version, sizeof(*out_version));

    str_t      splits[]  = {STR_LIT(".")};
    str_list_t part_list = str_split(tmp.arena, s, 1, splits);

    if (part_list.count > 0 && part_list.count <= 3) {
      str_array_t parts = str_array_from_list(tmp.arena, part_list);

      if (str_parse_u16(parts.items[0], &out_version->major)) {
        if (parts.count > 1) {
          if (str_parse_u16(parts.items[1], &out_version->minor)) {
            if (parts.count > 2) {
              result = str_parse_u32(parts.items[2], &out_version->patch);
            } else {
              result = true;
            }
          }
        } else {
          result = true;
        }
      }
    }
  }
  scratch_end(tmp);
  return result;
}

static bool
mod_parse_option_type(str_t s, mod_option_type_t *out)
{
  static str_t names[MOD_OPTION_MAX] = {
    [MOD_OPTION_NONE]    = STR_CLIT("none"),
    [MOD_OPTION_BOOL]    = STR_CLIT("bool"),
    [MOD_OPTION_INT]     = STR_CLIT("int"),
    [MOD_OPTION_FLOAT]   = STR_CLIT("float"),
    [MOD_OPTION_ENUM]    = STR_CLIT("enum"),
    [MOD_OPTION_STRING]  = STR_CLIT("string"),
    [MOD_OPTION_KEYBIND] = STR_CLIT("keybind"),
    [MOD_OPTION_COLOR]   = STR_CLIT("color"),
  };

  for (int i = 0; i < COUNTOF(names); ++i) {
    str_t name = names[i];
    if (str_equal_icase(s, name)) {
      *out = (mod_option_type_t)i;
      return true;
    }
  }
  return false;
}

static bool
mod_parse_color(str_t s, mod_color_t *c)
{
  tmp_arena_t tmp    = scratch_begin(NULL);
  bool        result = false;

  str_t       splits[]   = {STR_LIT(".")};
  str_list_t  parts_list = str_split(tmp.arena, s, 1, splits);
  str_array_t parts      = str_array_from_list(tmp.arena, parts_list);

  if (parts.count == 4) {
    result = str_parse_u8(parts.items[0], &c->r) && str_parse_u8(parts.items[1], &c->g) && str_parse_u8(parts.items[2], &c->b) && str_parse_u8(parts.items[3], &c->a);
  }

  scratch_end(tmp);
  return result;
}

bool
mod_manager_check_manifest(mod_manager_t *manager, mod_manifest_t *m)
{
  if (m->info.id.len <= 0) {
    LOG_WARN("%.*s: mod-info: missing mod id", STR_ARG(m->manifest_path));
    return false;
  }

  if (version_compare(m->info.version, (version_t)MAKE_VERSION(0, 0, 0)) == 0) {
    LOG_WARN("%.*s: mod-info: missing/invalid mod version: %u.%u.%u", STR_ARG(m->manifest_path), VERSION_ARG(m->info.version));
    return false;
  }

  if (m->info.name.len <= 0) {
    LOG_WARN("%.*s: mod-info: missing mod name", STR_ARG(m->manifest_path));
    return false;
  }

  bool has_assets = (m->asset.pak_path.len > 0 || (m->asset.utoc_path.len > 0 && m->asset.ucas_path.len > 0));
  bool has_bp     = (m->blueprint_count > 0);

  if (has_bp && !has_assets) {
    LOG_WARN("%.*s: blueprint: no asset files specified", STR_ARG(m->manifest_path));
    return false;
  }

  if (m->asset.utoc_path.len > 0 && m->asset.ucas_path.len <= 0) {
    LOG_WARN("%.*s: asset: missing .ucas file", STR_ARG(m->manifest_path));
    return false;
  }

  if (m->asset.ucas_path.len > 0 && m->asset.utoc_path.len <= 0) {
    LOG_WARN("%.*s: asset: missing .utoc file", STR_ARG(m->manifest_path));
    return false;
  }

  if (find_mod(manager, m->info.id)) {
    LOG_WARN("%.*s: there is already a mod registered with such id ('%.*s')", STR_ARG(m->manifest_path), STR_ARG(m->info.id));
    return false;
  }

  if (m->blueprints) {
    for (int i = 0; i < m->blueprint_count - 1; ++i) {
      str_t a_id = m->blueprints[i].id;

      for (int j = i + 1; j < m->blueprint_count; ++j) {
        str_t b_id = m->blueprints[j].id;

        if (str_equal_icase(a_id, b_id)) {
          LOG_WARN("%.*s: duplicate blueprint id: '%.*s'", STR_ARG(m->manifest_path), STR_ARG(a_id));
          return false;
        }
      }
    }
  }

  if (m->options) {
    for (int i = 0; i < m->option_count - 1; ++i) {
      str_t a_id = m->options[i].id;

      for (int j = i + 1; j < m->option_count; ++j) {
        str_t b_id = m->options[j].id;

        if (str_equal_icase(a_id, b_id)) {
          LOG_WARN("%.*s: duplicate option id: '%.*s'", STR_ARG(m->manifest_path), STR_ARG(a_id));
          return false;
        }
      }
    }

    for (int i = 0; i < m->option_count; ++i) {
      mod_option_info_t *opt_info = &m->options[i];
      if (opt_info->type == MOD_OPTION_NONE) {
        LOG_WARN("%.*s: missing or invalid option type for '%.*s'", STR_ARG(m->manifest_path), STR_ARG(opt_info->id));
        return false;
      }
    }
  }

  return true;
}

static void
mod_manifest_parse_info_section(arena_t *arena, ini_section_t *section, mod_manifest_t *out)
{
  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    if (str_equal_icase(key, STR_LIT("id"))) {
      out->info.id = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("name"))) {
      out->info.name = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("author"))) {
      out->info.author = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("version"))) {
      parse_version(value, &out->info.version);
    } else if (str_equal_icase(key, STR_LIT("kind"))) {
      mod_parse_kind(value, &out->info.kind);
    }
  }
}

static void
mod_manifest_parse_description_section(arena_t *arena, ini_section_t *section, mod_manifest_t *out)
{
  out->info.description = str_list_join_lines(arena, section->lines);
}

static void
mod_manifest_parse_dll_section(arena_t *arena, ini_section_t *section, str_t mod_dir, mod_manifest_t *out)
{
  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    if (str_equal_icase(key, STR_LIT("path"))) {
      str_t dll_path = value;
      if (!path_is_abs(dll_path)) {
        out->dll.path = path_join(arena, mod_dir, dll_path);
      } else {
        out->dll.path = str_push_copy(arena, dll_path);
      }
    }
  }
}

static void
mod_manifest_parse_assets_section(arena_t *arena, ini_section_t *section, str_t mod_dir, mod_manifest_t *out)
{
  tmp_arena_t tmp = scratch_begin(arena);

  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    if (str_equal_icase(key, STR_LIT("path_without_ext"))) {
      str_t asset_dir = path_join(tmp.arena, mod_dir, path_dir(value));

      str_t asset_basename = path_base(value);
      str_t pattern        = str_push_fmt(tmp.arena, "%.*s*", STR_ARG(asset_basename));

      dir_entry_list_t entries = dir_list_filter(asset_dir, pattern, tmp.arena);
      for (int i = 0; i < entries.count; ++i) {
        dir_entry_t *ent = &entries.items[i];
        if (ent->kind != DIR_ENTRY_FILE) {
          continue;
        }

        str_t ext = path_get_ext(ent->path);
        if (str_equal_icase(ext, STR_LIT("pak"))) {
          out->asset.pak_path = str_push_concat(arena, value, STR_LIT(".pak"));
        } else if (str_equal_icase(ext, STR_LIT("utoc"))) {
          out->asset.utoc_path = str_push_concat(arena, value, STR_LIT(".utoc"));
        } else if (str_equal_icase(ext, STR_LIT("ucas"))) {
          out->asset.ucas_path = str_push_concat(arena, value, STR_LIT(".ucas"));
        }
      }
    }
  }

  scratch_end(tmp);
}

static void
mod_manifest_parse_blueprint_section(arena_t *arena, ini_section_t *section, mod_manifest_t *out)
{
  if (!out->blueprints) {
    return;
  }

  mod_blueprint_info_t *bp = &out->blueprints[out->blueprint_count++];
  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    if (str_equal_icase(key, STR_LIT("id"))) {
      bp->id = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("mod_actor_class_path"))) {
      bp->mod_actor_class_path = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("attach_to"))) {
      mod_parse_attach_to(value, &bp->attach_to);
    } else if (str_equal_icase(key, STR_LIT("custom_attach_class_path"))) {
      bp->custom_attach_class_path = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("auto_spawn"))) {
      bp->auto_spawn = str_parse_bool(value, false);
    } else if (str_equal_icase(key, STR_LIT("default_spawn_keybind"))) {
      bp->default_spawn_keybind = keybind_parse(value, KEYBIND_NULL);
    }
  }
}

static void
mod_manifest_parse_option_section(arena_t *arena, ini_section_t *section, mod_manifest_t *out)
{
  if (!out->options) {
    return;
  }

  mod_option_type_t opt_type = MOD_OPTION_NONE;
  if (!mod_parse_option_type(section->arg, &opt_type)) {
    return;
  }

  mod_option_info_t *opt = &out->options[out->option_count++];
  opt->type = opt_type;

  tmp_arena_t tmp = scratch_begin(arena);
  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    if (str_equal_icase(key, STR_LIT("id"))) {
      opt->id = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("label"))) {
      opt->label = str_push_copy(arena, value);
    } else if (str_equal_icase(key, STR_LIT("description"))) {
      opt->description = str_push_copy(arena, value);
    }

    switch (opt->type) {
    case MOD_OPTION_BOOL: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        opt->val.boolean.default_val = str_parse_bool(value, false);
      }
    } break;

    case MOD_OPTION_INT: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        str_parse_int(value, &opt->val.integer.default_val);
      } else if (str_equal_icase(key, STR_LIT("min_value"))) {
        str_parse_int(value, &opt->val.integer.min_val);
      } else if (str_equal_icase(key, STR_LIT("max_value"))) {
        str_parse_int(value, &opt->val.integer.max_val);
      } else if (str_equal_icase(key, STR_LIT("step"))) {
        str_parse_int(value, &opt->val.integer.step);
      }
    } break;

    case MOD_OPTION_FLOAT: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        str_parse_float(value, &opt->val.floating.default_val);
      } else if (str_equal_icase(key, STR_LIT("min_value"))) {
        str_parse_float(value, &opt->val.floating.min_val);
      } else if (str_equal_icase(key, STR_LIT("max_value"))) {
        str_parse_float(value, &opt->val.floating.max_val);
      } else if (str_equal_icase(key, STR_LIT("step"))) {
        str_parse_float(value, &opt->val.floating.step);
      }
    } break;

    case MOD_OPTION_ENUM: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        opt->val.enumeration.default_val = str_push_copy(arena, value);
      } else if (str_equal_icase(key, STR_LIT("enum_values"))) {
        str_t      splits[] = {STR_LIT("|")};
        str_list_t parts    = str_split(tmp.arena, value, 1, splits);

        opt->val.enumeration.values = str_array_copy_from_list(arena, parts);
      }
    } break;

    case MOD_OPTION_STRING: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        opt->val.string.default_val = str_push_copy(arena, value);
      }
    } break;

    case MOD_OPTION_KEYBIND: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        opt->val.keybind.default_val = keybind_parse(value, KEYBIND_NULL);
      }
    } break;

    case MOD_OPTION_COLOR: {
      if (str_equal_icase(key, STR_LIT("default_value"))) {
        mod_parse_color(value, &opt->val.color.default_val);
      }
    } break;
    }
  }

  scratch_end(tmp);
}

static void
mod_info_copy(mod_info_t *dst, mod_info_t *src, arena_t *arena)
{
  mem_zero(dst, sizeof(*dst));

  dst->id          = str_push_copy(arena, src->id);
  dst->name        = str_push_copy(arena, src->name);
  dst->author      = str_push_copy(arena, src->author);
  dst->description = str_push_copy(arena, src->description);

  dst->kind    = src->kind;
  dst->version = src->version;
}

static void
mod_dll_info_copy(mod_dll_info_t *dst, mod_dll_info_t *src, arena_t *arena)
{
  mem_zero(dst, sizeof(*dst));

  dst->path = str_push_copy(arena, src->path);
}

static void
mod_asset_info_copy(mod_asset_info_t *dst, mod_asset_info_t *src, arena_t *arena)
{
  mem_zero(dst, sizeof(*dst));

  dst->pak_path  = str_push_copy(arena, src->pak_path);
  dst->utoc_path = str_push_copy(arena, src->utoc_path);
  dst->ucas_path = str_push_copy(arena, src->ucas_path);
}

static void
mod_blueprint_info_copy(mod_blueprint_info_t *dst, mod_blueprint_info_t *src, arena_t *arena)
{
  mem_zero(dst, sizeof(*dst));

  dst->id                       = str_push_copy(arena, src->id);
  dst->mod_actor_class_path     = str_push_copy(arena, src->mod_actor_class_path);
  dst->attach_to                = src->attach_to;
  dst->custom_attach_class_path = str_push_copy(arena, src->custom_attach_class_path);
  dst->auto_spawn               = src->auto_spawn;
  dst->default_spawn_keybind    = src->default_spawn_keybind;
}

static void
mod_option_info_copy(mod_option_info_t *dst, mod_option_info_t *src, arena_t *arena)
{
  mem_zero(dst, sizeof(*dst));

  dst->type        = src->type;
  dst->id          = str_push_copy(arena, src->id);
  dst->label       = str_push_copy(arena, src->label);
  dst->description = str_push_copy(arena, src->description);

  switch (src->type) {
  case MOD_OPTION_NONE: {
  } break;

  case MOD_OPTION_BOOL: {
    dst->val.boolean.default_val = src->val.boolean.default_val;
  } break;

  case MOD_OPTION_INT: {
    dst->val.integer.default_val = src->val.integer.default_val;
    dst->val.integer.min_val     = src->val.integer.min_val;
    dst->val.integer.max_val     = src->val.integer.max_val;
    dst->val.integer.step        = src->val.integer.step;
  } break;

  case MOD_OPTION_FLOAT: {
    dst->val.floating.default_val = src->val.floating.default_val;
    dst->val.floating.min_val     = src->val.floating.min_val;
    dst->val.floating.max_val     = src->val.floating.max_val;
    dst->val.floating.step        = src->val.floating.step;
  } break;

  case MOD_OPTION_ENUM: {
    dst->val.enumeration.values      = str_array_copy(arena, src->val.enumeration.values);
    dst->val.enumeration.default_val = str_push_copy(arena, src->val.enumeration.default_val);
  } break;

  case MOD_OPTION_STRING: {
    dst->val.string.default_val = str_push_copy(arena, src->val.string.default_val);
  } break;

  case MOD_OPTION_KEYBIND: {
    dst->val.keybind.default_val = src->val.keybind.default_val;
  } break;

  case MOD_OPTION_COLOR: {
    dst->val.color.default_val = src->val.color.default_val;
  } break;

  default: {
    ASSERT(0);
  }
  }
}

static void
mod_manifest_copy(mod_manifest_t *dst, mod_manifest_t *src, arena_t *arena)
{
  mem_zero(dst, sizeof(*dst));

  dst->mod_dir_name  = str_push_copy(arena, src->mod_dir_name);
  dst->mod_dir       = str_push_copy(arena, src->mod_dir);
  dst->manifest_path = str_push_copy(arena, src->manifest_path);
  dst->config_path   = str_push_copy(arena, src->config_path);

  mod_info_copy(&dst->info, &src->info, arena);
  mod_dll_info_copy(&dst->dll, &src->dll, arena);
  mod_asset_info_copy(&dst->asset, &src->asset, arena);

  dst->blueprint_count = src->blueprint_count;
  dst->blueprints      = ARENA_PUSH_ARRAY(arena, mod_blueprint_info_t, dst->blueprint_count);
  if (!dst->blueprints) {
    dst->blueprint_count = 0;
  }

  for (int i = 0; i < dst->blueprint_count; ++i) {
    mod_blueprint_info_copy(&dst->blueprints[i], &src->blueprints[i], arena);
  }

  dst->option_count = src->option_count;
  dst->options      = ARENA_PUSH_ARRAY(arena, mod_option_info_t, dst->option_count);
  if (!dst->options) {
    dst->option_count = 0;
  }

  for (int i = 0; i < dst->option_count; ++i) {
    mod_option_info_copy(&dst->options[i], &src->options[i], arena);
  }
}

static bool
mod_manifest_parse(arena_t *arena, str_t mod_dir, mod_manifest_t *out)
{
  tmp_arena_t tmp    = scratch_begin(arena);
  bool        result = false;

  str_t manifest_path = path_join(tmp.arena, mod_dir, CONFIG_MOD_MANIFEST_FILE_NAME);
  str_t config_path   = path_join(tmp.arena, mod_dir, CONFIG_MOD_CONFIG_FILE_NAME);

  mem_zero(out, sizeof(*out));
  if (file_exists(manifest_path)) {
    str_t text = file_read_all(manifest_path, tmp.arena);
    if (!str_is_empty(text)) {
      out->mod_dir_name  = str_push_copy(arena, path_base(mod_dir));
      out->mod_dir       = str_push_copy(arena, mod_dir);
      out->manifest_path = str_push_copy(arena, manifest_path);
      out->config_path   = str_push_copy(arena, config_path);

      str_array_t        lines    = ini_split_lines(tmp.arena, text);
      ini_section_list_t sections = ini_parse_sections(tmp.arena, lines);

      int blueprint_count = 0;
      int option_count    = 0;

      /* count blueprint and option sections first */
      for (ini_section_t *section = sections.first; section; section = section->next) {
        if (str_equal_icase(section->name, STR_LIT("blueprint"))) {
          blueprint_count++;
        } else if (str_equal_icase(section->name, STR_LIT("option"))) {
          option_count++;
        }
      }

      out->blueprints = ARENA_PUSH_ARRAY_ZERO(arena, mod_blueprint_info_t, blueprint_count);
      out->options    = ARENA_PUSH_ARRAY_ZERO(arena, mod_option_info_t, option_count);

      for (ini_section_t *section = sections.first; section; section = section->next) {
        if (str_equal_icase(section->name, STR_LIT("info"))) {
          mod_manifest_parse_info_section(arena, section, out);
        } else if (str_equal_icase(section->name, STR_LIT("description"))) {
          mod_manifest_parse_description_section(arena, section, out);
        } else if (str_equal_icase(section->name, STR_LIT("code"))) {
          mod_manifest_parse_dll_section(arena, section, out->mod_dir, out);
        } else if (str_equal_icase(section->name, STR_LIT("assets"))) {
          mod_manifest_parse_assets_section(arena, section, out->mod_dir, out);
        } else if (str_equal_icase(section->name, STR_LIT("blueprint"))) {
          mod_manifest_parse_blueprint_section(arena, section, out);
        } else if (str_equal_icase(section->name, STR_LIT("option"))) {
          mod_manifest_parse_option_section(arena, section, out);
        }
      }

      result = true;
    }
  }

  scratch_end(tmp);
  return result;
}

bool
mod_cfg_string_set(mod_cfg_string_t *dst, str_t src)
{
  if (!dst) {
    return false;
  }

  if (src.len > CONFIG_MOD_CFG_MAX_STR_LEN) {
    return false;
  }

  dst->len = 0;
  if (str_is_empty(src)) {
    return true;
  }

  dst->len = (int)src.len;
  mem_copy(dst->data, src.data, dst->len);
  return true;
}

str_t
mod_cfg_string_as_str(mod_cfg_string_t *s)
{
  str_t result = {0};
  if (s) {
    result = str_make((uint8_t *)s->data, s->len);
  }
  return result;
}

static void
mod_cfg_parse_options_section(mod_t *m, ini_section_t *section)
{
  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    int opt_idx = -1;
    for (int i = 0; i < m->option_count; ++i) {
      if (str_equal_icase(key, m->manifest.options[i].id)) {
        opt_idx = i;
        break;
      }
    }

    if (opt_idx < 0) {
      /* unknown option */
      continue;
    }

    mod_option_info_t    *opt_info = &m->manifest.options[opt_idx];
    mod_option_runtime_t *opt      = &m->options[opt_idx];
    switch (opt_info->type) {
    case MOD_OPTION_BOOL: {
      opt->cfg.boolean = str_parse_bool(value, opt->cfg.boolean);
    } break;

    case MOD_OPTION_INT: {
      str_parse_int(value, &opt->cfg.integer);
    } break;

    case MOD_OPTION_FLOAT: {
      str_parse_float(value, &opt->cfg.floating);
    } break;

    case MOD_OPTION_ENUM: {
      uint64_t idx = 0;
      if (str_array_find(opt_info->val.enumeration.values, value, STR_CMP_FLAG_IGNORE_CASE, &idx)) {
        opt->cfg.enum_item_id = (int)idx;
      }
    } break;

    case MOD_OPTION_STRING: {
      mod_cfg_string_set(&opt->cfg.string, value);
    } break;

    case MOD_OPTION_KEYBIND: {
      opt->cfg.keybind = keybind_parse(value, opt->cfg.keybind);
    } break;

    case MOD_OPTION_COLOR: {
      mod_parse_color(value, &opt->cfg.color);
    } break;
    }
  }
}

static void
mod_cfg_parse_blueprint_section(mod_t *mod, ini_section_t *section)
{
  int bp_idx = -1;
  for (int i = 0; i < mod->blueprint_count; ++i) {
    if (str_equal(mod->manifest.blueprints[i].id, section->arg, 0)) {
      bp_idx = i;
      break;
    }
  }

  if (bp_idx < 0) {
    /* no matching blueprint */
    return;
  }

  for (str_node_t *node = section->lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    mod_blueprint_info_t    *bp_info = &mod->manifest.blueprints[bp_idx];
    mod_blueprint_runtime_t *bp      = &mod->blueprints[bp_idx];

    if (str_equal_icase(key, STR_LIT("spawn_keybind"))) {
      bp->cfg.spawn_keybind = keybind_parse(value, bp_info->default_spawn_keybind);
    } else if (str_equal_icase(key, STR_LIT("auto_spawn"))) {
      bp->cfg.auto_spawn = str_parse_bool(value, bp_info->auto_spawn);
    }
  }
}

bool
mod_cfg_load(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return false;
  }

  mod_t *mod = mod_handle_resolve(manager, h);
  if (!mod) {
    return false;
  }

  mod_cfg_reset(manager, h);

  tmp_arena_t tmp    = scratch_begin(NULL);
  bool        result = false;
  if (file_exists(mod->manifest.config_path)) {
    str_t text = file_read_all(mod->manifest.config_path, tmp.arena);
    if (!str_is_empty(text)) {
      str_array_t        lines    = ini_split_lines(tmp.arena, text);
      ini_section_list_t sections = ini_parse_sections(tmp.arena, lines);
      for (ini_section_t *section = sections.first; section; section = section->next) {

        if (str_equal_icase(section->name, STR_LIT("options"))) {
          mod_cfg_parse_options_section(mod, section);
        } else if (str_equal_icase(section->name, STR_LIT("blueprint"))) {
          mod_cfg_parse_blueprint_section(mod, section);
        }
      }

      result = true;
    }
  }

  for (int i = 0; i < mod->option_count; ++i) {
    mod->options[i].saved_cfg = mod->options[i].cfg;
  }

  for (int i = 0; i < mod->blueprint_count; ++i) {
    mod->blueprints[i].saved_cfg = mod->blueprints[i].cfg;
  }

  scratch_end(tmp);
  return result;
}

static void
mod_cfg_save_options(arena_t *arena, str_list_t *lines, mod_t *mod)
{
  tmp_arena_t tmp = scratch_begin(arena);
  {
    for (int i = 0; i < mod->manifest.option_count; ++i) {
      mod_option_info_t    *opt_info = &mod->manifest.options[i];
      mod_option_runtime_t *opt      = &mod->options[i];

      switch (opt_info->type) {
      case MOD_OPTION_BOOL: {
        str_list_push_fmt(arena, lines, "%.*s = %s", STR_ARG(opt_info->id), opt->cfg.boolean ? "true" : "false");
      } break;

      case MOD_OPTION_INT: {
        str_list_push_fmt(arena, lines, "%.*s = %d", STR_ARG(opt_info->id), opt->cfg.integer);
      } break;

      case MOD_OPTION_FLOAT: {
        str_list_push_fmt(arena, lines, "%.*s = %.9g", STR_ARG(opt_info->id), opt->cfg.floating);
      } break;

      case MOD_OPTION_ENUM: {
        str_t  item       = STR_NULL;
        int    item_id    = opt->cfg.enum_item_id;
        int    item_count = (int)opt_info->val.enumeration.values.count;
        str_t *items      = opt_info->val.enumeration.values.items;

        if (item_id >= 0 && item_id < item_count) {
          item = items[item_id];
        }

        str_list_push_fmt(arena, lines, "%.*s = %.*s", STR_ARG(opt_info->id), STR_ARG(item));
      } break;

      case MOD_OPTION_STRING: {
        str_list_push_fmt(arena, lines, "%.*s = %.*s", STR_ARG(opt_info->id), STR_ARG(opt->cfg.string));
      } break;

      case MOD_OPTION_KEYBIND: {
        str_t value = keybind_to_str(opt->cfg.keybind, tmp.arena);
        str_list_push_fmt(arena, lines, "%.*s = %.*s", STR_ARG(opt_info->id), STR_ARG(value));
      } break;

      case MOD_OPTION_COLOR: {
        str_t value = mod_color_to_str(tmp.arena, opt->cfg.color);
        str_list_push_fmt(arena, lines, "%.*s = %.*s", STR_ARG(opt_info->id), STR_ARG(value));
      } break;
      }
    }
  }
  scratch_end(tmp);
}

bool
mod_cfg_save(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return false;
  }

  mod_t *mod = mod_handle_resolve(manager, h);
  if (!mod) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  bool        result = false;

  str_list_t lines = {0};
  if (!str_is_empty(mod->manifest.config_path)) {
    if (mod->has_options) {
      str_list_push(tmp.arena, &lines, STR_LIT("[options]"));
      mod_cfg_save_options(tmp.arena, &lines, mod);
      str_list_push(tmp.arena, &lines, STR_NULL);
    }

    if (mod->has_blueprints) {
      for (int i = 0; i < mod->manifest.blueprint_count; ++i) {
        mod_blueprint_info_t    *bp_info = &mod->manifest.blueprints[i];
        mod_blueprint_runtime_t *bp      = &mod->blueprints[i];

        str_list_push_fmt(tmp.arena, &lines, "[blueprint.%.*s]", STR_ARG(bp_info->id));
        str_list_push_fmt(tmp.arena, &lines, "auto_spawn = %s", BOOL_AS_STR(bp->cfg.auto_spawn));
        str_list_push_fmt(tmp.arena, &lines, "spawn_keybind = %.*s", STR_ARG(keybind_to_str(bp->cfg.spawn_keybind, tmp.arena)));
        str_list_push(tmp.arena, &lines, STR_NULL);
      }
    }

    result = file_write_lines(lines, mod->manifest.config_path);
  }

  if (result) {
    for (int i = 0; i < mod->option_count; ++i) {
      mod->options[i].saved_cfg = mod->options[i].cfg;
    }

    for (int i = 0; i < mod->blueprint_count; ++i) {
      mod->blueprints[i].saved_cfg = mod->blueprints[i].cfg;
    }
  }

  scratch_end(tmp);
  return result;
}

bool
mod_cfg_reset(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return false;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (m->has_options) {
    for (int i = 0; i < m->manifest.option_count; ++i) {
      mod_option_info_t    *opt_info = &m->manifest.options[i];
      mod_option_runtime_t *opt      = &m->options[i];

      /* copy default values */
      switch (opt_info->type) {
      case MOD_OPTION_BOOL: {
        opt->cfg.boolean = opt_info->val.boolean.default_val;
      } break;

      case MOD_OPTION_INT: {
        opt->cfg.integer = opt_info->val.integer.default_val;
      } break;

      case MOD_OPTION_FLOAT: {
        opt->cfg.floating = opt_info->val.floating.default_val;
      } break;

      case MOD_OPTION_ENUM: {
        uint64_t idx = 0;
        if (str_array_find(opt_info->val.enumeration.values, opt_info->val.enumeration.default_val, STR_CMP_FLAG_IGNORE_CASE, &idx)) {
          opt->cfg.enum_item_id = (int)idx;
        } else {
          opt->cfg.enum_item_id = -1;
        }
      } break;

      case MOD_OPTION_STRING: {
        mod_cfg_string_set(&opt->cfg.string, opt_info->val.string.default_val);
      } break;

      case MOD_OPTION_KEYBIND: {
        opt->cfg.keybind = opt_info->val.keybind.default_val;
      } break;

      case MOD_OPTION_COLOR: {
        opt->cfg.color = opt_info->val.color.default_val;
      } break;
      }
    }
  }
  return true;
}

bool
mod_cfg_revert(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return false;
  }

  mod_t *mod = mod_handle_resolve(manager, h);
  if (!mod) {
    return false;
  }

  for (int i = 0; i < mod->blueprint_count; ++i) {
    mod->blueprints[i].cfg = mod->blueprints[i].saved_cfg;
  }

  for (int i = 0; i < mod->option_count; ++i) {
    mod->options[i].cfg = mod->options[i].saved_cfg;
  }

  return true;
}

bool
mod_manager_load_cfg(mod_manager_t *manager)
{
  tmp_arena_t tmp    = scratch_begin(NULL);
  bool        result = false;

  if (file_exists(manager->config_path)) {
    str_t text = file_read_all(manager->config_path, tmp.arena);
    if (!str_is_empty(text)) {
      str_array_t        lines    = ini_split_lines(tmp.arena, text);
      ini_section_list_t sections = ini_parse_sections(tmp.arena, lines);

      bool console_ok = true;
      for (ini_section_t *section = sections.first; section; section = section->next) {
        if (str_equal_icase(section->name, STR_LIT("loader"))) {
          for (str_node_t *node = section->lines.first; node; node = node->next) {
            str_t key   = {0};
            str_t value = {0};
            if (!ini_parse_kv(node->str, &key, &value)) {
              continue;
            }

            if (str_equal_icase(key, STR_LIT("mod_dir"))) {
              mod_cfg_string_set(&manager->cfg.root_mod_dir, value);
            } else if (str_equal_icase(key, STR_LIT("overlay_toggle_keybind"))) {
              manager->cfg.overlay_toggle = keybind_parse(value, KEYBIND_NULL);
            }
          }
        } else if (str_equal_icase(section->name, STR_LIT("mod_order"))) {
          manager->mod_order.initial_ids = str_array_copy_from_list(&manager->perm, section->lines);
        } else if (str_equal_icase(section->name, STR_LIT("disabled_mods"))) {
          manager->disabled_mods = str_array_copy_from_list(&manager->perm, section->lines);
        } else if (str_equal_icase(section->name, STR_LIT("console"))) {
          console_ok = ui_console_load_cfg(&globals.ui_manager.console, section->lines);
        }
      }
      result = (manager->cfg.root_mod_dir.len > 0) && console_ok;
    }
  }

  if (result) {
    manager->saved_cfg = manager->cfg;
    ui_console_commit_cfg(&globals.ui_manager.console);
  }

  scratch_end(tmp);
  return result;
}

bool
mod_manager_save_cfg(mod_manager_t *manager)
{
  bool        result = false;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    if (!str_is_empty(manager->config_path)) {
      str_t root_mod_dir            = mod_cfg_string_as_str(&manager->cfg.root_mod_dir);
      str_t overlay_toggle_bind_str = keybind_to_str(manager->cfg.overlay_toggle, tmp.arena);

      str_list_t lines = {0};
      str_list_push(tmp.arena, &lines, STR_LIT("[loader]"));
      str_list_push_fmt(tmp.arena, &lines, "mod_dir                = %.*s", STR_ARG(root_mod_dir));
      str_list_push_fmt(tmp.arena, &lines, "overlay_toggle_keybind = %.*s", STR_ARG(overlay_toggle_bind_str));
      str_list_push(tmp.arena, &lines, STR_LIT(""));

      str_list_t mod_order_id_list = mod_manager_get_mod_order_id_list(manager, tmp.arena);
      if (mod_order_id_list.count > 0) {
        str_list_push(tmp.arena, &lines, STR_LIT("[mod_order]"));
        str_list_concat_inplace(&lines, &mod_order_id_list);
        str_list_push(tmp.arena, &lines, STR_LIT(""));
      }

      str_list_t disabled_mods = mod_manager_get_disabled_mods_list(manager, tmp.arena);
      if (disabled_mods.count > 0) {
        str_list_push(tmp.arena, &lines, STR_LIT("[disabled_mods]"));
        str_list_concat_inplace(&lines, &disabled_mods);
        str_list_push(tmp.arena, &lines, STR_LIT(""));
      }

      str_list_push(tmp.arena, &lines, STR_LIT("[console]"));
      ui_console_save_cfg(&globals.ui_manager.console, tmp.arena, &lines);
      str_list_push(tmp.arena, &lines, STR_LIT(""));

      result = file_write_lines(lines, manager->config_path);
    }

    if (result) {
      manager->saved_cfg = manager->cfg;
      ui_console_commit_cfg(&globals.ui_manager.console);
    }
  }
  scratch_end(tmp);
  return result;
}

bool
mod_get_info(mod_manager_t *manager, mod_handle_t h, mod_info_t *info)
{
  if (!manager || !info) {
    return false;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (info) {
    *info = m->manifest.info;
  }
  return true;
}

bool
mod_get_dll_info(mod_manager_t *manager, mod_handle_t h, mod_dll_info_t *info)
{
  if (!manager || !info) {
    return false;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_code) {
    return false;
  }

  if (info) {
    *info = m->manifest.dll;
  }
  return true;
}

bool
mod_get_asset_info(mod_manager_t *manager, mod_handle_t h, mod_asset_info_t *info)
{
  if (!manager || !info) {
    return false;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_assets) {
    return false;
  }

  if (info) {
    *info = m->manifest.asset;
  }
  return true;
}

int
mod_get_blueprints_info(mod_manager_t *manager, mod_handle_t h, mod_blueprint_info_t *out, int cap)
{
  if (!manager) {
    return 0;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return 0;
  }

  if (!m->has_blueprints) {
    return 0;
  }

  if (!out || cap == 0) {
    return m->manifest.blueprint_count;
  }

  int len = MIN_VAL(m->manifest.blueprint_count, cap);
  if (len > 0) {
    mem_copy(out, m->manifest.blueprints, len * sizeof(m->manifest.blueprints[0]));
  }
  return len;
}

int
mod_get_options_info(mod_manager_t *manager, mod_handle_t h, mod_option_info_t *out, int cap)
{
  if (!manager) {
    return 0;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return 0;
  }

  if (!m->has_options) {
    return 0;
  }

  if (!out || cap == 0) {
    return m->manifest.option_count;
  }

  int len = MIN_VAL(m->manifest.option_count, cap);
  if (len > 0) {
    mem_copy(out, m->manifest.options, len * sizeof(m->manifest.options[0]));
  }
  return len;
}

str_t
mod_manager_get_mod_dir(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return STR_NULL;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return STR_NULL;
  }

  return m->manifest.mod_dir;
}

str_t
mod_manager_get_config_path(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return STR_NULL;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return STR_NULL;
  }

  return m->manifest.config_path;
}

str_t
mod_manager_get_manifest_path(mod_manager_t *manager, mod_handle_t h)
{
  if (!manager) {
    return STR_NULL;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return STR_NULL;
  }

  return m->manifest.manifest_path;
}

mod_cfg_handle_t
mod_cfg_get_by_id(mod_manager_t *manager, mod_handle_t h, str_t id)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return MOD_CFG_HANDLE_INVALID;
  }

  for (int i = 0; i < m->manifest.option_count; ++i) {
    mod_option_info_t    *opt_info = &m->manifest.options[i];
    mod_option_runtime_t *opt      = &m->options[i];
    if (str_equal(opt_info->id, id, 0)) {
      return mod_cfg_handle_make(opt);
    }
  }

  return MOD_CFG_HANDLE_INVALID;
}

bool
mod_manager_cfg_get_bool(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_BOOL) {
    return false;
  }
  return opt->cfg.boolean;
}

int
mod_manager_cfg_get_int(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_INT) {
    return 0;
  }
  return opt->cfg.integer;
}

float
mod_manager_cfg_get_float(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_FLOAT) {
    return 0.0f;
  }
  return opt->cfg.floating;
}

int
mod_manager_cfg_get_enum(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_ENUM) {
    return 0;
  }
  return opt->cfg.enum_item_id;
}

uint64_t
mod_manager_cfg_get_string_len(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_STRING) {
    return 0;
  }

  return opt->cfg.string.len;
}

uint64_t
mod_manager_cfg_get_string_data(mod_manager_t *manager, mod_cfg_handle_t h, void *buf, uint64_t cap)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_STRING || !buf || cap == 0) {
    return 0;
  }

  uint64_t len = MIN_VAL(opt->cfg.string.len, cap);
  mem_copy(buf, opt->cfg.string.data, len);
  return len;
}

str_t
mod_cfg_get_string(mod_manager_t *manager, mod_cfg_handle_t h, arena_t *arena)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_STRING) {
    return (str_t){0};
  }

  return str_push_copy(arena, mod_cfg_string_as_str(&opt->cfg.string));
}

keybind_t
mod_manager_cfg_get_keybind(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_KEYBIND) {
    return KEYBIND_NULL;
  }
  return opt->cfg.keybind;
}

mod_color_t
mod_manager_cfg_get_color(mod_manager_t *manager, mod_cfg_handle_t h)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_COLOR) {
    return (mod_color_t){0};
  }
  return opt->cfg.color;
}

void
mod_manager_cfg_set_bool(mod_manager_t *manager, mod_cfg_handle_t h, bool val)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_BOOL) {
    return;
  }
  opt->cfg.boolean = val;
}

void
mod_manager_cfg_set_int(mod_manager_t *manager, mod_cfg_handle_t h, int val)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_INT) {
    return;
  }

  opt->cfg.integer = CLAMP(val, info->val.integer.min_val, info->val.integer.max_val);
}

void
mod_manager_cfg_set_float(mod_manager_t *manager, mod_cfg_handle_t h, float val)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_FLOAT) {
    return;
  }
  opt->cfg.floating = CLAMP(val, info->val.floating.min_val, info->val.floating.max_val);
}

void
mod_manager_cfg_set_enum(mod_manager_t *manager, mod_cfg_handle_t h, int val)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_ENUM) {
    return;
  }

  if (info->val.enumeration.values.count == 0) {
    opt->cfg.enum_item_id = -1;
    return;
  }

  opt->cfg.enum_item_id = CLAMP(val, 0, (int)info->val.enumeration.values.count - 1);
}

void
mod_manager_cfg_set_string(mod_manager_t *manager, mod_cfg_handle_t h, str_t val)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);
  mod_t                *m    = mod_cfg_handle_resolve_mod(manager, h);

  if (!opt || !info || !m || info->type != MOD_OPTION_STRING) {
    return;
  }

  mod_cfg_string_set(&opt->cfg.string, val);
}

void
mod_manager_cfg_set_keybind(mod_manager_t *manager, mod_cfg_handle_t h, keybind_t bind)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_KEYBIND) {
    return;
  }

  opt->cfg.keybind = bind;
}

void
mod_manager_cfg_set_color(mod_manager_t *manager, mod_cfg_handle_t h, mod_color_t color)
{
  mod_option_runtime_t *opt  = mod_cfg_handle_resolve(manager, h);
  mod_option_info_t    *info = mod_option_info_get(manager, h);

  if (!opt || !info || info->type != MOD_OPTION_COLOR) {
    return;
  }
  opt->cfg.color = color;
}

static void
mod_dll_arenas_init_free_list(mod_dll_runtime_t *rt)
{
  ASSERT(rt != NULL);

  rt->arena_first_free = NULL;

  for (int i = CONFIG_MOD_MAX_ARENAS - 1; i >= 0; --i) {
    rt->arenas[i].next   = rt->arena_first_free;
    rt->arena_first_free = &rt->arenas[i];
  }
}

static void
mod_init_dll_runtime(mod_t *m, bool is_builtin)
{
  m->dll = (mod_dll_runtime_t){
    .state       = MOD_DLL_STATE_UNLOADED,
    .is_builtin  = is_builtin,
  };
  m->has_code = is_builtin || !str_is_empty(m->manifest.dll.path);
}

static void
mod_init_asset_runtime(mod_t *m)
{
  m->asset = (mod_asset_runtime_t){
    .state    = MOD_ASSET_STATE_UNMOUNTED,
    .priority = 0,
  };
  m->has_assets = (m->manifest.asset.pak_path.len > 0) || (m->manifest.asset.utoc_path.len > 0) || (m->manifest.asset.ucas_path.len > 0);
}

static void
mod_init_blueprint_runtime(arena_t *perm, mod_t *m)
{
  m->blueprint_count = m->manifest.blueprint_count;
  m->blueprints      = ARENA_PUSH_ARRAY_ZERO(perm, mod_blueprint_runtime_t, m->blueprint_count);
  if (!m->blueprints) {
    return;
  }

  for (int i = 0; i < m->blueprint_count; ++i) {
    mod_blueprint_runtime_t *bp   = &m->blueprints[i];
    mod_blueprint_info_t    *info = &m->manifest.blueprints[i];

    *bp = (mod_blueprint_runtime_t){
      .active = false,
      .actor  = NULL,
    };

    bp->cfg.spawn_keybind = info->default_spawn_keybind;
    bp->cfg.auto_spawn    = info->auto_spawn;

    bp->saved_cfg = bp->cfg;
  }

  m->has_blueprints = (m->blueprint_count > 0);
}

static void
mod_init_option_runtime(arena_t *perm, mod_t *m)
{
  m->option_count = m->manifest.option_count;
  m->options      = ARENA_PUSH_ARRAY_ZERO(perm, mod_option_runtime_t, m->option_count);
  if (!m->options) {
    return;
  }

  for (int i = 0; i < m->option_count; ++i) {
    mod_option_runtime_t *opt  = &m->options[i];
    mod_option_info_t    *info = &m->manifest.options[i];

    opt->idx     = i;
    opt->mod_idx = m->idx;

    /* copy default values */
    switch (info->type) {
    case MOD_OPTION_BOOL: {
      opt->cfg.boolean = info->val.boolean.default_val;
    } break;

    case MOD_OPTION_INT: {
      opt->cfg.integer = info->val.integer.default_val;
    } break;

    case MOD_OPTION_FLOAT: {
      opt->cfg.floating = info->val.floating.default_val;
    } break;

    case MOD_OPTION_ENUM: {
      uint64_t idx = 0;
      if (str_array_find(info->val.enumeration.values, info->val.enumeration.default_val, STR_CMP_FLAG_IGNORE_CASE, &idx)) {
        opt->cfg.enum_item_id = (int)idx;
      } else {
        opt->cfg.enum_item_id = -1;
      }
    } break;

    case MOD_OPTION_STRING: {
      mod_cfg_string_set(&opt->cfg.string, info->val.string.default_val);
    } break;

    case MOD_OPTION_KEYBIND: {
      opt->cfg.keybind = info->val.keybind.default_val;
    } break;

    case MOD_OPTION_COLOR: {
      opt->cfg.color = info->val.color.default_val;
    } break;
    }

    opt->saved_cfg = opt->cfg;
  }

  m->has_options = (m->option_count > 0);
}

mod_handle_t
mod_register(mod_manager_t *manager, mod_manifest_t *manifest)
{
  mod_t *m = mod_alloc_slot(manager, manifest->info.id);
  if (!m) {
    return MOD_HANDLE_INVALID;
  }

  mod_handle_t mod_h = mod_handle_make(m);

  mod_manifest_copy(&m->manifest, manifest, &manager->perm);
  m->kind = m->manifest.info.kind;

  mod_init_asset_runtime(m);
  mod_init_blueprint_runtime(&manager->perm, m);
  mod_init_dll_runtime(m, false);
  mod_init_option_runtime(&manager->perm, m);

  if (m->has_options || m->has_blueprints) {
    if (!mod_cfg_load(manager, mod_h)) {
      mod_cfg_reset(manager, mod_h);
      mod_cfg_save(manager, mod_h);
    }
  }

  return mod_h;
}

mod_handle_t
mod_register_builtin(mod_manager_t *manager, mod_manifest_t *manifest, mod_dll_runtime_funcs_t funcs)
{
  mod_t *m = mod_alloc_slot(manager, manifest->info.id);
  if (!m) {
    return MOD_HANDLE_INVALID;
  }

  mod_handle_t mod_h = mod_handle_make(m);

  mod_manifest_copy(&m->manifest, manifest, &manager->perm);
  m->kind = m->manifest.info.kind;

  mod_init_asset_runtime(m);
  mod_init_blueprint_runtime(&manager->perm, m);
  mod_init_dll_runtime(m, true);
  mod_init_option_runtime(&manager->perm, m);

  m->dll.funcs = funcs;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t root_mod_dir = mod_cfg_string_as_str(&manager->cfg.root_mod_dir);
    if (!path_is_abs(root_mod_dir)) {
      root_mod_dir = path_join(tmp.arena, manager->game_dir, root_mod_dir);
    }

    str_t mod_dir_name = str_push_concat(tmp.arena, STR_LIT("builtin."), manifest->info.id);
    str_t mod_dir      = path_join(tmp.arena, root_mod_dir, mod_dir_name);
    str_t config_path  = path_join(tmp.arena, mod_dir, CONFIG_MOD_CONFIG_FILE_NAME);

    dir_create_recursive(mod_dir);

    m->manifest.mod_dir_name = str_push_copy(&manager->perm, mod_dir_name);
    m->manifest.mod_dir      = str_push_copy(&manager->perm, mod_dir);
    m->manifest.config_path  = str_push_copy(&manager->perm, config_path);

    if (m->has_options || m->has_blueprints) {
      if (!mod_cfg_load(manager, mod_h)) {
        mod_cfg_reset(manager, mod_h);
        mod_cfg_save(manager, mod_h);
      }
    }
  }
  scratch_end(tmp);

  return mod_h;
}

static void
mod_manager_discover(mod_manager_t *manager)
{
  tmp_arena_t tmp          = scratch_begin(NULL);
  str_t       root_mod_dir = mod_cfg_string_as_str(&manager->cfg.root_mod_dir);

  if (!path_is_abs(root_mod_dir)) {
    root_mod_dir = path_join(tmp.arena, manager->game_dir, root_mod_dir);
  }

  dir_entry_list_t entries = dir_list(root_mod_dir, tmp.arena);
  for (int i = 0; i < entries.count; ++i) {
    dir_entry_t *ent = &entries.items[i];
    if (ent->kind != DIR_ENTRY_DIR) {
      continue;
    }

    mod_manifest_t tmp_manifest = {0};
    if (!mod_manifest_parse(tmp.arena, ent->path, &tmp_manifest)) {
      continue;
    }

    if (!mod_manager_check_manifest(manager, &tmp_manifest)) {
      continue;
    }

    mod_register(manager, &tmp_manifest);
  }

  scratch_end(tmp);
}

static int
mod_dll_hook_find(mod_dll_runtime_t *r, void *target)
{
  for (int i = 0; i < CONFIG_MOD_MAX_HOOKS; ++i) {
    mod_dll_runtime_hook_t *hook = &r->hooks[i];
    if (hook->occupied && hook->chain.target == target) {
      return i;
    }
  }
  return -1;
}

bool
mod_dll_hook_create(mod_manager_t *manager, mod_handle_t h, void *target, void *detour, void **original)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!target || !detour || !original) {
    return false;
  }

  ASSERT(m->has_code);
  mod_dll_runtime_t *rt = &m->dll;

  if (mod_dll_hook_find(rt, target) >= 0) {
    /* already exists */
    return false;
  }

  if (rt->hook_count >= CONFIG_MOD_MAX_HOOKS) {
    /* max count reached */
    return false;
  }

  int free_idx = -1;
  for (int i = 0; i < CONFIG_MOD_MAX_HOOKS; ++i) {
    if (!rt->hooks[i].occupied) {
      free_idx = i;
      break;
    }
  }
  ASSERT(free_idx >= 0);

  mod_dll_runtime_hook_t *new_slot = &rt->hooks[free_idx];
  if (!hook_chain_create(&new_slot->chain, target, detour, original)) {
    return false;
  }

  new_slot->occupied = true;
  rt->hook_count += 1;
  return true;
}

bool
mod_dll_hook_enable(mod_manager_t *manager, mod_handle_t h, void *target)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!target) {
    return false;
  }

  ASSERT(m->has_code);
  mod_dll_runtime_t *rt = &m->dll;

  int idx = mod_dll_hook_find(rt, target);
  if (idx < 0) {
    /* not found */
    return false;
  }

  return hook_chain_enable(&rt->hooks[idx].chain);
}

bool
mod_dll_hook_disable(mod_manager_t *manager, mod_handle_t h, void *target)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!target) {
    return false;
  }

  ASSERT(m->has_code);
  mod_dll_runtime_t *rt = &m->dll;

  int idx = mod_dll_hook_find(rt, target);
  if (idx < 0) {
    /* not found */
    return false;
  }

  return hook_chain_disable(&rt->hooks[idx].chain);
}

bool
mod_dll_hook_remove(mod_manager_t *manager, mod_handle_t h, void *target)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!target) {
    return false;
  }

  ASSERT(m->has_code);
  mod_dll_runtime_t *rt = &m->dll;

  if (rt->hook_count == 0) {
    return false;
  }

  int idx = mod_dll_hook_find(rt, target);
  if (idx < 0) {
    /* not found */
    return false;
  }

  mod_dll_runtime_hook_t *hook = &rt->hooks[idx];
  if (!hook_chain_remove(&hook->chain)) {
    return false;
  }

  hook->occupied = false;
  rt->hook_count -= 1;
  return true;
}

bool
mod_dll_uobject_listener_register(mod_manager_t *manager, mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_code || !m->dll.listeners) {
    return false;
  }

  if (kind != UOBJECT_LISTENER_KIND_CREATE && kind != UOBJECT_LISTENER_KIND_DELETE) {
    return false;
  }

  if (!notify_cb) {
    return false;
  }

  int free_idx = -1;
  for (int i = 0; i < CONFIG_MOD_MAX_LISTENERS; ++i) {
    uobject_listener_t *current = &m->dll.listeners[i];
    if (!current->occupied) {
      if (free_idx < 0) {
        free_idx = i;
      }
      continue;
    }

    if (current->kind == kind && current->on_notify == notify_cb && current->user == user) {
      return false; // duplicate
    }
  }

  if (free_idx < 0) {
    return false; // no available slots
  }

  uobject_listener_t *slot = &m->dll.listeners[free_idx];
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
  return true;
}

void
mod_dll_uobject_listener_deregister(mod_manager_t *manager, mod_handle_t h, uobject_listener_kind_t kind, uobject_on_notify_cb_t notify_cb, void *user)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return;
  }

  if (!m->has_code || !m->dll.listeners) {
    return;
  }

  for (int i = 0; i < CONFIG_MOD_MAX_LISTENERS; ++i) {
    uobject_listener_t *current = &m->dll.listeners[i];
    if (!current->occupied) {
      continue;
    }

    if (current->kind == kind && current->on_notify == notify_cb && current->user == user) {
      unreal_uobject_listener_del(current);
      return;
    }
  }
}

arena_t *
mod_dll_arena_alloc(mod_manager_t *manager, mod_handle_t h, uint64_t reserve_size, uint64_t commit_size)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m || !m->has_code) {
    return NULL;
  }

  mod_dll_runtime_t *rt = &m->dll;

  if (rt->state != MOD_DLL_STATE_LOADED) {
    return NULL;
  }

  mod_dll_arena_t *slot = rt->arena_first_free;
  if (!slot) {
    return NULL;
  }

  arena_t arena = arena_new_dynamic(reserve_size, commit_size);
  if (!arena.backing) {
    return NULL;
  }

  rt->arena_first_free = slot->next;

  slot->arena    = arena;
  slot->occupied = true;
  slot->next     = NULL;

  return &slot->arena;
}

bool
mod_dll_arena_free(mod_manager_t *manager, mod_handle_t h, arena_t *arena)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m || !m->has_code || !arena) {
    return false;
  }

  for (int i = 0; i < CONFIG_MOD_MAX_ARENAS; ++i) {
    mod_dll_arena_t *slot = &m->dll.arenas[i];

    if (slot->occupied && &slot->arena == arena) {
      arena_destroy(&slot->arena);

      slot->arena    = (arena_t){0};
      slot->occupied = false;

      slot->next              = m->dll.arena_first_free;
      m->dll.arena_first_free = slot;

      return true;
    }
  }

  return false;
}

static void
mod_cleanup_dll_runtime_hooks(mod_t *m)
{
  ASSERT(m != NULL);

  if (!m->dll.hooks) {
    return;
  }

  for (int i = 0; i < CONFIG_MOD_MAX_HOOKS; ++i) {
    mod_dll_runtime_hook_t *hook = &m->dll.hooks[i];
    if (!hook->occupied) {
      continue;
    }

    hook_chain_remove(&hook->chain);
    hook->occupied = false;
  }
  m->dll.hook_count = 0;
}

static void
mod_cleanup_dll_runtime_listeners(mod_t *m)
{
  ASSERT(m != NULL);

  if (!m->dll.listeners) {
    return;
  }

  for (int i = 0; i < CONFIG_MOD_MAX_LISTENERS; ++i) {
    uobject_listener_t *listener = &m->dll.listeners[i];
    if (listener->occupied) {
      unreal_uobject_listener_del(listener);
    }
  }
}

static void
mod_cleanup_dll_runtime_arenas(mod_t *m)
{
  ASSERT(m != NULL);

  if (!m->dll.arenas) {
    return;
  }

  for (int i = 0; i < CONFIG_MOD_MAX_ARENAS; ++i) {
    mod_dll_arena_t *slot = &m->dll.arenas[i];

    if (slot->occupied) {
      arena_destroy(&slot->arena);
      slot->arena    = (arena_t){0};
      slot->occupied = false;
    }
  }
}

static void
mod_cleanup_dll_runtime(mod_t *m)
{
  if (!m || !m->has_code) {
    return;
  }

  mod_cleanup_dll_runtime_hooks(m);
  mod_cleanup_dll_runtime_listeners(m);
  mod_cleanup_dll_runtime_arenas(m);

  arena_pop_to(&m->dll.perm, 0); // decommit everything
}

static HMODULE
mod_win32_load_library(str_t path)
{
  if (str_is_empty(path)) {
    return NULL;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  HMODULE h_mod = LoadLibraryW((const wchar_t *)path16.data);

  scratch_end(tmp);
  return h_mod;
}

static FARPROC
mod_win32_get_proc(HMODULE dll, const char *name)
{
  if (!dll || !name) {
    return NULL;
  }
  return GetProcAddress(dll, name);
}

static void
mod_win32_free_library(HMODULE dll)
{
  FreeLibrary(dll);
}

static void
err_msg_set(err_msg_t *err, str_t msg)
{
  err->len = (int)MIN_VAL(msg.len, sizeof(err->msg));
  mem_copy(err->msg, msg.data, err->len);
}

static void
err_msg_append(err_msg_t *err, str_t msg)
{
  if (!err || str_is_empty(msg) || err->len >= (int)sizeof(err->msg)) {
    return;
  }

  uint64_t available = sizeof(err->msg) - (uint64_t)err->len;
  uint64_t written   = MIN_VAL(msg.len, available);
  mem_copy(err->msg + err->len, msg.data, written);
  err->len += (int)written;
}

static bool
mod_pe_range_is_valid(uint64_t file_size, uint64_t offset, uint64_t size)
{
  return offset <= file_size && size <= file_size - offset;
}

typedef struct mod_pe_file_s mod_pe_file_t;
struct mod_pe_file_s {
  uint8_t              *data;
  uint64_t              size;
  IMAGE_FILE_HEADER     file_header;
  IMAGE_SECTION_HEADER *sections;
  uint32_t              size_of_headers;
  IMAGE_DATA_DIRECTORY  import_directory;
  bool                  is_pe64;
};

static bool
mod_pe_file_parse(str_t file, mod_pe_file_t *pe)
{
  if (!pe || !file.data || !mod_pe_range_is_valid(file.len, 0, sizeof(IMAGE_DOS_HEADER))) {
    return false;
  }

  IMAGE_DOS_HEADER dos = {0};
  mem_copy(&dos, file.data, sizeof(dos));
  if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) {
    return false;
  }

  uint64_t nt_offset = (uint64_t)dos.e_lfanew;
  if (!mod_pe_range_is_valid(file.len, nt_offset, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER))) {
    return false;
  }

  DWORD signature = 0;
  mem_copy(&signature, file.data + nt_offset, sizeof(signature));
  if (signature != IMAGE_NT_SIGNATURE) {
    return false;
  }

  uint64_t file_header_offset = nt_offset + sizeof(signature);
  mem_copy(&pe->file_header, file.data + file_header_offset, sizeof(pe->file_header));

  uint64_t optional_offset = file_header_offset + sizeof(IMAGE_FILE_HEADER);
  if (!mod_pe_range_is_valid(file.len, optional_offset, pe->file_header.SizeOfOptionalHeader)) {
    return false;
  }

  WORD optional_magic = 0;
  if (pe->file_header.SizeOfOptionalHeader < sizeof(optional_magic)) {
    return false;
  }
  mem_copy(&optional_magic, file.data + optional_offset, sizeof(optional_magic));

  if (optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    IMAGE_OPTIONAL_HEADER64 optional = {0};
    if (pe->file_header.SizeOfOptionalHeader < sizeof(optional)) {
      return false;
    }

    mem_copy(&optional, file.data + optional_offset, sizeof(optional));
    pe->size_of_headers  = optional.SizeOfHeaders;
    pe->import_directory = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    pe->is_pe64          = true;
  } else if (optional_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    IMAGE_OPTIONAL_HEADER32 optional = {0};
    if (pe->file_header.SizeOfOptionalHeader < sizeof(optional)) {
      return false;
    }

    mem_copy(&optional, file.data + optional_offset, sizeof(optional));
    pe->size_of_headers  = optional.SizeOfHeaders;
    pe->import_directory = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    pe->is_pe64          = false;
  } else {
    return false;
  }

  uint64_t sections_offset = optional_offset + pe->file_header.SizeOfOptionalHeader;
  uint64_t sections_size   = (uint64_t)pe->file_header.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
  if (!mod_pe_range_is_valid(file.len, sections_offset, sections_size)) {
    return false;
  }

  pe->data     = file.data;
  pe->size     = file.len;
  pe->sections = (IMAGE_SECTION_HEADER *)(file.data + sections_offset);
  return true;
}

static uint8_t *
mod_pe_rva_to_ptr(mod_pe_file_t *pe, uint32_t rva, uint64_t required_size, uint64_t *available_out)
{
  if (!pe) {
    return NULL;
  }

  if (rva < pe->size_of_headers) {
    uint64_t available = pe->size - MIN_VAL((uint64_t)rva, pe->size);
    if (required_size <= available) {
      if (available_out) {
        *available_out = available;
      }
      return pe->data + rva;
    }
    return NULL;
  }

  for (uint16_t i = 0; i < pe->file_header.NumberOfSections; ++i) {
    IMAGE_SECTION_HEADER section = {0};
    mem_copy(&section, &pe->sections[i], sizeof(section));

    uint64_t section_start = section.VirtualAddress;
    uint64_t section_span  = MAX_VAL(section.Misc.VirtualSize, section.SizeOfRawData);
    uint64_t section_end   = section_start + section_span;
    if ((uint64_t)rva < section_start || (uint64_t)rva >= section_end) {
      continue;
    }

    uint64_t delta = (uint64_t)rva - section_start;
    if (delta > section.SizeOfRawData) {
      return NULL;
    }

    uint64_t available = section.SizeOfRawData - delta;
    uint64_t offset    = (uint64_t)section.PointerToRawData + delta;
    if (required_size > available || !mod_pe_range_is_valid(pe->size, offset, available)) {
      return NULL;
    }

    if (available_out) {
      *available_out = available;
    }
    return pe->data + offset;
  }

  return NULL;
}

static str_t
mod_pe_read_cstr(mod_pe_file_t *pe, uint32_t rva)
{
  uint64_t available = 0;
  uint8_t *data      = mod_pe_rva_to_ptr(pe, rva, 1, &available);
  if (!data) {
    return STR_NULL;
  }

  uint64_t len = calc_cstr_len_with_cap((const char *)data, available);
  if (len == available) {
    return STR_NULL;
  }
  return str_make(data, len);
}

static bool
mod_pe_check_import(HMODULE overdub, str_t name, err_msg_t *err, bool *header_written)
{
  bool        found = false;
  tmp_arena_t tmp   = scratch_begin(NULL);
  {
    char *cstr = str_push_cstr(tmp.arena, name);

    found = cstr && GetProcAddress(overdub, cstr) != NULL;
  }
  scratch_end(tmp);

  if (!found) {
    if (!*header_written) {
      err_msg_set(err, STR_LIT("Missing Overdub API functions:"));
      *header_written = true;
    }
    err_msg_append(err, STR_LIT("\n    "));
    err_msg_append(err, name);
  }
  return found;
}

static bool
mod_pe_check_ordinal_import(HMODULE overdub, WORD ordinal, err_msg_t *err, bool *header_written)
{
  if (GetProcAddress(overdub, MAKEINTRESOURCEA(ordinal))) {
    return true;
  }

  if (!*header_written) {
    err_msg_set(err, STR_LIT("Missing Overdub API functions:"));
    *header_written = true;
  }

  char ordinal_buf[16] = {0};
  str_write_fmt(ordinal_buf, sizeof(ordinal_buf), "\n    #%u", ordinal);
  err_msg_append(err, str_from_cstr(ordinal_buf));
  return false;
}

static bool
mod_pe_preflight_overdub_imports(str_t path, err_msg_t *err)
{
  bool        ok      = false;
  bool        missing = false;
  tmp_arena_t tmp     = scratch_begin(NULL);
  {
    str_t file = file_read_all(path, tmp.arena);
    if (!str_is_empty(file)) {
      mod_pe_file_t pe = {0};
      if (mod_pe_file_parse(file, &pe)) {
        if (pe.import_directory.VirtualAddress != 0 && pe.import_directory.Size != 0) {
          HMODULE overdub = NULL;
          DWORD   flags   = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
          if (GetModuleHandleExW(flags, (LPCWSTR)(uintptr_t)&mod_pe_preflight_overdub_imports, &overdub)) {
            uint64_t                 descriptor_bytes = 0;
            IMAGE_IMPORT_DESCRIPTOR *descriptors      = (IMAGE_IMPORT_DESCRIPTOR *)mod_pe_rva_to_ptr(&pe, pe.import_directory.VirtualAddress, sizeof(IMAGE_IMPORT_DESCRIPTOR), &descriptor_bytes);
            if (descriptors) {
              uint64_t descriptor_count = MIN_VAL(descriptor_bytes, pe.import_directory.Size) / sizeof(IMAGE_IMPORT_DESCRIPTOR);
              bool     header_written   = false;

              ok = true;
              for (uint64_t descriptor_idx = 0; descriptor_idx < descriptor_count && ok; ++descriptor_idx) {
                IMAGE_IMPORT_DESCRIPTOR descriptor = {0};
                mem_copy(&descriptor, &descriptors[descriptor_idx], sizeof(descriptor));

                if (descriptor.Name == 0 && descriptor.FirstThunk == 0 && descriptor.OriginalFirstThunk == 0) {
                  break;
                }

                str_t dll_name = mod_pe_read_cstr(&pe, descriptor.Name);
                if (str_is_empty(dll_name)) {
                  err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid imported DLL name"));
                  ok = false;
                  break;
                }

                if (!str_equal(dll_name, STR_LIT("overdub.dll"), STR_CMP_FLAG_IGNORE_CASE)) {
                  continue;
                }

                uint32_t thunk_rva  = descriptor.OriginalFirstThunk ? descriptor.OriginalFirstThunk : descriptor.FirstThunk;
                uint32_t thunk_size = pe.is_pe64                    ? sizeof(IMAGE_THUNK_DATA64)    : sizeof(IMAGE_THUNK_DATA32);

                for (uint64_t thunk_idx = 0;; ++thunk_idx) {
                  uint64_t thunk_offset = thunk_idx * thunk_size;
                  if (thunk_offset > UINT32_MAX || thunk_rva > UINT32_MAX - (uint32_t)thunk_offset) {
                    err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import thunk"));
                    ok = false;
                    break;
                  }

                  uint8_t *thunk_ptr = mod_pe_rva_to_ptr(&pe, thunk_rva + (uint32_t)thunk_offset, thunk_size, NULL);
                  if (!thunk_ptr) {
                    err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import thunk"));
                    ok = false;
                    break;
                  }

                  uint64_t thunk_value = 0;
                  if (pe.is_pe64) {
                    IMAGE_THUNK_DATA64 thunk = {0};
                    mem_copy(&thunk, thunk_ptr, sizeof(thunk));
                    thunk_value = thunk.u1.AddressOfData;
                  } else {
                    IMAGE_THUNK_DATA32 thunk = {0};
                    mem_copy(&thunk, thunk_ptr, sizeof(thunk));
                    thunk_value = thunk.u1.AddressOfData;
                  }

                  if (thunk_value == 0) {
                    break;
                  }

                  bool is_ordinal = pe.is_pe64 ? IMAGE_SNAP_BY_ORDINAL64(thunk_value) : IMAGE_SNAP_BY_ORDINAL32((DWORD)thunk_value);
                  if (is_ordinal) {
                    WORD ordinal = pe.is_pe64 ? IMAGE_ORDINAL64(thunk_value) : IMAGE_ORDINAL32((DWORD)thunk_value);
                    missing     |= !mod_pe_check_ordinal_import(overdub, ordinal, err, &header_written);
                    continue;
                  }

                  if (thunk_value > UINT32_MAX) {
                    err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import name RVA"));
                    ok = false;
                    break;
                  }

                  uint32_t name_rva = (uint32_t)thunk_value;
                  if (name_rva > UINT32_MAX - sizeof(WORD)) {
                    err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import name RVA"));
                    ok = false;
                    break;
                  }

                  if (!mod_pe_rva_to_ptr(&pe, name_rva, sizeof(WORD) + 1, NULL)) {
                    err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import name"));
                    ok = false;
                    break;
                  }

                  str_t import_name = mod_pe_read_cstr(&pe, name_rva + sizeof(WORD));
                  if (str_is_empty(import_name)) {
                    err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import name"));
                    ok = false;
                    break;
                  }

                  missing |= !mod_pe_check_import(overdub, import_name, err, &header_written);
                }
              }
            } else {
              err_msg_set(err, STR_LIT("failed to inspect DLL imports: invalid import directory"));
            }
          } else {
            err_msg_set(err, STR_LIT("failed to inspect DLL imports: Overdub module not found"));
          }
        }
      } else {
        err_msg_set(err, STR_LIT("failed to inspect DLL imports: malformed PE image"));
      }
    } else {
      err_msg_set(err, STR_LIT("failed to inspect DLL imports: could not read file"));
    }
  }
  scratch_end(tmp);
  return ok && !missing;
}

bool
mod_dll_load(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_code) {
    m->dll.state = MOD_DLL_STATE_UNLOADED;
    return true;
  }

  if (m->dll.state == MOD_DLL_STATE_LOADED) {
    return true;
  }

  m->dll.err_stage = MOD_DLL_ERROR_NONE;
  err_msg_set(&m->dll.err_msg, STR_NULL);

  /* built-in package: callbacks are already wired */
  if (m->dll.is_builtin) {
    m->dll.state = MOD_DLL_STATE_LOADED;
    m->dll.perm  = arena_new_dynamic(CONFIG_MOD_CODE_ARENA_SIZE, 64 * KB);
    return true;
  }

  if (str_is_empty(m->manifest.dll.path)) {
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("dll path is empty"));
    return false;
  }

  if (!file_exists(m->manifest.dll.path)) {
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("dll file does not exist"));
    return false;
  }

  if (!mod_pe_preflight_overdub_imports(m->manifest.dll.path, &m->dll.err_msg)) {
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    return false;
  }

  HMODULE dll = mod_win32_load_library(m->manifest.dll.path);
  if (!dll) {
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("LoadLibraryW failed"));
    return false;
  }

  FARPROC abi_version_proc = mod_win32_get_proc(dll, MOD_ABI_VERSION_EXPORT);
  if (!abi_version_proc) {
    mod_win32_free_library(dll);
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("missing mod_abi_version export"));
    return false;
  }

  mod_abi_version_fn_t mod_abi_version = (mod_abi_version_fn_t)(uintptr_t)abi_version_proc;
  mod_version_t        abi_version     = mod_abi_version();
  if (!mod_abi_compatible(MOD_ABI_VERSION, abi_version)) {
    mod_win32_free_library(dll);
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("incompatible ABI version"));
    return false;
  }

  FARPROC entry_proc = mod_win32_get_proc(dll, MOD_ENTRY_EXPORT);
  if (!entry_proc) {
    mod_win32_free_library(dll);
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("missing mod_entry export"));
    return false;
  }

  mod_entry_fn_t   mod_entry = (mod_entry_fn_t)(uintptr_t)entry_proc;
  const mod_api_t *mod_api   = mod_entry();
  if (!mod_api) {
    mod_win32_free_library(dll);
    m->dll.err_stage = MOD_DLL_ERROR_LOAD;
    err_msg_set(&m->dll.err_msg, STR_LIT("mod_entry returned NULL"));
    return false;
  }

  #define API_HAS_FIELD(API, NAME) \
    ((API)->struct_size >= (offsetof(mod_api_t, NAME) + sizeof((API)->NAME)))

  mod_init_fn_t             init             = API_HAS_FIELD(mod_api, init)             ? mod_api->init             : NULL;
  mod_deinit_fn_t           deinit           = API_HAS_FIELD(mod_api, deinit)           ? mod_api->deinit           : NULL;
  mod_tick_fn_t             tick             = API_HAS_FIELD(mod_api, tick)             ? mod_api->tick             : NULL;
  mod_input_fn_t            input            = API_HAS_FIELD(mod_api, input)            ? mod_api->input            : NULL;
  mod_pe_pre_fn_t           pe_pre           = API_HAS_FIELD(mod_api, pe_pre)           ? mod_api->pe_pre           : NULL;
  mod_pe_post_fn_t          pe_post          = API_HAS_FIELD(mod_api, pe_post)          ? mod_api->pe_post          : NULL;
  mod_func_invoke_pre_fn_t  func_invoke_pre  = API_HAS_FIELD(mod_api, func_invoke_pre)  ? mod_api->func_invoke_pre  : NULL;
  mod_func_invoke_post_fn_t func_invoke_post = API_HAS_FIELD(mod_api, func_invoke_post) ? mod_api->func_invoke_post : NULL;
  mod_draw_panel_fn_t       draw_panel       = API_HAS_FIELD(mod_api, draw_panel)       ? mod_api->draw_panel       : NULL;
  mod_draw_config_fn_t      draw_config      = API_HAS_FIELD(mod_api, draw_config)      ? mod_api->draw_config      : NULL;

  #undef API_HAS_FIELD

  m->dll.dll_handle = dll;
  m->dll.state      = MOD_DLL_STATE_LOADED;
  m->dll.perm       = arena_new_dynamic(CONFIG_MOD_CODE_ARENA_SIZE, 64 * KB);
  m->dll.funcs      = (mod_dll_runtime_funcs_t){
    .init             = init,
    .deinit           = deinit,
    .tick             = tick,
    .input            = input,
    .pe_pre           = pe_pre,
    .pe_post          = pe_post,
    .func_invoke_pre  = func_invoke_pre,
    .func_invoke_post = func_invoke_post,
    .draw_panel       = draw_panel,
    .draw_config      = draw_config,
  };
  return true;
}

void
mod_dll_unload(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return;
  }

  if (!m->has_code) {
    m->dll.state = MOD_DLL_STATE_UNLOADED;
    return;
  }

  if (m->dll.state == MOD_DLL_STATE_UNLOADED) {
    return;
  }

  m->dll.err_stage = MOD_DLL_ERROR_NONE;
  err_msg_set(&m->dll.err_msg, STR_NULL);

  if (m->dll.active) {
    if (!mod_dll_stop(manager, h)) {
      return;
    }
  }

  if (m->dll.is_builtin) {
    m->dll.state = MOD_DLL_STATE_UNLOADED;
    arena_destroy(&m->dll.perm);
    return;
  }

  HMODULE dll = (HMODULE)m->dll.dll_handle;
  if (dll) {
    mod_win32_free_library(dll);
  }

  m->dll.dll_handle = NULL;
  m->dll.state      = MOD_DLL_STATE_UNLOADED;
  m->dll.funcs      = (mod_dll_runtime_funcs_t){0};
  arena_destroy(&m->dll.perm);
}

bool
mod_dll_start(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_code) {
    return true;
  }

  if (m->dll.state != MOD_DLL_STATE_LOADED) {
    if (!mod_dll_load(manager, h)) {
      return false;
    }
  }

  m->dll.err_stage = MOD_DLL_ERROR_NONE;
  err_msg_set(&m->dll.err_msg, STR_NULL);

  if (m->dll.active) {
    return true;
  }

  if (!m->dll.funcs.init) {
    m->dll.err_stage = MOD_DLL_ERROR_INIT;
    err_msg_set(&m->dll.err_msg, STR_LIT("init function is missing"));
    return false;
  }

  m->dll.commands      = ARENA_PUSH_ARRAY_ZERO(&m->dll.perm, mod_cmd_t, CONFIG_MOD_MAX_COMMANDS);
  m->dll.command_count = 0;
  m->dll.hooks         = ARENA_PUSH_ARRAY_ZERO(&m->dll.perm, mod_dll_runtime_hook_t, CONFIG_MOD_MAX_HOOKS);
  m->dll.hook_count    = 0;
  m->dll.listeners     = ARENA_PUSH_ARRAY_ZERO(&m->dll.perm, uobject_listener_t, CONFIG_MOD_MAX_LISTENERS);
  m->dll.arenas        = ARENA_PUSH_ARRAY_ZERO(&m->dll.perm, mod_dll_arena_t, CONFIG_MOD_MAX_ARENAS);

  if (!m->dll.commands || !m->dll.hooks || !m->dll.listeners || !m->dll.arenas) {
    mod_cleanup_dll_runtime(m);

    m->dll.err_stage = MOD_DLL_ERROR_INIT;
    err_msg_set(&m->dll.err_msg, STR_LIT("failed to allocate memory for internal structures"));
    return false;
  }

  mod_dll_arenas_init_free_list(&m->dll);

  if (!m->dll.funcs.init(mod_handle_make(m))) {
    mod_cleanup_dll_runtime(m);

    m->dll.err_stage = MOD_DLL_ERROR_INIT;
    err_msg_set(&m->dll.err_msg, STR_LIT("init function failed"));
    return false;
  }

  m->dll.active = true;
  return true;
}

bool
mod_dll_stop(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_code) {
    return true;
  }

  m->dll.err_stage = MOD_DLL_ERROR_NONE;
  err_msg_set(&m->dll.err_msg, STR_NULL);

  if (!m->dll.active) {
    return true;
  }

  if (m->dll.funcs.deinit) {
    m->dll.funcs.deinit(mod_handle_make(m));
  }

  mod_cleanup_dll_runtime(m);

  m->dll.active = false;
  return true;
}

bool
mod_dll_restart(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!mod_dll_stop(manager, h)) {
    return false;
  }

  return mod_dll_start(manager, h);
}

bool
mod_dll_reload(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!mod_dll_stop(manager, h)) {
    return false;
  }

  mod_dll_unload(manager, h);
  if (!mod_dll_load(manager, h)) {
    return false;
  }

  return mod_dll_start(manager, h);
}

static uobject_t *
mod_blueprint_resolve_spawn_context(mod_blueprint_info_t *info)
{
  ASSERT(info != NULL);

  switch (info->attach_to) {
  case MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER:
    return unreal_uobject_find_first_of(globals.unreal.player_ctrl);
  case MOD_SPAWN_CONTEXT_LOCAL_PLAYER:
    return unreal_uobject_find_first_of(globals.unreal.local_player);
  case MOD_SPAWN_CONTEXT_PAWN:
    return unreal_uobject_find_first_of(globals.unreal.pawn);
  case MOD_SPAWN_CONTEXT_HUD:
    return unreal_uobject_find_first_of(globals.unreal.hud);
  case MOD_SPAWN_CONTEXT_WORLD:
    return unreal_uobject_find_first_of(globals.unreal.world);
  case MOD_SPAWN_CONTEXT_GAME_INSTANCE:
    return unreal_uobject_find_first_of(globals.unreal.game_instance);
  case MOD_SPAWN_CONTEXT_CUSTOM: {
    uclass_t *cls = unreal_uobject_find_class_by_full_name(info->custom_attach_class_path);
    if (cls) {
      return unreal_uobject_find_first_of(cls);
    }
    return NULL;
  }

  case MOD_SPAWN_CONTEXT_NONE:
  default:
    return NULL;
  }
}

static uclass_t *
mod_blueprint_load_actor_class(mod_blueprint_info_t *info)
{
  ASSERT(info != NULL);
  return unreal_static_load_class(globals.unreal.actor, NULL, info->mod_actor_class_path, STR_LIT(""), 0);
}

static bool
mod_blueprint_refresh_actor(mod_blueprint_runtime_t *rt)
{
  if (rt->actor && unreal_uobject_is_valid(rt->actor)) {
    rt->active = true;
    return true;
  }

  rt->actor  = NULL;
  rt->active = false;
  return false;
}

bool
mod_blueprint_spawn(mod_manager_t *manager, mod_handle_t h, int idx)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m || !m->has_blueprints) {
    return false;
  }

  if (!m->enabled) {
    return false;
  }

  if (idx < 0 || idx >= m->blueprint_count) {
    return false;
  }

  mod_blueprint_info_t    *info = &m->manifest.blueprints[idx];
  mod_blueprint_runtime_t *rt   = &m->blueprints[idx];

  rt->err_stage = MOD_BP_ERROR_NONE;
  err_msg_set(&rt->err_msg, STR_NULL);

  mod_blueprint_refresh_actor(rt);
  if (rt->active) {
    /* already active */
    return true;
  }

  uobject_t *ctx = mod_blueprint_resolve_spawn_context(info);
  if (!ctx) {
    rt->active    = false;
    rt->actor     = NULL;
    rt->err_stage = MOD_BP_ERROR_RESOLVE_CONTEXT;
    err_msg_set(&rt->err_msg, STR_LIT("failed to resolve spawn context"));
    return false;
  }

  uclass_t *actor_class = mod_blueprint_load_actor_class(info);
  if (!actor_class) {
    rt->active    = false;
    rt->actor     = NULL;
    rt->err_stage = MOD_BP_ERROR_LOAD_CLASS;
    err_msg_set(&rt->err_msg, STR_LIT("failed to load blueprint class"));
    return false;
  }

  uobject_t *actor = unreal_spawn_actor(ctx, actor_class);
  if (!actor) {
    rt->active    = false;
    rt->actor     = NULL;
    rt->err_stage = MOD_BP_ERROR_SPAWN;
    err_msg_set(&rt->err_msg, STR_LIT("failed to spawn blueprint actor"));
    return false;
  }

  rt->actor  = actor;
  rt->active = true;
  return true;
}

bool
mod_blueprint_despawn(mod_manager_t *manager, mod_handle_t h, int idx)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m || !m->has_blueprints) {
    return false;
  }

  if (idx < 0 || idx >= m->blueprint_count) {
    return false;
  }

  mod_blueprint_runtime_t *rt = &m->blueprints[idx];

  rt->err_stage = MOD_BP_ERROR_NONE;
  err_msg_set(&rt->err_msg, STR_NULL);

  mod_blueprint_refresh_actor(rt);
  if (!rt->active) {
    return true;
  }

  unreal_despawn_actor(rt->actor);

  rt->actor  = NULL;
  rt->active = false;
  return true;
}

bool
mod_blueprint_start(mod_manager_t *manager, mod_handle_t h, int idx)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_blueprints) {
    return true;
  }

  if (idx < 0 || idx >= m->blueprint_count) {
    return false;
  }

  mod_blueprint_runtime_t *rt = &m->blueprints[idx];
  if (rt->cfg.auto_spawn) {
    return mod_blueprint_spawn(manager, h, idx);
  }

  return true;
}

bool
mod_blueprint_stop(mod_manager_t *manager, mod_handle_t h, int idx)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_blueprints) {
    return true;
  }

  if (idx < 0 || idx >= m->blueprint_count) {
    return false;
  }

  return mod_blueprint_despawn(manager, h, idx);
}

bool
mod_blueprint_cfg_reset(mod_manager_t *manager, mod_handle_t h, int idx)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_blueprints) {
    return true;
  }

  if (idx < 0 || idx >= m->blueprint_count) {
    return false;
  }

  mod_blueprint_runtime_t *rt   = &m->blueprints[idx];
  mod_blueprint_info_t    *info = &m->manifest.blueprints[idx];

  rt->cfg.auto_spawn    = info->auto_spawn;
  rt->cfg.spawn_keybind = info->default_spawn_keybind;
  return true;
}

void
mod_blueprint_tick(mod_manager_t *manager, mod_handle_t h, float delta)
{
  (void)delta;

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return;
  }

  if (!m->enabled) {
    return;
  }

  if (!m->has_blueprints) {
    return;
  }

  for (int i = 0; i < m->blueprint_count; ++i) {
    mod_blueprint_runtime_t *rt = &m->blueprints[i];

    mod_blueprint_refresh_actor(rt);

    if (rt->active) {
      /* already spawned */
      continue;
    }

    if (keybind_is_valid(rt->cfg.spawn_keybind) && keybind_is_pressed(rt->cfg.spawn_keybind)) {
      mod_blueprint_spawn(manager, h, i);
    }
  }
}

void
mod_manager_dispatch_tick(mod_manager_t *manager, float delta)
{
  if (!manager) {
    return;
  }

  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (m->has_code && rt->active && rt->funcs.tick) {
      rt->funcs.tick(h, delta);
    }

    if (m->has_blueprints) {
      mod_blueprint_tick(manager, h, delta);
    }
  }
}

bool
mod_manager_dispatch_input(mod_manager_t *manager, input_event_t *ev)
{
  if (!manager || !ev) {
    return false;
  }

  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (!m->has_code || !rt->active || !rt->funcs.input) {
      continue;
    }

    if (rt->funcs.input(h, ev)) {
      return true;
    }
  }

  return false;
}

bool
mod_manager_dispatch_process_event_pre(mod_manager_t *manager, uobject_t *obj, ufunc_t *func, void *params)
{
  if (!manager) {
    return false;
  }

  bool handled = false;
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (!m->has_code || !rt->active || !rt->funcs.pe_pre) {
      continue;
    }

    if (rt->funcs.pe_pre(h, obj, func, params)) {
      handled = true;
    }
  }

  return handled;
}

void
mod_manager_dispatch_process_event_post(mod_manager_t *manager, uobject_t *obj, ufunc_t *func, void *params, bool consumed)
{
  if (!manager) {
    return;
  }

  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (!m->has_code || !rt->active || !rt->funcs.pe_post) {
      continue;
    }

    rt->funcs.pe_post(h, obj, func, params, consumed);
  }
}

bool
mod_manager_dispatch_ufunction_invoke_pre(mod_manager_t *manager, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result)
{
  if (!manager) {
    return false;
  }

  bool handled = false;
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (!m->has_code || !rt->active || !rt->funcs.func_invoke_pre) {
      continue;
    }

    if (rt->funcs.func_invoke_pre(h, func, obj, stack, result)) {
      handled = true;
    }
  }

  return handled;
}

void
mod_manager_dispatch_ufunction_invoke_post(mod_manager_t *manager, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result, bool consumed)
{
  if (!manager) {
    return;
  }

  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (!m->has_code || !rt->active || !rt->funcs.func_invoke_post) {
      continue;
    }

    rt->funcs.func_invoke_post(h, func, obj, stack, result, consumed);
  }
}

bool
mod_manager_dispatch_command(mod_manager_t *manager, str_t name, str_t args)
{
  if (!manager || str_is_empty(name)) {
    return false;
  }

  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t       h  = manager->mod_order.runtime[i];
    mod_t             *m  = mod_handle_resolve(manager, h);
    mod_dll_runtime_t *rt = &m->dll;

    if (!m->has_code || !rt->active) {
      continue;
    }

    for (int j = 0; j < rt->command_count; ++j) {
      mod_cmd_t *cmd = &rt->commands[j];

      if (str_equal(cmd->name, name, 0)) {
        ASSERT(cmd->cb != NULL);
        cmd->cb(h, name, args, cmd->user);
        return true;
      }
    }
  }

  return false;
}

bool
mod_manager_register_cmd(mod_manager_t *manager, mod_handle_t h, str_t name, str_t description, mod_cmd_fn_t cb, void *user)
{
  if (!cb || str_is_empty(name)) {
    return false;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return false;
  }

  if (!m->has_code) {
    return false;
  }

  if (m->dll.command_count >= CONFIG_MOD_MAX_COMMANDS) {
    return false;
  }

  for (int i = 0; i < m->dll.command_count; ++i) {
    if (str_equal(m->dll.commands[i].name, name, 0)) {
      /* already registered */
      return false;
    }
  }

  mod_cmd_t *new_slot = &m->dll.commands[m->dll.command_count++];

  new_slot->name        = str_push_copy(&m->dll.perm, name);
  new_slot->description = str_push_copy(&m->dll.perm, description);
  new_slot->cb          = cb;
  new_slot->user        = user;
  return true;
}

arena_t *
mod_get_perm_arena(mod_manager_t *manager, mod_handle_t h)
{
  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return NULL;
  }

  ASSERT(m->has_code);
  return &m->dll.perm;
}

void
mod_manager_mount_assets(mod_manager_t *manager)
{
  ASSERT(manager != NULL);

  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t h = manager->mod_order.runtime[i];
    mod_t       *m = mod_handle_resolve(manager, h);
    if (!m || !m->enabled || !m->has_assets) {
      continue;
    }

    mod_asset_info_t    *asset_info = &m->manifest.asset;
    mod_asset_runtime_t *asset      = &m->asset;

    if (asset->state != MOD_ASSET_STATE_UNMOUNTED) {
      continue;
    }

    asset->priority = CONFIG_MOD_ASSET_BASE_PRIORITY + i;

    tmp_arena_t tmp = scratch_begin(NULL);
    {
      str_list_t path_parts = {0};

      str_list_push(tmp.arena, &path_parts, mod_cfg_string_as_str(&manager->cfg.root_mod_dir));
      str_list_push(tmp.arena, &path_parts, m->manifest.mod_dir_name);

      if (asset_info->pak_path.len > 0) {
        str_list_push(tmp.arena, &path_parts, asset_info->pak_path);
        str_t path = str_list_join(tmp.arena, path_parts, STR_NULL, STR_LIT("/"), STR_NULL);

        if (!unreal_mount_pak(path, asset->priority)) {
          err_msg_set(&asset->last_error, STR_LIT("failed to mount .pak file"));
          asset->state = MOD_ASSET_STATE_ERROR;
        } else {
          asset->state = MOD_ASSET_STATE_MOUNTED;
        }
      } else if (asset_info->utoc_path.len > 0 && asset_info->ucas_path.len > 0) {
        str_list_push(tmp.arena, &path_parts, asset_info->utoc_path);
        str_t path = str_list_join(tmp.arena, path_parts, STR_NULL, STR_LIT("/"), STR_NULL);

        if (!unreal_mount_iostore(path, asset->priority)) {
          err_msg_set(&asset->last_error, STR_LIT("failed to mount .utoc and .ucas files"));
          asset->state = MOD_ASSET_STATE_ERROR;
        } else {
          asset->state = MOD_ASSET_STATE_MOUNTED;
        }
      }
    }
    scratch_end(tmp);
  }
}

static void
mod_manager_load_mod_cfgs(mod_manager_t *manager)
{
  for (int i = 0; i < manager->mod_count; ++i) {
    mod_t *m = &manager->mods[i];
    mod_cfg_load(manager, mod_handle_make(m));
  }
}

void
mod_manager_init(mod_manager_t *manager, str_t game_dir)
{
  mem_zero(manager, sizeof(*manager));
  manager->perm        = arena_new_dynamic(CONFIG_MOD_MANAGER_PERM_ARENA_SIZE, 64 * KB);
  manager->game_dir    = str_push_copy(&manager->perm, game_dir);
  manager->config_path = path_join(&manager->perm, game_dir, CONFIG_MOD_MANAGER_CONFIG_PATH);
  manager->mods        = ARENA_PUSH_ARRAY_ZERO(&manager->perm, mod_t, CONFIG_MOD_MANAGER_MAX_MODS);

  ASSERT(!str_is_empty(manager->config_path));
  ASSERT(manager->mods != NULL);
}

void
mod_manager_start_dlls(mod_manager_t *manager)
{
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t h = manager->mod_order.runtime[i];
    mod_t       *m = mod_handle_resolve(manager, h);
    if (!m || !m->enabled || !m->has_code) {
      continue;
    }

    mod_dll_start(manager, mod_handle_make(m));
  }
}

void
mod_manager_start_blueprints(mod_manager_t *manager)
{
  for (int i = 0; i < manager->mod_order.count; ++i) {
    mod_handle_t h = manager->mod_order.runtime[i];
    mod_t       *m = mod_handle_resolve(manager, h);
    if (!m || !m->enabled || !m->has_blueprints) {
      continue;
    }

    for (int j = 0; j < m->blueprint_count; ++j) {
      mod_blueprint_start(manager, mod_handle_make(m), j);
    }
  }
}

void
mod_manager_startup_load_cfg(mod_manager_t *manager)
{
  ASSERT(manager != NULL);
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    if (!mod_manager_load_cfg(manager)) {
      dir_create_recursive(path_join(tmp.arena, manager->game_dir, CONFIG_MOD_MANAGER_MOD_DIR_NAME));

      str_t default_disabled_mods[] = {
        STR_LIT("uobject-search"),
        STR_LIT("ufunction-tracer"),
      };

      mod_cfg_string_set(&manager->cfg.root_mod_dir, CONFIG_MOD_MANAGER_MOD_DIR_NAME);
      manager->disabled_mods         = str_array_make(default_disabled_mods, COUNTOF(default_disabled_mods));
      manager->mod_order.initial_ids = (str_array_t){0};
      manager->mod_order.count       = 0;
      manager->cfg.overlay_toggle    = keybind_parse(CONFIG_NK_OVERLAY_DEFAULT_TOGGLE_KEY, KEYBIND_NULL);
    }
  }
  scratch_end(tmp);
}

void
mod_manager_startup_load_mods(mod_manager_t *manager)
{
  ASSERT(manager != NULL);

  mod_manager_discover(manager);
  mod_manager_apply_order_from_config(manager, manager->mod_order.initial_ids);
  mod_manager_apply_disabled_mods(manager, manager->disabled_mods);
  mod_manager_save_cfg(manager); // in case config files is missing

  mod_manager_load_mod_cfgs(manager);
}

void
mod_manager_save_mod_cfg_all(mod_manager_t *manager)
{
  for (int i = 0; i < manager->mod_count; ++i) {
    mod_t *m = &manager->mods[i];
    mod_cfg_save(manager, mod_handle_make(m));
  }
}

mod_t *
mod_manager_mod_from_owner(mod_manager_t *manager, uint64_t owner)
{
  return mod_handle_resolve(manager, (mod_handle_t)owner);
}

bool
mod_has_any_errors(mod_t *m)
{
  if (m->has_assets && m->asset.state == MOD_ASSET_STATE_ERROR) {
    return true;
  }

  if (m->has_code && m->dll.err_stage != MOD_DLL_ERROR_NONE) {
    return true;
  }

  if (m->has_blueprints) {
    for (int i = 0; i < m->blueprint_count; ++i) {
      if (m->blueprints[i].err_stage != MOD_BP_ERROR_NONE) {
        return true;
      }
    }
  }

  return false;
}

bool
mod_is_active(mod_t *m)
{
  if (m->has_code && m->dll.active) {
    return true;
  }

  if (m->has_blueprints) {
    for (int i = 0; i < m->blueprint_count; ++i) {
      if (m->blueprints[i].active) {
        return true;
      }
    }
  }

  if (m->has_assets && m->asset.state == MOD_ASSET_STATE_MOUNTED) {
    return true;
  }

  return false;
}

void
mod_manager_mod_set_enabled(mod_manager_t *manager, mod_handle_t h, bool enabled)
{
  if (!manager || h == MOD_HANDLE_INVALID) {
    return;
  }

  mod_t *m = mod_handle_resolve(manager, h);
  if (!m) {
    return;
  }

  /* NOTE: assets cannot be mounted/unmounted freely at the runtime */

  if (enabled) {
    if (m->has_code) {
      mod_dll_start(manager, h);
    }

    if (m->has_blueprints) {
      for (int i = 0; i < m->blueprint_count; ++i) {
        mod_blueprint_start(manager, h, i);
      }
    }
  } else {
    if (m->has_code) {
      mod_dll_unload(manager, h);
    }

    if (m->has_blueprints) {
      for (int i = 0; i < m->blueprint_count; ++i) {
        mod_blueprint_stop(manager, h, m->blueprint_count - i - 1); // reverse order
      }
    }
  }

  m->enabled = enabled;
}

bool
mod_blueprint_cfg_is_dirty(mod_blueprint_cfg_t *cfg, mod_blueprint_cfg_t *saved_cfg)
{
  if (!cfg || !saved_cfg) {
    return false;
  }

  if (cfg->auto_spawn != saved_cfg->auto_spawn || !keybind_equal(cfg->spawn_keybind, saved_cfg->spawn_keybind)) {
    return true;
  }

  return false;
}

bool
mod_option_cfg_is_dirty(mod_option_info_t *info, mod_option_cfg_t *cfg, mod_option_cfg_t *saved_cfg)
{
  if (!cfg || !saved_cfg) {
    return false;
  }

  switch (info->type) {
  case MOD_OPTION_BOOL:
    if (cfg->boolean != saved_cfg->boolean) {
      return true;
    }
    break;

  case MOD_OPTION_INT:
    if (cfg->integer != saved_cfg->integer) {
      return true;
    }
    break;

  case MOD_OPTION_FLOAT:
    if (!float_equal(cfg->floating, saved_cfg->floating, info->val.floating.min_val, info->val.floating.step)) {
      return true;
    }
    break;

  case MOD_OPTION_ENUM:
    if (cfg->enum_item_id != saved_cfg->enum_item_id) {
      return true;
    }
    break;

  case MOD_OPTION_STRING:
    if (!str_equal(mod_cfg_string_as_str(&cfg->string), mod_cfg_string_as_str(&saved_cfg->string), 0)) {
      return true;
    }
    break;

  case MOD_OPTION_KEYBIND:
    if (!keybind_equal(cfg->keybind, saved_cfg->keybind)) {
      return true;
    }
    break;

  case MOD_OPTION_COLOR:
    if (cfg->color.r != saved_cfg->color.r || cfg->color.g != saved_cfg->color.g || cfg->color.b != saved_cfg->color.b || cfg->color.a != saved_cfg->color.a) {
      return true;
    }
    break;

  default:
    break;
  }
  return false;
}

bool
mod_manager_cfg_is_dirty(mod_manager_cfg_t *cfg, mod_manager_cfg_t *saved_cfg)
{
  if (!cfg || !saved_cfg) {
    return false;
  }

  if (!str_equal(mod_cfg_string_as_str(&cfg->root_mod_dir), mod_cfg_string_as_str(&saved_cfg->root_mod_dir), 0)) {
    return true;
  }

  if (!keybind_equal(cfg->overlay_toggle, saved_cfg->overlay_toggle)) {
    return true;
  }

  if (ui_console_cfg_is_dirty(&globals.ui_manager.console)) {
    return true;
  }

  return false;
}

/*
 * RUNTIME MODEL RULES
 *
 * 1. The discovered mod set is frozen after startup.
 *    Runtime never adds, removes, or renames mods.
 *
 * 2. mods[] is canonical storage.
 *    Runtime presentation/load order is represented separately via mod_order[].
 *    Reordering never changes mod identity or handles.
 *
 * 3. Mod identity/metadata is immutable after startup load.
 *    This includes:
 *      - mod id
 *      - mod kind
 *      - manifest metadata
 *
 * 4. Config schema is immutable after startup load.
 *    Runtime may only change config values, not option definitions/types.
 *
 * 5. Startup-only content:
 *      - manifest
 *      - assets / blueprints
 *      - config schema
 *
 * 6. Runtime-reloadable content:
 *      - DLL / script code instance
 *      - runtime config values
 *
 * 7. If a mod type cannot be cleanly unloaded/reloaded, changing its enabled
 *    state requires restart.
 *
 * 8. Handles are stable for the whole process lifetime.
 *    They are never reused and do not depend on runtime order.
 *
 * 9. Runtime config edits affect only values.
 *    Persistence to disk is an explicit save action or shutdown writeback,
 *    not an implicit schema/content reload.
 *
 * 10. Failed reload/load does not change mod identity/config schema.
 *     Only runtime state changes: loaded, unloaded, failed, needs_restart, etc.
 */
