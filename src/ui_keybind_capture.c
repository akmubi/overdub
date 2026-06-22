#include "ui_keybind_capture.h"

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
  cap->live = live;

  if (keybind_is_valid(live)) {
    if (!cap->combo_active) {
      cap->combo_active = true;
      cap->pending      = live;
    } else if (!keybind_is_valid(cap->pending) || live.count >= cap->pending.count) {
      cap->pending = live;
    }
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
ui_keybind_capture_clear_candidate(ui_keybind_capture_t *cap)
{
  cap->combo_active = false;
  cap->live         = KEYBIND_NULL;
  cap->pending      = KEYBIND_NULL;
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
}

bool
ui_keybind_capture_input(ui_keybind_capture_t *cap, input_event_t *ev)
{
  if (!cap || !ev || !cap->active) {
    return false;
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

  struct nk_rect rect = centered_window_rect(420.0f, 180.0f, vw, vh);
  rect                = clamp_window_rect(rect, 320.0f, 160.0f, 420.0f, 180.0f, vw, vh);

  tmp_arena_t tmp = scratch_begin(NULL);
  if (nk_begin(ctx, name, rect, flags)) {
    struct nk_rect content     = nk_window_get_content_region(ctx);
    str_t          text        = ui_keybind_capture_display_text(cap, tmp.arena);
    float          gap_y       = ctx->style.window.spacing.y;
    float          group_pad_y = ctx->style.window.group_padding.y;
    float          button_h    = 32.0f;
    float          footer_h    = button_h + group_pad_y * 2.0f;
    float          body_h      = NK_MAX(80.0f, content.h - footer_h - gap_y * 3.0f);
    float          text_h      = body_h - group_pad_y * 2.0f;
    struct nk_rect body_r      = nk_rect(0, 0, content.w, body_h);
    struct nk_rect footer_r    = nk_rect(0, body_h + gap_y, content.w, footer_h);

    nk_layout_space_begin(ctx, NK_STATIC, content.h, 2);
    {
      nk_layout_space_push(ctx, body_r);
      if (nk_group_begin(ctx, "keybind_capture.body", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, text_h, 1);
        nk_text(ctx, (const char *)text.data, (int)text.len, NK_TEXT_CENTERED);
        nk_group_end(ctx);
      }

      nk_layout_space_push(ctx, footer_r);
      if (nk_group_begin(ctx, "keybind_capture.footer", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, button_h, 3);

        if (ui_button_str(ctx, STR_LIT("Apply"))) {
          ui_keybind_capture_finish(cap, true);
        }

        if (ui_button_str(ctx, STR_LIT("Clear"))) {
          ui_keybind_capture_clear_candidate(cap);
        }

        if (ui_button_str(ctx, STR_LIT("Cancel"))) {
          ui_keybind_capture_finish(cap, false);
        }
        nk_group_end(ctx);
      }
    }
    nk_layout_space_end(ctx);
  }
  nk_end(ctx);
  scratch_end(tmp);

  if (nk_window_is_closed(ctx, name)) {
    ui_keybind_capture_finish(cap, false);
  } else {
    nk_window_set_focus(ctx, name);
  }
}
