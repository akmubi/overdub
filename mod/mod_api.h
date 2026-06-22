#ifndef MOD_API_H
#define MOD_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOD_CALL   __cdecl
#define MOD_EXPORT __declspec(dllexport)

#ifdef __cplusplus
#  define MOD_EXTERN_C extern "C"
#else
#  define MOD_EXTERN_C
#endif

#define MOD_ENTRY_NAME "mod_entry"
#define MOD_ENTRY() \
  MOD_EXTERN_C MOD_EXPORT const mod_api_t * MOD_CALL mod_entry(void)

/* static assert */
#if defined(__cplusplus) || (defined _MSC_VER && (_MSC_VER >= 1600))
#  define MOD_STATIC_ASSERT(COND, MSG) static_assert((COND), MSG)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define MOD_STATIC_ASSERT(COND, MSG) _Static_assert((COND), MSG)
#endif

/* ====================================================== INPUT ===================================================== */

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
  INPUT_KEY_GAMEPAD_FACE_BOTTOM,   // A / cross
  INPUT_KEY_GAMEPAD_FACE_RIGHT,    // B / circle
  INPUT_KEY_GAMEPAD_FACE_LEFT,     // X / square
  INPUT_KEY_GAMEPAD_FACE_TOP,      // Y / triangle
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
  float    delta_x,  delta_y;  // movement delta  (MOUSE_MOVE only)
  float    wheel_delta;        // MOUSE_WHEEL only (>0 - up, <0 - down)
  float    analog_value;       // ANALOG only (trigger value range: 0.0 .. 1.0, stick value range: -1.0 .. 1.0)
};

#define KEYBIND_MAX_KEYS (10)

#ifdef __cplusplus
#  define KEYBIND_NULL (keybind_t){}
#else
#  define KEYBIND_NULL (keybind_t){0}
#endif

typedef struct keybind_s keybind_t;
struct keybind_s {
  input_key_kind_t keys[KEYBIND_MAX_KEYS];
  uint8_t          count;
};

/* ===================================================== UNREAL ===================================================== */

#define UNREAL_CALL __fastcall

typedef struct fframe_s         fframe_t;
typedef struct uobject_s        uobject_t;
typedef struct ufunc_s          ufunc_t;
typedef struct uclass_s         uclass_t;
typedef struct uworld_s         uworld_t;
typedef struct fname_pool_s     fname_pool_t;
typedef struct fuobject_array_s fuobject_array_t;
typedef struct fname_s          fname_t;

typedef enum {
  UOBJECT_LISTENER_KIND_CREATE,
  UOBJECT_LISTENER_KIND_DELETE,
} uobject_listener_kind_t;

typedef int efind_name_t;
enum {
  FNAME_FIND,
  FNAME_FIND_OR_ADD,
  FNAME_REPLACE_NOT_SAFE_FOR_THREADING,
};

typedef struct fname_s fname_t;
struct fname_s {
  uint32_t cmp_idx;
  uint32_t num;
};
MOD_STATIC_ASSERT(sizeof(fname_t) == 8, "size mismatch");

typedef void (UNREAL_CALL *fnative_func_ptr_t) (uobject_t *context, fframe_t *stack, void *result);
typedef void (MOD_CALL    *uobject_notify_cb_t)(uobject_t *obj, int32_t idx, void *user);

/* ===================================================== STRING ===================================================== */

/* UTF-8 string */
typedef struct str_s str_t;
struct str_s {
  uint8_t *data;
  uint64_t len;
};

/* UTF-16 string */
typedef struct str16_s str16_t;
struct str16_s {
  uint16_t *data;
  uint64_t  len;
};

/* ===================================================== SIGSCAN ==================================================== */

typedef int sigscan_err_t;
enum {
  SIGSCAN_ERR_OK                  = 0,
  SIGSCAN_ERR_INVALID_PATTERN     = -1,
  SIGSCAN_ERR_TARGET_NOT_READABLE = -2,
  SIGSCAN_ERR_NOT_FOUND           = -3,
};

/* ===================================================== ARENA ====================================================== */
#define MOD_ARENA_INVALID ((mod_arena_t)0)

typedef uint64_t mod_arena_t;

typedef struct tmp_arena_s tmp_arena_t;
struct tmp_arena_s {
  mod_arena_t arena;
  uint64_t    pos;
};

/* ====================================================== MOD ======================================================= */

#define MOD_INVALID     ((mod_t)0)
#define MOD_CFG_INVALID ((mod_cfg_t)0)

typedef uint64_t mod_t;
typedef uint64_t mod_cfg_t;

typedef int log_level_t;
enum {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO  = 1,
  LOG_LEVEL_WARN  = 2,
  LOG_LEVEL_ERROR = 3,
};

#define MAKE_VERSION(MAJOR, MINOR, PATCH) \
  { \
    .major = MAJOR, \
    .minor = MINOR, \
    .patch = PATCH, \
  }

typedef struct mod_version_s mod_version_t;
struct mod_version_s {
  uint16_t major;
  uint16_t minor;
  uint32_t patch;
};

typedef struct mod_color_s mod_color_t;
struct mod_color_s {
  uint8_t r, g, b, a;
};

typedef struct nk_context       nk_context_t;

typedef struct mod_host_api_s mod_host_api_t;

typedef void (MOD_CALL *mod_tick_fn_t)   (mod_t m, float delta);
typedef bool (MOD_CALL *mod_input_fn_t)  (mod_t m, input_event_t *ev);

typedef bool (MOD_CALL *mod_pe_pre_fn_t) (mod_t m, uobject_t *obj, ufunc_t *func, void *params);
typedef void (MOD_CALL *mod_pe_post_fn_t)(mod_t m, uobject_t *obj, ufunc_t *func, void *params, bool consumed);

typedef bool (MOD_CALL *mod_func_invoke_pre_fn_t) (mod_t m, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result);
typedef void (MOD_CALL *mod_func_invoke_post_fn_t)(mod_t m, ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result, bool consumed);

typedef void (MOD_CALL *mod_draw_panel_fn_t) (mod_t m, struct nk_context *ctx);
typedef void (MOD_CALL *mod_draw_config_fn_t)(mod_t m, struct nk_context *ctx);

typedef bool (MOD_CALL *mod_init_fn_t)  (const mod_host_api_t *host, mod_t m);
typedef void (MOD_CALL *mod_deinit_fn_t)(mod_t m);

typedef void (MOD_CALL *mod_cmd_fn_t)(mod_t m, str_t name, str_t args, void *user);

#define MOD_ABI_VERSION MAKE_VERSION(1, 0, 0)

typedef struct mod_api_s mod_api_t;
struct mod_api_s {
  uint32_t                  struct_size;
  mod_version_t             abi_version;
  mod_init_fn_t             init;
  mod_deinit_fn_t           deinit;
  mod_tick_fn_t             tick;
  mod_input_fn_t            input;
  mod_pe_pre_fn_t           pe_pre;
  mod_pe_post_fn_t          pe_post;
  mod_func_invoke_pre_fn_t  func_invoke_pre;
  mod_func_invoke_post_fn_t func_invoke_post;
  mod_draw_panel_fn_t       draw_panel;
  mod_draw_config_fn_t      draw_config;
};

typedef const mod_api_t *(MOD_CALL *mod_entry_fn_t)(void);

struct mod_host_api_s {
  uint32_t      struct_size;
  mod_version_t abi_version;

  /* MOD */

  str_t     (MOD_CALL *get_mod_dir)      (mod_t m);
  str_t     (MOD_CALL *get_config_path)  (mod_t m);
  str_t     (MOD_CALL *get_manifest_path)(mod_t m);
  void      (MOD_CALL *logv)             (mod_t m, log_level_t level, const char *fmt, va_list args);
  mod_cfg_t (MOD_CALL *get_cfg_by_id)    (mod_t m, str_t id);
  bool      (MOD_CALL *register_cmd)     (mod_t m, str_t name, str_t description, mod_cmd_fn_t fn, void *user);

  /* UI */
  struct nk_context *(MOD_CALL *get_nk_ctx)       (void);
  void               (MOD_CALL *get_viewport_size)(unsigned int *vw, unsigned int *vh);

  /* MOD CONFIGURATION */

  void (MOD_CALL *cfg_get_bool)       (mod_cfg_t h, bool        *val);
  void (MOD_CALL *cfg_get_int)        (mod_cfg_t h, int         *val);
  void (MOD_CALL *cfg_get_float)      (mod_cfg_t h, float       *val);
  void (MOD_CALL *cfg_get_enum)       (mod_cfg_t h, int         *val);
  void (MOD_CALL *cfg_get_string_len) (mod_cfg_t h, uint64_t    *len);
  void (MOD_CALL *cfg_get_string_data)(mod_cfg_t h, void        *buf, uint64_t cap, uint64_t *written);
  void (MOD_CALL *cfg_get_keybind)    (mod_cfg_t h, keybind_t   *val);
  void (MOD_CALL *cfg_get_color)      (mod_cfg_t h, mod_color_t *val);
  void (MOD_CALL *cfg_set_bool)       (mod_cfg_t h, bool         val);
  void (MOD_CALL *cfg_set_int)        (mod_cfg_t h, int          val);
  void (MOD_CALL *cfg_set_float)      (mod_cfg_t h, float        val);
  void (MOD_CALL *cfg_set_enum)       (mod_cfg_t h, int          val);
  void (MOD_CALL *cfg_set_string)     (mod_cfg_t h, str_t        val);
  void (MOD_CALL *cfg_set_keybind)    (mod_cfg_t h, keybind_t    val);
  void (MOD_CALL *cfg_set_color)      (mod_cfg_t h, mod_color_t  val);

  /* MEMORY ALLOCATION */

  mod_arena_t   (MOD_CALL *get_perm)     (mod_t       h);
  mod_arena_t   (MOD_CALL *arena_create) (mod_t       h, uint64_t reserve_size, uint64_t commit_size);
  void          (MOD_CALL *arena_destroy)(mod_t       h, mod_arena_t arena_h);
  void         *(MOD_CALL *arena_push)   (mod_arena_t h, uint64_t size, uint64_t alignment);
  uint64_t      (MOD_CALL *arena_pos)    (mod_arena_t h);
  void          (MOD_CALL *arena_pop_to) (mod_arena_t h, uint64_t pos);
  void          (MOD_CALL *arena_reset)  (mod_arena_t h);
  tmp_arena_t   (MOD_CALL *scratch_begin)(mod_arena_t conflict);
  void          (MOD_CALL *scratch_end)  (tmp_arena_t s);

  /* UNREAL ENGINE */

  fname_pool_t      *(MOD_CALL *get_name_pool)            (void);
  fuobject_array_t  *(MOD_CALL *get_object_array)         (void);
  uworld_t         **(MOD_CALL *get_gworld_ptr)           (void);
  void               (MOD_CALL *process_event)            (uobject_t *self, ufunc_t *func, void *params);
  uclass_t          *(MOD_CALL *uobject_load_class)       (uclass_t *base_cls, uobject_t *outer, str_t name, str_t filename, uint32_t load_flags);
  bool               (MOD_CALL *mount_pak)                (str_t file_path, int order);
  bool               (MOD_CALL *mount_iostore)            (str_t file_path, int order);
  fname_t            (MOD_CALL *fname_from_str)           (str_t str, efind_name_t find_type);
  uint64_t           (MOD_CALL *fname_utf8_len)           (fname_t fname);
  uint64_t           (MOD_CALL *fname_utf8_write)         (fname_t fname, void *buf, uint64_t cap);
  void              *(MOD_CALL *get_mcast_sparse_delegate)(uobject_t *delegate_owner, fname_t delegate_name);
  fnative_func_ptr_t (MOD_CALL *get_native)               (uint8_t opcode);

  /* INPUT EVENTS / KEYBINDS */

  input_key_kind_t (MOD_CALL *input_key_from_str)     (str_t name);
  str_t            (MOD_CALL *input_key_to_str)       (input_key_kind_t key);
  str_t            (MOD_CALL *input_event_kind_to_str)(input_event_kind_t kind);
  bool             (MOD_CALL *keybind_parse)          (str_t str, keybind_t *out);
  uint64_t         (MOD_CALL *keybind_utf8_len)       (keybind_t bind);
  uint64_t         (MOD_CALL *keybind_utf8_write)     (keybind_t bind, void *buf, uint64_t cap);
  bool             (MOD_CALL *keybind_is_down)        (keybind_t bind);
  bool             (MOD_CALL *keybind_was_down)       (keybind_t bind);

  /* UOBJECT CREATE/DELETE LISTENERS */

  bool (MOD_CALL *register_uobject_listener)  (mod_t m, int kind, uobject_notify_cb_t notify_cb, void *user);
  void (MOD_CALL *deregister_uobject_listener)(mod_t m, int kind, uobject_notify_cb_t notify_cb, void *user);

  /* HOOKING / SIGNATURE SCANNING */

  sigscan_err_t (MOD_CALL *sigscan)     (const char *name, const char *pattern, uint8_t op_offset, uint8_t num_deref,
                                         uintptr_t first_rvas_to_try[8], void **result);
  bool          (MOD_CALL *hook_create) (mod_t m, void *target, void *detour, void **original);
  bool          (MOD_CALL *hook_enable) (mod_t m, void *target);
  bool          (MOD_CALL *hook_disable)(mod_t m, void *target);
  bool          (MOD_CALL *hook_remove) (mod_t m, void *target);
};

#ifdef __cplusplus
}
#endif

#endif /* MOD_API_H */

