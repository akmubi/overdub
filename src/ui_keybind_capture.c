#include "ui_keybind_capture.h"

#include "actions.h"
#include "globals.h"
#include "input.h"
#include "scratch.h"
#include "ui_nuklear.h"

static bool
ui_keybind_has_key(keybind_t *bind, input_key_kind_t key)
{
  if (!bind || key == INPUT_KEY_NONE) {
    return false;
  }

  for (uint8_t i = 0; i < bind->count; ++i) {
    if (bind->keys[i] == key) {
      return true;
    }
  }
  return false;
}

static bool
ui_keybind_push_key(keybind_t *bind, input_key_kind_t key)
{
  if (!bind || key == INPUT_KEY_NONE) {
    return false;
  }

  if (ui_keybind_has_key(bind, key)) {
    return true;
  }

  if (bind->count >= KEYBIND_MAX_KEYS) {
    return false;
  }

  bind->keys[bind->count++] = key;
  return true;
}

static bool
ui_input_key_can_be_captured(input_key_kind_t key)
{
  switch (key) {
  case INPUT_KEY_NONE:
  case INPUT_KEY_MOUSE_LEFT:
  case INPUT_KEY_MOUSE_X:
  case INPUT_KEY_MOUSE_Y:
  case INPUT_KEY_GAMEPAD_LEFT_X:
  case INPUT_KEY_GAMEPAD_LEFT_Y:
  case INPUT_KEY_GAMEPAD_RIGHT_X:
  case INPUT_KEY_GAMEPAD_RIGHT_Y:
  case INPUT_KEY_GAMEPAD_LT_AXIS:
  case INPUT_KEY_GAMEPAD_RT_AXIS:
    return false;
  }

  return true;
}

static void
ui_keybind_collect_current_down(keybind_t *out)
{
  static const input_key_kind_t preferred[] = {
    INPUT_KEY_LEFT_CTRL,
    INPUT_KEY_RIGHT_CTRL,
    INPUT_KEY_LEFT_SHIFT,
    INPUT_KEY_RIGHT_SHIFT,
    INPUT_KEY_LEFT_ALT,
    INPUT_KEY_RIGHT_ALT,
  };

  keybind_clear(out);

  for (int i = 0; i < COUNTOF(preferred); ++i) {
    input_key_kind_t key = preferred[i];
    if (input_key_is_down(key) && ui_input_key_can_be_captured(key)) {
      ui_keybind_push_key(out, key);
    }
  }

  for (input_key_kind_t kind = INPUT_KEY_NONE + 1; kind < INPUT_KEY_MAX; ++kind) {
    if (!input_key_is_down(kind) || !ui_input_key_can_be_captured(kind)) {
      continue;
    }

    if (ui_keybind_has_key(out, kind)) {
      continue;
    }

    ui_keybind_push_key(out, kind);
  }
}

static void
ui_keybind_capture_sync_from_state(ui_keybind_capture_t *cap)
{
  keybind_t live = KEYBIND_NULL;
  if (!ui_keybind_capture_is_open(cap)) {
    return;
  }

  ui_keybind_collect_current_down(&live);

  {
    keybind_t pad   = keybind_gamepad_part(live);
    keybind_t order = KEYBIND_NULL;

    for (int i = 0; i < cap->pad_order.count; ++i) {
      if (ui_keybind_has_key(&pad, cap->pad_order.keys[i])) {
        ui_keybind_push_key(&order, cap->pad_order.keys[i]);
      }
    }

    for (int i = 0; i < pad.count; ++i) {
      ui_keybind_push_key(&order, pad.keys[i]);
    }

    cap->pad_order = order;
    live           = keybind_merge(live, order);
  }

  cap->live = live;

  if (keybind_is_valid(live)) {
    keybind_t keyboard = keybind_keyboard_part(live);
    keybind_t gamepad  = keybind_gamepad_part(live);
    keybind_t was_kb   = keybind_keyboard_part(cap->pending);
    keybind_t was_pad  = keybind_gamepad_part(cap->pending);

    /* keys let go: the part stays */
    if (keyboard.count > 0 && (!cap->combo_active || keyboard.count >= was_kb.count)) {
      was_kb = keyboard;
    }

    if (gamepad.count > 0 && (!cap->combo_active || gamepad.count >= was_pad.count)) {
      was_pad = gamepad;
    }

    cap->combo_active = true;
    cap->pending      = keybind_merge(was_kb, was_pad);
    } else {
    cap->combo_active = false;
  }
}

bool
ui_keybind_capture_is_open(ui_keybind_capture_t *cap)
{
  return cap && cap->active;
}

static void
ui_keybind_capture_clear_part(ui_keybind_capture_t *cap, bool gamepad)
{
  keybind_t keyboard = gamepad ? keybind_keyboard_part(cap->pending) : KEYBIND_NULL;
  keybind_t pad      = gamepad ? KEYBIND_NULL : keybind_gamepad_part(cap->pending);
  cap->combo_active  = false;
  cap->live          = KEYBIND_NULL;
  cap->pending       = keybind_merge(keyboard, pad);
}

static void
ui_keybind_capture_finish(ui_keybind_capture_t *cap, bool accepted)
{
  cap->result_ready    = true;
  cap->result_accepted = accepted;
  cap->result_hash     = cap->owner;
  cap->result          = cap->pending;

  cap->active       = false;
  cap->combo_active = false;
  cap->owner        = 0;
  cap->original     = KEYBIND_NULL;
  cap->live         = KEYBIND_NULL;
  cap->pending      = KEYBIND_NULL;
  cap->pad_order    = KEYBIND_NULL;
  cap->target       = NULL;
  cap->pad_pressed  = false;
}

static bool
ui_keybind_any_gamepad_down(void)
{
  for (input_key_kind_t key = INPUT_KEY_GAMEPAD_FACE_BOTTOM; key <= INPUT_KEY_GAMEPAD_DPAD_RIGHT; ++key) {
    if (input_key_is_down(key)) {
      return true;
    }
  }
  return false;
}

/* the dialog takes all gamepad input: releasing the buttons applies them */
static void
ui_keybind_capture_gamepad_released(ui_keybind_capture_t *cap)
{
  if (keybind_gamepad_part(cap->pending).count > 0) {
    ui_keybind_capture_finish(cap, true);
  }
}

/* holding B alone cancels: 0..1 */
static float
ui_keybind_capture_cancel_progress(ui_keybind_capture_t *cap)
{
  keybind_t pad = keybind_gamepad_part(cap->live);

  if (pad.count != 1 || pad.keys[0] != INPUT_KEY_GAMEPAD_FACE_RIGHT) {
    return 0.0f;
  }

  float cancel_us = (float)globals.mod_manager.cfg.gamepad_cancel_ms * 1000.0f;
  return NK_CLAMP(0.0f, (float)(time_now_us() - cap->pad_since_us) / cancel_us, 1.0f);
}

bool
ui_keybind_capture_input(ui_keybind_capture_t *cap, input_event_t *ev)
{
  if (!cap || !ev || !cap->active) {
    return false;
  }

  if (input_key_is_gamepad(ev->key) && (ev->kind == INPUT_EVENT_KEY_DOWN || ev->kind == INPUT_EVENT_KEY_UP) && !ev->is_repeat) {
    if (ev->kind == INPUT_EVENT_KEY_DOWN) {
      int down = 0;

      for (input_key_kind_t key = INPUT_KEY_GAMEPAD_FACE_BOTTOM; key <= INPUT_KEY_GAMEPAD_DPAD_RIGHT; ++key) {
        down += input_key_is_down(key);
      }

      if (down <= 1) {
        cap->pad_since_us = time_now_us();
      }

      cap->pad_pressed = true;
    }

    ui_keybind_capture_sync_from_state(cap);

    /* not the release of the button that opened the dialog */
    if (ev->kind == INPUT_EVENT_KEY_UP && cap->pad_pressed && !ui_keybind_any_gamepad_down()) {
      ui_keybind_capture_gamepad_released(cap);
    }
    return true;
  }

  bool should_consume = true;
  switch (ev->kind) {
  case INPUT_EVENT_KEY_DOWN: {
    if (ev->is_repeat) {
      break;
    }

    ui_keybind_capture_sync_from_state(cap);
    break;
  }

  case INPUT_EVENT_KEY_UP:
  case INPUT_EVENT_MOUSE_DOWN:
  case INPUT_EVENT_MOUSE_UP:
  case INPUT_EVENT_MOUSE_DBLCLICK:
  case INPUT_EVENT_MOUSE_WHEEL: {
    ui_keybind_capture_sync_from_state(cap);
    if (ev->key == INPUT_KEY_MOUSE_LEFT) {
      should_consume = false;
    }
    break;
  }

  case INPUT_EVENT_MOUSE_MOVE: {
    should_consume = false;
    break;
  }

  case INPUT_EVENT_APP_ACTIVATION: {
    if (!ev->app_activated) {
      ui_keybind_capture_finish(cap, false);
      break;
    }

    ui_keybind_capture_sync_from_state(cap);
    break;
  }
  }

  return should_consume;
}

str_t
ui_keybind_capture_display_text(ui_keybind_capture_t *cap, arena_t *arena)
{
  if (keybind_is_valid(cap->pending)) {
    return keybind_to_str(cap->pending, arena);
  }

  if (keybind_is_valid(cap->live)) {
    return keybind_to_str(cap->live, arena);
  }

  return STR_LIT("<unassigned>");
}

bool
ui_keybind_capture(ui_keybind_capture_t *cap, struct nk_context *ctx, str_t name, keybind_t *bind)
{
  if (!cap || !ctx || !bind || str_is_empty(name)) {
    return false;
  }

  uint64_t hash = 0;
  if (name.data[0] == '#') {
    hash = (uint64_t)nk_murmur_hash(name.data, (int)name.len, ctx->current->property.seq++);
  } else {
    hash = (uint64_t)nk_murmur_hash(name.data, (int)name.len, 42);
  }

  if (cap->result_ready && cap->result_hash == hash) {
    bool accepted = cap->result_accepted;
    if (accepted) {
      *bind = cap->result;
    }

    cap->result_ready    = false;
    cap->result_accepted = false;
    cap->result_hash     = 0;
    cap->result          = KEYBIND_NULL;

    tmp_arena_t tmp = scratch_begin(NULL);
    {
      str_t s = keybind_to_str(*bind, tmp.arena);
      ui_button_str(ctx, s);
    }
    scratch_end(tmp);

    return accepted;
  }

  struct nk_style_button saved_btn = ctx->style.button;

  ctx->style.button.normal       = ctx->style.property.normal;
  ctx->style.button.hover        = ctx->style.property.hover;
  ctx->style.button.active       = ctx->style.property.active;
  ctx->style.button.border_color = ctx->style.property.border_color;

  if (cap->active && cap->owner == hash) {
    if (keybind_is_valid(cap->pending)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t s = keybind_to_str(cap->pending, tmp.arena);
        ui_button_str(ctx, s);
      }
      scratch_end(tmp);
    } else {
      ui_button_str(ctx, STR_LIT("<empty>"));
    }
  } else {
    if (keybind_is_valid(*bind)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t s = keybind_to_str(*bind, tmp.arena);
        if (ui_button_str(ctx, s)) {
          cap->active       = true;
          cap->combo_active = false;
          cap->owner        = hash;
          cap->original     = *bind;
          cap->live         = KEYBIND_NULL;
          cap->pending      = *bind;
          cap->pad_order    = KEYBIND_NULL;
          cap->target       = bind;
                }
      }
      scratch_end(tmp);
    } else {
      if (ui_button_str(ctx, STR_LIT("<unassigned>"))) {
        cap->active       = true;
        cap->combo_active = false;
        cap->owner        = hash;
        cap->original     = *bind;
        cap->live         = KEYBIND_NULL;
        cap->pending      = KEYBIND_NULL;
        cap->pad_order    = KEYBIND_NULL;
        cap->target       = bind;
            }
    }
  }

  ctx->style.button = saved_btn;
  return false;
}

static struct nk_rect
clamp_window_rect(struct nk_rect r, float min_w, float min_h, float max_w, float max_h, unsigned int vw, unsigned int vh)
{
  float margin  = 16.0f;
  float avail_w = NK_MAX(64.0f, (float)vw - margin * 2.0f);
  float avail_h = NK_MAX(64.0f, (float)vh - margin * 2.0f);

  max_w = NK_MIN(max_w, avail_w);
  max_h = NK_MIN(max_h, avail_h);

  min_w = NK_MIN(min_w, max_w);
  min_h = NK_MIN(min_h, max_h);

  r.w = NK_CLAMP(r.w, min_w, max_w);
  r.h = NK_CLAMP(r.h, min_h, max_h);

  r.x = NK_CLAMP(r.x, margin, (float)vw - margin - r.w);
  r.y = NK_CLAMP(r.y, margin, (float)vh - margin - r.h);

  return r;
}

static struct nk_rect
centered_window_rect(float w, float h, unsigned int vw, unsigned int vh)
{
  struct nk_rect r = nk_rect(((float)vw - w) * 0.5f, ((float)vh - h) * 0.5f, w, h);
  return clamp_window_rect(r, w, h, w, h, vw, vh);
}

void
ui_keybind_capture_draw(ui_keybind_capture_t *cap, struct nk_context *ctx, unsigned int vw, unsigned int vh)
{
  if (!ui_keybind_capture_is_open(cap)) {
    return;
  }

  nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE;
  const char *name = "Set shortcut";

  struct nk_rect rect = centered_window_rect(520.0f, 260.0f, vw, vh);
  rect                = clamp_window_rect(rect, 380.0f, 240.0f, 520.0f, 260.0f, vw, vh);

  tmp_arena_t tmp = scratch_begin(NULL);
  if (nk_begin(ctx, name, rect, flags)) {
    keybind_t keyboard = keybind_keyboard_part(cap->pending);
    keybind_t gamepad  = keybind_gamepad_part(cap->pending);
    str_t     kb_text  = keyboard.count ? keybind_to_str(keyboard, tmp.arena) : STR_LIT("-");
    str_t     pad_text = gamepad.count  ? keybind_to_str(gamepad, tmp.arena) : STR_LIT("-");
    float     cancel   = ui_keybind_capture_cancel_progress(cap);
    str_t     hint     = str_push_fmt(tmp.arena, "Press keys or gamepad buttons, the first gamepad button leads. A gamepad combo works at once, a single button when held for %u ms. "
                                                 "Gamepad: release the buttons to apply, hold B alone to cancel.",
                                      actions_get_hold_ms(&globals.actions));

    nk_layout_row_template_begin(ctx, 26.0f);
    nk_layout_row_template_push_static(ctx, 90.0f);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);
    nk_label(ctx, "Keyboard:", NK_TEXT_LEFT);
    nk_label_colored(ctx, (const char *)kb_text.data, NK_TEXT_LEFT, UI_C_TEXT);
    nk_label(ctx, "Gamepad:", NK_TEXT_LEFT);
    nk_label_colored(ctx, (const char *)pad_text.data, NK_TEXT_LEFT, UI_C_TEXT);

    nk_layout_row_dynamic(ctx, 56.0f, 1);
    nk_label_colored_wrap(ctx, (const char *)hint.data, UI_C_TEXT_DIM);
    nk_layout_row_dynamic(ctx, 22.0f, 1);

    action_t *other = mod_keybind_find_conflict(&globals.mod_manager, &globals.actions, cap->pending, cap->target);

    if (cancel > 0.0f) {
      nk_size progress = (nk_size)(cancel * 100.0f);

      nk_layout_row_template_begin(ctx, 22.0f);
      nk_layout_row_template_push_static(ctx, 90.0f);
      nk_layout_row_template_push_dynamic(ctx);
      nk_layout_row_template_end(ctx);
      nk_label_colored(ctx, "Cancelling", NK_TEXT_LEFT, UI_C_TEXT_DIM);
      nk_progress(ctx, &progress, 100, nk_false);
    } else if (other) {
      str_t warning = str_push_fmt(tmp.arena, "Also used by '%.*s' (%.*s)", STR_ARG(other->label), STR_ARG(other->source));

      nk_text_colored(ctx, (const char *)warning.data, (int)warning.len, NK_TEXT_LEFT, UI_C_GOLD);
    } else {
      nk_spacing(ctx, 1);
    }

    nk_layout_row_dynamic(ctx, 32.0f, 4);

    if (ui_button_str(ctx, STR_LIT("Apply"))) {
      ui_keybind_capture_finish(cap, true);
    }

    if (ui_button_str(ctx, STR_LIT("Clear keys"))) {
      ui_keybind_capture_clear_part(cap, false);
    }

    if (ui_button_str(ctx, STR_LIT("Clear gamepad"))) {
      ui_keybind_capture_clear_part(cap, true);
    }

    if (ui_button_str(ctx, STR_LIT("Cancel"))) {
      ui_keybind_capture_finish(cap, false);
    }
  }
  nk_end(ctx);
  scratch_end(tmp);

  if (nk_window_is_closed(ctx, name) || ui_keybind_capture_cancel_progress(cap) >= 1.0f) {
    ui_keybind_capture_finish(cap, false);
  } else {
    nk_window_set_focus(ctx, name);
  }
}
