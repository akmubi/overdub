#include "ui_manager.h"
#include "input.h"
#include "mod_manager.h"
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
  (void)manager;

  nk_style_from_theme(ctx, NK_THEME_DARK_BLUE);

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
  ctx->style.window.group_border   = 0.0f;
  ctx->style.window.border         = 1.0f;
  ctx->style.window.scrollbar_size = nk_vec2(14.0f, 14.0f);

  /* Tab / tree */
  ctx->style.tab.indent = 12.0f;

  /* Checkbox / option */
  ctx->style.checkbox.padding = nk_vec2(2.0f, 2.0f);
  ctx->style.checkbox.border  = 1.0f;

  ctx->style.option = ctx->style.checkbox;

  /* Slider */
  ctx->style.slider.bar_height = 6.0f;

  /* Progress */
  ctx->style.progress.padding = nk_vec2(2.0f, 2.0f);
  ctx->style.progress.border  = 1.0f;

  /* Scrollbar */
  ctx->style.scrollh.padding         = nk_vec2(1.0f, 1.0f);
  ctx->style.scrollh.cursor_min_size = ctx->style.window.scrollbar_size.x;

  ctx->style.scrollv = ctx->style.scrollh;

  /* Edit */
  ctx->style.edit.cursor_size = 1.0f;
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
    .block_gamepad_when_open  = true,
    .vw                       = 0,
    .vh                       = 0,
  };

  ui_console_init(&manager->console, arena);
}

static void
on_toggle_mod_manager(void *user)
{
  ui_manager_t *manager = user;

  ui_mod_manager_toggle(&manager->main);
}

static void
on_toggle_console(void *user)
{
  ui_manager_t *manager = user;

  ui_console_toggle(&manager->console);
}

void
ui_manager_register_actions(ui_manager_t *manager, actions_t *registry)
{
  mod_manager_t *mod_manager = manager->mod_manager;

  actions_register(registry, STR_LIT("overdub.mod_manager"), STR_LIT("Mod manager"), STR_LIT("Overdub"), MOD_HANDLE_INVALID, &mod_manager->cfg.overlay_toggle,  on_toggle_mod_manager, manager);
  actions_register(registry, STR_LIT("overdub.console"),     STR_LIT("Console"),     STR_LIT("Overdub"), MOD_HANDLE_INVALID, &manager->console.cfg.toggle_bind, on_toggle_console,     manager);
}

void
ui_manager_init(ui_manager_t *manager, struct nk_context *ctx, struct nk_font *font, unsigned int vw, unsigned int vh)
{
  if (manager->inited || !ctx) {
    return;
  }

  ctx->clip.paste = nk_clipboard_paste;
  ctx->clip.copy  = nk_clipboard_copy;

  manager->ctx    = ctx;
  manager->inited = true;
  manager->vw     = vw;
  manager->vh     = vh;

  nk_style_set_font(ctx, &font->handle);
  ui_mod_manager_init(&manager->main, &manager->keybind_capture);
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
is_any_interactive_window_hovered(struct nk_context *ctx)
{
  ASSERT(ctx != NULL);

  struct nk_window *win = ctx->begin;
  while (win) {
    if (!(win->flags & (NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED | (enum nk_window_flags)NK_WINDOW_NO_INPUT))) {
      struct nk_rect bounds = win->bounds;
      if (win->flags & NK_WINDOW_MINIMIZED) {
        bounds.h = ctx->style.font_size + 2 * ctx->style.window.header.padding.y;
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
  /* not NK_WINDOW_ROM: nuklear sets it on every window but the top one */
  return is_any_window(ctx, 0, NK_WINDOW_CLOSED | NK_WINDOW_HIDDEN | NK_WINDOW_MINIMIZED | (enum nk_window_flags)NK_WINDOW_NO_INPUT);
}

static inline bool
window_captures_keyboard(struct nk_window *win)
{
  if (!win) {
    return false;
  }

  if (win->edit.active) {
    return true;
  }

  bool popup_active = win->popup.active && win->popup.win;
  if (!popup_active) {
    return false;
  }

  if (win->popup.type == NK_PANEL_COMBO) {
    return true;
  }

  return win->popup.win->edit.active;
}

static bool
is_any_keyboard_capture_active(struct nk_context *ctx)
{
  ASSERT(ctx != NULL);

  for (struct nk_window *win = ctx->begin; win; win = win->next) {
    if (!(win->flags & (NK_WINDOW_CLOSED | NK_WINDOW_HIDDEN | (enum nk_window_flags)NK_WINDOW_NO_INPUT)) &&
        window_captures_keyboard(win)) {
      return true;
    }
  }

  return false;
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

  if (input_event_is_keyboard(ev) && is_any_keyboard_capture_active(manager->ctx)) {
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
      /* the character still moves */
      if (ev->key == INPUT_KEY_GAMEPAD_LEFT_X || ev->key == INPUT_KEY_GAMEPAD_LEFT_Y || ev->key == INPUT_KEY_GAMEPAD_L3) {
        return false;
      }

      return manager->block_gamepad_when_open;
    }

    if (input_event_is_keyboard(ev)) {
      return manager->block_keyboard_when_open;
    }
  }

  return false;
}

/* [KEYBOARD]:
 * - arrows:    move (Ctrl: scroll, Alt: window, Ctrl+Shift: move, Alt+Shift: resize)
 * - Enter:     accept
 * - Escape:    cancel
 * - Tab:       focus
 * - Alt+M:     maximize
 * - Alt+W:     window menu
 *
 * [GAMEPAD]:
 * - D-Pad:       move
 * - A:           accept
 * - B:           cancel
 * - X:           menu
 * - Y:           keyboard
 * - LB/RB:       previous/next window/symbol
 * - LT/RT:       previous/next word
 * - right stick: scroll
 * - start:       window menu
 */
static void
feed_nav(ui_manager_t *manager, struct nk_context *ctx, input_event_t *ev)
{
  if (ev->kind == INPUT_EVENT_ANALOG) {
    if (ev->key == INPUT_KEY_GAMEPAD_RIGHT_X || ev->key == INPUT_KEY_GAMEPAD_RIGHT_Y) {
      float *axis = ev->key == INPUT_KEY_GAMEPAD_RIGHT_X ? &manager->right_x : &manager->right_y;
      float  was  = *axis;
      *axis       = ev->analog_value;

      if (NK_ABS(*axis) > 0.5f || NK_ABS(was) > 0.35f) {
        bool on = NK_ABS(*axis) > 0.35f;

        if (ev->key == INPUT_KEY_GAMEPAD_RIGHT_X) {
          nk_input_key(ctx, NK_KEY_NAV_SCROLL_LEFT,  on && *axis < 0);
          nk_input_key(ctx, NK_KEY_NAV_SCROLL_RIGHT, on && *axis > 0);
        } else {
          nk_input_key(ctx, NK_KEY_NAV_SCROLL_UP,   on && *axis > 0);
          nk_input_key(ctx, NK_KEY_NAV_SCROLL_DOWN, on && *axis < 0);
        }

        manager->gamepad_input = true;
      }
    }
    return;
  }

  if (ev->kind != INPUT_EVENT_KEY_DOWN && ev->kind != INPUT_EVENT_KEY_UP) {
    return;
  }

  int down = ev->kind == INPUT_EVENT_KEY_DOWN;

  /* nuklear repeats held keys itself */
  if (down && (ev->is_repeat || manager->nav_down[ev->key])) {
    return;
  }

  manager->nav_down[ev->key] = down;

  bool ctrl  = (ev->modifiers & INPUT_MOD_CTRL)  != 0;
  bool shift = (ev->modifiers & INPUT_MOD_SHIFT) != 0;
  bool alt   = (ev->modifiers & INPUT_MOD_ALT)   != 0;

  if (input_key_is_gamepad(ev->key)) {
    manager->gamepad_input = true;
  } else if (down) {
    manager->gamepad_input = false;
  }

  switch (ev->key) {
    case INPUT_KEY_UP:
    case INPUT_KEY_DOWN:
    case INPUT_KEY_LEFT:
    case INPUT_KEY_RIGHT: {
      int dir = ev->key == INPUT_KEY_UP ? 0 : ev->key == INPUT_KEY_DOWN ? 1 : ev->key == INPUT_KEY_LEFT ? 2 : 3;

      nk_input_key(ctx, (enum nk_keys)(NK_KEY_NAV_UP        + dir), down && ((!ctrl && !alt) || (alt && shift) || (ctrl && shift && !alt)));
      nk_input_key(ctx, (enum nk_keys)(NK_KEY_NAV_SCROLL_UP + dir), down && ctrl && !alt && !shift);
      nk_input_key(ctx, (enum nk_keys)(NK_KEY_NAV_WINDOW_UP + dir), down && alt && !shift);
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_SIZE, down && alt  && shift);
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_MOVE, down && ctrl && shift && !alt);
      break;
    }

    case INPUT_KEY_ENTER: {
      nk_input_key(ctx, NK_KEY_NAV_ACTIVATE, down);
      break;
    }

    case INPUT_KEY_ESCAPE: {
      nk_input_key(ctx, NK_KEY_NAV_CANCEL, down);
      break;
    }

    case INPUT_KEY_TAB: {
      nk_input_key(ctx, NK_KEY_FOCUS_NEXT, down && !shift && !ctrl && !alt);
      nk_input_key(ctx, NK_KEY_FOCUS_PREV, down && shift && !ctrl && !alt);
      break;
    }

    case INPUT_KEY_M: {
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_MAXIMIZE, down && alt);
      break;
    }

    case INPUT_KEY_W: {
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_MENU, down && alt);
      break;
    }

    case INPUT_KEY_GAMEPAD_DPAD_UP: {
      nk_input_key(ctx, NK_KEY_NAV_UP, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_DPAD_DOWN: {
      nk_input_key(ctx, NK_KEY_NAV_DOWN, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_DPAD_LEFT: {
      nk_input_key(ctx, NK_KEY_NAV_LEFT, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_DPAD_RIGHT: {
      nk_input_key(ctx, NK_KEY_NAV_RIGHT, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_FACE_BOTTOM: {
      nk_input_key(ctx, NK_KEY_NAV_ACTIVATE, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_FACE_RIGHT: {
      nk_input_key(ctx, NK_KEY_NAV_CANCEL, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_FACE_LEFT: {
      nk_input_key(ctx, NK_KEY_NAV_MENU, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_FACE_TOP: {
      nk_input_key(ctx, NK_KEY_NAV_KEYBOARD, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_LB: {
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_PREV, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_RB: {
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_NEXT, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_LT: {
      nk_input_key(ctx, NK_KEY_NAV_WORD_PREV, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_RT: {
      nk_input_key(ctx, NK_KEY_NAV_WORD_NEXT, down);
      break;
    }

    case INPUT_KEY_GAMEPAD_SPECIAL_RIGHT: {
      nk_input_key(ctx, NK_KEY_NAV_WINDOW_MENU, down);
      break;
    }

    default: {
      break;
    }
  }
}

static void
release_nav_keys(struct nk_context *ctx)
{
  for (int k = NK_KEY_FOCUS_NEXT; k < NK_KEY_MAX; ++k) {
    ctx->input.keyboard.keys[k].down = nk_false;
  }
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
    /* the engine also reports moves that did not move; they would end navigation */
    if (!manager->mouse_seen || x != manager->mouse_x || y != manager->mouse_y) {
      manager->mouse_seen = true;
      manager->mouse_x    = x;
      manager->mouse_y    = y;
      nk_input_motion(ctx, x, y);
    }
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
    nk_input_key(ctx, NK_KEY_CTRL,  (ev->modifiers & INPUT_MOD_CTRL) != 0);

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
      mem_zero(manager->nav_down, sizeof(manager->nav_down));
    }
    break;
  }

  default:
    break;
  }
}

void
ui_manager_note_input_device(ui_manager_t *manager, input_event_t *ev)
{
  switch (ev->kind) {
    case INPUT_EVENT_MOUSE_MOVE: {
      /* the engine also reports moves that did not move */
      float dx = (float)ev->client_x - manager->device_mouse_x;
      float dy = (float)ev->client_y - manager->device_mouse_y;

      if (dx * dx + dy * dy >= 9.0f) {
        manager->device_mouse_x = (float)ev->client_x;
        manager->device_mouse_y = (float)ev->client_y;
        manager->gamepad_input  = false;
      }
      break;
    }

    case INPUT_EVENT_MOUSE_DOWN:
    case INPUT_EVENT_MOUSE_DBLCLICK:
    case INPUT_EVENT_MOUSE_WHEEL: {
      manager->gamepad_input = false;
      break;
    }

    case INPUT_EVENT_KEY_DOWN: {
      manager->gamepad_input = input_key_is_gamepad(ev->key);
      break;
    }

    case INPUT_EVENT_ANALOG: {
      /* sticks and triggers at rest still report small values */
      if (input_key_is_gamepad(ev->key) && (ev->analog_value > 0.5f || ev->analog_value < -0.5f)) {
        manager->gamepad_input = true;
      }
      break;
    }

    default: {
      break;
    }
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

  bool capture_open = ui_keybind_capture_is_open(&manager->keybind_capture);

  if (!capture_open && keybind_activated_by_event(manager->mod_manager->cfg.overlay_toggle, ev)) {
    if (keybind_is_pressed(manager->mod_manager->cfg.overlay_toggle)) {
      ui_mod_manager_toggle(&manager->main);
    }
    return true;
  }

  if (!capture_open && keybind_activated_by_event(manager->console.cfg.toggle_bind, ev)) {
    if (keybind_is_pressed(manager->console.cfg.toggle_bind)) {
      ui_console_toggle(&manager->console);
    }
    return true;
  }

  bool consume = ui_manager_should_consume_input_pre(manager, ev);

  if (ui_manager_should_feed_input(manager)) {
    if (manager->input_frame != frame_counter) {
      nk_input_begin(manager->ctx);
      manager->input_begun = true;
      manager->input_frame = frame_counter;
    }

    feed_input(manager, ev);

    /* the mods will not see it (a field is being edited): Escape must still leave the field */
    if (consume) {
      feed_nav(manager, manager->ctx, ev);
    }
  }

  return consume;
}

void
ui_manager_on_input_event_nav(ui_manager_t *manager, input_event_t *ev)
{
  if (manager->inited && manager->ctx && ev && ui_manager_should_feed_input(manager) && manager->input_begun) {
    feed_nav(manager, manager->ctx, ev);
  }
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
ui_manager_on_frame_begin(ui_manager_t *manager, uint64_t frame_counter, float delta)
{
  ui_console_on_frame_begin(&manager->console, frame_counter);

  if (!manager->inited || !manager->ctx) {
    return;
  }

  manager->ctx->delta_time_seconds = NK_CLAMP(0.0f, delta, 0.1f);

  bool fed = ui_manager_should_feed_input(manager);

  if (manager->input_fed && !fed) {
    release_nav_keys(manager->ctx);
    mem_zero(manager->nav_down, sizeof(manager->nav_down));
    manager->right_x = manager->right_y = 0.0f;
  }

  manager->input_fed = fed;

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

  nk_window_set_maximize_bounds(ctx, nk_rect(0.0f, 0.0f, (float)vw, (float)vh));

  ui_mod_manager_draw(&manager->main, ctx, manager->mod_manager, vw, vh);
  ui_keybind_capture_draw(&manager->keybind_capture, ctx, vw, vh);

  manager->console.gamepad_input = manager->gamepad_input;
  ui_console_draw(&manager->console, ctx, manager->mod_manager, vw, vh);

  if (nk_nav_text_requested(ctx, NULL) && manager->gamepad_input) {
    nk_osk_open(ctx);
  }

  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_NAV_KEYBOARD)) {
    if (nk_osk_is_open(ctx)) {
      nk_osk_close(ctx);
    } else {
      nk_osk_open(ctx);
    }
  }

  nk_osk(ctx, nk_osk_place(ctx, nk_rect(0.0f, 0.0f, (float)vw, (float)vh), 640.0f, 280.0f));
}
