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

bool
ui_keybind_capture_is_open(ui_keybind_capture_t *cap)
{
  return cap && cap->active;
}

/* releasing all keys ends the capture */
static void
ui_keybind_capture_sync_from_state(ui_keybind_capture_t *cap)
{
  keybind_t live = KEYBIND_NULL;

  if (!ui_keybind_capture_is_open(cap) || !cap->capturing) {
    return;
  }

  ui_keybind_collect_current_down(&live);

  keybind_t part = cap->gamepad ? keybind_gamepad_part(live) : keybind_keyboard_part(live);

  if (cap->gamepad) {
    keybind_t order = KEYBIND_NULL;

    for (int i = 0; i < cap->pad_order.count; ++i) {
      if (ui_keybind_has_key(&part, cap->pad_order.keys[i])) {
        ui_keybind_push_key(&order, cap->pad_order.keys[i]);
      }
    }

    for (int i = 0; i < part.count; ++i) {
      ui_keybind_push_key(&order, part.keys[i]);
    }

    cap->pad_order = order;
    part           = order;
  }

  if (part.count > 0) {
    /* releasing some keys keeps the combo */
    if (!cap->combo_active || part.count >= cap->pending.count) {
      cap->pending = part;
    }

    cap->combo_active = true;
  } else if (cap->combo_active) {
    cap->combo_active = false;
    cap->capturing    = false;
    cap->pad_order    = KEYBIND_NULL;
  }
}

static keybind_t
ui_keybind_capture_result(ui_keybind_capture_t *cap)
{
  return cap->gamepad ? keybind_merge(keybind_keyboard_part(cap->original), cap->pending)
                      : keybind_merge(cap->pending, keybind_gamepad_part(cap->original));
}

static void
ui_keybind_capture_finish(ui_keybind_capture_t *cap, bool accepted)
{
  cap->result_ready    = true;
  cap->result_accepted = accepted;
  cap->result_hash     = cap->owner;
  cap->result          = ui_keybind_capture_result(cap);

  cap->active       = false;
  cap->capturing    = false;
  cap->combo_active = false;
  cap->owner        = 0;
  cap->original     = KEYBIND_NULL;
  cap->pending      = KEYBIND_NULL;
  cap->pad_order    = KEYBIND_NULL;
  cap->target       = NULL;
}

static void
ui_keybind_capture_start(ui_keybind_capture_t *cap, uint64_t owner, keybind_t *bind)
{
  /* sets the part of the device that opened the dialog */
  cap->gamepad      = globals.ui_manager.gamepad_input;
  cap->active       = true;
  cap->capturing    = false;
  cap->combo_active = false;
  cap->owner        = owner;
  cap->original     = *bind;
  cap->pending      = cap->gamepad ? keybind_gamepad_part(*bind) : keybind_keyboard_part(*bind);
  cap->pad_order    = KEYBIND_NULL;
  cap->target       = bind;
}

static bool
ui_keybind_capture_event_is_part(ui_keybind_capture_t *cap, input_event_t *ev)
{
  if (cap->gamepad) {
    return input_key_is_gamepad(ev->key);
  }

  switch (ev->kind) {
  case INPUT_EVENT_KEY_DOWN:
  case INPUT_EVENT_KEY_UP:
  case INPUT_EVENT_KEY_CHAR:
    return !input_key_is_gamepad(ev->key);

  case INPUT_EVENT_MOUSE_DOWN:
  case INPUT_EVENT_MOUSE_UP:
  case INPUT_EVENT_MOUSE_DBLCLICK:
    /* the left button stays the UI's */
    return ev->key != INPUT_KEY_MOUSE_LEFT;
  }
  return false;
}

/* false: the dialog gets the event like any window */
bool
ui_keybind_capture_input(ui_keybind_capture_t *cap, input_event_t *ev)
{
  if (!cap || !ev || !cap->active) {
    return false;
  }

  if (ev->kind == INPUT_EVENT_APP_ACTIVATION && !ev->app_activated) {
    cap->capturing    = false;
    cap->combo_active = false;
    return false;
  }

  if (!cap->capturing || !ui_keybind_capture_event_is_part(cap, ev)) {
    return false;
  }

  if (ev->kind != INPUT_EVENT_ANALOG && ev->kind != INPUT_EVENT_KEY_CHAR && !ev->is_repeat) {
    ui_keybind_capture_sync_from_state(cap);
  }
  return true;
}

str_t
ui_keybind_capture_display_text(ui_keybind_capture_t *cap, arena_t *arena)
{
  keybind_t result = ui_keybind_capture_result(cap);

  if (keybind_is_valid(result)) {
    return keybind_to_str(result, arena);
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

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    if (cap->active && cap->owner == hash) {
      ui_button_str(ctx, ui_keybind_capture_display_text(cap, tmp.arena));
    } else {
      str_t s = keybind_is_valid(*bind) ? keybind_to_str(*bind, tmp.arena) : STR_LIT("<unassigned>");

      if (ui_button_str(ctx, s)) {
        ui_keybind_capture_start(cap, hash, bind);
      }
    }
  }
  scratch_end(tmp);

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

  nk_flags    flags = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE;
  const char *name  = "keybind_capture";
  const char *title = cap->gamepad ? "Set gamepad shortcut" : "Set keyboard shortcut";

  struct nk_rect rect = centered_window_rect(460.0f, 190.0f, vw, vh);
  rect                = clamp_window_rect(rect, 380.0f, 190.0f, 460.0f, 190.0f, vw, vh);

  tmp_arena_t tmp = scratch_begin(NULL);
  if (nk_begin_titled(ctx, name, title, rect, flags)) {
    str_t text = cap->pending.count ? keybind_to_str(cap->pending, tmp.arena) : STR_LIT("<none>");

    if (cap->capturing && !cap->combo_active) {
      text = cap->gamepad ? STR_LIT("Press buttons...") : STR_LIT("Press keys...");
    }

    nk_layout_row_dynamic(ctx, 44.0f, 1);
    nk_label_colored(ctx, (const char *)text.data, NK_TEXT_CENTERED, UI_C_TEXT);

    action_t *other = cap->capturing ? NULL : mod_keybind_find_conflict(&globals.mod_manager, &globals.actions, ui_keybind_capture_result(cap), cap->target);
    str_t     status;

    if (other) {
      status = str_push_fmt(tmp.arena, "Also used by '%.*s' (%.*s)", STR_ARG(other->label), STR_ARG(other->source));
    } else if (cap->capturing) {
      status = cap->gamepad ? STR_LIT("The first button leads; release them all to finish") : STR_LIT("Release them all to finish");
    } else if (cap->gamepad) {
      status = str_push_fmt(tmp.arena, "A combo works at once, a single button when held for %u ms", actions_get_hold_ms(&globals.actions));
    } else {
      status = STR_LIT("");
    }

    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label_colored(ctx, (const char *)status.data, NK_TEXT_CENTERED, other ? UI_C_GOLD : UI_C_TEXT_DIM);

    nk_layout_row_dynamic(ctx, 32.0f, 4);

    if (ui_button_str(ctx, STR_LIT("Apply"))) {
      ui_keybind_capture_finish(cap, true);
    }

    /* starts on release, so that press is not captured */
    if (cap->capturing) {
      nk_widget_disable_begin(ctx);
    }

    if (ui_button_str(ctx, cap->capturing ? STR_LIT("Capturing") : STR_LIT("Capture"))) {
      cap->capturing    = true;
      cap->combo_active = false;
      cap->pad_order    = KEYBIND_NULL;
    }

    if (cap->capturing) {
      nk_widget_disable_end(ctx);
    }

    if (ui_button_str(ctx, STR_LIT("Clear"))) {
      cap->pending = KEYBIND_NULL;
    }

    if (ui_button_str(ctx, STR_LIT("Cancel"))) {
      ui_keybind_capture_finish(cap, false);
    }
  }
  nk_end(ctx);
  scratch_end(tmp);

  if (!ui_keybind_capture_is_open(cap)) {
    return;
  }

  if (nk_window_is_closed(ctx, name)) {
    ui_keybind_capture_finish(cap, false);
  } else {
    nk_window_set_focus(ctx, name);
  }
}
