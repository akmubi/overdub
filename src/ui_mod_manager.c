#include "ui_mod_manager.h"
#include "arena.h"
#include "globals.h"
#include "mod_manager.h"
#include "scratch.h"
#include "str.h"
#include "types.h"
#include "ui_keybind_capture.h"
#include "ui_nuklear.h"
#include "vendor_stb.h"

#include <math.h>

typedef enum {
  UI_ITEM_NONE = 0,
  UI_ITEM_ACTIVE,
  UI_ITEM_ERROR,
} ui_item_state_t;

static inline ui_item_state_t
ui_mod_state(mod_t *m)
{
  if (mod_has_any_errors(m)) {
    return UI_ITEM_ERROR;
  }

  if (mod_is_active(m)) {
    return UI_ITEM_ACTIVE;
  }

  return UI_ITEM_NONE;
}

static void
draw_key_value(struct nk_context *ctx, ui_text_span_t key, str_t value, struct nk_color value_color, bool show_tooltip)
{
  float width = nk_layout_widget_bounds(ctx).w;

  nk_layout_row_begin(ctx, NK_STATIC, 20.0f, 2);
  {
    nk_layout_row_push(ctx, key.width);
    ui_label_str(ctx, key.str, NK_TEXT_LEFT);

    nk_layout_row_push(ctx, width - key.width);
    {
      if (show_tooltip && nk_widget_is_hovered(ctx)) {
        nk_tooltip_text(ctx, (const char *)value.data, (int)value.len);
      }
      ui_label_str_colored(ctx, value, NK_TEXT_LEFT, value_color);
    }
  }
  nk_layout_row_end(ctx);
}

static void
form_key_value_begin(struct nk_context *ctx, ui_text_span_t key, str_t key_desc, struct nk_color key_color)
{
  float width = nk_layout_widget_bounds(ctx).w;
  float gap_x = ctx->style.window.spacing.x;
  nk_layout_row_begin(ctx, NK_STATIC, 20.0f, 2);
  {
    nk_layout_row_push(ctx, key.width + gap_x);
    {
      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip_text(ctx, (const char *)key_desc.data, (int)key_desc.len);
      }
      ui_label_str_colored(ctx, key.str, NK_TEXT_LEFT, key_color);
    }

    nk_layout_row_push(ctx, width - key.width);
  }
}

static void
form_key_value_end(struct nk_context *ctx)
{
  nk_layout_row_end(ctx);
}

static bool
form_bool(struct nk_context *ctx, ui_text_span_t label, str_t desc, bool *old, bool *v)
{
  ASSERT(ctx != NULL);
  ASSERT(v != NULL);

  bool before = *v;
  bool dirty  = (old) ? (*old != *v) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    int tmp = *v ? 1 : 0;
    if (nk_checkbox_label(ctx, "", &tmp)) {
      *v = (tmp != 0);
    }
  }
  form_key_value_end(ctx);

  return (*v != before);
}

static bool
form_int(struct nk_context *ctx, ui_text_span_t label, str_t desc, int *old, int *v, int min_v, int max_v, int step)
{
  ASSERT(ctx != NULL);
  ASSERT(v != NULL);

  int  before = *v;
  bool dirty  = (old) ? (*old != *v) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    nk_property_int(ctx, "#", min_v, v, max_v, step, 1);
  }
  form_key_value_end(ctx);

  return (*v != before);
}

static bool
form_float(struct nk_context *ctx, ui_text_span_t label, str_t desc, float *old, float *v, float min_v, float max_v, float step)
{
  ASSERT(ctx != NULL);
  ASSERT(v != NULL);

  float before = *v;
  bool  dirty  = (old) ? !float_equal(*old, *v, min_v, step) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    *v = nk_propertyf(ctx, "#", min_v, *v, max_v, step, step);

    if (step > 0.0f) {
      float q = floorf(((*v - min_v) / step) + 0.5f);
      *v      = min_v + q * step;
      *v      = NK_CLAMP(*v, min_v, max_v);
    }
  }
  form_key_value_end(ctx);

  return !float_equal(before, *v, min_v, step);
}

static bool
form_enum(struct nk_context *ctx, ui_text_span_t label, str_t desc, int *old_idx, int *idx, str_array_t enum_vals)
{
  ASSERT(ctx != NULL);
  ASSERT(idx != NULL);

  int  before = *idx;
  bool dirty  = (old_idx) ? (*old_idx != *idx) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    tmp_arena_t tmp = scratch_begin(NULL);
    {
      int          count  = (int)enum_vals.count;
      const char **values = ARENA_PUSH_ARRAY_ZERO(tmp.arena, const char *, count);
      ASSERT(values);
      for (int i = 0; i < count; ++i) {
        values[i] = str_push_cstr(tmp.arena, enum_vals.items[i]);
        ASSERT(values[i]);
      }

      *idx = nk_combo(ctx, values, count, *idx, 18, nk_vec2(nk_widget_width(ctx), 160.0f));
    }
    scratch_end(tmp);
  }
  form_key_value_end(ctx);

  return (*idx != before);
}

static bool
form_string(struct nk_context *ctx, ui_text_span_t label, str_t desc, mod_cfg_string_t *old, mod_cfg_string_t *v)
{
  ASSERT(ctx != NULL);
  ASSERT(v != NULL);

  mod_cfg_string_t before = *v;
  bool             dirty  = (old) ? (!str_equal(mod_cfg_string_as_str(old), mod_cfg_string_as_str(v), 0)) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    int len = (int)v->len;
    nk_edit_string(ctx, NK_EDIT_FIELD, v->data, &len, sizeof(v->data), nk_filter_default);
    v->len = len;
  }
  form_key_value_end(ctx);

  bool changed = !str_equal(mod_cfg_string_as_str(&before), mod_cfg_string_as_str(v), 0);
  return changed;
}

static float
u8_to_f32(uint8_t v)
{
  return (float)v / 255.0f;
}

static uint8_t
f32_to_u8(float v)
{
  v = NK_CLAMP(v, 0.0f, 1.0f);
  return (uint8_t)(v * 255.0f + 0.5f);
}

static struct nk_colorf
mod_color_to_nk_colorf(mod_color_t c)
{
  return (struct nk_colorf){
    .r = u8_to_f32(c.r),
    .g = u8_to_f32(c.g),
    .b = u8_to_f32(c.b),
    .a = u8_to_f32(c.a),
  };
}

static struct nk_color
mod_color_to_nk_color(mod_color_t c)
{
  return (struct nk_color){
    .r = (nk_byte)(c.r),
    .g = (nk_byte)(c.g),
    .b = (nk_byte)(c.b),
    .a = (nk_byte)(c.a),
  };
}

static mod_color_t
nk_colorf_to_mod_color(struct nk_colorf c)
{
  return (mod_color_t){
    .r = f32_to_u8(c.r),
    .g = f32_to_u8(c.g),
    .b = f32_to_u8(c.b),
    .a = f32_to_u8(c.a),
  };
}

static bool
mod_color_equal(mod_color_t a, mod_color_t b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool
form_color(struct nk_context *ctx, ui_text_span_t label, str_t desc, mod_color_t *old, mod_color_t *v)
{
  ASSERT(ctx != NULL);
  ASSERT(v != NULL);

  mod_color_t before = *v;
  bool        dirty  = (old) ? (!mod_color_equal(*old, *v)) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    if (nk_combo_begin_color(ctx, mod_color_to_nk_color(*v), nk_vec2(nk_widget_width(ctx), 320.0f))) {
      mod_color_t color = *v;

      nk_layout_row_dynamic(ctx, 120.0f, 1);
      {
        struct nk_colorf picked = mod_color_to_nk_colorf(color);
        picked                  = nk_color_picker(ctx, picked, NK_RGBA);
        color                   = nk_colorf_to_mod_color(picked);
      }

      nk_layout_row_dynamic(ctx, 24.0f, 1);
      {
        int r = color.r;
        int g = color.g;
        int b = color.b;
        int a = color.a;

        nk_property_int(ctx, "#R", 0, &r, 255, 1, 1);
        nk_property_int(ctx, "#G", 0, &g, 255, 1, 1);
        nk_property_int(ctx, "#B", 0, &b, 255, 1, 1);
        nk_property_int(ctx, "#A", 0, &a, 255, 1, 1);

        color.r = (uint8_t)r;
        color.g = (uint8_t)g;
        color.b = (uint8_t)b;
        color.a = (uint8_t)a;
      }

      *v = color;
      nk_combo_end(ctx);
    }
  }
  form_key_value_end(ctx);

  return !mod_color_equal(before, *v);
}

static bool
form_keybind(struct nk_context *ctx, ui_keybind_capture_t *capture, ui_text_span_t label, str_t desc, str_t name, keybind_t *old, keybind_t *v)
{
  ASSERT(ctx != NULL);
  ASSERT(v != NULL);

  keybind_t before = *v;
  keybind_t after  = before;
  bool      dirty  = (old) ? (!keybind_equal(*old, *v)) : false;

  form_key_value_begin(ctx, label, desc, (dirty) ? UI_C_GOLD : UI_C_TEXT);
  {
    if (ui_keybind_capture(capture, ctx, name, v)) {
      after = *v;
    }
  }
  form_key_value_end(ctx);

  return !keybind_equal(before, after);
}

static bool
form_blueprint_cfg(struct nk_context *ctx, ui_keybind_capture_t *capture, str_t name, mod_blueprint_cfg_t *cfg, mod_blueprint_cfg_t *saved_cfg)
{
  static ui_text_span_t label_auto_spawn    = {0};
  static ui_text_span_t label_spawn_keybind = {0};
  static bool           cached              = false;

  if (!cached) {
    label_auto_spawn    = ui_text_span_make(ctx, STR_LIT("Auto spawn:"));
    label_spawn_keybind = ui_text_span_make(ctx, STR_LIT("Spawn keybind:"));

    ui_text_cols_t cols = {0};
    ui_text_cols_reset(&cols, 1);

    ui_text_cols_include(&cols, 0, label_auto_spawn);
    ui_text_cols_include(&cols, 0, label_spawn_keybind);

    label_auto_spawn.width    = cols.width[0];
    label_spawn_keybind.width = cols.width[0];

    cached = true;
  }

  form_bool(ctx, label_auto_spawn, STR_LIT("Automatically spawn this blueprint actor"), &saved_cfg->auto_spawn, &cfg->auto_spawn);
  form_keybind(ctx, capture, label_spawn_keybind, STR_LIT("Keybind used for manual spawn"), name, &saved_cfg->spawn_keybind, &cfg->spawn_keybind);

  return mod_blueprint_cfg_is_dirty(cfg, saved_cfg);
}

static bool
form_option_cfg(struct nk_context *ctx, ui_keybind_capture_t *capture, ui_text_span_t label, mod_option_info_t *info, mod_option_cfg_t *cfg, mod_option_cfg_t *saved_cfg)
{
  switch (info->type) {
  case MOD_OPTION_BOOL: {
    form_bool(ctx, label, info->description, &saved_cfg->boolean, &cfg->boolean);
    break;
  }

  case MOD_OPTION_INT: {
    form_int(ctx, label, info->description, &saved_cfg->integer, &cfg->integer, info->val.integer.min_val, info->val.integer.max_val, info->val.integer.step);
    break;
  }

  case MOD_OPTION_FLOAT: {
    form_float(ctx, label, info->description, &saved_cfg->floating, &cfg->floating, info->val.floating.min_val, info->val.floating.max_val, info->val.floating.step);
    break;
  }

  case MOD_OPTION_ENUM: {
    form_enum(ctx, label, info->description, &saved_cfg->enum_item_id, &cfg->enum_item_id, info->val.enumeration.values);
    break;
  }

  case MOD_OPTION_STRING: {
    form_string(ctx, label, info->description, &saved_cfg->string, &cfg->string);
    break;
  }

  case MOD_OPTION_COLOR: {
    form_color(ctx, label, info->description, &saved_cfg->color, &cfg->color);
    break;
  }

  case MOD_OPTION_KEYBIND: {
    form_keybind(ctx, capture, label, info->description, info->id, &saved_cfg->keybind, &cfg->keybind);
    break;
  }
  }
  return mod_option_cfg_is_dirty(info, cfg, saved_cfg);
}

static bool
form_manager_cfg(struct nk_context *ctx, ui_keybind_capture_t *capture, mod_manager_t *manager)
{
  static ui_text_span_t label_mod_dir   = {0};
  static ui_text_span_t label_ui_toggle = {0};
  static bool           cached          = false;

  if (!cached) {
    label_mod_dir   = ui_text_span_make(ctx, STR_LIT("Mod directory: "));
    label_ui_toggle = ui_text_span_make(ctx, STR_LIT("Mod manager shortcut: "));

    /* align labels */

    ui_text_cols_t cols = {0};
    ui_text_cols_reset(&cols, 1);

    ui_text_cols_include(&cols, 0, label_mod_dir);
    ui_text_cols_include(&cols, 0, label_ui_toggle);

    label_mod_dir.width   = cols.width[0];
    label_ui_toggle.width = cols.width[0];

    cached = true;
  }

  mod_manager_cfg_t *cfg       = &manager->cfg;
  mod_manager_cfg_t *saved_cfg = &manager->saved_cfg;

  form_string(ctx, label_mod_dir, STR_LIT("Directory containing installed mods"), &saved_cfg->root_mod_dir, &cfg->root_mod_dir);
  form_keybind(ctx, capture, label_ui_toggle, STR_LIT("Shortcut that opens or closes the mod manager"), STR_LIT("mod_manager.toggle_keybind"), &saved_cfg->overlay_toggle, &cfg->overlay_toggle);

  return mod_manager_cfg_is_dirty(cfg, saved_cfg);
}

static bool
form_console_cfg(struct nk_context *ctx, ui_keybind_capture_t *capture, ui_console_t *console)
{
  static ui_text_span_t label_toggle_bind = {0};
  static ui_text_span_t label_auto_scroll = {0};
  static ui_text_span_t label_position    = {0};
  static ui_text_span_t label_min_level   = {0};
  static bool           cached            = false;

  if (!cached) {
    label_toggle_bind = ui_text_span_make(ctx, STR_LIT("Console shortcut: "));
    label_auto_scroll = ui_text_span_make(ctx, STR_LIT("Auto-scroll: "));
    label_position    = ui_text_span_make(ctx, STR_LIT("Position: "));
    label_min_level   = ui_text_span_make(ctx, STR_LIT("Minimum log level: "));

    /* align labels */

    ui_text_cols_t cols = {0};
    ui_text_cols_reset(&cols, 1);

    ui_text_cols_include(&cols, 0, label_toggle_bind);
    ui_text_cols_include(&cols, 0, label_auto_scroll);
    ui_text_cols_include(&cols, 0, label_position);
    ui_text_cols_include(&cols, 0, label_min_level);

    label_toggle_bind.width = cols.width[0];
    label_auto_scroll.width = cols.width[0];
    label_position.width    = cols.width[0];
    label_min_level.width   = cols.width[0];

    cached = true;
  }

  ui_console_cfg_t *cfg       = &console->cfg;
  ui_console_cfg_t *saved_cfg = &console->saved_cfg;

  form_keybind(ctx, capture, label_toggle_bind, STR_LIT("Shortcut that opens or closes the console"), STR_LIT("console.toggle_keybind"), &saved_cfg->toggle_bind, &cfg->toggle_bind);
  form_bool(ctx, label_auto_scroll, STR_LIT("Scroll to the newest message when console output changes"), &saved_cfg->auto_scroll, &cfg->auto_scroll);
  form_enum(ctx, label_position,  STR_LIT("Place the console at the top or bottom of the screen"), &saved_cfg->position, &cfg->position, ui_console_position_str_array());
  form_enum(ctx, label_min_level, STR_LIT("Hide messages below the selected severity"), &saved_cfg->min_level, &cfg->min_level, ui_console_log_level_str_array());

  return ui_console_cfg_is_dirty(console);
}

static void
draw_section_title(struct nk_context *ctx, const char *fmt, ...)
{
  nk_layout_row_dynamic(ctx, 28.0f, 1);
  va_list va;
  va_start(va, fmt);
  nk_labelfv(ctx, NK_TEXT_LEFT, fmt, va);
  va_end(va);
}

static void
draw_spacer(struct nk_context *ctx, float h)
{
  nk_layout_row_dynamic(ctx, h, 1);
  nk_spacing(ctx, 1);
}

static str_t
err_msg_as_str(err_msg_t err)
{
  if (err.len <= 0 || err.msg[0] == '\0') {
    return STR_NULL;
  }

  return str_from_cstr_with_cap(err.msg, (uint64_t)err.len);
}

static str_t
spawn_context_to_str(mod_spawn_context_kind_t kind)
{
  switch (kind) {
  case MOD_SPAWN_CONTEXT_NONE:
    return STR_LIT("None");
  case MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER:
    return STR_LIT("Engine.PlayerController");
  case MOD_SPAWN_CONTEXT_LOCAL_PLAYER:
    return STR_LIT("Engine.LocalPlayer");
  case MOD_SPAWN_CONTEXT_PAWN:
    return STR_LIT("Engine.Pawn");
  case MOD_SPAWN_CONTEXT_HUD:
    return STR_LIT("Engine.HUD");
  case MOD_SPAWN_CONTEXT_WORLD:
    return STR_LIT("Engine.World");
  case MOD_SPAWN_CONTEXT_GAME_INSTANCE:
    return STR_LIT("Engine.GameInstance");
  case MOD_SPAWN_CONTEXT_CUSTOM:
    return STR_LIT("Custom");
  }
  return STR_LIT("Unknown");
}

static str_t
mod_status_text(mod_t *m, arena_t *arena)
{
  if (!m) {
    return STR_LIT("Unknown");
  }

  if (!mod_has_any_errors(m)) {
    return mod_is_active(m) ? STR_LIT("Active") : STR_LIT("Inactive");
  }

  tmp_arena_t tmp = scratch_begin(arena);
  str_t       out = STR_NULL;
  {
    str_list_t parts = {0};

    if (m->has_code && m->dll.err_stage != MOD_DLL_ERROR_NONE) {
      str_list_push(tmp.arena, &parts, STR_LIT("Code"));
    }

    if (m->has_blueprints) {
      for (int i = 0; i < m->blueprint_count; ++i) {
        if (m->blueprints[i].err_stage != MOD_BP_ERROR_NONE) {
          str_list_push(tmp.arena, &parts, STR_LIT("Blueprints"));
          break;
        }
      }
    }

    if (m->has_assets && m->asset.state == MOD_ASSET_STATE_ERROR) {
      str_list_push(tmp.arena, &parts, STR_LIT("Asset"));
    }

    if (parts.count <= 0) {
      out = STR_LIT("Error");
    } else {
      out = str_list_join(arena, parts, STR_LIT("Error ("), STR_LIT(", "), STR_LIT(")"));
    }
  }
  scratch_end(tmp);

  return out;
}

typedef struct ui_badge_s ui_badge_t;
struct ui_badge_s {
  str_t           text;
  struct nk_color bg;
  struct nk_color fg;
};

static inline float
ui_badge_width(struct nk_context *ctx, str_t text)
{
  return ui_text_width(ctx, text) + 24.0f;
}

static void
ui_draw_badge(struct nk_context *ctx, ui_badge_t badge)
{
  struct nk_rect               rect = {0};
  enum nk_widget_layout_states ws   = nk_widget(&rect, ctx);
  if (ws == NK_WIDGET_INVALID) {
    return;
  }

  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  float badge_rounding = 5.0f;
  float pad_x          = 10.0f;

  struct nk_rect text_rect  = rect;
  text_rect.x              += pad_x;
  text_rect.w              -= pad_x * 2.0f;

  nk_fill_rect(canvas, rect, badge_rounding, badge.bg);

  ui_text_opts_t text_opts = {
    .text_alignment = NK_TEXT_CENTERED,
    .bg             = badge.bg,
    .fg             = badge.fg,
  };
  ui_text_draw(ctx, canvas, text_rect, badge.text, text_opts);
}

static void
ui_draw_badges_flow(struct nk_context *ctx, ui_badge_t *badges, int count, float row_h)
{
  if (!ctx || !badges || count <= 0) {
    return;
  }

  float avail_w = nk_window_get_content_region(ctx).w;
  float gap_x   = ctx->style.window.spacing.x;
  int   start   = 0;

  while (start < count) {
    int   end  = start;
    float used = 0.0f;

    while (end < count) {
      float w    = ui_badge_width(ctx, badges[end].text);
      float next = w + ((end > start) ? gap_x : 0.0f);
      if (end > start && used + next > avail_w) {
        break;
      }
      used += next;
      end  += 1;
    }

    if (end <= start) {
      end = start + 1;
    }

    nk_layout_row_begin(ctx, NK_STATIC, row_h, end - start);
    {
      for (int i = start; i < end; ++i) {
        float w = ui_badge_width(ctx, badges[i].text);
        nk_layout_row_push(ctx, NK_MIN(w, avail_w));
        ui_draw_badge(ctx, badges[i]);
      }
    }
    nk_layout_row_end(ctx);
    start = end;
  }
}

static void
draw_mod_header(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager, mod_t *m)
{
  static ui_text_span_t label_state = {0};
  static bool           cached      = false;

  if (!cached) {
    label_state = ui_text_span_make(ctx, STR_LIT("State:"));
    cached      = true;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    struct nk_color badge_bg     = nk_rgba(0, 0, 0, 96);
    struct nk_color badge_fg     = nk_rgba(228, 232, 238, 255);
    struct nk_color id_badge_bg  = nk_rgba(0, 0, 0, 120);
    struct nk_color status_fg    = ctx->style.text.color;
    str_t           kind_text    = STR_NULL;
    str_t           author_text  = STR_NULL;
    str_t           version_text = STR_NULL;
    str_t           status_text  = STR_NULL;

    if (mod_has_any_errors(m)) {
      status_fg = nk_rgb(220, 80, 80);
    } else if (mod_is_active(m)) {
      status_fg = nk_rgb(80, 200, 120);
    }

    if (m->dll.is_builtin) {
      kind_text = str_push_fmt(tmp.arena, "%.*s (builtin)", STR_ARG(mod_kind_to_str(m->kind)));
    } else {
      kind_text = mod_kind_to_str(m->kind);
    }

    author_text  = str_push_fmt(tmp.arena, "%.*s", STR_ARG(m->manifest.info.author));
    version_text = str_push_fmt(tmp.arena, "%d.%d.%d", VERSION_ARG(m->manifest.info.version));
    status_text  = mod_status_text(m, tmp.arena);

    if (ui->font_title) {
      nk_style_push_font(ctx, &ui->font_title->handle);
    }
    ui_label_wrap(ctx, m->manifest.info.name);
    if (ui->font_title) {
      nk_style_pop_font(ctx);
    }

    draw_spacer(ctx, 2.0f);

    ui_badge_t badges[] = {
      {kind_text, badge_bg, badge_fg},
      {author_text, badge_bg, badge_fg},
      {version_text, badge_bg, badge_fg},
      {m->manifest.info.id, id_badge_bg, badge_fg},
    };
    ui_draw_badges_flow(ctx, badges, COUNTOF(badges), 18.0f);

    draw_spacer(ctx, 2.0f);
    draw_key_value(ctx, label_state, status_text, status_fg, false);
    draw_spacer(ctx, 2.0f);

    str_list_t lines = str_split_lines(tmp.arena, m->manifest.info.description);
    for (str_node_t *node = lines.first; node; node = node->next) {
      ui_label_wrap(ctx, node->str);
    }
    draw_spacer(ctx, 2.0f);

    nk_layout_row_dynamic(ctx, 20.0f, 1);
    {
      const char *button_action_str = (m->enabled) ? "Disable" : "Enable";
      str_t       button_label      = str_push_fmt(tmp.arena, "%s %.*s", button_action_str, STR_ARG(mod_kind_to_str(m->kind)));

      if (m->has_assets && nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Restart required. Asset archives cannot be mounted or unmounted safely while the game is running");
      }

      struct nk_color button_normal      = (m->enabled) ? UI_C_CONTROL : UI_C_ACCENT;
      struct nk_color button_hover       = (m->enabled) ? UI_C_CONTROL_HOVER : UI_C_ACCENT_HOVER;
      struct nk_color button_active      = (m->enabled) ? UI_C_CONTROL_ACTIVE : UI_C_ACCENT_ACTIVE;
      struct nk_color button_border      = (m->enabled) ? UI_C_BORDER_SOFT : UI_C_BORDER_FOCUS;
      struct nk_color button_text_normal = (m->enabled) ? UI_C_RED : UI_C_TEXT;
      struct nk_color button_text_hover  = (m->enabled) ? UI_C_RED : UI_C_TEXT;
      struct nk_color button_text_active = (m->enabled) ? UI_C_RED : UI_C_TEXT;

      nk_style_push_style_item(ctx, &ctx->style.button.normal, UI_ITEM(button_normal));
      nk_style_push_style_item(ctx, &ctx->style.button.hover, UI_ITEM(button_hover));
      nk_style_push_style_item(ctx, &ctx->style.button.active, UI_ITEM(button_active));
      nk_style_push_color(ctx, &ctx->style.button.border_color, button_border);
      nk_style_push_color(ctx, &ctx->style.button.text_normal, button_text_normal);
      nk_style_push_color(ctx, &ctx->style.button.text_hover, button_text_hover);
      nk_style_push_color(ctx, &ctx->style.button.text_active, button_text_active);

      if (ui_button_str(ctx, button_label)) {
        mod_manager_mod_set_enabled(manager, mod_handle_make(m), !m->enabled);
        mod_manager_save_cfg(manager);
      }

      nk_style_pop_color(ctx);
      nk_style_pop_color(ctx);
      nk_style_pop_color(ctx);
      nk_style_pop_color(ctx);
      nk_style_pop_style_item(ctx);
      nk_style_pop_style_item(ctx);
      nk_style_pop_style_item(ctx);
    }

    draw_spacer(ctx, 2.0f);
  }
  scratch_end(tmp);
}

static void
draw_config_section(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager, mod_t *m)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Config", &ui->inspector.options_open)) {
    if (m->has_code && m->dll.funcs.draw_config) {
      m->dll.funcs.draw_config(mod_handle_make(m), ctx);
    } else {
      bool dirty = false;

      if (m->has_options) {
        float max_width = 0.0f;
        for (int i = 0; i < m->option_count; ++i) {
          mod_option_info_t *info = &m->manifest.options[i];

          float width = ui_text_width(ctx, info->label);
          max_width   = MAX_VAL(max_width, width);
        }

        for (int i = 0; i < m->option_count; ++i) {
          mod_option_info_t    *info = &m->manifest.options[i];
          mod_option_runtime_t *rt   = &m->options[i];

          ui_text_span_t label = {
            .str   = info->label,
            .width = max_width,
          };

          dirty |= form_option_cfg(ctx, ui->keybind_capture, label, info, &rt->cfg, &rt->saved_cfg);
        }
      }

      if (m->has_blueprints) {
        if (nk_tree_state_push(ctx, NK_TREE_NODE, "Blueprints", &ui->inspector.options_blueprints_open)) {
          for (int i = 0; i < m->blueprint_count; ++i) {
            mod_blueprint_info_t    *info = &m->manifest.blueprints[i];
            mod_blueprint_runtime_t *rt   = &m->blueprints[i];

            draw_section_title(ctx, "%d. %.*s", i + 1, STR_ARG(info->id));
            dirty |= form_blueprint_cfg(ctx, ui->keybind_capture, info->id, &rt->cfg, &rt->saved_cfg);

            draw_spacer(ctx, 4.0f);
          }
          nk_tree_pop(ctx);
        }
      }

      nk_layout_row_dynamic(ctx, 28.0f, 2);
      {
        if (!dirty) {
          nk_widget_disable_begin(ctx);
        }

        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip(ctx, "Save changes");
        }

        if (ui_button_str(ctx, STR_LIT("Save"))) {
          mod_cfg_save(manager, mod_handle_make(m));
        }

        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip(ctx, "Discard unsaved changes");
        }

        if (ui_button_str(ctx, STR_LIT("Discard"))) {
          mod_cfg_revert(manager, mod_handle_make(m));
        }

        if (!dirty) {
          nk_widget_disable_end(ctx);
        }
      }

      nk_layout_row_dynamic(ctx, 28.0f, 1);
      {
        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip(ctx, "Restore the mod's default settings");
        }

        if (ui_button_str(ctx, STR_LIT("Restore defaults"))) {
          mod_cfg_reset(manager, mod_handle_make(m));
        }
      }

      draw_spacer(ctx, 4.0f);
    }
    nk_tree_pop(ctx);
  }
  scratch_end(tmp);
}

static void
draw_code_section(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager, mod_t *m)
{
  static ui_text_span_t label_path   = {0};
  static ui_text_span_t label_state  = {0};
  static ui_text_span_t label_active = {0};
  static ui_text_span_t label_error  = {0};
  static bool           cached       = false;

  if (!cached) {
    label_path   = ui_text_span_make(ctx, STR_LIT("Path:"));
    label_state  = ui_text_span_make(ctx, STR_LIT("State:"));
    label_active = ui_text_span_make(ctx, STR_LIT("Running:"));
    label_error  = ui_text_span_make(ctx, STR_LIT("Error:"));

    /* align labels */

    ui_text_cols_t runtime_cols = {0};
    ui_text_cols_reset(&runtime_cols, 1);

    ui_text_cols_include(&runtime_cols, 0, label_state);
    ui_text_cols_include(&runtime_cols, 0, label_active);
    ui_text_cols_include(&runtime_cols, 0, label_error);

    label_state.width  = runtime_cols.width[0];
    label_active.width = runtime_cols.width[0];
    label_error.width  = runtime_cols.width[0];

    cached = true;
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Code", &ui->inspector.code_open)) {
    if (!m->dll.is_builtin) {
      if (nk_tree_state_push(ctx, NK_TREE_NODE, "DLL info", &ui->inspector.code_dll_info_open)) {
        draw_key_value(ctx, label_path, m->manifest.dll.path, UI_C_TEXT, false);
        nk_tree_pop(ctx);
      }
    }

    if (nk_tree_state_push(ctx, NK_TREE_NODE, "Runtime", &ui->inspector.code_runtime_open)) {
      draw_key_value(ctx, label_state, mod_dll_state_to_str(m->dll.state), UI_C_TEXT, false);
      draw_key_value(ctx, label_active, STR_BOOL(m->dll.active), (m->dll.active) ? UI_C_GREEN : UI_C_RED, false);

      if (m->dll.err_stage != MOD_DLL_ERROR_NONE) {
        str_t err_msg = err_msg_as_str(m->dll.err_msg);
        if (!str_is_empty(err_msg)) {
          draw_key_value(ctx, label_error, err_msg, UI_C_RED, false);
        }
      }

      nk_layout_row_dynamic(ctx, 28.0f, 3);
      if (m->dll.active) {
        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip(ctx, "Stop (deinit)");
        }

        if (ui_button_str(ctx, STR_LIT("Stop"))) {
          mod_dll_stop(manager, mod_handle_make(m));
        }
      } else {
        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip(ctx, "Start (init)");
        }

        if (ui_button_str(ctx, STR_LIT("Start"))) {
          mod_dll_start(manager, mod_handle_make(m));
        }
      }

      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Stop - Start");
      }

      if (ui_button_str(ctx, STR_LIT("Restart"))) {
        mod_dll_restart(manager, mod_handle_make(m));
      }

      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Stop - Unload - Load - Start");
      }

      if (ui_button_str(ctx, STR_LIT("Reload"))) {
        mod_dll_reload(manager, mod_handle_make(m));
      }

      nk_tree_pop(ctx);
    }

    nk_tree_pop(ctx);
    draw_spacer(ctx, 6.0f);
  }
}

static void
draw_assets_section(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager, mod_t *m)
{
  (void)manager;

  static ui_text_span_t label_pak         = {0};
  static ui_text_span_t label_utoc        = {0};
  static ui_text_span_t label_ucas        = {0};
  static ui_text_span_t label_mount_state = {0};
  static ui_text_span_t label_priority    = {0};
  static ui_text_span_t label_error       = {0};
  static bool           cached            = false;

  if (!cached) {
    label_pak         = ui_text_span_make(ctx, STR_LIT("PAK:"));
    label_utoc        = ui_text_span_make(ctx, STR_LIT("UTOC:"));
    label_ucas        = ui_text_span_make(ctx, STR_LIT("UCAS:"));
    label_mount_state = ui_text_span_make(ctx, STR_LIT("Mount state:"));
    label_priority    = ui_text_span_make(ctx, STR_LIT("Priority:"));
    label_error       = ui_text_span_make(ctx, STR_LIT("Error:"));

    /* align labels */

    ui_text_cols_t info_cols = {0};
    ui_text_cols_reset(&info_cols, 1);

    ui_text_cols_include(&info_cols, 0, label_pak);
    ui_text_cols_include(&info_cols, 0, label_utoc);
    ui_text_cols_include(&info_cols, 0, label_ucas);

    label_pak.width  = info_cols.width[0];
    label_utoc.width = info_cols.width[0];
    label_ucas.width = info_cols.width[0];

    ui_text_cols_t runtime_cols = {0};
    ui_text_cols_reset(&runtime_cols, 1);

    ui_text_cols_include(&runtime_cols, 0, label_mount_state);
    ui_text_cols_include(&runtime_cols, 0, label_priority);
    ui_text_cols_include(&runtime_cols, 0, label_error);

    label_mount_state.width = runtime_cols.width[0];
    label_priority.width    = runtime_cols.width[0];
    label_error.width       = runtime_cols.width[0];

    cached = true;
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Assets", &ui->inspector.assets_open)) {
    if (nk_tree_state_push(ctx, NK_TREE_NODE, "Asset files", &ui->inspector.assets_info_open)) {
      if (!str_is_empty(m->manifest.asset.pak_path)) {
        draw_key_value(ctx, label_pak, m->manifest.asset.pak_path, UI_C_TEXT, true);
      }

      if (!str_is_empty(m->manifest.asset.utoc_path)) {
        draw_key_value(ctx, label_utoc, m->manifest.asset.utoc_path, UI_C_TEXT, true);
      }

      if (!str_is_empty(m->manifest.asset.ucas_path)) {
        draw_key_value(ctx, label_ucas, m->manifest.asset.ucas_path, UI_C_TEXT, true);
      }
      nk_tree_pop(ctx);
    }

    if (nk_tree_state_push(ctx, NK_TREE_NODE, "Runtime", &ui->inspector.assets_runtime_open)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        draw_key_value(ctx, label_mount_state, mod_asset_state_to_str(m->asset.state), UI_C_TEXT, false);
        draw_key_value(ctx, label_priority, str_push_fmt(tmp.arena, "%d", m->asset.priority), UI_C_TEXT, false);
        if (m->asset.state == MOD_ASSET_STATE_ERROR) {
          str_t err_msg = err_msg_as_str(m->asset.last_error);
          if (!str_is_empty(err_msg)) {
            draw_key_value(ctx, label_error, err_msg, UI_C_RED, false);
          }
        }
      }
      scratch_end(tmp);
      nk_tree_pop(ctx);
    }

    nk_tree_pop(ctx);
    draw_spacer(ctx, 6.0f);
  }
}

static void
draw_blueprints_section(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager, mod_t *m)
{
  static ui_text_span_t label_class_path      = {0};
  static ui_text_span_t label_context         = {0};
  static ui_text_span_t label_custom_context  = {0};
  static ui_text_span_t label_default_keybind = {0};
  static ui_text_span_t label_active          = {0};
  static ui_text_span_t label_actor           = {0};
  static ui_text_span_t label_error           = {0};
  static bool           cached                = false;

  if (!cached) {
    label_class_path      = ui_text_span_make(ctx, STR_LIT("Class path:"));
    label_context         = ui_text_span_make(ctx, STR_LIT("Spawn context:"));
    label_custom_context  = ui_text_span_make(ctx, STR_LIT("Custom class:"));
    label_default_keybind = ui_text_span_make(ctx, STR_LIT("Default keybind:"));
    label_active          = ui_text_span_make(ctx, STR_LIT("Active:"));
    label_actor           = ui_text_span_make(ctx, STR_LIT("Instance:"));
    label_error           = ui_text_span_make(ctx, STR_LIT("Error:"));

    /* align labels */

    ui_text_cols_t info_cols = {0};
    ui_text_cols_reset(&info_cols, 1);

    ui_text_cols_include(&info_cols, 0, label_class_path);
    ui_text_cols_include(&info_cols, 0, label_context);
    ui_text_cols_include(&info_cols, 0, label_custom_context);
    ui_text_cols_include(&info_cols, 0, label_default_keybind);

    label_class_path.width      = info_cols.width[0];
    label_context.width         = info_cols.width[0];
    label_custom_context.width  = info_cols.width[0];
    label_default_keybind.width = info_cols.width[0];

    ui_text_cols_t runtime_cols = {0};
    ui_text_cols_reset(&runtime_cols, 1);

    ui_text_cols_include(&runtime_cols, 0, label_active);
    ui_text_cols_include(&runtime_cols, 0, label_actor);
    ui_text_cols_include(&runtime_cols, 0, label_error);

    label_active.width = runtime_cols.width[0];
    label_actor.width  = runtime_cols.width[0];
    label_error.width  = runtime_cols.width[0];

    cached = true;
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Blueprints", &ui->inspector.blueprints_open)) {
    if (nk_tree_state_push(ctx, NK_TREE_NODE, "Blueprint definition", &ui->inspector.blueprints_info_open)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      for (int i = 0; i < m->blueprint_count; ++i) {
        mod_blueprint_info_t *info = &m->manifest.blueprints[i];

        draw_section_title(ctx, "%d. %.*s", i + 1, STR_ARG(info->id));
        {
          draw_key_value(ctx, label_class_path, info->mod_actor_class_path, UI_C_TEXT, true);
          draw_key_value(ctx, label_context, spawn_context_to_str(info->attach_to), UI_C_TEXT, false);

          if (info->attach_to == MOD_SPAWN_CONTEXT_CUSTOM) {
            draw_key_value(ctx, label_custom_context, info->custom_attach_class_path, UI_C_TEXT, true);
          }

          if (keybind_is_valid(info->default_spawn_keybind)) {
            str_t keybind_str = keybind_to_str(info->default_spawn_keybind, tmp.arena);
            draw_key_value(ctx, label_default_keybind, keybind_str, UI_C_TEXT, true);
          }
        }
        draw_spacer(ctx, 4.0f);
      }

      scratch_end(tmp);
      nk_tree_pop(ctx);
    }

    if (nk_tree_state_push(ctx, NK_TREE_NODE, "Runtime", &ui->inspector.blueprints_runtime_open)) {
      for (int i = 0; i < m->blueprint_count; ++i) {
        mod_blueprint_info_t    *info = &m->manifest.blueprints[i];
        mod_blueprint_runtime_t *rt   = &m->blueprints[i];

        draw_section_title(ctx, "%d. %.*s", i + 1, STR_ARG(info->id));
        {
          tmp_arena_t tmp = scratch_begin(NULL);
          {
            draw_key_value(ctx, label_active, STR_BOOL(rt->active), (rt->active) ? UI_C_GREEN : UI_C_RED, false);
            draw_key_value(ctx, label_actor, str_push_fmt(tmp.arena, "%p", rt->actor), UI_C_TEXT, false);
          }
          scratch_end(tmp);
        }

        if (rt->err_stage != MOD_BP_ERROR_NONE) {
          str_t err_msg = err_msg_as_str(rt->err_msg);
          if (!str_is_empty(err_msg)) {
            draw_key_value(ctx, label_error, err_msg, UI_C_RED, false);
          }
        }

        nk_layout_row_dynamic(ctx, 28.0f, 1);
        if (rt->active) {
          if (nk_widget_is_hovered(ctx)) {
            nk_tooltip(ctx, "Despawn active blueprint instance");
          }

          if (ui_button_str(ctx, STR_LIT("Despawn"))) {
            mod_blueprint_despawn(manager, mod_handle_make(m), i);
          }
        } else {
          if (nk_widget_is_hovered(ctx)) {
            nk_tooltip(ctx, "Spawn a blueprint instance");
          }

          if (ui_button_str(ctx, STR_LIT("Spawn"))) {
            mod_blueprint_spawn(manager, mod_handle_make(m), i);
          }
        }

        draw_spacer(ctx, 4.0f);
      }

      nk_tree_pop(ctx);
    }

    nk_tree_pop(ctx);
    draw_spacer(ctx, 6.0f);
  }
}

static void
reorder_sync_from_want(ui_mod_manager_t *ui, mod_manager_t *manager)
{
  ui->reorder.order_count  = manager->mod_order.count;
  ui->reorder.dirty        = false;
  ui->reorder.selected_idx = 0;

  for (int i = 0; i < manager->mod_order.count; ++i) {
    ui->reorder.order[i] = manager->mod_order.want[i];
    if (ui->selected == ui->reorder.order[i]) {
      ui->reorder.selected_idx = i;
    }
  }
}

static void
reorder_apply_to_want(ui_mod_manager_t *ui, mod_manager_t *manager)
{
  manager->mod_order.count = ui->reorder.order_count;
  for (int i = 0; i < ui->reorder.order_count; ++i) {
    manager->mod_order.want[i] = ui->reorder.order[i];
  }
}

static void
reorder_swap(ui_mod_manager_t *ui, int a, int b)
{
  if (a < 0 || b < 0 || a >= ui->reorder.order_count || b >= ui->reorder.order_count || a == b) {
    return;
  }

  mod_handle_t tmp     = ui->reorder.order[a];
  ui->reorder.order[a] = ui->reorder.order[b];
  ui->reorder.order[b] = tmp;

  ui->reorder.selected_idx = b;
  ui->reorder.dirty        = true;
}

static void
dialog_window_init_centered(ui_mod_manager_t *ui, ui_dialog_window_t *win, float w, float h)
{
  if (!win || win->inited) {
    return;
  }

  win->bounds.x = ((float)ui->vw - w) * 0.5f;
  win->bounds.y = ((float)ui->vh - h) * 0.5f;
  win->bounds.w = w;
  win->bounds.h = h;
  win->inited   = true;
}

static bool
dialog_is_open(ui_mod_manager_t *ui)
{
  return ui->dialog.kind != UI_DIALOG_NONE;
}

static void
dialog_open(ui_mod_manager_t *ui, ui_dialog_kind_t kind, mod_manager_t *manager)
{
  if (kind == UI_DIALOG_NONE) {
    return;
  }

  ui->dialog.kind = kind;
  switch (kind) {
  case UI_DIALOG_CONFIG: {
    dialog_window_init_centered(ui, &ui->dialog.config_win, 520.0f, 420.0f);
  } break;

  case UI_DIALOG_REORDER: {
    dialog_window_init_centered(ui, &ui->dialog.reorder_win, 480.0f, 520.0f);
    reorder_sync_from_want(ui, manager);
  } break;
  }
}

static void
dialog_close(ui_mod_manager_t *ui)
{
  ui->dialog.kind = UI_DIALOG_NONE;
}

static void
draw_config_dialog(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager)
{
  if (ui->dialog.kind != UI_DIALOG_CONFIG) {
    return;
  }

  const char *name  = "Config";
  nk_flags    flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

  const float min_w = 460.0f;
  const float min_h = 240.0f;

  if (nk_begin(ctx, name, ui->dialog.config_win.bounds, flags)) {
    struct nk_rect content  = nk_window_get_content_region(ctx);
    float          gap_y    = ctx->style.window.spacing.y;
    float          footer_h = 28.0f;
    float          body_h   = NK_MAX(80.0f, content.h - footer_h - gap_y * 3.0f);

    ui_win_clamp_bounds(ctx, name, nk_vec2(min_w, min_h), nk_vec2((float)ui->vw, (float)ui->vh));

    bool dirty = false;

    nk_layout_row_dynamic(ctx, body_h, 1);
    {
      if (nk_group_begin(ctx, "config.body", 0)) {
        if (nk_tree_state_push(ctx, NK_TREE_TAB, "Manager", &ui->inspector.config_manager)) {
          dirty |= form_manager_cfg(ctx, ui->keybind_capture, manager);
          nk_tree_state_pop(ctx);
        }

        if (nk_tree_state_push(ctx, NK_TREE_TAB, "Console", &ui->inspector.config_console)) {
          dirty |= form_console_cfg(ctx, ui->keybind_capture, &globals.ui_manager.console);
          nk_tree_state_pop(ctx);
        }

        nk_group_end(ctx);
      }
    }

    nk_layout_row_dynamic(ctx, footer_h, 2);
    {
      if (!dirty) {
        nk_widget_disable_begin(ctx);
      }

      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Apply pending changes");
      }

      if (ui_button_str(ctx, STR_LIT("Apply"))) {
        mod_manager_save_cfg(manager);
        dialog_close(ui);
      }

      if (!dirty) {
        nk_widget_disable_end(ctx);
      }

      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Cancel pending changes and close window");
      }

      if (ui_button_str(ctx, STR_LIT("Close"))) {
        dialog_close(ui);
      }
    }
  }
  nk_end(ctx);

  if (nk_window_is_closed(ctx, name)) {
    dialog_close(ui);
  }
}

static void
draw_reorder_content(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager)
{
  struct nk_rect content  = nk_window_get_content_region(ctx);
  float          gap_x    = ctx->style.window.spacing.x;
  float          action_w = NK_CLAMP(content.w * 0.28f, 120.0f, 180.0f);
  float          list_w   = NK_MAX(120.0f, content.w - action_w - gap_x);
  float          row_h    = NK_MAX(120.0f, content.h);

  nk_layout_row_begin(ctx, NK_STATIC, row_h, 2);
  {
    nk_layout_row_push(ctx, list_w);
    if (nk_group_begin(ctx, "reorder.list", 0)) {
      tmp_arena_t tmp = scratch_begin(NULL);

      nk_layout_row_dynamic(ctx, 24.0f, 1);
      for (int i = 0; i < ui->reorder.order_count; ++i) {
        mod_t *m = mod_handle_resolve(manager, ui->reorder.order[i]);
        if (!m) {
          continue;
        }

        nk_bool selected = (i == ui->reorder.selected_idx);
        str_t   kind     = str_push_fmt(tmp.arena, "[%.*s]", STR_ARG(mod_kind_to_str(m->kind)));
        str_t   name     = m->manifest.info.name;
        str_t   line     = str_push_fmt(tmp.arena, "%02d. %-6.*s %.*s", i + 1, STR_ARG(kind), STR_ARG(name));

        nk_selectable_text(ctx, (char *)line.data, (int)line.len, NK_TEXT_LEFT, &selected);
        if (selected) {
          ui->reorder.selected_idx = i;
        }
      }

      scratch_end(tmp);
      nk_group_end(ctx);
    }

    nk_layout_row_push(ctx, action_w);
    if (nk_group_begin(ctx, "reorder.actions", NK_WINDOW_NO_SCROLLBAR)) {
      nk_layout_row_dynamic(ctx, 28.0f, 1);
      if (ui_button_str(ctx, STR_LIT("Move Up"))) {
        reorder_swap(ui, ui->reorder.selected_idx, ui->reorder.selected_idx - 1);
      }

      nk_layout_row_dynamic(ctx, 28.0f, 1);
      if (ui_button_str(ctx, STR_LIT("Move Down"))) {
        reorder_swap(ui, ui->reorder.selected_idx, ui->reorder.selected_idx + 1);
      }

      nk_layout_row_dynamic(ctx, 8.0f, 1);
      nk_spacing(ctx, 1);

      nk_layout_row_dynamic(ctx, 28.0f, 1);

      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Apply the new mod order");
      }

      if (ui_button_str(ctx, STR_LIT("Apply"))) {
        reorder_apply_to_want(ui, manager);
        mod_manager_save_cfg(manager);
        dialog_close(ui);
      }

      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "Discard changes and close");
      }

      if (ui_button_str(ctx, STR_LIT("Cancel"))) {
        dialog_close(ui);
      }

      nk_group_end(ctx);
    }
  }
  nk_layout_row_end(ctx);
}

static void
draw_reorder_dialog(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager)
{
  if (ui->dialog.kind != UI_DIALOG_REORDER) {
    return;
  }

  const char *name  = "Reorder Mods";
  nk_flags    flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

  const float min_w = 420.0f;
  const float min_h = 320.0f;

  if (nk_begin(ctx, name, ui->dialog.reorder_win.bounds, flags)) {
    ui_win_clamp_bounds(ctx, name, nk_vec2(min_w, min_h), nk_vec2((float)ui->vw, (float)ui->vh));
    draw_reorder_content(ui, ctx, manager);
  }
  nk_end(ctx);

  if (nk_window_is_closed(ctx, name)) {
    dialog_close(ui);
  } else {
    nk_window_set_focus(ctx, name);
  }
}

static bool
draw_mod_list_row(struct nk_context *ctx, str_t name, ui_item_state_t st, bool is_selected)
{
  struct nk_color dot_color = nk_rgb(120, 130, 145);

  if (st == UI_ITEM_ACTIVE) {
    dot_color = nk_rgb(80, 200, 120);
  } else if (st == UI_ITEM_ERROR) {
    dot_color = nk_rgb(220, 80, 80);
  }

  ui_nk_select_row_opts_t opts = {
    .text      = name,
    .selected  = is_selected,
    .show_dot  = true,
    .dot_color = dot_color,
    .pad_x     = 10.0f,
    .dot_d     = 8.0f,
  };

  return ui_select_row(ctx, &opts);
}

static void
draw_right_panel(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager)
{
  mod_t *m = mod_handle_resolve(manager, ui->selected);
  if (!m) {
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_label(ctx, "Select a mod to view its details", NK_TEXT_LEFT);
    return;
  }

  draw_mod_header(ui, ctx, manager, m);

  if (m->has_options || m->has_blueprints || (m->has_code && m->dll.funcs.draw_config)) {
    draw_config_section(ui, ctx, manager, m);
  }

  if (m->has_code) {
    draw_code_section(ui, ctx, manager, m);
  }

  if (m->has_assets) {
    draw_assets_section(ui, ctx, manager, m);
  }

  if (m->has_blueprints) {
    draw_blueprints_section(ui, ctx, manager, m);
  }

  if (m->has_code && m->dll.funcs.draw_panel) {
    m->dll.funcs.draw_panel(mod_handle_make(m), ctx);
  }

  if (!m->has_options && !m->has_code && !m->has_assets && !m->has_blueprints) {
    nk_layout_row_dynamic(ctx, 20.0f, 1);
    nk_label(ctx, "This mod does not expose any settings or runtime controls", NK_TEXT_LEFT);
  }

  draw_spacer(ctx, 2.0f);
}

static void
draw_body(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager)
{
  struct nk_rect content = nk_window_get_content_region(ctx);

  float pad_x = ctx->style.window.padding.x;
  float pad_y = ctx->style.window.padding.y;

  content.x += pad_x;
  content.y += pad_y;
  content.w -= 2.0f * pad_x;
  content.h -= 2.0f * pad_y;

  float gap_x      = ctx->style.window.spacing.x;
  float gap_y      = ctx->style.window.spacing.y;
  float splitter_w = 8.0f;
  float button_h   = 28.0f;
  float footer_h   = button_h * 2.0f + gap_y;
  float list_h     = content.h - footer_h;

  float left_w  = (float)((int)(content.w * ui->split.ratio));
  float right_w = content.w - left_w - splitter_w - gap_x * 2.0f;

  list_h = NK_MAX(list_h, 80.0f);
  left_w = NK_MAX(left_w, 120.0f);

  if (right_w < 240.0f) {
    right_w = 240.0f;
    left_w  = content.w - right_w - splitter_w - gap_x * 2.0f;
  }

  struct nk_rect list_r     = nk_rect(0, 0, left_w, list_h - gap_y);
  struct nk_rect button1_r  = nk_rect(0, list_h, left_w, button_h);
  struct nk_rect button2_r  = nk_rect(0, list_h + button_h + gap_y, left_w, button_h);
  struct nk_rect splitter_r = nk_rect(left_w + gap_x, 0, splitter_w, content.h);
  struct nk_rect right_r    = nk_rect(splitter_r.x + splitter_r.w + gap_x, 0, right_w, content.h);

  nk_layout_space_begin(ctx, NK_STATIC, content.h, 5);
  {
    struct nk_rect track_screen = nk_layout_space_rect_to_screen(ctx, nk_rect(0.0f, 0.0f, content.w, content.h));

    nk_layout_space_push(ctx, list_r);
    if (nk_group_begin(ctx, "left.list", NK_WINDOW_NO_SCROLLBAR_H)) {
      nk_layout_row_dynamic(ctx, 24.0f, 1);
      for (int i = 0; i < manager->mod_order.count; ++i) {
        mod_handle_t h = manager->mod_order.want[i];
        mod_t       *m = mod_handle_resolve(manager, h);
        if (!m) {
          continue;
        }

        bool            is_selected = (h == ui->selected);
        ui_item_state_t st          = ui_mod_state(m);

        if (draw_mod_list_row(ctx, m->manifest.info.name, st, is_selected)) {
          ui->selected = h;
        }
      }
      nk_group_end(ctx);
    }

    nk_layout_space_push(ctx, button1_r);
    if (ui_button_str(ctx, STR_LIT("Reorder mods"))) {
      dialog_open(ui, UI_DIALOG_REORDER, manager);
    }

    nk_layout_space_push(ctx, button2_r);
    if (ui_button_str(ctx, STR_LIT("Settings"))) {
      dialog_open(ui, UI_DIALOG_CONFIG, manager);
    }

    nk_layout_space_push(ctx, splitter_r);
    {
      ui_nk_splitter_opts_t split_opts = {
        .axis           = UI_NK_AXIS_X,
        .track          = track_screen,
        .gap_before     = gap_x,
        .min_before     = 180.0f,
        .min_after      = 320.0f,
        .line_thickness = 2.0f,
        .disabled       = dialog_is_open(ui),
      };

      ui_splitter(ctx, &ui->split, &split_opts);
    }

    nk_layout_space_push(ctx, right_r);
    if (nk_group_begin(ctx, "right.panel", NK_WINDOW_NO_SCROLLBAR_H)) {
      draw_right_panel(ui, ctx, manager);
      nk_group_end(ctx);
    }
  }
  nk_layout_space_end(ctx);
}

void
ui_mod_manager_draw(ui_mod_manager_t *ui, struct nk_context *ctx, mod_manager_t *manager, unsigned int viewport_width, unsigned int viewport_height)
{
  const char *name = "mod_manager";

  if (!ui_mod_manager_is_open(ui)) {
    if (!nk_window_is_hidden(ctx, name)) {
      nk_window_show(ctx, name, NK_HIDDEN);
    }
    return;
  }

  bool has_modal = dialog_is_open(ui);

  nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MINIMIZABLE |
                   NK_WINDOW_CLOSE_BUTTON_HIDES;

  if (has_modal) {
    flags |= NK_WINDOW_NOT_INTERACTIVE;
  }

  ui->vw = viewport_width;
  ui->vh = viewport_height;

  int win_width  = 1000;
  int win_height = 600;

  if (ui->bounds.x == 0 && ui->bounds.y == 0 && ui->bounds.w == 0 && ui->bounds.h == 0) {
    ui->bounds = (struct nk_rect){
      .x = (ui->vw - win_width) * 0.5f,
      .y = (ui->vh - win_height) * 0.5f,
      .w = (float)win_width,
      .h = (float)win_height,
    };
  }

  if (ui->dialog.config_win.inited) {
    draw_config_dialog(ui, ctx, manager);
  }

  if (ui->dialog.reorder_win.inited) {
    draw_reorder_dialog(ui, ctx, manager);
  }

  static char title[256] = {0};
  stbsp_snprintf(title, sizeof(title), "Mod manager - %.1f FPS", globals.fps);

  if (nk_begin_titled(ctx, name, title, ui->bounds, flags)) {
    ui->bounds = nk_window_get_bounds(ctx);

    draw_body(ui, ctx, manager);

    if (has_modal) {
      nk_fill_rect(nk_window_get_canvas(ctx), ui->bounds, 0.0f, nk_rgba(0, 0, 0, 90));
    }
  }
  nk_end(ctx);

  if (nk_window_is_hidden(ctx, name)) {
    ui_mod_manager_close(ui);
  }
}

void
ui_mod_manager_toggle(ui_mod_manager_t *ui)
{
  if (ui) {
    ui->closed = !ui->closed;
  }
}

void
ui_mod_manager_open(ui_mod_manager_t *ui)
{
  if (ui) {
    ui->closed = false;
  }
}

void
ui_mod_manager_close(ui_mod_manager_t *ui)
{
  if (ui) {
    ui->closed = true;
  }
}

bool
ui_mod_manager_is_open(ui_mod_manager_t *ui)
{
  if (!ui) {
    return false;
  }
  return !ui->closed;
}

void
ui_mod_manager_init(ui_mod_manager_t *ui, ui_keybind_capture_t *cap, struct nk_font *font_body, struct nk_font *font_title)
{
  *ui = (ui_mod_manager_t){
    .keybind_capture = cap,
    .split =
      {
        .ratio    = 0.28f,
        .dragging = false,
        .hovering = false,
      },
    .dialog =
      {
        .kind = UI_DIALOG_NONE,
      },
    .inspector =
      {
        .info_open               = NK_MAXIMIZED,
        .code_open               = NK_MAXIMIZED,
        .code_dll_info_open      = NK_MINIMIZED,
        .code_runtime_open       = NK_MAXIMIZED,
        .blueprints_open         = NK_MAXIMIZED,
        .blueprints_info_open    = NK_MINIMIZED,
        .blueprints_runtime_open = NK_MAXIMIZED,
        .assets_open             = NK_MAXIMIZED,
        .assets_info_open        = NK_MAXIMIZED,
        .assets_runtime_open     = NK_MAXIMIZED,
        .options_open            = NK_MAXIMIZED,
        .options_code_open       = NK_MINIMIZED,
        .options_blueprints_open = NK_MINIMIZED,
        .options_assets_open     = NK_MINIMIZED,
        .config_manager          = NK_MAXIMIZED,
        .config_console          = NK_MAXIMIZED,
      },
    .selected   = MOD_HANDLE_INVALID,
    .font_body  = font_body,
    .font_title = font_title,
    .closed     = true,
    .inited     = true,
  };
}
