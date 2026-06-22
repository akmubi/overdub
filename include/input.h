#ifndef INPUT_H
#define INPUT_H

#include "arena.h"
#include "str.h"
#include "types.h"
#include "unreal.h"

typedef uint8_t input_event_kind_t;
enum {
  INPUT_EVENT_KEY_DOWN,
  INPUT_EVENT_KEY_UP,
  INPUT_EVENT_KEY_CHAR,
  INPUT_EVENT_MOUSE_DOWN,
  INPUT_EVENT_MOUSE_UP,
  INPUT_EVENT_MOUSE_DBLCLICK,
  INPUT_EVENT_MOUSE_MOVE,
  INPUT_EVENT_MOUSE_WHEEL,
  INPUT_EVENT_ANALOG,
  INPUT_EVENT_APP_ACTIVATION,

  INPUT_EVENT_MAX,
};

typedef uint8_t input_mod_flags_t;
enum {
  INPUT_MOD_SHIFT = (1 << 0),
  INPUT_MOD_CTRL  = (1 << 1),
  INPUT_MOD_ALT   = (1 << 2),
};

typedef uint16_t input_key_kind_t;
enum {
  INPUT_KEY_NONE = 0,

  /* keyboard: letters */
  INPUT_KEY_A,
  INPUT_KEY_B,
  INPUT_KEY_C,
  INPUT_KEY_D,
  INPUT_KEY_E,
  INPUT_KEY_F,
  INPUT_KEY_G,
  INPUT_KEY_H,
  INPUT_KEY_I,
  INPUT_KEY_J,
  INPUT_KEY_K,
  INPUT_KEY_L,
  INPUT_KEY_M,
  INPUT_KEY_N,
  INPUT_KEY_O,
  INPUT_KEY_P,
  INPUT_KEY_Q,
  INPUT_KEY_R,
  INPUT_KEY_S,
  INPUT_KEY_T,
  INPUT_KEY_U,
  INPUT_KEY_V,
  INPUT_KEY_W,
  INPUT_KEY_X,
  INPUT_KEY_Y,
  INPUT_KEY_Z,

  /* keyboard: punctuation */
  INPUT_KEY_COMMA,         // , or <
  INPUT_KEY_PERIOD,        // . or >
  INPUT_KEY_SLASH,         // / or ?
  INPUT_KEY_SEMICOLON,     // ; or :
  INPUT_KEY_APOSTROPHE,    // ' or "
  INPUT_KEY_LEFT_BRACKET,  // [ or {
  INPUT_KEY_RIGHT_BRACKET, // ] or }
  INPUT_KEY_BACKSLASH,     // \ or |
  INPUT_KEY_HYPHEN,        // - or _
  INPUT_KEY_EQUALS,        // = or +
  INPUT_KEY_BACKTICK,      // ` or ~

  /* keyboard: digits */
  INPUT_KEY_1, // 1 or !
  INPUT_KEY_2, // 2 or @
  INPUT_KEY_3, // 3 or #
  INPUT_KEY_4, // 4 or $
  INPUT_KEY_5, // 5 or %
  INPUT_KEY_6, // 6 or ^
  INPUT_KEY_7, // 7 or &
  INPUT_KEY_8, // 8 or *
  INPUT_KEY_9, // 9 or (
  INPUT_KEY_0, // 0 or )

  /* keyboard: function */
  INPUT_KEY_F1,
  INPUT_KEY_F2,
  INPUT_KEY_F3,
  INPUT_KEY_F4,
  INPUT_KEY_F5,
  INPUT_KEY_F6,
  INPUT_KEY_F7,
  INPUT_KEY_F8,
  INPUT_KEY_F9,
  INPUT_KEY_F10,
  INPUT_KEY_F11,
  INPUT_KEY_F12,

  /* keyboard: navigation */
  INPUT_KEY_UP,
  INPUT_KEY_DOWN,
  INPUT_KEY_LEFT,
  INPUT_KEY_RIGHT,
  INPUT_KEY_HOME,
  INPUT_KEY_END,
  INPUT_KEY_PAGE_UP,
  INPUT_KEY_PAGE_DOWN,

  /* keyboard: modifiers */
  INPUT_KEY_LEFT_SHIFT,
  INPUT_KEY_RIGHT_SHIFT,
  INPUT_KEY_LEFT_CTRL,
  INPUT_KEY_RIGHT_CTRL,
  INPUT_KEY_LEFT_ALT,
  INPUT_KEY_RIGHT_ALT,

  /* keyboard: common */
  INPUT_KEY_SPACE,
  INPUT_KEY_ENTER,
  INPUT_KEY_ESCAPE,
  INPUT_KEY_TAB,
  INPUT_KEY_BACKSPACE,
  INPUT_KEY_CAPS_LOCK,
  INPUT_KEY_INSERT,
  INPUT_KEY_DELETE,

  /* keyboard: numpad */
  INPUT_KEY_NUM_0,
  INPUT_KEY_NUM_1,
  INPUT_KEY_NUM_2,
  INPUT_KEY_NUM_3,
  INPUT_KEY_NUM_4,
  INPUT_KEY_NUM_5,
  INPUT_KEY_NUM_6,
  INPUT_KEY_NUM_7,
  INPUT_KEY_NUM_8,
  INPUT_KEY_NUM_9,
  INPUT_KEY_NUM_MULTIPLY, // *
  INPUT_KEY_NUM_ADD,      // +
  INPUT_KEY_NUM_SUBTRACT, // -
  INPUT_KEY_NUM_DECIMAL,  // .
  INPUT_KEY_NUM_DIVIDE,   // /

  /* keyboard: misc */
  INPUT_KEY_PAUSE,
  INPUT_KEY_PRINT_SCREEN,
  INPUT_KEY_SCROLL_LOCK,
  INPUT_KEY_NUM_LOCK,

  /* mouse: buttons */
  INPUT_KEY_MOUSE_LEFT,
  INPUT_KEY_MOUSE_RIGHT,
  INPUT_KEY_MOUSE_MIDDLE,
  INPUT_KEY_MOUSE_THUMB_1,
  INPUT_KEY_MOUSE_THUMB_2,

  /* mouse: axes */
  INPUT_KEY_MOUSE_X,
  INPUT_KEY_MOUSE_Y,
  INPUT_KEY_MOUSE_WHEEL_UP,
  INPUT_KEY_MOUSE_WHEEL_DOWN,

  /* gamepad: buttons */
  INPUT_KEY_GAMEPAD_FACE_BOTTOM, // A / cross
  INPUT_KEY_GAMEPAD_FACE_RIGHT,  // B / circle
  INPUT_KEY_GAMEPAD_FACE_LEFT,   // X / square
  INPUT_KEY_GAMEPAD_FACE_TOP,    // Y / triangle
  INPUT_KEY_GAMEPAD_LB,
  INPUT_KEY_GAMEPAD_RB,
  INPUT_KEY_GAMEPAD_LT,
  INPUT_KEY_GAMEPAD_RT,
  INPUT_KEY_GAMEPAD_L3,
  INPUT_KEY_GAMEPAD_R3,
  INPUT_KEY_GAMEPAD_SPECIAL_LEFT,  // select / share
  INPUT_KEY_GAMEPAD_SPECIAL_RIGHT, // start  / options
  INPUT_KEY_GAMEPAD_DPAD_UP,
  INPUT_KEY_GAMEPAD_DPAD_DOWN,
  INPUT_KEY_GAMEPAD_DPAD_LEFT,
  INPUT_KEY_GAMEPAD_DPAD_RIGHT,

  /* gamepad: axes */
  INPUT_KEY_GAMEPAD_LEFT_X,
  INPUT_KEY_GAMEPAD_LEFT_Y,
  INPUT_KEY_GAMEPAD_RIGHT_X,
  INPUT_KEY_GAMEPAD_RIGHT_Y,
  INPUT_KEY_GAMEPAD_LT_AXIS,
  INPUT_KEY_GAMEPAD_RT_AXIS,

  INPUT_KEY_MAX,
};

typedef struct input_event_s input_event_t;
struct input_event_s {
  input_event_kind_t kind;
  input_mod_flags_t  modifiers; // keyboard modifiers
  input_key_kind_t   key;       // key, button, axis etc.

  uint32_t character;          // unicode codepoint (KEY_CHAR only)
  bool     is_repeat;          // KEY_DOWN only
  bool     app_activated;      // APP_ACTIVATION only
  uint32_t screen_x, screen_y; // screen position (mouse events)
  uint32_t client_x, client_y; // client position (mouse events)
  float    delta_x, delta_y;   // movement delta  (MOUSE_MOVE only)
  float    wheel_delta;        // MOUSE_WHEEL only (>0 - up, <0 - down)
  float    analog_value;       // ANALOG only (trigger value range: 0.0 .. 1.0, stick value range: -1.0 .. 1.0)
};

#define KEYBIND_MAX_KEYS (10)
#define KEYBIND_NULL     (keybind_t){0}

typedef struct keybind_s keybind_t;
struct keybind_s {
  input_key_kind_t keys[KEYBIND_MAX_KEYS];
  uint8_t          count;
};

void
input_key_map_init(void);

input_key_kind_t
input_key_from_fname(fname_t key_name);
input_key_kind_t
input_key_from_str(str_t name);

bool
input_key_is_down(input_key_kind_t kind);
bool
input_key_was_down(input_key_kind_t kind);

str_t
input_key_to_str(input_key_kind_t key);
str_t
input_event_kind_to_str(input_event_kind_t kind);

input_event_t
input_event_from_key_event(input_event_kind_t kind, fkey_event_t *e);
input_event_t
input_event_from_character_event(fcharacter_event_t *e);
input_event_t
input_event_from_pointer_event(input_event_kind_t kind, fpointer_event_t *e);
input_event_t
input_event_from_analog_event(fanalog_input_event_t *e);
input_event_t
input_event_from_app_activation_event(bool app_activated);

bool
input_event_is_mouse(input_event_t *ev);
bool
input_event_is_keyboard(input_event_t *ev);
bool
input_event_is_gamepad(input_event_t *ev);

keybind_t
keybind_parse(str_t str, keybind_t default_val);
uint64_t
keybind_utf8_len(keybind_t bind);
uint64_t
keybind_utf8_write(keybind_t bind, void *buf, uint64_t cap);
str_t
keybind_to_str(keybind_t bind, arena_t *arena);
bool
keybind_is_valid(keybind_t bind);
bool
keybind_equal(keybind_t a, keybind_t b);
void
keybind_clear(keybind_t *bind);

/* NOTE: used in engine tick */
bool
keybind_is_down(keybind_t bind);
bool
keybind_was_down(keybind_t bind);
bool
keybind_is_pressed(keybind_t bind);
bool
keybind_is_released(keybind_t bind);

static inline bool
keybind_str_is_down(str_t keybind_str)
{
  return keybind_is_down(keybind_parse(keybind_str, KEYBIND_NULL));
}

static inline bool
keybind_str_was_down(str_t keybind_str)
{
  return keybind_was_down(keybind_parse(keybind_str, KEYBIND_NULL));
}

static inline bool
keybind_str_is_pressed(str_t keybind_str)
{
  return keybind_is_pressed(keybind_parse(keybind_str, KEYBIND_NULL));
}

static inline bool
keybind_str_is_released(str_t keybind_str)
{
  return keybind_is_released(keybind_parse(keybind_str, KEYBIND_NULL));
}

/* NOTE: used for input events */
bool
input_event_covers_keybind(input_event_t *ev, keybind_t bind);

#endif /* INPUT_H */
