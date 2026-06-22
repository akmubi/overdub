#include "ui_console.h"

#include "config.h"
#include "globals.h"
#include "log.h"
#include "scratch.h"
#include "str.h"
#include "ui_nuklear.h"

static struct nk_color
ui_console_level_color(ui_console_log_level_t level)
{
  switch (level) {
  case UI_CONSOLE_LOG_DEBUG:
    return nk_rgb(150, 150, 150);
  case UI_CONSOLE_LOG_INFO:
    return nk_rgb(220, 220, 220);
  case UI_CONSOLE_LOG_WARN:
    return nk_rgb(255, 200, 50);
  case UI_CONSOLE_LOG_ERROR:
    return nk_rgb(255, 80, 80);
  default:
    return nk_rgb(200, 200, 200);
  }
}

static inline ui_console_position_t
ui_console_position_from_str(str_t s)
{
  if (str_equal_icase(s, STR_LIT("bottom"))) {
    return UI_CONSOLE_POSITION_BOTTOM;
  } else if (str_equal_icase(s, STR_LIT("top"))) {
    return UI_CONSOLE_POSITION_TOP;
  }

  return UI_CONSOLE_POSITION_BOTTOM;
}

static inline ui_console_log_level_t
ui_console_log_level_from_str(str_t s)
{
  if (str_equal_icase(s, STR_LIT("debug"))) {
    return UI_CONSOLE_LOG_DEBUG;
  } else if (str_equal_icase(s, STR_LIT("info"))) {
    return UI_CONSOLE_LOG_INFO;
  } else if (str_equal_icase(s, STR_LIT("warn"))) {
    return UI_CONSOLE_LOG_WARN;
  } else if (str_equal_icase(s, STR_LIT("error"))) {
    return UI_CONSOLE_LOG_ERROR;
  }

  return UI_CONSOLE_LOG_DEBUG;
}

static str_t g_level_names[UI_CONSOLE_LOG_LEVEL_MAX] = {
  [UI_CONSOLE_LOG_DEBUG] = STR_CLIT("Debug"),
  [UI_CONSOLE_LOG_INFO]  = STR_CLIT("Info"),
  [UI_CONSOLE_LOG_WARN]  = STR_CLIT("Warn"),
  [UI_CONSOLE_LOG_ERROR] = STR_CLIT("Error"),
};

static str_t g_position_names[UI_CONSOLE_POSITION_MAX] = {
  [UI_CONSOLE_POSITION_BOTTOM] = STR_CLIT("Bottom"),
  [UI_CONSOLE_POSITION_TOP]    = STR_CLIT("Top"),
};

static inline str_t
ui_console_position_to_str(ui_console_position_t pos)
{
  if (pos < UI_CONSOLE_POSITION_MAX) {
    return g_position_names[pos];
  }
  return g_position_names[UI_CONSOLE_POSITION_BOTTOM];
}

static inline str_t
ui_console_log_level_to_str(ui_console_log_level_t level)
{
  if (level < UI_CONSOLE_LOG_LEVEL_MAX) {
    return g_level_names[level];
  }
  return g_level_names[UI_CONSOLE_LOG_DEBUG];
}

str_array_t
ui_console_log_level_str_array(void)
{
  return str_array_make(g_level_names, COUNTOF(g_level_names));
}

str_array_t
ui_console_position_str_array(void)
{
  return str_array_make(g_position_names, COUNTOF(g_position_names));
}

static uint8_t *
ui_console_line_slot_ptr(ui_console_t *console, int slot_idx)
{
  return console->line_text_storage + ((uint64_t)slot_idx * (uint64_t)console->max_line_len);
}

static uint8_t *
ui_console_history_slot_ptr(ui_console_t *console, int slot_idx)
{
  return console->history_storage + ((uint64_t)slot_idx * (uint64_t)console->input_max);
}

static str_t
ui_console_slot_copy(uint8_t *dst, int dst_cap, str_t src)
{
  if (!dst || dst_cap <= 0) {
    return (str_t){0};
  }

  if (str_is_empty(src)) {
    dst[0] = 0;
    return str_make(dst, 0);
  }

  uint64_t n = MIN_VAL(src.len, (uint64_t)(dst_cap - 1));
  mem_copy(dst, src.data, n);
  dst[n] = 0;
  return str_make(dst, n);
}

static void
ui_console_set_input(ui_console_t *console, str_t text)
{
  console->input = ui_console_slot_copy(console->input_buf, console->input_max, text);
}

static void
ui_console_clear_input(ui_console_t *console)
{
  ui_console_set_input(console, (str_t){0});
  console->history_pos = -1;
}

static int
ui_console_history_oldest_abs_index(ui_console_t *console)
{
  int oldest = console->history_count - console->history_max;
  return (oldest > 0) ? oldest : 0;
}

static void
ui_console_push_history(ui_console_t *console, str_t text)
{
  if (!console->history || !console->history_storage) {
    return;
  }

  text = str_trim_space(text);
  if (str_is_empty(text)) {
    return;
  }

  int      abs_idx  = console->history_count;
  int      slot_idx = abs_idx % console->history_max;
  uint8_t *slot     = ui_console_history_slot_ptr(console, slot_idx);

  console->history[slot_idx]  = ui_console_slot_copy(slot, console->input_max, text);
  console->history_count     += 1;
  console->history_pos        = -1;
}

static void
ui_console_history_prev(ui_console_t *console)
{
  if (!console->history || !console->history_storage) {
    return;
  }

  if (console->history_count <= 0) {
    return;
  }

  int oldest = ui_console_history_oldest_abs_index(console);
  if (console->history_pos < 0) {
    console->history_pos = console->history_count - 1;
  } else if (console->history_pos > oldest) {
    console->history_pos -= 1;
  }

  int slot_idx = console->history_pos % console->history_max;
  ui_console_set_input(console, console->history[slot_idx]);
}

static void
ui_console_history_next(ui_console_t *console)
{
  if (!console->history || !console->history_storage) {
    return;
  }

  if (console->history_pos < 0) {
    return;
  }

  console->history_pos += 1;
  if (console->history_pos >= console->history_count) {
    ui_console_clear_input(console);
    return;
  }

  int slot_idx = console->history_pos % console->history_max;
  ui_console_set_input(console, console->history[slot_idx]);
}

static void
ui_console_store_line(ui_console_t *console, ui_console_log_level_t level, str_t text)
{
  if (!console->lines || !console->line_text_storage) {
    return;
  }

  int                slot_idx = console->line_count % console->max_lines;
  ui_console_line_t *l        = &console->lines[slot_idx];
  uint8_t           *slot     = ui_console_line_slot_ptr(console, slot_idx);

  l->text  = ui_console_slot_copy(slot, console->max_line_len, text);
  l->level = level;
  l->row_w = 0.0f;

  console->line_count       += 1;
  console->scroll_to_bottom  = true;
}

void
ui_console_init(ui_console_t *console, arena_t *arena)
{
  if (console->inited) {
    return;
  }

  console->max_lines    = CONFIG_NK_CONSOLE_MAX_LINES;
  console->max_line_len = CONFIG_NK_CONSOLE_MAX_LINE_LEN;
  console->input_max    = CONFIG_NK_CONSOLE_MAX_INPUT_LEN;
  console->history_max  = CONFIG_NK_CONSOLE_MAX_HISTORY;
  console->history_pos  = -1;

  console->lines             = ARENA_PUSH_ARRAY_ZERO(arena, ui_console_line_t, console->max_lines);
  console->line_text_storage = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, (uint64_t)console->max_lines *(uint64_t)console->max_line_len);

  console->input_buf = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, console->input_max);
  console->input     = str_make(console->input_buf, 0);

  console->history         = ARENA_PUSH_ARRAY_ZERO(arena, str_t, console->history_max);
  console->history_storage = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, (uint64_t)console->history_max *(uint64_t)console->input_max);

  console->cfg.toggle_bind        = keybind_parse(CONFIG_NK_CONSOLE_DEFAULT_TOGGLE_KEY, KEYBIND_NULL);
  console->cfg.min_level             = UI_CONSOLE_LOG_DEBUG;
  console->cfg.position              = UI_CONSOLE_POSITION_BOTTOM;
  console->cfg.auto_scroll = true;
  console->saved_cfg                 = console->cfg;

  console->drag.height_ratio = CONFIG_NK_CONSOLE_DEFAULT_HEIGHT_RATIO;
  console->drag.dragging     = false;
  console->drag.hovering     = false;
  console->scroll            = (struct nk_scroll){0};
  console->scroll_to_bottom  = true;
  console->focus_input       = false;
  console->inited            = true;
  console->closed            = true;
}

bool
ui_console_load_cfg(ui_console_t *console, str_list_t lines)
{
  ASSERT(console != NULL);

  for (str_node_t *node = lines.first; node; node = node->next) {
    str_t key   = {0};
    str_t value = {0};
    if (!ini_parse_kv(node->str, &key, &value)) {
      continue;
    }

    if (str_equal_icase(key, STR_LIT("toggle_keybind"))) {
      console->cfg.toggle_bind = keybind_parse(value, KEYBIND_NULL);
    } else if (str_equal_icase(key, STR_LIT("auto_scroll"))) {
      console->cfg.auto_scroll = str_parse_bool(value, true);
    } else if (str_equal_icase(key, STR_LIT("position"))) {
      console->cfg.position = ui_console_position_from_str(value);
    } else if (str_equal_icase(key, STR_LIT("min_level"))) {
      console->cfg.min_level = ui_console_log_level_from_str(value);
    }
  }

  return true;
}

void
ui_console_commit_cfg(ui_console_t *console)
{
  if (console) {
    console->saved_cfg = console->cfg;
  }
}

void
ui_console_save_cfg(ui_console_t *console, arena_t *arena, str_list_t *lines)
{
  ASSERT(console != NULL);
  ASSERT(lines != NULL);

  str_t toggle_bind_str = keybind_to_str(console->cfg.toggle_bind, arena);

  str_list_push_fmt(arena, lines, "toggle_keybind = %.*s", STR_ARG(toggle_bind_str));
  str_list_push_fmt(arena, lines, "auto_scroll    = %.*s", STR_ARG(STR_BOOL(console->cfg.auto_scroll)));
  str_list_push_fmt(arena, lines, "position       = %.*s", STR_ARG(ui_console_position_to_str(console->cfg.position)));
  str_list_push_fmt(arena, lines, "min_level      = %.*s", STR_ARG(ui_console_log_level_to_str(console->cfg.min_level)));
}

bool
ui_console_cfg_is_dirty(ui_console_t *console)
{
  if (console) {
    if (!keybind_equal(console->cfg.toggle_bind, console->saved_cfg.toggle_bind)) {
      return true;
    }

    if (console->cfg.auto_scroll != console->saved_cfg.auto_scroll) {
      return true;
    }

    if (console->cfg.position != console->saved_cfg.position) {
      return true;
    }

    if (console->cfg.min_level != console->saved_cfg.min_level) {
      return true;
    }
  }

  return false;
}

void
ui_console_logv(ui_console_t *console, ui_console_log_level_t level, const char *fmt, va_list ap)
{
  if (!console->inited) {
    return;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t      msg   = str_push_vfmt(tmp.arena, fmt, ap);
    str_list_t lines = str_split_lines(tmp.arena, msg);
    for (str_node_t *node = lines.first; node; node = node->next) {
      ui_console_store_line(console, level, node->str);
    }
  }
  scratch_end(tmp);
}

static bool
ui_console_execute_builtin(ui_console_t *console, mod_manager_t *mod_manager, str_t cmd_name, str_t cmd_args)
{
  (void)console;
  (void)cmd_args;

  if (str_equal(cmd_name, STR_LIT("help"), STR_CMP_FLAG_IGNORE_CASE)) {
    CONSOLE_INFO("Built-in commands:");
    CONSOLE_INFO("  help - List available commands");

    for (int i = 0; i < mod_manager->mod_order.count; ++i) {
      mod_handle_t h = mod_manager->mod_order.runtime[i];
      mod_t       *m = mod_handle_resolve(mod_manager, h);
      if (m && m->has_code && m->dll.command_count > 0) {
        CONSOLE_INFO("Commands provided by '%.*s':", STR_ARG(m->manifest.info.name));
        for (int i = 0; i < m->dll.command_count; ++i) {
          mod_cmd_t *cmd = &m->dll.commands[i];
          CONSOLE_INFO("  %.*s - %.*s", STR_ARG(cmd->name), STR_ARG(cmd->description));
        }
      }
    }
    return true;
  }
  return false;
}

static void
ui_console_execute(ui_console_t *console, mod_manager_t *mod_manager, str_t input)
{
  input = str_trim_space(input);
  if (str_is_empty(input)) {
    return;
  }

  ui_console_push_history(console, input);
  CONSOLE_INFO("> %.*s", STR_ARG(input));

  str_t cmd_name = input;
  str_t cmd_args = {0};

  uint64_t split_idx = 0;
  if (str_find(input, STR_LIT(" "), 0, &split_idx)) {
    cmd_name = str_slice(input, 0, split_idx);
    cmd_args = str_trim_leading_space(str_slice(input, split_idx + 1, input.len));
  }

  if (ui_console_execute_builtin(console, mod_manager, cmd_name, cmd_args)) {
    return;
  }

  if (mod_manager_dispatch_command(mod_manager, cmd_name, cmd_args)) {
    return;
  }

  CONSOLE_ERROR("Unknown command '%.*s'. Type 'help' to list available commands.", STR_ARG(cmd_name));
}

static void
ui_console_draw_lines(ui_console_t *console, struct nk_context *ctx)
{
  if (!console->lines) {
    return;
  }

  int total   = console->line_count;
  int visible = MIN_VAL(total, console->max_lines);
  int start   = MAX_VAL(0, total - visible);
  int shown   = 0;

  for (int i = 0; i < visible; ++i) {
    int                slot_idx = (start + i) % console->max_lines;
    ui_console_line_t *line     = &console->lines[slot_idx];

    if (line->level < console->cfg.min_level) {
      continue;
    }

    struct nk_color level_color = ui_console_level_color(line->level);

    nk_layout_row_static(ctx, CONFIG_NK_CONSOLE_LINE_HEIGHT, (int)line->row_w, 1);
    ui_text(ctx, line->text, line->row_w, NK_TEXT_LEFT, level_color);

    shown += 1;
  }

  if (shown == 0) {
    nk_layout_row_static(ctx, CONFIG_NK_CONSOLE_LINE_HEIGHT, 1, 1);
    nk_spacing(ctx, 1);
  }
}

static void
ui_console_draw_log_widget(ui_console_t *console, struct nk_context *ctx)
{
  if (nk_group_scrolled_begin(ctx, &console->scroll, "console.log", 0)) {
    ui_console_draw_lines(console, ctx);
    nk_group_scrolled_end(ctx);
  }

  if (console->cfg.auto_scroll) {
    if (console->scroll_to_bottom) {
      console->scroll.y = console->scroll.prev_max_y;
    }
    console->scroll_to_bottom = false;
  }
}

static void
ui_console_draw_cmd_widget(ui_console_t *console, mod_manager_t *mod_manager, struct nk_context *ctx)
{
  struct nk_input *in = &ctx->input;

  bool history_changed = false;
  if (nk_window_is_any_hovered(ctx) || nk_item_is_any_active(ctx)) {
    if (nk_input_is_key_pressed(in, NK_KEY_UP)) {
      ui_console_history_prev(console);
      history_changed = true;
    } else if (nk_input_is_key_pressed(in, NK_KEY_DOWN)) {
      ui_console_history_next(console);
      history_changed = true;
    }
  }

  if (console->focus_input) {
    nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE);
    console->focus_input = false;
  }

  int   input_len = (int)console->input.len;
  int   max       = console->input_max;
  char *buf       = (char *)console->input_buf;

  nk_flags flags =
    (nk_flags)NK_EDIT_FIELD |
    NK_EDIT_SELECTABLE |
    NK_EDIT_CLIPBOARD |
    NK_EDIT_SIG_ENTER |
    NK_EDIT_GOTO_END_ON_ACTIVATE |
    NK_EDIT_ALWAYS_INSERT_MODE;

  nk_flags edit = nk_edit_string(ctx, flags, buf, &input_len, max, nk_filter_default);

  console->input = str_make(console->input_buf, (uint64_t)input_len);

  if (history_changed) {
    console->input = str_make(console->input_buf, console->input.len);
  }

  if (edit & NK_EDIT_COMMITED) {
    ui_console_execute(console, mod_manager, console->input);
    ui_console_clear_input(console);
  }
}

static void
ui_console_draw_level_selector(ui_console_t *console, struct nk_context *ctx, float popup_w)
{
  const char *items[UI_CONSOLE_LOG_LEVEL_MAX] = {
    [UI_CONSOLE_LOG_DEBUG] = (const char *)g_level_names[UI_CONSOLE_LOG_DEBUG].data,
    [UI_CONSOLE_LOG_INFO]  = (const char *)g_level_names[UI_CONSOLE_LOG_INFO].data,
    [UI_CONSOLE_LOG_WARN]  = (const char *)g_level_names[UI_CONSOLE_LOG_WARN].data,
    [UI_CONSOLE_LOG_ERROR] = (const char *)g_level_names[UI_CONSOLE_LOG_ERROR].data,
  };

  int selected = console->cfg.min_level;
  selected = nk_combo(ctx, items, COUNTOF(items), selected, (int)CONFIG_NK_CONSOLE_INPUT_HEIGHT, nk_vec2(popup_w, FLT_MAX));
  if (selected >= 0 && selected < UI_CONSOLE_LOG_LEVEL_MAX) {
    ui_console_log_level_t new_level = (ui_console_log_level_t)selected;

    if (new_level != console->cfg.min_level) {
      console->cfg.min_level    = new_level;
      console->scroll_to_bottom = true;
    }
  }
}

void
ui_console_toggle(ui_console_t *console)
{
  if (console) {
    console->closed = !console->closed;
    if (!console->closed) {
      console->focus_input = true;
    }
  }
}

void
ui_console_open(ui_console_t *console)
{
  if (console) {
    console->closed      = false;
    console->focus_input = true;
  }
}

void
ui_console_close(ui_console_t *console)
{
  if (console) {
    console->closed = true;
  }
}

bool
ui_console_is_open(ui_console_t *console)
{
  if (!console) {
    return false;
  }
  return !console->closed;
}

static void
ui_console_draw_handle(ui_console_t *console, struct nk_context *ctx)
{
  struct nk_rect               r  = {0};
  enum nk_widget_layout_states ws = nk_widget(&r, ctx);
  if (ws == NK_WIDGET_INVALID) {
    return;
  }

  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
  struct nk_color           idle   = ui_nk_style_item_color_or(ctx->style.selectable.normal_active, ctx->style.text.color);
  struct nk_color           hover  = ui_nk_style_item_color_or(ctx->style.selectable.hover_active, idle);
  struct nk_color           drag   = ui_nk_style_item_color_or(ctx->style.selectable.pressed_active, hover);

  struct nk_color color = idle;
  if (console->drag.dragging) {
    color = drag;
  } else if (console->drag.hovering) {
    color = hover;
  }

  float line_h = 2.0f;
  float line_y = r.y + (r.h - line_h) * 0.5f;

  nk_fill_rect(canvas, nk_rect(r.x, line_y, r.w, line_h), 0.0f, color);
}

static void
ui_console_update_handle(ui_console_t      *console,
                         struct nk_context *ctx,
                         float              viewport_h,
                         float              window_y,
                         float              window_h,
                         struct nk_rect     drag_r)
{
  bool is_mouse_down = nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);
  bool is_hovering   = nk_input_is_mouse_hovering_rect(&ctx->input, drag_r);
  bool is_pressed    = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);

  console->drag.hovering = is_hovering;

  if (!console->drag.dragging && is_hovering && is_pressed) {
    console->drag.dragging      = true;
    console->drag.drag_offset_y = ctx->input.mouse.pos.y - drag_r.y;
  }

  float min_ratio = CONFIG_NK_CONSOLE_MIN_HEIGHT / viewport_h;
  float max_ratio = 1.0f;

  if (console->drag.dragging) {
    if (is_mouse_down) {
      float handle_y  = ctx->input.mouse.pos.y - console->drag.drag_offset_y;
      float console_h = 0.0f;

      if (console->cfg.position == UI_CONSOLE_POSITION_BOTTOM) {
        float handle_offset = drag_r.y - window_y;
        float new_window_y  = handle_y - handle_offset;

        console_h = viewport_h - new_window_y;
      } else {
        float handle_bottom_gap = (window_y + window_h) - (drag_r.y + drag_r.h);

        console_h = handle_y + drag_r.h + handle_bottom_gap - window_y;
      }

      console->drag.height_ratio = console_h / viewport_h;
    } else {
      console->drag.dragging = false;
    }
  }

  if (min_ratio > max_ratio) {
    console->drag.height_ratio = CONFIG_NK_CONSOLE_DEFAULT_HEIGHT_RATIO;
  } else {
    console->drag.height_ratio = NK_CLAMP(console->drag.height_ratio, min_ratio, max_ratio);
  }
}

void
ui_console_compute_row_widths(ui_console_t *console, struct nk_context *ctx)
{
  if (!console->lines) {
    return;
  }

  for (int i = 0; i < console->line_count; ++i) {
    ui_console_line_t *line = &console->lines[i % console->max_lines];
    if (line->row_w == 0.0f) {
      line->row_w = ui_text_width(ctx, line->text);
    }
  }
}

void
ui_console_draw(ui_console_t *console, struct nk_context *ctx, mod_manager_t *mod_manager, unsigned int viewport_width, unsigned int viewport_height)
{
  if (!console->inited || !ctx) {
    return;
  }

  if (!ui_console_is_open(console)) {
    if (!nk_window_is_hidden(ctx, CONFIG_UI_CONSOLE_WINDOW_NAME)) {
      nk_window_show(ctx, CONFIG_UI_CONSOLE_WINDOW_NAME, NK_HIDDEN);
    }
    return;
  }

  float vw = (float)viewport_width;
  float vh = (float)viewport_height;

  nk_window_set_maximize_bounds(ctx, nk_rect(0.0f, 0.0f, vw, vh));

  float console_x = 0.0f;
  float console_w = vw;
  float console_h = vh * console->drag.height_ratio;
  float console_y = (console->cfg.position == UI_CONSOLE_POSITION_TOP) ? 0.0f : vh - console_h;

  nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_CLOSE_BUTTON_HIDES;

  struct nk_rect bounds = nk_rect(console_x, console_y, console_w, console_h);
  if (nk_begin(ctx, CONFIG_UI_CONSOLE_WINDOW_NAME, bounds, flags)) {
    struct nk_rect content = nk_window_get_content_region(ctx);

    ui_console_compute_row_widths(console, ctx);

    float pad_y = ctx->style.window.padding.y;
    float gap_y = ctx->style.window.spacing.y;
    float gap_x = ctx->style.window.spacing.x;

    float handle_h = CONFIG_NK_CONSOLE_HANDLE_HEIGHT;
    float input_h  = CONFIG_NK_CONSOLE_INPUT_HEIGHT;
    float log_h    = content.h - handle_h - input_h - gap_y * 2.0f - pad_y * 2.0f;

    log_h = NK_MAX(log_h, CONFIG_NK_CONSOLE_LINE_HEIGHT);

    float level_w = 90.0f;
    float input_w = content.w - gap_x - level_w;

    input_w = NK_MAX(input_w, 80.0f);

    struct nk_rect handle_r = {0};
    struct nk_rect log_r    = {0};
    struct nk_rect input_r  = {0};
    struct nk_rect level_r  = {0};

    if (console->cfg.position == UI_CONSOLE_POSITION_BOTTOM) {
      handle_r = nk_rect(0, 0, content.w, handle_h);

      log_r    = nk_rect(0, handle_r.y + handle_r.h + gap_y, content.w, log_h);
      input_r  = nk_rect(0, log_r.y + log_r.h + gap_y, input_w, input_h);
      level_r  = nk_rect(input_w + gap_x, log_r.y + log_r.h + gap_y, level_w, input_h);
    } else {
      log_r    = nk_rect(0, 0, content.w, log_h);
      input_r  = nk_rect(0, log_r.y + log_r.h + gap_y, input_w, input_h);
      level_r  = nk_rect(input_w + gap_x, log_r.y + log_r.h + gap_y, level_w, input_h);

      handle_r = nk_rect(0, input_r.y + input_r.h + gap_y, content.w, handle_h);
    }

    nk_layout_space_begin(ctx, NK_STATIC, content.h, 4);
    {
      struct nk_rect handle_r_screen = nk_layout_space_rect_to_screen(ctx, handle_r);

      ui_console_update_handle(console, ctx, vh, console_y, console_h, handle_r_screen);

      nk_layout_space_push(ctx, handle_r);
      ui_console_draw_handle(console, ctx);

      nk_layout_space_push(ctx, log_r);
      ui_console_draw_log_widget(console, ctx);

      nk_layout_space_push(ctx, input_r);
      ui_console_draw_cmd_widget(console, mod_manager, ctx);

      nk_layout_space_push(ctx, level_r);
      ui_console_draw_level_selector(console, ctx, level_r.w);
    }
    nk_layout_space_end(ctx);
  }
  nk_end(ctx);

  if (nk_window_is_hidden(ctx, CONFIG_UI_CONSOLE_WINDOW_NAME)) {
    ui_console_close(console);
  }
}
