#include "ui_nuklear.h"

#include "globals.h"
#include "scratch.h"

static inline float
ui_nk_axis_pos(struct nk_vec2 p, ui_nk_axis_t axis)
{
  return axis == UI_NK_AXIS_X ? p.x : p.y;
}

static inline float
ui_nk_rect_pos(struct nk_rect r, ui_nk_axis_t axis)
{
  return axis == UI_NK_AXIS_X ? r.x : r.y;
}

static inline float
ui_nk_rect_size(struct nk_rect r, ui_nk_axis_t axis)
{
  return axis == UI_NK_AXIS_X ? r.w : r.h;
}

bool
ui_nk_current_panel_accepts_input(struct nk_context *ctx)
{
  if (!ctx || !ctx->current || !ctx->current->layout) {
    return false;
  }

  if (ctx->active == ctx->current) {
    return true;
  }

  if ((int)ctx->current->layout->type & (int)NK_PANEL_SET_POPUP) {
    return true;
  }

  return false;
}

bool
ui_nk_item_begin(struct nk_context *ctx, ui_nk_item_t *item)
{
  NK_ASSERT(ctx != NULL);
  NK_ASSERT(item != NULL);

  if (!ctx || !item || !ctx->current || !ctx->current->layout) {
    return false;
  }

  item->bounds       = nk_rect(0, 0, 0, 0);
  item->layout_state = nk_widget(&item->bounds, ctx);
  item->in           = NULL;
  item->widget_state = 0;

  if (item->layout_state == NK_WIDGET_INVALID) {
    return false;
  }

  // if (item->layout_state != NK_WIDGET_ROM && item->layout_state != NK_WIDGET_DISABLED &&
  //     !(ctx->current->layout->flags & NK_WINDOW_ROM) && !(ctx->current->layout->flags & NK_WINDOW_NO_INPUT)) {
  //   item->in = &ctx->input;
  // }

  if (item->layout_state != NK_WIDGET_ROM && item->layout_state != NK_WIDGET_DISABLED && !(ctx->current->layout->flags & NK_WINDOW_ROM) &&
      !(ctx->current->layout->flags & NK_WINDOW_NO_INPUT) && ui_nk_current_panel_accepts_input(ctx)) {
    item->in = &ctx->input;
  }

  return true;
}

bool
ui_nk_item_behavior(struct nk_context *ctx, ui_nk_item_t *item)
{
  NK_ASSERT(ctx != NULL);
  NK_ASSERT(item != NULL);

  bool clicked = false;
  if (item->in) {
    item->widget_state     = 0;
    clicked                = nk_button_behavior(&item->widget_state, item->bounds, item->in, NK_BUTTON_DEFAULT);
    ctx->last_widget_state = item->widget_state;
  }
  return clicked;
}

bool
ui_nk_selectable_behavior(struct nk_context *ctx, ui_nk_item_t *item)
{
  NK_ASSERT(ctx != NULL);
  NK_ASSERT(item != NULL);

  bool clicked = false;
  if (item->in) {
    struct nk_style_selectable *style = &ctx->style.selectable;
    struct nk_rect              touch = item->bounds;

    touch.x -= style->touch_padding.x;
    touch.y -= style->touch_padding.y;
    touch.w += style->touch_padding.x * 2.0f;
    touch.h += style->touch_padding.y * 2.0f;

    item->widget_state     = 0;
    clicked                = nk_button_behavior(&item->widget_state, touch, item->in, NK_BUTTON_DEFAULT);
    ctx->last_widget_state = item->widget_state;
  }
  return clicked;
}

struct nk_color
ui_nk_select_bg(struct nk_context *ctx, nk_flags state, bool selected)
{
  struct nk_style_selectable *s = &ctx->style.selectable;

  if (selected) {
    if (state & NK_WIDGET_STATE_ACTIVE) {
      return ui_nk_style_item_color_or(s->pressed_active, ctx->style.window.background);
    }

    if (state & NK_WIDGET_STATE_HOVER) {
      return ui_nk_style_item_color_or(s->hover_active, ctx->style.window.background);
    }

    return ui_nk_style_item_color_or(s->normal_active, ctx->style.window.background);
  }

  if (state & NK_WIDGET_STATE_ACTIVE) {
    return ui_nk_style_item_color_or(s->pressed, ctx->style.window.background);
  }

  if (state & NK_WIDGET_STATE_HOVER) {
    return ui_nk_style_item_color_or(s->hover, ctx->style.window.background);
  }

  return ui_nk_style_item_color_or(s->normal, ctx->style.window.background);
}

struct nk_color
ui_nk_select_fg(struct nk_context *ctx, nk_flags state, bool selected)
{
  struct nk_style_selectable *s = &ctx->style.selectable;

  if (selected) {
    if (state & NK_WIDGET_STATE_ACTIVE) {
      return s->text_pressed_active;
    }

    if (state & NK_WIDGET_STATE_HOVER) {
      return s->text_hover_active;
    }

    return s->text_normal_active;
  }

  if (state & NK_WIDGET_STATE_ACTIVE) {
    return s->text_pressed;
  }

  if (state & NK_WIDGET_STATE_HOVER) {
    return s->text_hover;
  }

  return s->text_normal;
}

float
ui_text_width(struct nk_context *ctx, str_t text)
{
  if (!ctx || !ctx->style.font || !ctx->style.font->width || str_is_empty(text)) {
    return 0.0f;
  }

  return ctx->style.font->width(ctx->style.font->userdata, ctx->style.font->height, (const char *)text.data, (int)text.len);
}

bool
ui_text_fits_width(struct nk_context *ctx, str_t text, float max_w)
{
  if (!ctx || str_is_empty(text) || max_w <= 1.0f) {
    return true;
  }

  return (ui_text_width(ctx, text) <= max_w);
}

void
ui_text_draw(struct nk_context *ctx, struct nk_command_buffer *out, struct nk_rect bounds, str_t text, ui_text_opts_t opts)
{
  if (!ctx || !out || !ctx->style.font || str_is_empty(text) || bounds.w <= 0.0f || bounds.h <= 0.0f) {
    return;
  }

  nk_flags a = opts.text_alignment;

  if (!(a & (NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_CENTERED | NK_TEXT_ALIGN_RIGHT))) {
    a |= NK_TEXT_ALIGN_LEFT;
  }

  if (!(a & (NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_BOTTOM))) {
    a |= NK_TEXT_ALIGN_TOP;
  }

  float inner_x = bounds.x + opts.pad.x;
  float inner_y = bounds.y + opts.pad.y;
  float inner_w = bounds.w - 2.0f * opts.pad.x;
  float inner_h = bounds.h - 2.0f * opts.pad.y;

  if (inner_w <= 0.0f || inner_h <= 0.0f) {
    return;
  }

  float font_h = ctx->style.font->height;
  float text_w = opts.text_width;
  if ((a & (NK_TEXT_ALIGN_CENTERED | NK_TEXT_ALIGN_RIGHT)) && text_w <= 0.0f) {
    text_w = ui_text_width(ctx, text);
  }

  struct nk_rect label = {0};

  if (a & NK_TEXT_ALIGN_LEFT) {
    label.x = inner_x;
    label.w = inner_w;
  } else {
    float used_w = NK_MIN(text_w, inner_w);

    if (a & NK_TEXT_ALIGN_CENTERED) {
      label.x = inner_x + (inner_w - used_w) * 0.5f;
      label.w = used_w;
    } else { /* NK_TEXT_ALIGN_RIGHT */
      label.x = inner_x + inner_w - used_w;
      label.w = used_w;
    }
  }

  if (a & NK_TEXT_ALIGN_TOP) {
    label.y = inner_y;
    label.h = inner_h;
  } else {
    float used_h = NK_MIN(font_h, inner_h);

    if (a & NK_TEXT_ALIGN_MIDDLE) {
      label.y = inner_y + (inner_h - used_h) * 0.5f;
      label.h = used_h;
    } else { /* NK_TEXT_ALIGN_BOTTOM */
      label.y = inner_y + inner_h - used_h;
      label.h = used_h;
    }
  }

  if (label.w <= 0.0f || label.h <= 0.0f) {
    return;
  }

  if (out->use_clipping) {
    const struct nk_rect *clip = &out->clip;

    if (clip->w == 0.0f || clip->h == 0.0f || !NK_INTERSECT(label.x, label.y, label.w, label.h, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  struct nk_command_text *cmd = (struct nk_command_text *)nk_command_buffer_push(out, NK_COMMAND_TEXT, sizeof(*cmd) + (nk_size)text.len + 1);
  if (!cmd) {
    return;
  }

  cmd->x          = (short)label.x;
  cmd->y          = (short)label.y;
  cmd->w          = (unsigned short)NK_MAX(0.0f, label.w);
  cmd->h          = (unsigned short)NK_MAX(0.0f, label.h);
  cmd->background = opts.bg;
  cmd->foreground = opts.fg;
  cmd->font       = ctx->style.font;
  cmd->length     = (int)text.len;
  cmd->height     = ctx->style.font->height;

  NK_MEMCPY(cmd->string, text.data, (nk_size)text.len);
  cmd->string[text.len] = 0;
}

void
ui_text(struct nk_context *ctx, str_t text, float text_width, nk_flags alignment, struct nk_color color)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  struct nk_rect               bounds = {0};
  enum nk_widget_layout_states state  = nk_widget(&bounds, ctx);
  if (state == NK_WIDGET_INVALID) {
    return;
  }

  const struct nk_style *style = &ctx->style;

  ui_text_opts_t text_opts = {
    .text_width     = text_width,
    .text_alignment = alignment,
    .pad            = style->text.padding,
    .bg             = style->window.background,
    .fg             = nk_rgb_factor(color, style->text.color_factor),
  };

  ui_text_draw(ctx, &ctx->current->buffer, bounds, text, text_opts);
}

bool
ui_select_row(struct nk_context *ctx, ui_nk_select_row_opts_t *opts)
{
  ui_nk_item_t item = {0};

  if (!ctx || !opts) {
    return false;
  }

  if (!ui_nk_item_begin(ctx, &item)) {
    return false;
  }

  bool clicked = ui_nk_item_behavior(ctx, &item);

  struct nk_command_buffer *out = nk_window_get_canvas(ctx);
  struct nk_color           bg  = ui_nk_select_bg(ctx, item.widget_state, opts->selected);
  struct nk_color           fg  = ui_nk_select_fg(ctx, item.widget_state, opts->selected);

  float pad_x = opts->pad_x > 0.0f ? opts->pad_x : 8.0f;
  float dot_d = opts->dot_d > 0.0f ? opts->dot_d : 8.0f;

  nk_fill_rect(out, item.bounds, ctx->style.selectable.rounding, bg);

  float x      = item.bounds.x + pad_x;
  float text_w = item.bounds.w - pad_x * 2.0f;

  if (opts->show_dot) {
    struct nk_rect dot = nk_rect(x, item.bounds.y + (item.bounds.h - dot_d) * 0.5f, dot_d, dot_d);

    nk_fill_circle(out, dot, opts->dot_color);

    x      += dot_d + 6.0f;
    text_w -= dot_d + 6.0f;
  }

  if (!str_is_empty(opts->text)) {
    ui_text_opts_t text_opts = {
      .text_alignment = NK_TEXT_ALIGN_MIDDLE,
      .bg             = bg,
      .fg             = fg,
    };
    ui_text_draw(ctx, out, nk_rect(x, item.bounds.y, text_w, item.bounds.h), opts->text, text_opts);
  }

  return clicked;
}

bool
ui_splitter(struct nk_context *ctx, ui_nk_splitter_state_t *state, ui_nk_splitter_opts_t *opts)
{
  NK_ASSERT(ctx != NULL);
  NK_ASSERT(state != NULL);
  NK_ASSERT(opts != NULL);

  if (!ctx || !state || !opts) {
    return false;
  }

  if (state->ratio <= 0.0f) {
    state->ratio = 0.5f;
  }

  ui_nk_item_t item = {0};
  if (!ui_nk_item_begin(ctx, &item)) {
    return false;
  }

  bool changed = false;

  struct nk_input *in = NULL;
  if (!opts->disabled) {
    in = item.in;
  } else {
    state->dragging = false;
  }

  ui_nk_item_behavior(ctx, &item);

  state->hovering = (item.widget_state & NK_WIDGET_STATE_HOVER) != 0;
  if (!state->dragging && in) {
    bool pressed_in_splitter = nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT) && nk_input_has_mouse_click_in_button_rect(in, NK_BUTTON_LEFT, item.bounds);

    if (pressed_in_splitter) {
      state->dragging    = true;
      state->drag_offset = ui_nk_axis_pos(in->mouse.pos, opts->axis) - ui_nk_rect_pos(item.bounds, opts->axis);
    }
  }

  if (state->dragging) {
    const struct nk_input *drag_in = &ctx->input;

    if (nk_input_is_mouse_down(drag_in, NK_BUTTON_LEFT)) {
      float mouse_pos    = ui_nk_axis_pos(drag_in->mouse.pos, opts->axis);
      float track_pos    = ui_nk_rect_pos(opts->track, opts->axis);
      float track_size   = ui_nk_rect_size(opts->track, opts->axis);
      float splitter_pos = mouse_pos - state->drag_offset;
      float before_size  = splitter_pos - track_pos - opts->gap_before;

      if (track_size > 1.0f) {
        float old_ratio = state->ratio;
        state->ratio    = before_size / track_size;
        changed         = changed || old_ratio != state->ratio;
      }
    } else {
      state->dragging = false;
    }
  }

  {
    float track_size = ui_nk_rect_size(opts->track, opts->axis);
    float min_ratio  = opts->min_before / NK_MAX(track_size, 1.0f);
    float max_ratio  = 1.0f - opts->min_after / NK_MAX(track_size, 1.0f);

    if (min_ratio > max_ratio) {
      state->ratio = 0.5f;
    } else {
      state->ratio = NK_CLAMP(min_ratio, state->ratio, max_ratio);
    }
  }

  struct nk_command_buffer *out = nk_window_get_canvas(ctx);

  struct nk_color idle  = ui_nk_style_item_color_or(ctx->style.selectable.normal_active, ctx->style.text.color);
  struct nk_color hover = ui_nk_style_item_color_or(ctx->style.selectable.hover_active, idle);
  struct nk_color drag  = ui_nk_style_item_color_or(ctx->style.selectable.pressed_active, hover);

  struct nk_color color = idle;
  if (state->dragging) {
    color = drag;
  } else if (state->hovering) {
    color = hover;
  }

  float          thickness = opts->line_thickness > 0.0f ? opts->line_thickness : 2.0f;
  struct nk_rect line      = item.bounds;

  if (opts->axis == UI_NK_AXIS_X) {
    line.x += (line.w - thickness) * 0.5f;
    line.w  = thickness;
  } else {
    line.y += (line.h - thickness) * 0.5f;
    line.h  = thickness;
  }

  nk_fill_rect(out, line, 0.0f, color);

  return changed;
}

static struct nk_rect
ui_rect_union(struct nk_rect a, struct nk_rect b)
{
  float x0 = NK_MIN(a.x, b.x);
  float y0 = NK_MIN(a.y, b.y);
  float x1 = NK_MAX(a.x + a.w, b.x + b.w);
  float y1 = NK_MAX(a.y + a.h, b.y + b.h);

  return nk_rect(x0, y0, x1 - x0, y1 - y0);
}

static uint64_t
ui_wrap_find_eol(str_t text)
{
  for (uint64_t i = 0; i < text.len; ++i) {
    if (text.data[i] == '\n' || text.data[i] == '\r') {
      return i;
    }
  }
  return (int)text.len;
}

static int
ui_wrap_skip_eol(str_t text)
{
  if (str_is_empty(text)) {
    return 0;
  }

  if (text.data[0] == '\r') {
    if (text.len > 1 && text.data[1] == '\n') {
      return 2;
    }
    return 1;
  }

  if (text.data[0] == '\n') {
    return 1;
  }

  return 0;
}

static nk_bool
ui_is_soft_break(nk_rune rune)
{
  return rune == ' ' || rune == '\t';
}

static int
ui_wrap_fit_line(struct nk_user_font *font, str_t text, float space, int *advance_bytes)
{
  NK_ASSERT(font != NULL);
  NK_ASSERT(advance_bytes != NULL);

  int last_break_draw    = -1; // draw up to here
  int last_break_advance = -1; // resume from here

  const char *data = (const char *)text.data;
  int         len  = (int)text.len;
  int         at   = 0;

  while (at < len) {
    nk_rune rune = 0;
    int     step = nk_utf_decode(data + at, &rune, len - at);
    if (step <= 0) {
      break;
    }

    float next_w = font->width(font->userdata, font->height, data, at + step);
    if (next_w > space) {
      if (last_break_draw >= 0) {
        *advance_bytes = last_break_advance;
        return last_break_draw;
      }

      *advance_bytes = (at > 0) ? at : step;
      return (at > 0) ? at : step;
    }

    if (ui_is_soft_break(rune)) {
      last_break_draw    = at;
      last_break_advance = at + step;
    }

    at += step;
  }

  *advance_bytes = at;
  return at;
}

static void
ui_wrap_text_draw(struct nk_context *ctx, struct nk_command_buffer *out, struct nk_rect bounds, struct nk_color color, str_t text)
{
  if (!ctx || !out || str_is_empty(text)) {
    return;
  }

  struct nk_vec2       padding = ctx->style.text.padding;
  struct nk_user_font *font    = (struct nk_user_font *)ctx->style.font;
  if (!font) {
    return;
  }

  float line_h = font->height + 2.0f * padding.y;
  float wrap_w = bounds.w - 2.0f * padding.x;
  wrap_w       = NK_MAX(wrap_w, 1.0f);

  struct nk_text line_style = {
    .padding    = nk_vec2(0.0f, 0.0f),
    .background = ctx->style.window.background,
    .text       = color,
  };

  struct nk_rect line_bounds = {
    .x = bounds.x + padding.x,
    .y = bounds.y + padding.y,
    .w = wrap_w,
    .h = line_h,
  };

  uint8_t *it        = text.data;
  int      remaining = (int)text.len;

  while (remaining > 0) {
    int logical_len = (int)ui_wrap_find_eol(str_make(it, remaining));

    if (logical_len == 0) {
      if (line_bounds.y + line_bounds.h > bounds.y + bounds.h) {
        break;
      }

      {
        int skip   = ui_wrap_skip_eol(str_make(it + logical_len, remaining - logical_len));
        it        += logical_len + skip;
        remaining -= logical_len + skip;
      }

      line_bounds.y += line_h;
      continue;
    }

    {
      int consumed = 0;

      while (consumed < logical_len) {
        int draw_bytes    = 0;
        int advance_bytes = 0;

        if (line_bounds.y + line_bounds.h > bounds.y + bounds.h) {
          return;
        }

        draw_bytes = ui_wrap_fit_line(font, str_make(it + consumed, logical_len - consumed), wrap_w, &advance_bytes);
        if (advance_bytes <= 0) {
          return;
        }

        if (draw_bytes > 0) {
          nk_widget_text(out, line_bounds, (const char *)(it + consumed), draw_bytes, &line_style, NK_TEXT_LEFT, font);
        }

        consumed      += advance_bytes;
        line_bounds.y += line_h;
      }
    }

    {
      int skip   = ui_wrap_skip_eol(str_make(it + logical_len, remaining - logical_len));
      it        += logical_len + skip;
      remaining -= logical_len + skip;
    }
  }
}

static float
ui_text_wrap_height(struct nk_context *ctx, float bounds_w, str_t text)
{
  if (!ctx) {
    return 0.0f;
  }

  struct nk_vec2       padding = ctx->style.text.padding;
  struct nk_user_font *font    = (struct nk_user_font *)ctx->style.font;
  if (!font) {
    return 0.0f;
  }

  float line_h = font->height + 2.0f * padding.y;
  float wrap_w = bounds_w - 2.0f * padding.x;

  wrap_w = NK_MAX(wrap_w, 1.0f);

  if (str_is_empty(text)) {
    return line_h;
  }

  uint8_t *it         = text.data;
  int      remaining  = (int)text.len;
  int      line_count = 0;
  while (remaining > 0) {
    int logical_len = (int)ui_wrap_find_eol(str_make(it, remaining));

    if (logical_len == 0) {
      line_count += 1;
    } else {
      int consumed = 0;
      while (consumed < logical_len) {
        int advance_bytes = 0;
        ui_wrap_fit_line(font, str_make(it + consumed, logical_len - consumed), wrap_w, &advance_bytes);

        if (advance_bytes <= 0) {
          break;
        }

        consumed   += advance_bytes;
        line_count += 1;
      }
    }

    {
      int skip   = ui_wrap_skip_eol(str_make(it + logical_len, remaining - logical_len));
      it        += logical_len + skip;
      remaining -= logical_len + skip;
    }
  }

  if (line_count <= 0) {
    line_count = 1;
  }

  return line_h * (float)line_count;
}

static float
ui_layout_dynamic_item_width(struct nk_context *ctx, int cols)
{
  NK_ASSERT(ctx != NULL);
  NK_ASSERT(ctx->current != NULL);
  NK_ASSERT(ctx->current->layout != NULL);
  NK_ASSERT(cols > 0);

  struct nk_panel *layout   = ctx->current->layout;
  float            usable_w = nk_layout_row_calculate_usable_space(&ctx->style, layout->type, layout->bounds.w, cols);
  return NK_MAX(1.0f, usable_w) / (float)cols;
}

float
ui_label_wrap(struct nk_context *ctx, str_t text)
{
  NK_ASSERT(ctx != NULL);

  float h = ui_text_wrap_height(ctx, ui_layout_dynamic_item_width(ctx, 1), text);
  nk_layout_row_dynamic(ctx, h, 1);

  struct nk_rect               bounds = {0};
  struct nk_color              color  = nk_rgb_factor(ctx->style.text.color, ctx->style.text.color_factor);
  enum nk_widget_layout_states ws     = nk_widget(&bounds, ctx);
  if (ws != NK_WIDGET_INVALID) {
    ui_wrap_text_draw(ctx, nk_window_get_canvas(ctx), bounds, color, text);
  }
  return h;
}

float
ui_label_wrap_colored(struct nk_context *ctx, str_t text, struct nk_color color)
{
  NK_ASSERT(ctx != NULL);

  float h = ui_text_wrap_height(ctx, ui_layout_dynamic_item_width(ctx, 1), text);
  nk_layout_row_dynamic(ctx, h, 1);

  struct nk_rect               bounds = {0};
  enum nk_widget_layout_states ws     = nk_widget(&bounds, ctx);
  if (ws != NK_WIDGET_INVALID) {
    ui_wrap_text_draw(ctx, nk_window_get_canvas(ctx), bounds, color, text);
  }
  return h;
}

void
ui_win_clamp_bounds(struct nk_context *ctx, const char *name, struct nk_vec2 min_size, struct nk_vec2 max_size)
{
  ASSERT(ctx != NULL);

  struct nk_window *win = nk_window_find(ctx, name);
  if (win) {
    win->bounds.w = NK_CLAMP(min_size.x, win->bounds.w, max_size.x);
    win->bounds.h = NK_CLAMP(min_size.y, win->bounds.h, max_size.y);
  }
}

void
ui_win_make_centered(struct nk_context *ctx, const char *name, unsigned int vw, unsigned int vh)
{
  ASSERT(ctx != NULL);

  struct nk_window *win = nk_window_find(ctx, name);
  if (win) {
    win->bounds.x = ((float)vw - win->bounds.w) * 0.5f;
    win->bounds.y = ((float)vh - win->bounds.h) * 0.5f;
  }
}

bool
ui_tree_push(struct nk_context *ctx, enum nk_tree_type type, str_t title, bool maximized)
{
  enum nk_collapse_states nk_state = (enum nk_collapse_states)maximized;
  return nk_tree_push_text(ctx, type, (const char *)title.data, (int)title.len, nk_state);
}

bool
ui_tree_state_push(struct nk_context *ctx, enum nk_tree_type type, str_t title, bool *state)
{
  enum nk_collapse_states nk_state = (enum nk_collapse_states) * state;
  bool                    result   = nk_tree_state_push_text(ctx, type, (const char *)title.data, (int)title.len, &nk_state);
  *state                           = (bool)nk_state;
  return result;
}

ui_text_span_t
ui_text_span_make(struct nk_context *ctx, str_t str)
{
  return (ui_text_span_t){
    .str   = str,
    .width = ui_text_width(ctx, str),
  };
}

void
ui_text_span_align(ui_text_span_t *spans, int count)
{
  float max_width = 0.0f;
  for (int i = 0; i < count; ++i) {
    max_width = MAX_VAL(max_width, spans[i].width);
  }

  for (int i = 0; i < count; ++i) {
    spans[i].width = max_width;
  }
}

void
ui_text_cell_draw(struct nk_context *ctx, struct nk_command_buffer *out, struct nk_rect bounds, ui_text_cell_t cell, struct nk_color bg)
{
  ASSERT(ctx != NULL);
  ASSERT(out != NULL);

  ui_text_opts_t text_opts = {
    .text_width     = cell.text.width,
    .text_alignment = cell.align,
    .bg             = bg,
    .fg             = cell.fg,
  };
  ui_text_draw(ctx, out, bounds, cell.text.str, text_opts);
}

void
ui_text_cell(struct nk_context *ctx, ui_text_cell_t cell)
{
  ASSERT(ctx != NULL);
  ui_text(ctx, cell.text.str, cell.text.width, cell.align, cell.fg);
}
