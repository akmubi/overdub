#include "input.h"

#include "arena.h"
#include "globals.h"
#include "scratch.h"
#include "str.h"
#include "types.h"

#include <windows.h>

/* ================================================= INPUT KEY MAP ================================================== */

typedef struct input_key_map_entry_s input_key_map_entry_t;
struct input_key_map_entry_s {
  str_t    ue_name; // FKey name (e.g. "A", "Gamepad_RightTrigger")
  uint32_t cmp_idx; // resolved at init, 0 - unresolved
};

static input_key_map_entry_t g_key_map[INPUT_KEY_MAX] = {
  /* keyboard: letters */
  [INPUT_KEY_A] = {STR_CLIT("A"), 0},
  [INPUT_KEY_B] = {STR_CLIT("B"), 0},
  [INPUT_KEY_C] = {STR_CLIT("C"), 0},
  [INPUT_KEY_D] = {STR_CLIT("D"), 0},
  [INPUT_KEY_E] = {STR_CLIT("E"), 0},
  [INPUT_KEY_F] = {STR_CLIT("F"), 0},
  [INPUT_KEY_G] = {STR_CLIT("G"), 0},
  [INPUT_KEY_H] = {STR_CLIT("H"), 0},
  [INPUT_KEY_I] = {STR_CLIT("I"), 0},
  [INPUT_KEY_J] = {STR_CLIT("J"), 0},
  [INPUT_KEY_K] = {STR_CLIT("K"), 0},
  [INPUT_KEY_L] = {STR_CLIT("L"), 0},
  [INPUT_KEY_M] = {STR_CLIT("M"), 0},
  [INPUT_KEY_N] = {STR_CLIT("N"), 0},
  [INPUT_KEY_O] = {STR_CLIT("O"), 0},
  [INPUT_KEY_P] = {STR_CLIT("P"), 0},
  [INPUT_KEY_Q] = {STR_CLIT("Q"), 0},
  [INPUT_KEY_R] = {STR_CLIT("R"), 0},
  [INPUT_KEY_S] = {STR_CLIT("S"), 0},
  [INPUT_KEY_T] = {STR_CLIT("T"), 0},
  [INPUT_KEY_U] = {STR_CLIT("U"), 0},
  [INPUT_KEY_V] = {STR_CLIT("V"), 0},
  [INPUT_KEY_W] = {STR_CLIT("W"), 0},
  [INPUT_KEY_X] = {STR_CLIT("X"), 0},
  [INPUT_KEY_Y] = {STR_CLIT("Y"), 0},
  [INPUT_KEY_Z] = {STR_CLIT("Z"), 0},

  /* keyboard: punctuation */
  [INPUT_KEY_COMMA]         = {STR_CLIT("Comma"), 0},
  [INPUT_KEY_PERIOD]        = {STR_CLIT("Period"), 0},
  [INPUT_KEY_SLASH]         = {STR_CLIT("Slash"), 0},
  [INPUT_KEY_SEMICOLON]     = {STR_CLIT("Semicolon"), 0},
  [INPUT_KEY_APOSTROPHE]    = {STR_CLIT("Apostrophe"), 0},
  [INPUT_KEY_LEFT_BRACKET]  = {STR_CLIT("LeftBracket"), 0},
  [INPUT_KEY_RIGHT_BRACKET] = {STR_CLIT("RightBracket"), 0},
  [INPUT_KEY_BACKSLASH]     = {STR_CLIT("Backslash"), 0},
  [INPUT_KEY_HYPHEN]        = {STR_CLIT("Hyphen"), 0},
  [INPUT_KEY_EQUALS]        = {STR_CLIT("Equals"), 0},
  [INPUT_KEY_BACKTICK]      = {STR_CLIT("Tilde"), 0},

  /* keyboard: digits */
  [INPUT_KEY_1] = {STR_CLIT("One"), 0},
  [INPUT_KEY_2] = {STR_CLIT("Two"), 0},
  [INPUT_KEY_3] = {STR_CLIT("Three"), 0},
  [INPUT_KEY_4] = {STR_CLIT("Four"), 0},
  [INPUT_KEY_5] = {STR_CLIT("Five"), 0},
  [INPUT_KEY_6] = {STR_CLIT("Six"), 0},
  [INPUT_KEY_7] = {STR_CLIT("Seven"), 0},
  [INPUT_KEY_8] = {STR_CLIT("Eight"), 0},
  [INPUT_KEY_9] = {STR_CLIT("Nine"), 0},
  [INPUT_KEY_0] = {STR_CLIT("Zero"), 0},

  /* keyboard: function */
  [INPUT_KEY_F1]  = {STR_CLIT("F1"), 0},
  [INPUT_KEY_F2]  = {STR_CLIT("F2"), 0},
  [INPUT_KEY_F3]  = {STR_CLIT("F3"), 0},
  [INPUT_KEY_F4]  = {STR_CLIT("F4"), 0},
  [INPUT_KEY_F5]  = {STR_CLIT("F5"), 0},
  [INPUT_KEY_F6]  = {STR_CLIT("F6"), 0},
  [INPUT_KEY_F7]  = {STR_CLIT("F7"), 0},
  [INPUT_KEY_F8]  = {STR_CLIT("F8"), 0},
  [INPUT_KEY_F9]  = {STR_CLIT("F9"), 0},
  [INPUT_KEY_F10] = {STR_CLIT("F10"), 0},
  [INPUT_KEY_F11] = {STR_CLIT("F11"), 0},
  [INPUT_KEY_F12] = {STR_CLIT("F12"), 0},

  /* keyboard: navigation */
  [INPUT_KEY_UP]        = {STR_CLIT("Up"), 0},
  [INPUT_KEY_DOWN]      = {STR_CLIT("Down"), 0},
  [INPUT_KEY_LEFT]      = {STR_CLIT("Left"), 0},
  [INPUT_KEY_RIGHT]     = {STR_CLIT("Right"), 0},
  [INPUT_KEY_HOME]      = {STR_CLIT("Home"), 0},
  [INPUT_KEY_END]       = {STR_CLIT("End"), 0},
  [INPUT_KEY_PAGE_UP]   = {STR_CLIT("PageUp"), 0},
  [INPUT_KEY_PAGE_DOWN] = {STR_CLIT("PageDown"), 0},

  /* keyboard: modifiers */
  [INPUT_KEY_LEFT_SHIFT]  = {STR_CLIT("LeftShift"), 0},
  [INPUT_KEY_RIGHT_SHIFT] = {STR_CLIT("RightShift"), 0},
  [INPUT_KEY_LEFT_CTRL]   = {STR_CLIT("LeftControl"), 0},
  [INPUT_KEY_RIGHT_CTRL]  = {STR_CLIT("RightControl"), 0},
  [INPUT_KEY_LEFT_ALT]    = {STR_CLIT("LeftAlt"), 0},
  [INPUT_KEY_RIGHT_ALT]   = {STR_CLIT("RightAlt"), 0},

  /* keyboard: common */
  [INPUT_KEY_SPACE]     = {STR_CLIT("SpaceBar"), 0},
  [INPUT_KEY_ENTER]     = {STR_CLIT("Enter"), 0},
  [INPUT_KEY_ESCAPE]    = {STR_CLIT("Escape"), 0},
  [INPUT_KEY_TAB]       = {STR_CLIT("Tab"), 0},
  [INPUT_KEY_BACKSPACE] = {STR_CLIT("BackSpace"), 0},
  [INPUT_KEY_CAPS_LOCK] = {STR_CLIT("CapsLock"), 0},
  [INPUT_KEY_INSERT]    = {STR_CLIT("Insert"), 0},
  [INPUT_KEY_DELETE]    = {STR_CLIT("Delete"), 0},

  /* keyboard: numpad */
  [INPUT_KEY_NUM_0]        = {STR_CLIT("NumPadZero"), 0},
  [INPUT_KEY_NUM_1]        = {STR_CLIT("NumPadOne"), 0},
  [INPUT_KEY_NUM_2]        = {STR_CLIT("NumPadTwo"), 0},
  [INPUT_KEY_NUM_3]        = {STR_CLIT("NumPadThree"), 0},
  [INPUT_KEY_NUM_4]        = {STR_CLIT("NumPadFour"), 0},
  [INPUT_KEY_NUM_5]        = {STR_CLIT("NumPadFive"), 0},
  [INPUT_KEY_NUM_6]        = {STR_CLIT("NumPadSix"), 0},
  [INPUT_KEY_NUM_7]        = {STR_CLIT("NumPadSeven"), 0},
  [INPUT_KEY_NUM_8]        = {STR_CLIT("NumPadEight"), 0},
  [INPUT_KEY_NUM_9]        = {STR_CLIT("NumPadNine"), 0},
  [INPUT_KEY_NUM_MULTIPLY] = {STR_CLIT("Multiply"), 0},
  [INPUT_KEY_NUM_ADD]      = {STR_CLIT("Add"), 0},
  [INPUT_KEY_NUM_SUBTRACT] = {STR_CLIT("Subtract"), 0},
  [INPUT_KEY_NUM_DECIMAL]  = {STR_CLIT("Decimal"), 0},
  [INPUT_KEY_NUM_DIVIDE]   = {STR_CLIT("Divide"), 0},

  /* keyboard: misc */
  [INPUT_KEY_PAUSE]        = {STR_CLIT("Pause"), 0},
  [INPUT_KEY_PRINT_SCREEN] = {STR_CLIT("PrintScreen"), 0},
  [INPUT_KEY_SCROLL_LOCK]  = {STR_CLIT("ScrollLock"), 0},
  [INPUT_KEY_NUM_LOCK]     = {STR_CLIT("NumLock"), 0},

  /* mouse: buttons */
  [INPUT_KEY_MOUSE_LEFT]    = {STR_CLIT("LeftMouseButton"), 0},
  [INPUT_KEY_MOUSE_RIGHT]   = {STR_CLIT("RightMouseButton"), 0},
  [INPUT_KEY_MOUSE_MIDDLE]  = {STR_CLIT("MiddleMouseButton"), 0},
  [INPUT_KEY_MOUSE_THUMB_1] = {STR_CLIT("ThumbMouseButton"), 0},
  [INPUT_KEY_MOUSE_THUMB_2] = {STR_CLIT("ThumbMouseButton2"), 0},

  /* mouse: axes */
  [INPUT_KEY_MOUSE_X]          = {STR_CLIT("MouseX"), 0},
  [INPUT_KEY_MOUSE_Y]          = {STR_CLIT("MouseY"), 0},
  [INPUT_KEY_MOUSE_WHEEL_UP]   = {STR_CLIT("MouseWheelUp"), 0},   // not an actual name
  [INPUT_KEY_MOUSE_WHEEL_DOWN] = {STR_CLIT("MouseWheelDown"), 0}, // not an actual name

  /* gamepad: buttons */
  [INPUT_KEY_GAMEPAD_FACE_BOTTOM]   = {STR_CLIT("Gamepad_FaceButton_Bottom"), 0},
  [INPUT_KEY_GAMEPAD_FACE_RIGHT]    = {STR_CLIT("Gamepad_FaceButton_Right"), 0},
  [INPUT_KEY_GAMEPAD_FACE_LEFT]     = {STR_CLIT("Gamepad_FaceButton_Left"), 0},
  [INPUT_KEY_GAMEPAD_FACE_TOP]      = {STR_CLIT("Gamepad_FaceButton_Top"), 0},
  [INPUT_KEY_GAMEPAD_LB]            = {STR_CLIT("Gamepad_LeftShoulder"), 0},
  [INPUT_KEY_GAMEPAD_RB]            = {STR_CLIT("Gamepad_RightShoulder"), 0},
  [INPUT_KEY_GAMEPAD_LT]            = {STR_CLIT("Gamepad_LeftTrigger"), 0},
  [INPUT_KEY_GAMEPAD_RT]            = {STR_CLIT("Gamepad_RightTrigger"), 0},
  [INPUT_KEY_GAMEPAD_L3]            = {STR_CLIT("Gamepad_LeftThumbstick"), 0},
  [INPUT_KEY_GAMEPAD_R3]            = {STR_CLIT("Gamepad_RightThumbstick"), 0},
  [INPUT_KEY_GAMEPAD_SPECIAL_LEFT]  = {STR_CLIT("Gamepad_Special_Left"), 0},
  [INPUT_KEY_GAMEPAD_SPECIAL_RIGHT] = {STR_CLIT("Gamepad_Special_Right"), 0},
  [INPUT_KEY_GAMEPAD_DPAD_UP]       = {STR_CLIT("Gamepad_DPad_Up"), 0},
  [INPUT_KEY_GAMEPAD_DPAD_DOWN]     = {STR_CLIT("Gamepad_DPad_Down"), 0},
  [INPUT_KEY_GAMEPAD_DPAD_LEFT]     = {STR_CLIT("Gamepad_DPad_Left"), 0},
  [INPUT_KEY_GAMEPAD_DPAD_RIGHT]    = {STR_CLIT("Gamepad_DPad_Right"), 0},

  /* gamepad: axes */
  [INPUT_KEY_GAMEPAD_LEFT_X]  = {STR_CLIT("Gamepad_LeftX"), 0},
  [INPUT_KEY_GAMEPAD_LEFT_Y]  = {STR_CLIT("Gamepad_LeftY"), 0},
  [INPUT_KEY_GAMEPAD_RIGHT_X] = {STR_CLIT("Gamepad_RightX"), 0},
  [INPUT_KEY_GAMEPAD_RIGHT_Y] = {STR_CLIT("Gamepad_RightY"), 0},
  [INPUT_KEY_GAMEPAD_LT_AXIS] = {STR_CLIT("Gamepad_LeftTriggerAxis"), 0},
  [INPUT_KEY_GAMEPAD_RT_AXIS] = {STR_CLIT("Gamepad_RightTriggerAxis"), 0},
};

void
input_key_map_init(void)
{
  MASSERT(globals.name_pool != NULL, "name pool is missing");

  /* resolve each UE4 key name to its cmp_idx by scanning the name pool */
  fname_entry_allocator_t *entries    = &globals.name_pool->entries;
  uint32_t                 num_blocks = entries->current_block + 1;

  for (uint32_t block = 0; block < num_blocks && block < FNAME_MAX_BLOCKS; ++block) {
    uint8_t *base = entries->blocks[block];
    if (!base) {
      continue;
    }

    uint32_t end_offset = (1u << FNAME_BLOCK_OFFSET_BITS) * 2;
    if (block == entries->current_block) {
      end_offset = entries->current_byte_cursor;
    }

    uint32_t byte_offset = 0;
    while (byte_offset < end_offset) {
      fname_entry_t *entry = (fname_entry_t *)(base + byte_offset);
      uint16_t       len   = entry->header.len;

      if (len > 0 && !entry->header.is_wide) {
        for (int i = 0; i < COUNTOF(g_key_map); ++i) {
          if (g_key_map[i].cmp_idx != 0) {
            continue; // already resolved
          }

          str_t ue_name    = g_key_map[i].ue_name;
          str_t entry_name = str_make(entry->name.ansi, (uint64_t)len);

          if (str_equal(ue_name, entry_name, 0)) {
            uint32_t cmp_idx     = (block << FNAME_BLOCK_OFFSET_BITS) | (byte_offset / 2);
            g_key_map[i].cmp_idx = cmp_idx;

            // LOG_DEBUG("%.*s: cmp_idx: %d, block: %d, byte_offset: %d", STR_ARG(ue_name), cmp_idx, block,
            // byte_offset);
            break;
          }
        }
      }

      /* advance to next entry: header (2 bytes) + payload, aligned to 2 */
      uint32_t entry_size = sizeof(fname_entry_header_t);

      entry_size  += entry->header.is_wide ? (len * 2) : len;
      entry_size   = ALIGN_UP(entry_size, 2);
      byte_offset += entry_size;
    }
  }
}

input_key_kind_t
input_key_from_fname(fname_t key_name)
{
  for (int i = 0; i < COUNTOF(g_key_map); ++i) {
    if (key_name.cmp_idx == g_key_map[i].cmp_idx) {
      return (input_key_kind_t)(i);
    }
  }
  return INPUT_KEY_NONE;
}

input_key_kind_t
input_key_from_str(str_t name)
{
  for (int i = 0; i < COUNTOF(g_key_map); ++i) {
    if (str_equal(name, g_key_map[i].ue_name, 0)) {
      return (input_key_kind_t)(i);
    }
  }
  return INPUT_KEY_NONE;
}

bool
input_key_is_down(input_key_kind_t kind)
{
  if (kind >= INPUT_KEY_MAX) {
    return false;
  }
  return globals.keys_down[kind];
}

bool
input_key_was_down(input_key_kind_t kind)
{
  if (kind >= INPUT_KEY_MAX) {
    return false;
  }
  return globals.prev_keys_down[kind];
}

str_t
input_key_to_str(input_key_kind_t key)
{
  if (key >= INPUT_KEY_MAX) {
    return (str_t){0};
  }
  return g_key_map[key].ue_name;
}

str_t
input_event_kind_to_str(input_event_kind_t kind)
{
  switch (kind) {
  case INPUT_EVENT_KEY_DOWN:
    return STR_LIT("key down");
  case INPUT_EVENT_KEY_UP:
    return STR_LIT("key up");
  case INPUT_EVENT_KEY_CHAR:
    return STR_LIT("key character");
  case INPUT_EVENT_MOUSE_DOWN:
    return STR_LIT("mouse down");
  case INPUT_EVENT_MOUSE_UP:
    return STR_LIT("mouse up");
  case INPUT_EVENT_MOUSE_DBLCLICK:
    return STR_LIT("mouse double click");
  case INPUT_EVENT_MOUSE_MOVE:
    return STR_LIT("mouse move");
  case INPUT_EVENT_MOUSE_WHEEL:
    return STR_LIT("mouse wheel");
  case INPUT_EVENT_ANALOG:
    return STR_LIT("analog");
  case INPUT_EVENT_APP_ACTIVATION:
    return STR_LIT("app activation");
  }
  return (str_t){0};
}

/* ================================================ EVENT CONVERSION ================================================ */

static input_mod_flags_t
mod_flags_from_keys_state(fmodifier_keys_state_t s)
{
  input_mod_flags_t flags = 0;
  if (s.is_left_shift_down | s.is_right_shift_down) {
    flags |= INPUT_MOD_SHIFT;
  }

  if (s.is_left_control_down | s.is_right_control_down) {
    flags |= INPUT_MOD_CTRL;
  }

  if (s.is_left_alt_down | s.is_right_alt_down) {
    flags |= INPUT_MOD_ALT;
  }
  return flags;
}

input_event_t
input_event_from_key_event(input_event_kind_t kind, fkey_event_t *e)
{
  return (input_event_t){
    .kind      = kind,
    .modifiers = mod_flags_from_keys_state(e->base.modifier_keys),
    .key       = input_key_from_fname(e->key.name),
    .is_repeat = e->base.is_repeat,
  };
}

input_event_t
input_event_from_character_event(fcharacter_event_t *e)
{
  return (input_event_t){
    .kind      = INPUT_EVENT_KEY_CHAR,
    .modifiers = mod_flags_from_keys_state(e->base.modifier_keys),
    .character = (uint32_t)e->character,
    .is_repeat = e->base.is_repeat,
  };
}

input_event_t
input_event_from_pointer_event(input_event_kind_t kind, fpointer_event_t *e)
{
  input_event_t out = {
    .kind        = kind,
    .modifiers   = mod_flags_from_keys_state(e->base.modifier_keys),
    .key         = input_key_from_fname(e->effecting_button.name),
    .screen_x    = (uint32_t)e->screen_space_pos.x,
    .screen_y    = (uint32_t)e->screen_space_pos.y,
    .delta_x     = e->cursor_delta.x,
    .delta_y     = e->cursor_delta.y,
    .wheel_delta = e->wheel_or_gesture_delta.y,
  };

  HWND hwnd = (HWND)globals.hwnd;

  if (hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);

    out.client_x = pt.x;
    out.client_y = pt.y;
  } else {
    out.client_x = out.screen_x;
    out.client_y = out.screen_y;
  }

  if (kind == INPUT_EVENT_MOUSE_WHEEL) {
    if (out.wheel_delta > 0) {
      out.key = INPUT_KEY_MOUSE_WHEEL_UP;
    } else {
      out.key = INPUT_KEY_MOUSE_WHEEL_DOWN;
    }
  }

  return out;
}

input_event_t
input_event_from_analog_event(fanalog_input_event_t *e)
{
  return (input_event_t){
    .kind         = INPUT_EVENT_ANALOG,
    .modifiers    = mod_flags_from_keys_state(e->base.base.modifier_keys),
    .key          = input_key_from_fname(e->base.key.name),
    .is_repeat    = e->base.base.is_repeat,
    .analog_value = e->analog_value,
  };
}

input_event_t
input_event_from_app_activation_event(bool app_activated)
{
  return (input_event_t){
    .kind          = INPUT_EVENT_APP_ACTIVATION,
    .app_activated = app_activated,
  };
}

bool
input_event_is_mouse(input_event_t *ev)
{
  switch (ev->kind) {
  case INPUT_EVENT_MOUSE_DOWN:
  case INPUT_EVENT_MOUSE_UP:
  case INPUT_EVENT_MOUSE_DBLCLICK:
  case INPUT_EVENT_MOUSE_MOVE:
  case INPUT_EVENT_MOUSE_WHEEL: {
    return true;
  }
  }
  return false;
}

bool
input_event_is_keyboard(input_event_t *ev)
{
  switch (ev->kind) {
  case INPUT_EVENT_KEY_DOWN:
  case INPUT_EVENT_KEY_UP:
  case INPUT_EVENT_KEY_CHAR: {
    return true;
  }
  }
  return false;
}

bool
input_event_is_gamepad(input_event_t *ev)
{
  switch (ev->key) {
  case INPUT_KEY_GAMEPAD_FACE_BOTTOM:
  case INPUT_KEY_GAMEPAD_FACE_RIGHT:
  case INPUT_KEY_GAMEPAD_FACE_LEFT:
  case INPUT_KEY_GAMEPAD_FACE_TOP:
  case INPUT_KEY_GAMEPAD_LB:
  case INPUT_KEY_GAMEPAD_RB:
  case INPUT_KEY_GAMEPAD_LT:
  case INPUT_KEY_GAMEPAD_RT:
  case INPUT_KEY_GAMEPAD_L3:
  case INPUT_KEY_GAMEPAD_R3:
  case INPUT_KEY_GAMEPAD_SPECIAL_LEFT:
  case INPUT_KEY_GAMEPAD_SPECIAL_RIGHT:
  case INPUT_KEY_GAMEPAD_DPAD_UP:
  case INPUT_KEY_GAMEPAD_DPAD_DOWN:
  case INPUT_KEY_GAMEPAD_DPAD_LEFT:
  case INPUT_KEY_GAMEPAD_DPAD_RIGHT:
  case INPUT_KEY_GAMEPAD_LEFT_X:
  case INPUT_KEY_GAMEPAD_LEFT_Y:
  case INPUT_KEY_GAMEPAD_RIGHT_X:
  case INPUT_KEY_GAMEPAD_RIGHT_Y:
  case INPUT_KEY_GAMEPAD_LT_AXIS:
  case INPUT_KEY_GAMEPAD_RT_AXIS: {
    return true;
  }
  }

  return false;
}

/* ================================================= KEYBIND PARSER ================================================= */

typedef struct key_alias_s key_alias_t;
struct key_alias_s {
  str_t            name;
  input_key_kind_t key;
};

static const key_alias_t g_key_aliases[] = {
  {STR_CLIT("None"), INPUT_KEY_NONE},

  /* letters */
  {STR_CLIT("A"), INPUT_KEY_A},
  {STR_CLIT("B"), INPUT_KEY_B},
  {STR_CLIT("C"), INPUT_KEY_C},
  {STR_CLIT("D"), INPUT_KEY_D},
  {STR_CLIT("E"), INPUT_KEY_E},
  {STR_CLIT("F"), INPUT_KEY_F},
  {STR_CLIT("G"), INPUT_KEY_G},
  {STR_CLIT("H"), INPUT_KEY_H},
  {STR_CLIT("I"), INPUT_KEY_I},
  {STR_CLIT("J"), INPUT_KEY_J},
  {STR_CLIT("K"), INPUT_KEY_K},
  {STR_CLIT("L"), INPUT_KEY_L},
  {STR_CLIT("M"), INPUT_KEY_M},
  {STR_CLIT("N"), INPUT_KEY_N},
  {STR_CLIT("O"), INPUT_KEY_O},
  {STR_CLIT("P"), INPUT_KEY_P},
  {STR_CLIT("Q"), INPUT_KEY_Q},
  {STR_CLIT("R"), INPUT_KEY_R},
  {STR_CLIT("S"), INPUT_KEY_S},
  {STR_CLIT("T"), INPUT_KEY_T},
  {STR_CLIT("U"), INPUT_KEY_U},
  {STR_CLIT("V"), INPUT_KEY_V},
  {STR_CLIT("W"), INPUT_KEY_W},
  {STR_CLIT("X"), INPUT_KEY_X},
  {STR_CLIT("Y"), INPUT_KEY_Y},
  {STR_CLIT("Z"), INPUT_KEY_Z},

  /* digits */
  {STR_CLIT("0"), INPUT_KEY_0},
  {STR_CLIT("1"), INPUT_KEY_1},
  {STR_CLIT("2"), INPUT_KEY_2},
  {STR_CLIT("3"), INPUT_KEY_3},
  {STR_CLIT("4"), INPUT_KEY_4},
  {STR_CLIT("5"), INPUT_KEY_5},
  {STR_CLIT("6"), INPUT_KEY_6},
  {STR_CLIT("7"), INPUT_KEY_7},
  {STR_CLIT("8"), INPUT_KEY_8},
  {STR_CLIT("9"), INPUT_KEY_9},

  /* function keys */
  {STR_CLIT("F1"), INPUT_KEY_F1},
  {STR_CLIT("F2"), INPUT_KEY_F2},
  {STR_CLIT("F3"), INPUT_KEY_F3},
  {STR_CLIT("F4"), INPUT_KEY_F4},
  {STR_CLIT("F5"), INPUT_KEY_F5},
  {STR_CLIT("F6"), INPUT_KEY_F6},
  {STR_CLIT("F7"), INPUT_KEY_F7},
  {STR_CLIT("F8"), INPUT_KEY_F8},
  {STR_CLIT("F9"), INPUT_KEY_F9},
  {STR_CLIT("F10"), INPUT_KEY_F10},
  {STR_CLIT("F11"), INPUT_KEY_F11},
  {STR_CLIT("F12"), INPUT_KEY_F12},

  /* modifiers - first entry is canonical display name */
  {STR_CLIT("Ctrl"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("LCtrl"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("LeftCtrl"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("left_ctrl"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("Control"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("LControl"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("LeftControl"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("left_control"), INPUT_KEY_LEFT_CTRL},
  {STR_CLIT("RCtrl"), INPUT_KEY_RIGHT_CTRL},
  {STR_CLIT("RightCtrl"), INPUT_KEY_RIGHT_CTRL},
  {STR_CLIT("right_ctrl"), INPUT_KEY_RIGHT_CTRL},
  {STR_CLIT("RControl"), INPUT_KEY_RIGHT_CTRL},
  {STR_CLIT("RightControl"), INPUT_KEY_RIGHT_CTRL},
  {STR_CLIT("right_control"), INPUT_KEY_RIGHT_CTRL},
  {STR_CLIT("Shift"), INPUT_KEY_LEFT_SHIFT},
  {STR_CLIT("LShift"), INPUT_KEY_LEFT_SHIFT},
  {STR_CLIT("LeftShift"), INPUT_KEY_LEFT_SHIFT},
  {STR_CLIT("left_shift"), INPUT_KEY_LEFT_SHIFT},
  {STR_CLIT("RShift"), INPUT_KEY_RIGHT_SHIFT},
  {STR_CLIT("RightShift"), INPUT_KEY_RIGHT_SHIFT},
  {STR_CLIT("right_shift"), INPUT_KEY_RIGHT_SHIFT},
  {STR_CLIT("Alt"), INPUT_KEY_LEFT_ALT},
  {STR_CLIT("LAlt"), INPUT_KEY_LEFT_ALT},
  {STR_CLIT("LeftAlt"), INPUT_KEY_LEFT_ALT},
  {STR_CLIT("left_alt"), INPUT_KEY_LEFT_ALT},
  {STR_CLIT("RAlt"), INPUT_KEY_RIGHT_ALT},
  {STR_CLIT("RightAlt"), INPUT_KEY_RIGHT_ALT},
  {STR_CLIT("right_alt"), INPUT_KEY_RIGHT_ALT},

  /* common keys */
  {STR_CLIT("Space"), INPUT_KEY_SPACE},
  {STR_CLIT("Enter"), INPUT_KEY_ENTER},
  {STR_CLIT("Return"), INPUT_KEY_ENTER},
  {STR_CLIT("Esc"), INPUT_KEY_ESCAPE},
  {STR_CLIT("Escape"), INPUT_KEY_ESCAPE},
  {STR_CLIT("Tab"), INPUT_KEY_TAB},
  {STR_CLIT("Backspace"), INPUT_KEY_BACKSPACE},
  {STR_CLIT("BS"), INPUT_KEY_BACKSPACE},
  {STR_CLIT("CapsLock"), INPUT_KEY_CAPS_LOCK},
  {STR_CLIT("Caps"), INPUT_KEY_CAPS_LOCK},
  {STR_CLIT("Insert"), INPUT_KEY_INSERT},
  {STR_CLIT("Ins"), INPUT_KEY_INSERT},
  {STR_CLIT("Delete"), INPUT_KEY_DELETE},
  {STR_CLIT("Del"), INPUT_KEY_DELETE},

  /* navigation */
  {STR_CLIT("Up"), INPUT_KEY_UP},
  {STR_CLIT("Down"), INPUT_KEY_DOWN},
  {STR_CLIT("Left"), INPUT_KEY_LEFT},
  {STR_CLIT("Right"), INPUT_KEY_RIGHT},
  {STR_CLIT("Home"), INPUT_KEY_HOME},
  {STR_CLIT("End"), INPUT_KEY_END},
  {STR_CLIT("PageUp"), INPUT_KEY_PAGE_UP},
  {STR_CLIT("page_up"), INPUT_KEY_PAGE_UP},
  {STR_CLIT("PgUp"), INPUT_KEY_PAGE_UP},
  {STR_CLIT("PageDown"), INPUT_KEY_PAGE_DOWN},
  {STR_CLIT("page_down"), INPUT_KEY_PAGE_DOWN},
  {STR_CLIT("PgDn"), INPUT_KEY_PAGE_DOWN},
  {STR_CLIT("PgDown"), INPUT_KEY_PAGE_DOWN},

  /* punctuation */
  {STR_CLIT(","), INPUT_KEY_COMMA},
  {STR_CLIT("Comma"), INPUT_KEY_COMMA},
  {STR_CLIT("."), INPUT_KEY_PERIOD},
  {STR_CLIT("Period"), INPUT_KEY_PERIOD},
  {STR_CLIT("Dot"), INPUT_KEY_PERIOD},
  {STR_CLIT("/"), INPUT_KEY_SLASH},
  {STR_CLIT("Slash"), INPUT_KEY_SLASH},
  {STR_CLIT(";"), INPUT_KEY_SEMICOLON},
  {STR_CLIT("Semicolon"), INPUT_KEY_SEMICOLON},
  {STR_CLIT("'"), INPUT_KEY_APOSTROPHE},
  {STR_CLIT("Apostrophe"), INPUT_KEY_APOSTROPHE},
  {STR_CLIT("\""), INPUT_KEY_APOSTROPHE},
  {STR_CLIT("Quote"), INPUT_KEY_APOSTROPHE},
  {STR_CLIT("["), INPUT_KEY_LEFT_BRACKET},
  {STR_CLIT("LBracket"), INPUT_KEY_LEFT_BRACKET},
  {STR_CLIT("LeftBracket"), INPUT_KEY_LEFT_BRACKET},
  {STR_CLIT("left_bracket"), INPUT_KEY_LEFT_BRACKET},
  {STR_CLIT("]"), INPUT_KEY_RIGHT_BRACKET},
  {STR_CLIT("RBracket"), INPUT_KEY_RIGHT_BRACKET},
  {STR_CLIT("RightBracket"), INPUT_KEY_RIGHT_BRACKET},
  {STR_CLIT("right_bracket"), INPUT_KEY_RIGHT_BRACKET},
  {STR_CLIT("\\"), INPUT_KEY_BACKSLASH},
  {STR_CLIT("Backslash"), INPUT_KEY_BACKSLASH},
  {STR_CLIT("-"), INPUT_KEY_HYPHEN},
  {STR_CLIT("Minus"), INPUT_KEY_HYPHEN},
  {STR_CLIT("Hyphen"), INPUT_KEY_HYPHEN},
  {STR_CLIT("="), INPUT_KEY_EQUALS},
  {STR_CLIT("Equals"), INPUT_KEY_EQUALS},
  {STR_CLIT("`"), INPUT_KEY_BACKTICK},
  {STR_CLIT("Backtick"), INPUT_KEY_BACKTICK},

  /* misc */
  {STR_CLIT("Pause"), INPUT_KEY_PAUSE},
  {STR_CLIT("Break"), INPUT_KEY_PAUSE},
  {STR_CLIT("PrintScreen"), INPUT_KEY_PRINT_SCREEN},
  {STR_CLIT("print_screen"), INPUT_KEY_PRINT_SCREEN},
  {STR_CLIT("PrtSc"), INPUT_KEY_PRINT_SCREEN},
  {STR_CLIT("ScrollLock"), INPUT_KEY_SCROLL_LOCK},
  {STR_CLIT("scroll_lock"), INPUT_KEY_SCROLL_LOCK},
  {STR_CLIT("ScrLk"), INPUT_KEY_SCROLL_LOCK},
  {STR_CLIT("NumLock"), INPUT_KEY_NUM_LOCK},
  {STR_CLIT("num_lock"), INPUT_KEY_NUM_LOCK},
  {STR_CLIT("NumLk"), INPUT_KEY_NUM_LOCK},

  /* numpad */
  {STR_CLIT("Num0"), INPUT_KEY_NUM_0},
  {STR_CLIT("Num1"), INPUT_KEY_NUM_1},
  {STR_CLIT("Num2"), INPUT_KEY_NUM_2},
  {STR_CLIT("Num3"), INPUT_KEY_NUM_3},
  {STR_CLIT("Num4"), INPUT_KEY_NUM_4},
  {STR_CLIT("Num5"), INPUT_KEY_NUM_5},
  {STR_CLIT("Num6"), INPUT_KEY_NUM_6},
  {STR_CLIT("Num7"), INPUT_KEY_NUM_7},
  {STR_CLIT("Num8"), INPUT_KEY_NUM_8},
  {STR_CLIT("Num9"), INPUT_KEY_NUM_9},
  {STR_CLIT("NumMul"), INPUT_KEY_NUM_MULTIPLY},
  {STR_CLIT("num_mul"), INPUT_KEY_NUM_MULTIPLY},
  {STR_CLIT("NumMultiply"), INPUT_KEY_NUM_MULTIPLY},
  {STR_CLIT("NumAdd"), INPUT_KEY_NUM_ADD},
  {STR_CLIT("num_add"), INPUT_KEY_NUM_ADD},
  {STR_CLIT("NumSub"), INPUT_KEY_NUM_SUBTRACT},
  {STR_CLIT("num_sub"), INPUT_KEY_NUM_SUBTRACT},
  {STR_CLIT("NumSubstract"), INPUT_KEY_NUM_SUBTRACT},
  {STR_CLIT("NumDec"), INPUT_KEY_NUM_DECIMAL},
  {STR_CLIT("num_dec"), INPUT_KEY_NUM_DECIMAL},
  {STR_CLIT("NumDecimal"), INPUT_KEY_NUM_DECIMAL},
  {STR_CLIT("NumDiv"), INPUT_KEY_NUM_DIVIDE},
  {STR_CLIT("num_div"), INPUT_KEY_NUM_DIVIDE},
  {STR_CLIT("NumDivde"), INPUT_KEY_NUM_DIVIDE},
  {STR_CLIT("NumPlus"), INPUT_KEY_NUM_ADD},
  {STR_CLIT("num_plus"), INPUT_KEY_NUM_ADD},

  /* mouse */
  {STR_CLIT("LMB"), INPUT_KEY_MOUSE_LEFT},
  {STR_CLIT("MouseLeft"), INPUT_KEY_MOUSE_LEFT},
  {STR_CLIT("mouse_left"), INPUT_KEY_MOUSE_LEFT},
  {STR_CLIT("RMB"), INPUT_KEY_MOUSE_RIGHT},
  {STR_CLIT("MouseRight"), INPUT_KEY_MOUSE_RIGHT},
  {STR_CLIT("mouse_right"), INPUT_KEY_MOUSE_RIGHT},
  {STR_CLIT("MMB"), INPUT_KEY_MOUSE_MIDDLE},
  {STR_CLIT("MouseMiddle"), INPUT_KEY_MOUSE_MIDDLE},
  {STR_CLIT("mouse_middle"), INPUT_KEY_MOUSE_MIDDLE},
  {STR_CLIT("Mouse4"), INPUT_KEY_MOUSE_THUMB_1},
  {STR_CLIT("mouse_4"), INPUT_KEY_MOUSE_THUMB_1},
  {STR_CLIT("Thumb1"), INPUT_KEY_MOUSE_THUMB_1},
  {STR_CLIT("Mouse5"), INPUT_KEY_MOUSE_THUMB_2},
  {STR_CLIT("mouse_5"), INPUT_KEY_MOUSE_THUMB_2},
  {STR_CLIT("Thumb2"), INPUT_KEY_MOUSE_THUMB_2},
  {STR_CLIT("WheelUp"), INPUT_KEY_MOUSE_WHEEL_UP},
  {STR_CLIT("MouseWheelUp"), INPUT_KEY_MOUSE_WHEEL_UP},
  {STR_CLIT("mouse_wheel_up"), INPUT_KEY_MOUSE_WHEEL_UP},
  {STR_CLIT("WheelDown"), INPUT_KEY_MOUSE_WHEEL_DOWN},
  {STR_CLIT("MouseWheelDown"), INPUT_KEY_MOUSE_WHEEL_DOWN},
  {STR_CLIT("mouse_wheel_down"), INPUT_KEY_MOUSE_WHEEL_DOWN},

  /* gamepad */
  {STR_CLIT("ButtonA"), INPUT_KEY_GAMEPAD_FACE_BOTTOM},
  {STR_CLIT("button_a"), INPUT_KEY_GAMEPAD_FACE_BOTTOM},
  {STR_CLIT("Cross"), INPUT_KEY_GAMEPAD_FACE_BOTTOM},
  {STR_CLIT("GamepadButtonBottom"), INPUT_KEY_GAMEPAD_FACE_BOTTOM},
  {STR_CLIT("ButtonB"), INPUT_KEY_GAMEPAD_FACE_RIGHT},
  {STR_CLIT("button_b"), INPUT_KEY_GAMEPAD_FACE_RIGHT},
  {STR_CLIT("Circle"), INPUT_KEY_GAMEPAD_FACE_RIGHT},
  {STR_CLIT("GamepadButtonRight"), INPUT_KEY_GAMEPAD_FACE_RIGHT},
  {STR_CLIT("ButtonX"), INPUT_KEY_GAMEPAD_FACE_LEFT},
  {STR_CLIT("button_x"), INPUT_KEY_GAMEPAD_FACE_LEFT},
  {STR_CLIT("Square"), INPUT_KEY_GAMEPAD_FACE_LEFT},
  {STR_CLIT("GamepadButtonLeft"), INPUT_KEY_GAMEPAD_FACE_LEFT},
  {STR_CLIT("ButtonY"), INPUT_KEY_GAMEPAD_FACE_TOP},
  {STR_CLIT("button_y"), INPUT_KEY_GAMEPAD_FACE_TOP},
  {STR_CLIT("Triangle"), INPUT_KEY_GAMEPAD_FACE_TOP},
  {STR_CLIT("GamepadButtonTop"), INPUT_KEY_GAMEPAD_FACE_TOP},
  {STR_CLIT("LB"), INPUT_KEY_GAMEPAD_LB},
  {STR_CLIT("L1"), INPUT_KEY_GAMEPAD_LB},
  {STR_CLIT("LeftBumper"), INPUT_KEY_GAMEPAD_LB},
  {STR_CLIT("left_bumper"), INPUT_KEY_GAMEPAD_LB},
  {STR_CLIT("RB"), INPUT_KEY_GAMEPAD_RB},
  {STR_CLIT("R1"), INPUT_KEY_GAMEPAD_RB},
  {STR_CLIT("RightBumper"), INPUT_KEY_GAMEPAD_RB},
  {STR_CLIT("right_bumper"), INPUT_KEY_GAMEPAD_RB},
  {STR_CLIT("LT"), INPUT_KEY_GAMEPAD_LT},
  {STR_CLIT("L2"), INPUT_KEY_GAMEPAD_LT},
  {STR_CLIT("LeftTrigger"), INPUT_KEY_GAMEPAD_LT},
  {STR_CLIT("left_trigger"), INPUT_KEY_GAMEPAD_LT},
  {STR_CLIT("RT"), INPUT_KEY_GAMEPAD_RT},
  {STR_CLIT("R2"), INPUT_KEY_GAMEPAD_RT},
  {STR_CLIT("RightTrigger"), INPUT_KEY_GAMEPAD_RT},
  {STR_CLIT("right_trigger"), INPUT_KEY_GAMEPAD_RT},
  {STR_CLIT("LS"), INPUT_KEY_GAMEPAD_L3},
  {STR_CLIT("L3"), INPUT_KEY_GAMEPAD_L3},
  {STR_CLIT("LeftStick"), INPUT_KEY_GAMEPAD_L3},
  {STR_CLIT("left_stick"), INPUT_KEY_GAMEPAD_L3},
  {STR_CLIT("RS"), INPUT_KEY_GAMEPAD_R3},
  {STR_CLIT("R3"), INPUT_KEY_GAMEPAD_R3},
  {STR_CLIT("RightStick"), INPUT_KEY_GAMEPAD_R3},
  {STR_CLIT("right_stick"), INPUT_KEY_GAMEPAD_R3},
  {STR_CLIT("Start"), INPUT_KEY_GAMEPAD_SPECIAL_RIGHT},
  {STR_CLIT("Options"), INPUT_KEY_GAMEPAD_SPECIAL_RIGHT},
  {STR_CLIT("Select"), INPUT_KEY_GAMEPAD_SPECIAL_LEFT},
  {STR_CLIT("Share"), INPUT_KEY_GAMEPAD_SPECIAL_LEFT},
  {STR_CLIT("Back"), INPUT_KEY_GAMEPAD_SPECIAL_LEFT},
  {STR_CLIT("DPadUp"), INPUT_KEY_GAMEPAD_DPAD_UP},
  {STR_CLIT("dpad_up"), INPUT_KEY_GAMEPAD_DPAD_UP},
  {STR_CLIT("DPadDown"), INPUT_KEY_GAMEPAD_DPAD_DOWN},
  {STR_CLIT("dpad_down"), INPUT_KEY_GAMEPAD_DPAD_DOWN},
  {STR_CLIT("DPadLeft"), INPUT_KEY_GAMEPAD_DPAD_LEFT},
  {STR_CLIT("dpad_left"), INPUT_KEY_GAMEPAD_DPAD_LEFT},
  {STR_CLIT("DPadRight"), INPUT_KEY_GAMEPAD_DPAD_RIGHT},
  {STR_CLIT("dpad_right"), INPUT_KEY_GAMEPAD_DPAD_RIGHT},
};

keybind_t
keybind_parse(str_t str, keybind_t default_val)
{
  keybind_t result = {0};

  uint64_t pos = 0;
  while (pos < str.len) {
    /* find next '+' or end */
    uint64_t end = pos;
    while (end < str.len && str.data[end] != '+') {
      end += 1;
    }

    str_t token = str_trim_space(str_slice(str, pos, end));
    if (token.len == 0) {
      pos = end + 1;
      continue;
    }

    if (result.count >= KEYBIND_MAX_KEYS) {
      return default_val;
    }

    input_key_kind_t key = INPUT_KEY_NONE;
    for (int i = 0; i < COUNTOF(g_key_aliases); ++i) {
      if (str_equal(token, g_key_aliases[i].name, STR_CMP_FLAG_IGNORE_CASE)) {
        key = g_key_aliases[i].key;
        break;
      }
    }

    /* fallback: try exact UE4 name match */
    if (key == INPUT_KEY_NONE) {
      key = input_key_from_str(token);
    }

    if (key == INPUT_KEY_NONE) {
      return default_val;
    }

    result.keys[result.count++] = key;
    pos                         = end + 1;
  }

  if (result.count == 0) {
    return default_val;
  }

  return result;
}

static str_t
keybind_key_canonical_name(input_key_kind_t key)
{
  for (int i = 0; i < COUNTOF(g_key_aliases); ++i) {
    if (g_key_aliases[i].key == key) {
      return g_key_aliases[i].name;
    }
  }

  return STR_NULL;
}

uint64_t
keybind_utf8_len(keybind_t bind)
{
  uint64_t len = 0;

  for (int i = 0; i < bind.count; i++) {
    str_t name = keybind_key_canonical_name(bind.keys[i]);

    if (i > 0) {
      len += 1; // '+'
    }

    if (str_is_empty(name)) {
      len += 1; // '?'
    } else {
      len += name.len;
    }
  }

  return len;
}

uint64_t
keybind_utf8_write(keybind_t bind, void *buf, uint64_t cap)
{
  uint8_t *dst    = (uint8_t *)buf;
  uint64_t total_len = 0;

  for (int i = 0; i < bind.count && total_len < cap; i++) {
    str_t name = keybind_key_canonical_name(bind.keys[i]);

    if (i > 0 && total_len < cap) {
      dst[total_len++] = '+';
    }

    if (total_len >= cap) {
      break;
    }

    if (str_is_empty(name)) {
      dst[total_len++] = '?';
    } else {
      uint64_t rem = cap - total_len;
      uint64_t len = MIN_VAL(name.len, rem);

      mem_copy(dst + total_len, name.data, len);
      total_len += len;
    }
  }
  return total_len;
}

str_t
keybind_to_str(keybind_t bind, arena_t *arena)
{
  str_t     result = STR_NULL;
  uint64_t  len    = keybind_utf8_len(bind);
  uint8_t  *buf    = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, len + 1);
  if (buf) {
    ASSERT(keybind_utf8_write(bind, buf, len) == len);
    buf[len] = '\0';
    result   = str_make(buf, len);
  }
  return result;
}

bool
keybind_is_valid(keybind_t bind)
{
  bool is_empty = true;
  for (int i = 0; i < bind.count; ++i) {
    if (bind.keys[i] != INPUT_KEY_NONE) {
      is_empty = false;
      break;
    }
  }
  return !is_empty;
}

bool
keybind_equal(keybind_t a, keybind_t b)
{
  if (a.count != b.count) {
    return false;
  }

  for (uint8_t i = 0; i < a.count; ++i) {
    if (a.keys[i] != b.keys[i]) {
      return false;
    }
  }
  return true;
}

void
keybind_clear(keybind_t *bind)
{
  if (bind) {
    mem_zero(bind, sizeof(*bind));
  }
}

bool
keybind_is_down(keybind_t bind)
{
  if (bind.count == 0) {
    return false;
  }

  for (int i = 0; i < bind.count; ++i) {
    if (!input_key_is_down(bind.keys[i])) {
      return false;
    }
  }
  return true;
}

bool
keybind_was_down(keybind_t bind)
{
  if (bind.count == 0) {
    return false;
  }

  for (int i = 0; i < bind.count; ++i) {
    if (!input_key_was_down(bind.keys[i])) {
      return false;
    }
  }
  return true;
}

bool
keybind_is_pressed(keybind_t bind)
{
  return keybind_is_down(bind) && !keybind_was_down(bind);
}

bool
keybind_is_released(keybind_t bind)
{
  return !keybind_is_down(bind) && keybind_was_down(bind);
}

bool
input_event_covers_keybind(input_event_t *ev, keybind_t bind)
{
  bool has_event_key = false;
  for (int i = 0; i < bind.count; ++i) {
    if (bind.keys[i] == ev->key) {
      has_event_key = true;
      break;
    }
  }
  return has_event_key;
}

