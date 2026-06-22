#include "ui_manager.h"
#include "input.h"
#include "scratch.h"
#include "str.h"
#include "ui_console.h"
#include "ui_mod_manager.h"
#include "ui_nuklear.h"

#include "vendor_nuklear.h"

#include <windows.h>

static void
set_style(ui_manager_t *manager, struct nk_context *ctx)
{
  nk_style_default(ctx);

  manager->color_table[NK_COLOR_TEXT]                    = UI_C_TEXT;
  manager->color_table[NK_COLOR_WINDOW]                  = UI_C_BG_WINDOW;
  manager->color_table[NK_COLOR_HEADER]                  = UI_C_HEADER;
  manager->color_table[NK_COLOR_BORDER]                  = UI_C_BORDER;
  manager->color_table[NK_COLOR_BUTTON]                  = UI_C_CONTROL;
  manager->color_table[NK_COLOR_BUTTON_HOVER]            = UI_C_CONTROL_HOVER;
  manager->color_table[NK_COLOR_BUTTON_ACTIVE]           = UI_C_CONTROL_ACTIVE;
  manager->color_table[NK_COLOR_TOGGLE]                  = UI_C_CONTROL;
  manager->color_table[NK_COLOR_TOGGLE_HOVER]            = UI_C_CONTROL_HOVER;
  manager->color_table[NK_COLOR_TOGGLE_CURSOR]           = UI_C_CONTROL_ACTIVE;
  manager->color_table[NK_COLOR_SELECT]                  = UI_C_TRANSPARENT;
  manager->color_table[NK_COLOR_SELECT_ACTIVE]           = UI_C_SELECT;
  manager->color_table[NK_COLOR_SLIDER]                  = UI_C_CONTROL;
  manager->color_table[NK_COLOR_SLIDER_CURSOR]           = UI_C_ACCENT;
  manager->color_table[NK_COLOR_SLIDER_CURSOR_HOVER]     = UI_C_ACCENT_HOVER;
  manager->color_table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = UI_C_ACCENT_ACTIVE;
  manager->color_table[NK_COLOR_PROPERTY]                = UI_C_CONTROL;
  manager->color_table[NK_COLOR_EDIT]                    = UI_C_EDIT;
  manager->color_table[NK_COLOR_EDIT_CURSOR]             = UI_C_ACCENT;
  manager->color_table[NK_COLOR_COMBO]                   = UI_C_CONTROL;
  manager->color_table[NK_COLOR_CHART]                   = UI_C_BG_PANEL;
  manager->color_table[NK_COLOR_CHART_COLOR]             = UI_C_ACCENT;
  manager->color_table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = UI_C_ACCENT_ACTIVE;
  manager->color_table[NK_COLOR_SCROLLBAR]               = UI_C_SCROLLBAR;
  manager->color_table[NK_COLOR_SCROLLBAR_CURSOR]        = UI_C_SCROLLBAR_CURSOR;
  manager->color_table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = UI_C_SCROLLBAR_CURSOR_HOVER;
  manager->color_table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = UI_C_SCROLLBAR_ACTIVE;
  manager->color_table[NK_COLOR_TAB_HEADER]              = UI_C_BG_PANEL;

  nk_style_from_table(ctx, manager->color_table);

  ctx->style.button.rounding          = 4.0f;
  ctx->style.selectable.rounding      = 4.0f;
  ctx->style.edit.rounding            = 4.0f;
  ctx->style.property.rounding        = 4.0f;
  ctx->style.combo.rounding           = 4.0f;
  ctx->style.progress.rounding        = 4.0f;
  ctx->style.progress.cursor_rounding = 2.0f;
  ctx->style.scrollh.rounding         = 2.0f;
  ctx->style.scrollh.rounding_cursor  = 2.0f;
  ctx->style.tab.rounding             = 4.0f;

  /* Window */
  ctx->style.window.group_border_color   = UI_C_BORDER_SOFT;
  ctx->style.window.tooltip_border_color = UI_C_BORDER_FOCUS;
  ctx->style.window.group_border         = 0.0f;
  ctx->style.window.border               = 1.0f;
  ctx->style.window.scrollbar_size       = nk_vec2(14.0f, 14.0f);

  /* Window header */
  ctx->style.window.header.normal = UI_ITEM(UI_C_HEADER);
  ctx->style.window.header.hover  = UI_ITEM(UI_C_HEADER_HOVER);
  ctx->style.window.header.active = UI_ITEM(UI_C_HEADER_ACTIVE);

  ctx->style.window.header.minimize_button.normal = UI_ITEM(UI_C_HEADER);
  ctx->style.window.header.close_button.normal    = UI_ITEM(UI_C_HEADER);

  /* Selectable */
  ctx->style.selectable.normal         = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.selectable.hover          = UI_ITEM(UI_C_BG_HOVER);
  ctx->style.selectable.pressed        = UI_ITEM(UI_C_SELECT);
  ctx->style.selectable.normal_active  = UI_ITEM(UI_C_SELECT);
  ctx->style.selectable.hover_active   = UI_ITEM(UI_C_SELECT_HOVER);
  ctx->style.selectable.pressed_active = UI_ITEM(UI_C_SELECT_ACTIVE);

  /* Edit */
  ctx->style.edit.normal             = UI_ITEM(UI_C_EDIT);
  ctx->style.edit.hover              = UI_ITEM(UI_C_EDIT_HOVER);
  ctx->style.edit.active             = UI_ITEM(UI_C_EDIT_ACTIVE);
  ctx->style.edit.border_color       = UI_C_BORDER_SOFT;
  ctx->style.edit.cursor_normal      = UI_C_ACCENT;
  ctx->style.edit.cursor_hover       = UI_C_ACCENT_HOVER;
  ctx->style.edit.cursor_text_normal = UI_C_BG_WINDOW;
  ctx->style.edit.cursor_text_hover  = UI_C_BG_WINDOW;
  ctx->style.edit.selected_normal    = UI_C_SELECT_ACTIVE;
  ctx->style.edit.selected_hover     = UI_C_ACCENT_ACTIVE;

  /* Button */
#if 0
  ctx->style.button.normal          = UI_ITEM(UI_C_ACCENT);
  ctx->style.button.hover           = UI_ITEM(UI_C_ACCENT_HOVER);
  ctx->style.button.active          = UI_ITEM(UI_C_ACCENT_ACTIVE);
  ctx->style.button.border_color    = UI_C_BORDER_FOCUS;
  ctx->style.button.text_background = UI_C_TRANSPARENT;
#endif
  ctx->style.button.normal          = UI_ITEM(UI_C_CONTROL);
  ctx->style.button.hover           = UI_ITEM(UI_C_CONTROL_HOVER);
  ctx->style.button.active          = UI_ITEM(UI_C_CONTROL_ACTIVE);
  ctx->style.button.border_color    = UI_C_BORDER_SOFT;
  ctx->style.button.text_normal     = UI_C_TEXT;
  ctx->style.button.text_hover      = UI_C_TEXT;
  ctx->style.button.text_active     = UI_C_TEXT;
  ctx->style.button.text_background = UI_C_TRANSPARENT;

  /* Tab/Node button */
  ctx->style.tab.tab_maximize_button.normal        = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.tab.tab_maximize_button.border_color  = UI_C_BORDER_SOFT;
  ctx->style.tab.tab_minimize_button.normal        = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.tab.tab_minimize_button.border_color  = UI_C_BORDER_SOFT;
  ctx->style.tab.node_maximize_button.normal       = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.tab.node_maximize_button.border_color = UI_C_BORDER_SOFT;
  ctx->style.tab.node_minimize_button.normal       = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.tab.node_minimize_button.border_color = UI_C_BORDER_SOFT;

  /* Tab / tree */
  ctx->style.tab.background   = UI_ITEM(UI_C_BG_PANEL);
  ctx->style.tab.border_color = UI_C_BORDER_SOFT;
  ctx->style.tab.indent       = 12.0f;

  /* Chart */
  ctx->style.chart.background     = UI_ITEM(UI_C_BG_PANEL);
  ctx->style.chart.border_color   = UI_C_BORDER_SOFT;
  ctx->style.chart.selected_color = UI_C_ACCENT_ACTIVE;
  ctx->style.chart.color          = UI_C_ACCENT;

  /* Checkbox / option */
  ctx->style.checkbox.normal          = UI_ITEM(UI_C_CONTROL);
  ctx->style.checkbox.hover           = UI_ITEM(UI_C_CONTROL_HOVER);
  ctx->style.checkbox.active          = UI_ITEM(UI_C_CONTROL_ACTIVE);
  ctx->style.checkbox.cursor_normal   = UI_ITEM(UI_C_ACCENT);
  ctx->style.checkbox.cursor_hover    = UI_ITEM(UI_C_ACCENT_HOVER);
  ctx->style.checkbox.border_color    = UI_C_BORDER_SOFT;
  ctx->style.checkbox.text_background = UI_C_TRANSPARENT;
  ctx->style.checkbox.padding         = nk_vec2(2.0f, 2.0f);
  ctx->style.checkbox.border          = 1.0f;

  ctx->style.option = ctx->style.checkbox;

  /* Slider */
  ctx->style.slider.normal        = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.slider.hover         = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.slider.active        = UI_ITEM(UI_C_TRANSPARENT);
  ctx->style.slider.bar_normal    = UI_C_BG_PANEL_ALT;
  ctx->style.slider.bar_hover     = UI_C_BG_HOVER;
  ctx->style.slider.bar_active    = UI_C_BG_ACTIVE;
  ctx->style.slider.bar_filled    = UI_C_ACCENT;
  ctx->style.slider.cursor_normal = UI_ITEM(UI_C_ACCENT);
  ctx->style.slider.cursor_hover  = UI_ITEM(UI_C_ACCENT_HOVER);
  ctx->style.slider.cursor_active = UI_ITEM(UI_C_ACCENT_ACTIVE);
  ctx->style.slider.bar_height    = 6.0f;

  /* Progress */
  ctx->style.progress.normal              = UI_ITEM(UI_C_CONTROL);
  ctx->style.progress.hover               = UI_ITEM(UI_C_CONTROL);
  ctx->style.progress.active              = UI_ITEM(UI_C_CONTROL);
  ctx->style.progress.cursor_normal       = UI_ITEM(UI_C_ACCENT);
  ctx->style.progress.cursor_hover        = UI_ITEM(UI_C_ACCENT_HOVER);
  ctx->style.progress.cursor_active       = UI_ITEM(UI_C_ACCENT_ACTIVE);
  ctx->style.progress.border_color        = UI_C_BORDER_SOFT;
  ctx->style.progress.cursor_border_color = UI_C_ACCENT_ACTIVE;
  ctx->style.progress.padding             = nk_vec2(2.0f, 2.0f);
  ctx->style.progress.border              = 1.0f;

#if 0
  /* Scrollbar */
  ctx->style.scrollh.normal              = UI_ITEM(nk_rgba(22, 27, 34, 140));
  ctx->style.scrollh.hover               = UI_ITEM(nk_rgba(30, 37, 46, 190));
  ctx->style.scrollh.active              = UI_ITEM(nk_rgba(35, 43, 53, 220));
  ctx->style.scrollh.cursor_normal       = UI_ITEM(nk_rgba(83, 100, 117, 220));
  ctx->style.scrollh.cursor_hover        = UI_ITEM(nk_rgba(98, 119, 138, 235));
  ctx->style.scrollh.cursor_active       = UI_ITEM(UI_C_ACCENT_ACTIVE);
#endif
  ctx->style.scrollh.normal              = UI_ITEM(UI_C_SCROLLBAR);
  ctx->style.scrollh.hover               = UI_ITEM(UI_C_SCROLLBAR_HOVER);
  ctx->style.scrollh.active              = UI_ITEM(UI_C_SCROLLBAR_ACTIVE);
  ctx->style.scrollh.cursor_normal       = UI_ITEM(UI_C_SCROLLBAR_CURSOR);
  ctx->style.scrollh.cursor_hover        = UI_ITEM(UI_C_SCROLLBAR_CURSOR_HOVER);
  ctx->style.scrollh.cursor_active       = UI_ITEM(UI_C_ACCENT_ACTIVE);
  ctx->style.scrollh.border_color        = UI_C_TRANSPARENT;
  ctx->style.scrollh.cursor_border_color = UI_C_BORDER_SOFT;
  ctx->style.scrollh.padding             = nk_vec2(1.0f, 1.0f);
  ctx->style.scrollh.cursor_min_size     = ctx->style.window.scrollbar_size.x;

  ctx->style.scrollv = ctx->style.scrollh;

  /* Edit */
  ctx->style.edit.normal               = UI_ITEM(UI_C_EDIT);
  ctx->style.edit.hover                = UI_ITEM(UI_C_EDIT_HOVER);
  ctx->style.edit.active               = UI_ITEM(UI_C_EDIT_ACTIVE);
  ctx->style.edit.border_color         = UI_C_BORDER_SOFT;
  ctx->style.edit.cursor_normal        = UI_C_ACCENT;
  ctx->style.edit.cursor_hover         = UI_C_ACCENT_HOVER;
  ctx->style.edit.text_normal          = UI_C_TEXT;
  ctx->style.edit.text_hover           = UI_C_TEXT;
  ctx->style.edit.text_active          = UI_C_TEXT;
  ctx->style.edit.selected_normal      = UI_C_SELECT_ACTIVE;
  ctx->style.edit.selected_hover       = UI_C_ACCENT_ACTIVE;
  ctx->style.edit.selected_text_normal = UI_C_TEXT;
  ctx->style.edit.selected_text_hover  = UI_C_TEXT;
  ctx->style.edit.cursor_size          = 1.0f;

  /* Property */
  ctx->style.property.normal       = UI_ITEM(UI_C_CONTROL);
  ctx->style.property.hover        = UI_ITEM(UI_C_CONTROL_HOVER);
  ctx->style.property.active       = UI_ITEM(UI_C_CONTROL_ACTIVE);
  ctx->style.property.border_color = UI_C_BORDER_SOFT;

  /* Combo */
  ctx->style.combo.normal       = UI_ITEM(UI_C_CONTROL);
  ctx->style.combo.hover        = UI_ITEM(UI_C_CONTROL_HOVER);
  ctx->style.combo.active       = UI_ITEM(UI_C_CONTROL_ACTIVE);
  ctx->style.combo.border_color = UI_C_BORDER_SOFT;
}

static void
nk_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
  (void)usr;

  if (!OpenClipboard(NULL)) {
    return;
  }

  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (h) {
    wchar_t *w = (wchar_t *)GlobalLock(h);
    if (w) {
      str16_t s16 = str16_from_wstr(w);
      if (!str16_is_empty(s16)) {
        tmp_arena_t tmp = scratch_begin(NULL);

        str_t s = str_from_str16(tmp.arena, s16);
        nk_textedit_paste(edit, (const char *)s.data, (int)s.len);

        scratch_end(tmp);
      }
      GlobalUnlock(h);
    }
  }

  CloseClipboard();
}

static void
nk_clipboard_copy(nk_handle usr, const char *text, int len)
{
  (void)usr;

  if (!text || len <= 0) {
    return;
  }

  if (!OpenClipboard(NULL)) {
    return;
  }

  EmptyClipboard();

  if (len > 0) {
    tmp_arena_t tmp = scratch_begin(NULL);

    str_t   s   = str_make((uint8_t *)text, (uint64_t)len);
    str16_t s16 = str16_from_str(tmp.arena, s);

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(s16.len + 1) * sizeof(s16.data[0]));
    if (mem) {
      wchar_t *w = (wchar_t *)GlobalLock(mem);
      if (w) {
        mem_copy(w, s16.data, s16.len * sizeof(s16.data[0]));
        w[s16.len] = L'\0';
        GlobalUnlock(mem);
        SetClipboardData(CF_UNICODETEXT, mem);
      } else {
        GlobalFree(mem);
      }
    }

    scratch_end(tmp);
  }

  CloseClipboard();
}

void
ui_manager_preinit(ui_manager_t *manager, mod_manager_t *mod_manager, arena_t *arena)
{
  ASSERT(mod_manager != NULL);

  *manager = (ui_manager_t){
    .mod_manager              = mod_manager,
    .mouse_button_mask        = 0,
    .block_keyboard_when_open = true,
    .block_gamepad_when_open  = false,
    .vw                       = 0,
    .vh                       = 0,
  };

  ui_console_init(&manager->console, arena);
}

void
ui_manager_init(ui_manager_t *manager, struct nk_context *ctx, struct nk_font *font_body, struct nk_font *font_title, unsigned int vw, unsigned int vh)
{
  if (manager->inited || !ctx) {
    return;
  }

  ctx->clip.paste = nk_clipboard_paste;
  ctx->clip.copy  = nk_clipboard_copy;

  manager->ctx        = ctx;
  manager->font_body  = font_body;
  manager->font_title = font_title;
  manager->inited     = true;
  manager->vw         = vw;
  manager->vh         = vh;

  ui_mod_manager_init(&manager->main, &manager->keybind_capture, font_body, font_title);
  set_style(manager, ctx);
}

void
ui_manager_shutdown(ui_manager_t *manager)
{
  manager->ctx         = NULL;
  manager->inited      = false;
  manager->input_begun = false;
}

void
ui_manager_console_toggle(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  ui_console_toggle(&manager->console);
}

void
ui_manager_console_open(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  ui_console_open(&manager->console);
}

void
ui_manager_console_close(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  ui_console_close(&manager->console);
}

bool
ui_manager_console_is_open(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  return ui_console_is_open(&manager->console);
}

void
ui_manager_main_toggle(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  ui_mod_manager_toggle(&manager->main);
}

void
ui_manager_main_open(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  ui_mod_manager_open(&manager->main);
}

void
ui_manager_main_close(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  ui_mod_manager_close(&manager->main);
}

bool
ui_manager_main_is_open(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  return ui_mod_manager_is_open(&manager->main);
}

bool
ui_manager_keybind_capture_is_open(ui_manager_t *manager)
{
  ASSERT(manager != NULL);
  return ui_keybind_capture_is_open(&manager->keybind_capture);
}

static bool
is_any_window(struct nk_context *ctx, uint32_t inc_flags, uint32_t exc_flags)
{
  if (!ctx) {
    return false;
  }

  struct nk_window *win = ctx->begin;
  while (win) {
    bool result = false;
    if (inc_flags && exc_flags) {
      result = (win->flags & inc_flags) && !(win->flags & exc_flags);
    } else if (inc_flags) {
      result = (win->flags & inc_flags);
    } else if (exc_flags) {
      result = !(win->flags & exc_flags);
    }

    if (result) {
      return true;
    }

    win = win->next;
  }

  return false;
}

static bool
is_any_interactive_window_hovered(const struct nk_context *ctx)
{
  if (!ctx) {
    return false;
  }

  struct nk_window *win = ctx->begin;
  while (win) {
    if (!(win->flags & (NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED | (enum nk_window_flags)NK_WINDOW_NO_INPUT))) {
      struct nk_rect bounds = win->bounds;
      if (win->flags & NK_WINDOW_MINIMIZED) {
        bounds.h = ctx->style.font->height + 2 * ctx->style.window.header.padding.y;
      }

      if (nk_input_is_mouse_hovering_rect(&ctx->input, bounds)) {
        return true;
      }
    }
    win = win->next;
  }
  return false;
}

static inline bool
is_any_window_open(struct nk_context *ctx)
{
  return is_any_window(ctx, 0, NK_WINDOW_CLOSED | NK_WINDOW_HIDDEN);
}

static inline bool
is_any_window_interactive(struct nk_context *ctx)
{
  return is_any_window(ctx, 0, NK_WINDOW_CLOSED | NK_WINDOW_HIDDEN | NK_WINDOW_MINIMIZED | NK_WINDOW_NOT_INTERACTIVE);
}

bool
ui_manager_should_show_cursor(ui_manager_t *manager)
{
  return is_any_window_open(manager->ctx);
}

static inline bool
ui_manager_should_feed_input(ui_manager_t *manager)
{
  return is_any_window_open(manager->ctx);
}

static uint32_t
input_key_to_mouse_button_bitflag(input_key_kind_t key)
{
  switch (key) {
  case INPUT_KEY_MOUSE_LEFT:
    return (1 << 0);
  case INPUT_KEY_MOUSE_RIGHT:
    return (1 << 1);
  case INPUT_KEY_MOUSE_MIDDLE:
    return (1 << 2);
  case INPUT_KEY_MOUSE_THUMB_1:
    return (1 << 3);
  case INPUT_KEY_MOUSE_THUMB_2:
    return (1 << 4);
  }
  return 0;
}

static bool
ui_manager_mouse_event_should_consume(ui_manager_t *manager, input_event_t *ev)
{
  struct nk_context *ctx          = manager->ctx;
  bool               hovering_ui  = is_any_interactive_window_hovered(ctx);
  uint32_t           mouse_button = input_key_to_mouse_button_bitflag(ev->key);

  switch (ev->kind) {
  case INPUT_EVENT_MOUSE_DOWN:
  case INPUT_EVENT_MOUSE_DBLCLICK: {
    if (hovering_ui && mouse_button != 0) {
      manager->mouse_button_mask |= mouse_button;
      return true;
    }

    return false;
  }

  case INPUT_EVENT_MOUSE_UP: {
    if (mouse_button != 0 && (manager->mouse_button_mask & mouse_button)) {
      manager->mouse_button_mask &= ~mouse_button;
      return true;
    }

    return false;
  }

  case INPUT_EVENT_MOUSE_MOVE: {
    return manager->mouse_button_mask != 0 || hovering_ui;
  }

  case INPUT_EVENT_MOUSE_WHEEL: {
    return hovering_ui;
  }
  }

  return false;
}

static bool
ui_manager_should_consume_input_pre(ui_manager_t *manager, input_event_t *ev)
{
  if (ui_keybind_capture_is_open(&manager->keybind_capture)) {
    return true;
  }

  if (is_any_window_open(manager->ctx)) {
    if (input_event_is_mouse(ev)) {
      return ui_manager_mouse_event_should_consume(manager, ev);
    }
  }

  return false;
}

static bool
ui_manager_should_consume_input_post(ui_manager_t *manager, input_event_t *ev)
{
  if (is_any_window_interactive(manager->ctx)) {
    if (input_event_is_gamepad(ev)) {
      return manager->block_gamepad_when_open;
    }

    if (input_event_is_keyboard(ev)) {
      return manager->block_keyboard_when_open;
    }
  }

  return false;
}

static void
feed_input(ui_manager_t *manager, input_event_t *ev)
{
  ASSERT(manager != NULL);
  ASSERT(manager->ctx != NULL);

  struct nk_context *ctx = manager->ctx;

  int x = (int)ev->client_x;
  int y = (int)ev->client_y;

  switch (ev->kind) {
  case INPUT_EVENT_MOUSE_DOWN: {
    if (ev->key == INPUT_KEY_MOUSE_LEFT) {
      nk_input_button(ctx, NK_BUTTON_LEFT, x, y, 1);
    } else if (ev->key == INPUT_KEY_MOUSE_RIGHT) {
      nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, 1);
    } else if (ev->key == INPUT_KEY_MOUSE_MIDDLE) {
      nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, 1);
    }
    break;
  }

  case INPUT_EVENT_MOUSE_UP: {
    if (ev->key == INPUT_KEY_MOUSE_LEFT) {
      nk_input_button(ctx, NK_BUTTON_LEFT, x, y, 0);
      nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, 0);
    } else if (ev->key == INPUT_KEY_MOUSE_RIGHT) {
      nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, 0);
    } else if (ev->key == INPUT_KEY_MOUSE_MIDDLE) {
      nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, 0);
    }
    break;
  }

  case INPUT_EVENT_MOUSE_DBLCLICK: {
    if (ev->key == INPUT_KEY_MOUSE_LEFT) {
      nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, 1);
      nk_input_button(ctx, NK_BUTTON_LEFT, x, y, 1);
    } else if (ev->key == INPUT_KEY_MOUSE_RIGHT) {
      nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, 1);
    } else if (ev->key == INPUT_KEY_MOUSE_MIDDLE) {
      nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, 1);
    }
    break;
  }

  case INPUT_EVENT_MOUSE_WHEEL: {
    nk_input_scroll(ctx, nk_vec2(0, ev->wheel_delta));
    break;
  }

  case INPUT_EVENT_MOUSE_MOVE: {
    nk_input_motion(ctx, x, y);
    break;
  }

  case INPUT_EVENT_KEY_CHAR: {
    if (ev->character >= 32 && ev->character < 0x10FFFF) {
      nk_input_unicode(ctx, (nk_rune)ev->character);
    }
    break;
  }

  case INPUT_EVENT_KEY_DOWN:
  case INPUT_EVENT_KEY_UP: {
    int down = (ev->kind == INPUT_EVENT_KEY_DOWN) ? 1 : 0;

    nk_input_key(ctx, NK_KEY_SHIFT, (ev->modifiers & INPUT_MOD_SHIFT) != 0);
    nk_input_key(ctx, NK_KEY_CTRL, (ev->modifiers & INPUT_MOD_CTRL) != 0);

    switch (ev->key) {
    case INPUT_KEY_BACKSPACE: {
      if (ev->modifiers & INPUT_MOD_CTRL) {
        nk_input_key(ctx, NK_KEY_TEXT_WORD_DELETE_LEFT, down);
      } else {
        nk_input_key(ctx, NK_KEY_BACKSPACE, down);
      }
      break;
    }

    case INPUT_KEY_DELETE: {
      if (ev->modifiers & INPUT_MOD_CTRL) {
        nk_input_key(ctx, NK_KEY_TEXT_WORD_DELETE_RIGHT, down);
      } else {
        nk_input_key(ctx, NK_KEY_DEL, down);
      }
      break;
    }

    case INPUT_KEY_ENTER:
      nk_input_key(ctx, NK_KEY_ENTER, down);
      break;
    case INPUT_KEY_TAB:
      nk_input_key(ctx, NK_KEY_TAB, down);
      break;
    case INPUT_KEY_UP:
      nk_input_key(ctx, NK_KEY_UP, down);
      break;
    case INPUT_KEY_DOWN:
      nk_input_key(ctx, NK_KEY_DOWN, down);
      break;
    case INPUT_KEY_PAGE_UP:
      nk_input_key(ctx, NK_KEY_SCROLL_UP, down);
      break;
    case INPUT_KEY_PAGE_DOWN:
      nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down);
      break;

    case INPUT_KEY_LEFT: {
      if (ev->modifiers & INPUT_MOD_CTRL) {
        nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
      } else {
        nk_input_key(ctx, NK_KEY_LEFT, down);
      }
      break;
    }

    case INPUT_KEY_RIGHT: {
      if (ev->modifiers & INPUT_MOD_CTRL) {
        nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
      } else {
        nk_input_key(ctx, NK_KEY_RIGHT, down);
      }
      break;
    }

    case INPUT_KEY_HOME: {
      nk_input_key(ctx, NK_KEY_TEXT_START, down);
      nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down);
      nk_input_key(ctx, NK_KEY_SCROLL_START, down);
      break;
    }

    case INPUT_KEY_END: {
      nk_input_key(ctx, NK_KEY_TEXT_END, down);
      nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down);
      nk_input_key(ctx, NK_KEY_SCROLL_END, down);
      break;
    }

    default:
      break;
    }

    if (ev->modifiers & INPUT_MOD_CTRL) {
      switch (ev->key) {
      case INPUT_KEY_A:
        nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, down);
        break;
      case INPUT_KEY_C:
        nk_input_key(ctx, NK_KEY_COPY, down);
        break;
      case INPUT_KEY_V:
        nk_input_key(ctx, NK_KEY_PASTE, down);
        break;
      case INPUT_KEY_X:
        nk_input_key(ctx, NK_KEY_CUT, down);
        break;
      case INPUT_KEY_Z:
        nk_input_key(ctx, NK_KEY_TEXT_UNDO, down);
        break;
      case INPUT_KEY_Y:
        nk_input_key(ctx, NK_KEY_TEXT_REDO, down);
        break;
      }
    }
    break;
  }

  case INPUT_EVENT_APP_ACTIVATION: {
    if (!ev->app_activated) {
      manager->mouse_button_mask = 0;
    }
    break;
  }

  default:
    break;
  }
}

bool
ui_manager_on_input_event_pre(ui_manager_t *manager, uint64_t frame_counter, input_event_t *ev)
{
  if (!manager->inited || !manager->ctx || !ev) {
    return false;
  }

  if (ui_keybind_capture_input(&manager->keybind_capture, ev)) {
    return true;
  }

  if (input_event_covers_keybind(ev, manager->mod_manager->cfg.overlay_toggle)) {
    if (keybind_is_pressed(manager->mod_manager->cfg.overlay_toggle)) {
      ui_mod_manager_toggle(&manager->main);
    }
    return true;
  }

  if (input_event_covers_keybind(ev, manager->console.cfg.toggle_bind)) {
    if (keybind_is_pressed(manager->console.cfg.toggle_bind)) {
      ui_console_toggle(&manager->console);
    }
    return true;
  }

  if (ui_manager_should_feed_input(manager)) {
    if (manager->input_frame != frame_counter) {
      nk_input_begin(manager->ctx);
      manager->input_begun = true;
      manager->input_frame = frame_counter;
    }

    feed_input(manager, ev);
  }

  return ui_manager_should_consume_input_pre(manager, ev);
}

bool
ui_manager_on_input_event_post(ui_manager_t *manager, input_event_t *ev)
{
  if (!manager->inited || !manager->ctx || !ev) {
    return false;
  }

  return ui_manager_should_consume_input_post(manager, ev);
}

void
ui_manager_on_frame_begin(ui_manager_t *manager, uint64_t frame_counter)
{
  if (!manager->inited || !manager->ctx) {
    return;
  }

  if (!manager->input_begun) {
    nk_input_begin(manager->ctx);
    manager->input_begun = true;
    manager->input_frame = frame_counter;
  }

  nk_input_end(manager->ctx);
  manager->input_begun = false;
}

void
ui_manager_on_frame_end(ui_manager_t *manager, unsigned int vw, unsigned int vh)
{
  struct nk_context *ctx = manager->ctx;
  if (!manager->inited || !ctx) {
    return;
  }

  manager->vw = vw;
  manager->vh = vh;

  ui_mod_manager_draw(&manager->main, ctx, manager->mod_manager, vw, vh);
  ui_keybind_capture_draw(&manager->keybind_capture, ctx, vw, vh);
  ui_console_draw(&manager->console, ctx, manager->mod_manager, vw, vh);
}
