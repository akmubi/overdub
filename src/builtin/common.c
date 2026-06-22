#include "common.h"

#include "globals.h"
#include "scratch.h"
#include "str.h"
#include "ui_nuklear.h"
#include "unreal.h"

#include <string.h> // memcmp

bool
query_matches_fname_entry(fname_entry_t *entry, query_t *query, bool ignore_case, bool exact_match)
{
  ASSERT(entry != NULL);
  ASSERT(query != NULL);

  if (entry->header.is_wide) {
    return unreal_fname_entry_match_text16(entry, str16_make(query->wide_name, query->wide_len), ignore_case, exact_match);
  } else {
    return unreal_fname_entry_match_text(entry, str_make(query->ansi_name, query->ansi_len), ignore_case, exact_match);
  }
}

bool
query_matches_fname(fname_t name, query_t *query, bool ignore_case, bool exact_match)
{
  ASSERT(query != NULL);
  if (query_is_empty(query)) {
    return false;
  }

  fname_entry_t *entry = unreal_fname_entry_get(name.cmp_idx);
  if (!entry || entry->header.len == 0) {
    return false;
  }

  return query_matches_fname_entry(entry, query, ignore_case, exact_match);
}

void
query_set(query_t *query, str_t text)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    mem_zero(query, sizeof(*query));

    if (!str_is_empty(text)) {
      str_t   text_ansi = text;
      str16_t text_wide = str16_from_str(tmp.arena, text);

      query->ansi_len = (int)MIN_VAL(text_ansi.len, COUNTOF(query->ansi_name));
      query->wide_len = (int)MIN_VAL(text_wide.len, COUNTOF(query->wide_name));

      mem_copy(query->ansi_name, text_ansi.data, query->ansi_len * sizeof(query->ansi_name[0]));
      mem_copy(query->wide_name, text_wide.data, query->wide_len * sizeof(query->wide_name[0]));
    }
  }
  scratch_end(tmp);
}

uobject_kind_t
uobject_kind(uobject_t *obj)
{
  if (!obj) {
    return UOBJECT_KIND_UNKNOWN;
  }

  if (unreal_uobject_is_default(obj)) {
    return UOBJECT_KIND_CDO;
  }

  uclass_t *kind_to_uclass_map[UOBJECT_KIND_MAX] = {
    [UOBJECT_KIND_PACKAGE]           = globals.unreal.core_package,
    [UOBJECT_KIND_CLASS]             = globals.unreal.core_class,
    [UOBJECT_KIND_STRUCT]            = globals.unreal.core_scriptstruct,
    [UOBJECT_KIND_ENUM]              = globals.unreal.core_enum,
    [UOBJECT_KIND_FUNC]              = globals.unreal.core_func,
    [UOBJECT_KIND_WORLD]             = globals.unreal.world,
    [UOBJECT_KIND_LEVEL]             = globals.unreal.level,
    [UOBJECT_KIND_ACTOR]             = globals.unreal.actor,
    [UOBJECT_KIND_ACTOR_COMPONENT]   = globals.unreal.actor_component,
    [UOBJECT_KIND_GAME_INSTANCE]     = globals.unreal.game_instance,
    [UOBJECT_KIND_LOCAL_PLAYER]      = globals.unreal.local_player,
    [UOBJECT_KIND_PLAYER_CONTROLLER] = globals.unreal.player_ctrl,
    [UOBJECT_KIND_PAWN]              = globals.unreal.pawn,
    [UOBJECT_KIND_HUD]               = globals.unreal.hud,
    [UOBJECT_KIND_WIDGET]            = globals.unreal.widget,
    [UOBJECT_KIND_BLUEPRINT]         = globals.unreal.blueprint,
    [UOBJECT_KIND_DATA_ASSET]        = globals.unreal.data_asset,
    [UOBJECT_KIND_DATA_TABLE]        = globals.unreal.data_table,
  };

  for (uclass_t *cls = obj->cls; cls; cls = (uclass_t *)cls->base.super_struct) {
    for (uobject_kind_t kind = UOBJECT_KIND_PACKAGE; kind < UOBJECT_KIND_MAX; ++kind) {
      if (kind_to_uclass_map[kind] == cls) {
        return kind;
      }
    }
  }

  return UOBJECT_KIND_UNKNOWN;
}

struct nk_color
uobject_kind_color(uobject_kind_t kind)
{
  struct nk_color color_map[UOBJECT_KIND_MAX] = {
    [UOBJECT_KIND_UNKNOWN]           = nk_rgba(144, 127, 110, 255),
    [UOBJECT_KIND_CDO]               = nk_rgba(218, 105,  72, 255),
    [UOBJECT_KIND_PACKAGE]           = nk_rgba(148, 121,  90, 255),
    [UOBJECT_KIND_CLASS]             = nk_rgba(218, 156,  62, 255),
    [UOBJECT_KIND_STRUCT]            = nk_rgba( 95, 178, 145, 255),
    [UOBJECT_KIND_ENUM]              = nk_rgba(174, 126, 207, 255),
    [UOBJECT_KIND_FUNC]              = nk_rgba( 91, 166, 214, 255),
    [UOBJECT_KIND_WORLD]             = nk_rgba( 87, 169, 103, 255),
    [UOBJECT_KIND_LEVEL]             = nk_rgba(128, 171,  83, 255),
    [UOBJECT_KIND_ACTOR]             = nk_rgba( 79, 136, 213, 255),
    [UOBJECT_KIND_ACTOR_COMPONENT]   = nk_rgba(116, 128, 220, 255),
    [UOBJECT_KIND_GAME_INSTANCE]     = nk_rgba( 82, 188, 168, 255),
    [UOBJECT_KIND_LOCAL_PLAYER]      = nk_rgba( 94, 183, 220, 255),
    [UOBJECT_KIND_PLAYER_CONTROLLER] = nk_rgba(226, 135,  62, 255),
    [UOBJECT_KIND_PAWN]              = nk_rgba(112, 199, 124, 255),
    [UOBJECT_KIND_HUD]               = nk_rgba(224, 114, 156, 255),
    [UOBJECT_KIND_WIDGET]            = nk_rgba(202, 101, 218, 255),
    [UOBJECT_KIND_BLUEPRINT]         = nk_rgba( 66, 148, 232, 255),
    [UOBJECT_KIND_DATA_ASSET]        = nk_rgba(142, 190,  92, 255),
    [UOBJECT_KIND_DATA_TABLE]        = nk_rgba(211, 180,  72, 255),
  };

  if (kind >= UOBJECT_KIND_MAX) {
    return color_map[UOBJECT_KIND_UNKNOWN];
  }
  return color_map[kind];
}

str_t
uobject_kind_to_str(uobject_kind_t kind)
{
  str_t name_map[UOBJECT_KIND_MAX] = {
    [UOBJECT_KIND_UNKNOWN]           = STR_CLIT("Other"),
    [UOBJECT_KIND_CDO]               = STR_CLIT("CDO"),
    [UOBJECT_KIND_PACKAGE]           = STR_CLIT("Package"),
    [UOBJECT_KIND_CLASS]             = STR_CLIT("Class"),
    [UOBJECT_KIND_STRUCT]            = STR_CLIT("Struct"),
    [UOBJECT_KIND_ENUM]              = STR_CLIT("Enum"),
    [UOBJECT_KIND_FUNC]              = STR_CLIT("Function"),
    [UOBJECT_KIND_WORLD]             = STR_CLIT("World"),
    [UOBJECT_KIND_LEVEL]             = STR_CLIT("Level"),
    [UOBJECT_KIND_ACTOR]             = STR_CLIT("Actor"),
    [UOBJECT_KIND_ACTOR_COMPONENT]   = STR_CLIT("Actor Component"),
    [UOBJECT_KIND_GAME_INSTANCE]     = STR_CLIT("Game Instance"),
    [UOBJECT_KIND_LOCAL_PLAYER]      = STR_CLIT("Local Player"),
    [UOBJECT_KIND_PLAYER_CONTROLLER] = STR_CLIT("Player Controller"),
    [UOBJECT_KIND_PAWN]              = STR_CLIT("Pawn"),
    [UOBJECT_KIND_HUD]               = STR_CLIT("HUD"),
    [UOBJECT_KIND_WIDGET]            = STR_CLIT("Widget"),
    [UOBJECT_KIND_BLUEPRINT]         = STR_CLIT("Blueprint"),
    [UOBJECT_KIND_DATA_ASSET]        = STR_CLIT("Data Asset"),
    [UOBJECT_KIND_DATA_TABLE]        = STR_CLIT("Data Table"),
  };

  if (kind >= UOBJECT_KIND_MAX) {
    return name_map[UOBJECT_KIND_UNKNOWN];
  }
  return name_map[kind];
}

bool
uobject_kind_checkbox(struct nk_context *ctx, uint32_t *enabled_kind_mask, uobject_kind_t kind)
{
  ASSERT(ctx != NULL);
  ASSERT(enabled_kind_mask != NULL);

  uint32_t        bit   = FLAG(kind);
  struct nk_color color = uobject_kind_color(kind);
  str_t           name  = uobject_kind_to_str(kind);

  nk_bool before = AS_BOOL(*enabled_kind_mask & bit);
  nk_bool after  = before;

  {
    nk_style_push_style_item(ctx, &ctx->style.checkbox.cursor_normal, UI_ITEM(color));
    nk_style_push_style_item(ctx, &ctx->style.checkbox.cursor_hover, UI_ITEM(color));
    nk_style_push_color(ctx, &ctx->style.checkbox.text_normal, color);
    nk_style_push_color(ctx, &ctx->style.checkbox.text_hover, color);
    nk_style_push_color(ctx, &ctx->style.checkbox.text_active, color);

    if (nk_checkbox_text(ctx, (const char *)name.data, (int)name.len, &after)) {
      if (after) {
        *enabled_kind_mask |= bit;
      } else {
        *enabled_kind_mask &= ~bit;
      }
    }

    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_color(ctx);
    nk_style_pop_style_item(ctx);
    nk_style_pop_style_item(ctx);
  }

  return before != after;
}
