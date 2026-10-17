#include "builtin.h"

#include "debug.h"
#include "file.h"
#include "globals.h"
#include "log.h"
#include "mod.h"
#include "path.h"
#include "scratch.h"
#include "ui_nuklear.h"
#include "unreal_prop.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CFG_DISABLE_TUTORIALS     "disable-tutorials"
#define CFG_RESTORE_JUKEBOX       "restore-jukebox-state"
#define CFG_DEBUG_WIDGET_KEYBIND  "debug-widget-keybind"
#define CFG_ENABLE_INFINITE_JUMPS "enable-infinite-jumps"
#define CFG_INFINITE_DASH         "infinite-dash"
#define MOD_ID                    "cheats"

typedef enum {
  JBOX_PLAYBACK_STOPPED = 0,
  JBOX_PLAYBACK_PLAYING = 1,
  JBOX_PLAYBACK_PAUSED  = 2,
} jbox_playback_state_t;

static const char *
jbox_playback_state_to_str(jbox_playback_state_t state)
{
  switch (state) {
  case JBOX_PLAYBACK_STOPPED:
    return "stopped";
  case JBOX_PLAYBACK_PLAYING:
    return "playing";
  case JBOX_PLAYBACK_PAUSED:
    return "paused";
  }
  return "<unknown>";
}

typedef uint8_t jbox_repeat_mode_t;
enum {
  JBOX_REPEAT_MODE_OFF      = 0,
  JBOX_REPEAT_MODE_PLAYLIST = 1,
  JBOX_REPEAT_MODE_SONG     = 2,
};

typedef struct jbox_gallery_product_info_s jbox_gallery_product_info_t;
struct jbox_gallery_product_info_s {
  uint8_t opaque[0x08];
};

typedef struct jbox_purchased_info_s jbox_purchased_info_t;
struct jbox_purchased_info_s {
  uint8_t opaque[0x10];
};

typedef struct jbox_music_data_t jbox_music_data_t;
struct jbox_music_data_t {
  void           *vftable;    // 0x00, FTableRowBase
  fgameplay_tag_t product_id; // 0x08
  uobject_t      *music_data; // 0x10, UAkAudioEvent
  fstring_t       str_key;    // 0x18
};
STATIC_ASSERT(sizeof(jbox_music_data_t) == 0x28, "size mismatch");
STATIC_ASSERT(offsetof(jbox_music_data_t, product_id) == 0x08, "invalid offset");
STATIC_ASSERT(offsetof(jbox_music_data_t, music_data) == 0x10, "invalid offset");
STATIC_ASSERT(offsetof(jbox_music_data_t, str_key) == 0x18, "invalid offset");

typedef struct {
  uint8_t opaque[0x28];
} tsoft_class_ptr_t;
STATIC_ASSERT(sizeof(tsoft_class_ptr_t) == 0x28, "size mismatch");

typedef TARRAY(jbox_gallery_product_info_t) jbox_gallery_product_info_array_t;
typedef TARRAY(jbox_purchased_info_t) jbox_purchased_info_array_t;
typedef TARRAY(int32_t) int32_array_t;
typedef TARRAY(jbox_music_data_t) jbox_music_data_array_t;

typedef struct ahbk_jukebox_controller_s jbox_controller_t;
struct ahbk_jukebox_controller_s {
  uobject_t                         base;
  uint8_t                           unknown_0[0x210];         // 0x228
  fmulticast_script_delegate_t      on_stop_music;            // 0x238
  fmulticast_script_delegate_t      on_play_music;            // 0x248
  uint8_t                           unknown_258[0x50];        // 0x258
  fmulticast_script_delegate_t      on_repeat_mode_changed;   // 0x2A8
  fmulticast_script_delegate_t      on_shuffle_changed;       // 0x2B8
  uint8_t                           unknown_2c8[0x10];        // 0x2C8
  int32_t                           selected_music_idx;       // 0x2D8
  int32_t                           playing_music_idx;        // 0x2DC
  int32_t                           previous_music_idx;       // 0x2E0
  uint8_t                           unknown_2e4;              // 0x2E4
  uint8_t                           shuffle_enabled;          // 0x2E5
  uint8_t                           unknown_2e6[0x06];        // 0x2E6
  jbox_playback_state_t             playback_state;           // 0x2EC
  uint32_t                          playing_id;               // 0x2F0
  jbox_repeat_mode_t                repeat_mode;              // 0x2F4
  uint8_t                           padding_2f5[3];           // 0x2F5
  jbox_gallery_product_info_array_t sound_gallery_list;       // 0x2F8
  jbox_purchased_info_array_t       purchase_gallery_list;    // 0x308
  int32_array_t                     shuffle_index_list;       // 0x318
  fweak_object_ptr_t                jukebox_camera;           // 0x328
  uobject_t                        *jukebox_widget;           // 0x330
  uobject_t                        *string_table;             // 0x338
  uobject_t                        *music_data_table;         // 0x340
  uobject_t                        *stop_audio;               // 0x348
  uobject_t                        *menu_in_event;            // 0x350
  uobject_t                        *menu_out_event;           // 0x358
  tsoft_class_ptr_t                 jukebox_widget_class;     // 0x360
  float                             play_previous_music_time; // 0x388
  int32_t                           history_saved_num;        // 0x38C
  jbox_music_data_array_t           music_data_list;          // 0x390
  jbox_music_data_t                 playing_music_data;       // 0x3A0
  jbox_music_data_array_t           previous_music_data_list; // 0x3C8
  uint8_t                           unknown_3d8[0x10];        // 0x3D8
};
STATIC_ASSERT(offsetof(jbox_controller_t, on_stop_music) == 0x238, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, on_play_music) == 0x248, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, on_repeat_mode_changed) == 0x2A8, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, on_shuffle_changed) == 0x2B8, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, selected_music_idx) == 0x2D8, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, playing_music_idx) == 0x2DC, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, previous_music_idx) == 0x2E0, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, shuffle_enabled) == 0x2E5, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, playback_state) == 0x2EC, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, playing_id) == 0x2F0, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, repeat_mode) == 0x2F4, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, sound_gallery_list) == 0x2F8, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, shuffle_index_list) == 0x318, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, stop_audio) == 0x348, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, music_data_list) == 0x390, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, playing_music_data) == 0x3A0, "invalid offset");
STATIC_ASSERT(offsetof(jbox_controller_t, previous_music_data_list) == 0x3C8, "invalid offset");
STATIC_ASSERT(sizeof(jbox_controller_t) == 0x3E8, "controller size mismatch");

typedef struct {
  fname_t product_id;
  uint8_t playback_state;
  uint8_t repeat_mode;
  bool    shuffle_enabled;
} jbox_state_t;

typedef bool(__fastcall *jbox_play_music_fn_t)(void *self, fgameplay_tag_t *product_id, bool save_previous);

typedef struct cheats_jbox_s cheats_jbox_t;
struct cheats_jbox_s {
  jbox_state_t         saved_state;
  jbox_controller_t   *controller;
  bool                 should_restore_state;
  jbox_play_music_fn_t play_music;
  str_t                state_path;
  bool                 in_arcade_mode;
  fname_t              controller_cls_name;
};

typedef struct cheats_cfg_s cheats_cfg_t;
struct cheats_cfg_s {
  mod_cfg_handle_t disable_tutorials_h;
  mod_cfg_handle_t restore_jbox_h;
  mod_cfg_handle_t debug_widget_keybind_h;
  mod_cfg_handle_t enable_infinite_jumps_h;
  mod_cfg_handle_t infinite_dash_h;
};

typedef struct cheats_item_s cheats_item_t;
struct cheats_item_s {
  fgameplay_tag_t id;
  str_t           tag;
  str_t           name;
  int             category;
};

static const struct {
  const char *prefix;
  const char *label;
} g_item_categories[] = {
  {"Inventory.Variable.",      "Upgrades"       },
  {"Inventory.Chip.",          "Chips"          },
  {"Inventory.SpecialAttack.", "Special attacks"},
  {"Inventory.Ability.",       "Abilities"      },
  {"Inventory.Attack.",        "Attacks"        },
  {"Inventory.Combo.",         "Combos"         },
  {"Inventory.Partner.",       "Partner skills" },
  {"Inventory.Costume.",       "Costumes"       },
  {"Inventory.File.",          "Files"          },
};

typedef struct cheats_s cheats_t;
struct cheats_s {
  cheats_cfg_t  cfg;
  cheats_jbox_t jbox;
  arena_t      *perm;
  bool          inited;
  int           tutorial_close_ticks;

  /* where the dodge cooldown and the "air dash used" bit are on the player character */
  int32_t dash_cooldown_offset;
  int32_t air_dash_offset;

  uobject_t *player_manager;
  double     player_manager_search_time;

  cheats_item_t *items;
  int            item_count;
  int            item_category;
  int            item_give_count;
  char           item_filter[64];
  char           money_text[16];
};

static cheats_t g_cheats = {0};

static bool
fname_is(fname_t name, str_t text)
{
  return unreal_fname_match_text(name, text, false, true);
}

static bool
uobject_class_is(uobject_t *obj, str_t cls_name)
{
  if (!obj || !obj->cls) {
    return false;
  }

  return fname_is(obj->cls->name, cls_name);
}

static bool
ufunc_is(ufunc_t *func, str_t func_name)
{
  if (!func) {
    return false;
  }

  return fname_is(func->name, func_name);
}

static bool
should_disable_tutorials(cheats_t *cheats)
{
  return cheats && cheats->inited && mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.disable_tutorials_h);
}

static bool
should_restore_jbox_music(cheats_t *cheats)
{
  return cheats && cheats->inited && mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.restore_jbox_h);
}

static bool
jbox_play_music(jbox_controller_t *controller, jbox_state_t state)
{
  ASSERT(controller != NULL);
  ASSERT(unreal_uobject_is_valid((uobject_t *)controller));

  if (!g_cheats.jbox.play_music) {
    return false;
  }

  controller->repeat_mode     = state.repeat_mode;
  controller->shuffle_enabled = state.shuffle_enabled;

  fgameplay_tag_t product_id = {
    .tag_name = state.product_id,
  };

  return g_cheats.jbox.play_music(controller, &product_id, false);
}

static void
cheats_save_jbox_state(jbox_state_t *state, str_t state_path)
{
  if (str_is_empty(state_path)) {
    return;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_list_t lines = {0};
    {
      str_t product_id = unreal_fname_to_str(state->product_id, tmp.arena);

      str_list_push(tmp.arena, &lines, STR_LIT("[state]"));
      str_list_push_fmt(tmp.arena, &lines, "product_id      = %.*s", STR_ARG(product_id));
      str_list_push_fmt(tmp.arena, &lines, "playback_state  = %d", state->playback_state);
      str_list_push_fmt(tmp.arena, &lines, "repeat_mode     = %d", state->repeat_mode);
      str_list_push_fmt(tmp.arena, &lines, "shuffle_enabled = %s", BOOL_AS_STR(state->shuffle_enabled));
    }
    file_write_lines(lines, state_path);
  }
  scratch_end(tmp);
}

static void
cheats_load_jbox_state(jbox_state_t *state, str_t state_path)
{
  if (str_is_empty(state_path) || !file_exists(state_path)) {
    return;
  }

  mem_set(state, 0, sizeof(*state));

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t text = file_read_all(state_path, tmp.arena);
    if (!str_is_empty(text)) {
      str_array_t        lines    = ini_split_lines(tmp.arena, text);
      ini_section_list_t sections = ini_parse_sections(tmp.arena, lines);
      for (ini_section_t *section = sections.first; section; section = section->next) {
        if (str_equal_icase(section->name, STR_LIT("state"))) {
          for (str_node_t *node = section->lines.first; node; node = node->next) {
            str_t key   = {0};
            str_t value = {0};
            if (!ini_parse_kv(node->str, &key, &value)) {
              continue;
            }

            if (str_equal_icase(key, STR_LIT("product_id"))) {
              state->product_id = unreal_fname_from_str(value, FNAME_FIND);
            } else if (str_equal_icase(key, STR_LIT("playback_state"))) {
              str_parse_u8(value, &state->playback_state);
              state->playback_state = CLAMP(state->playback_state, JBOX_PLAYBACK_STOPPED, JBOX_PLAYBACK_PAUSED);
            } else if (str_equal_icase(key, STR_LIT("repeat_mode"))) {
              str_parse_u8(value, &state->repeat_mode);
              state->repeat_mode = CLAMP(state->repeat_mode, JBOX_REPEAT_MODE_OFF, JBOX_REPEAT_MODE_SONG);
            } else if (str_equal_icase(key, STR_LIT("shuffle_enabled"))) {
              state->shuffle_enabled = str_parse_bool(value, false);
            }
          }
        }
      }
    }
  }
  scratch_end(tmp);
}

static void
cheats_on_jbox_controller_create(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  cheats_t *cheats = &g_cheats;

  if (obj && obj->cls && unreal_fname_equal(obj->cls->name, cheats->jbox.controller_cls_name, true) && !unreal_uobject_is_default(obj)) {
    cheats->jbox.controller           = (jbox_controller_t *)obj;
    cheats->jbox.should_restore_state = true;
  }
}

static void
cheats_on_jbox_controller_delete(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  cheats_t *cheats = &g_cheats;

  if (cheats->jbox.controller && cheats->jbox.controller == (jbox_controller_t *)obj) {
    cheats->jbox.controller           = NULL;
    cheats->jbox.in_arcade_mode       = false;
    cheats->jbox.should_restore_state = false;
  }
}

static void MOD_CALL
cheats_cmd_gears(mod_handle_t h, str_t name, str_t args, void *user);
static void MOD_CALL
cheats_cmd_give(mod_handle_t h, str_t name, str_t args, void *user);

static bool MOD_CALL
cheats_init(mod_handle_t h)
{
  cheats_t *cheats = &g_cheats;
  mem_zero(cheats, sizeof(*cheats));

  cheats->perm = mod_get_perm(h);
  ASSERT(cheats->perm != NULL);

  cheats->cfg.disable_tutorials_h     = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_DISABLE_TUTORIALS));
  cheats->cfg.restore_jbox_h          = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_RESTORE_JUKEBOX));
  cheats->cfg.debug_widget_keybind_h  = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_DEBUG_WIDGET_KEYBIND));
  cheats->cfg.enable_infinite_jumps_h = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_ENABLE_INFINITE_JUMPS));
  cheats->cfg.infinite_dash_h         = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_INFINITE_DASH));
  cheats->item_give_count             = 1;

  /* the offsets are read from the code that uses them: or byte [rdi+air], 1 and comiss xmm7, [rbx+cooldown] */
  void           *air_dash         = NULL;
  void           *dash_cooldown    = NULL;
  sigscan_entry_t air_dash_sig     = SIG_ENTRY_DIRECT(air_dash, "0F B6 87 ? ? ? ? 80 8F ? ? ? ? 01 24 CF 0C 4C", 0x02143C4C, 0x020C752C);
  sigscan_entry_t dash_cooldown_sig = SIG_ENTRY_DIRECT(dash_cooldown, "0F 57 FF 0F 2F BB ? ? ? ? 0F 82 ? ? ? ? 80 A3", 0x02142176, 0x020C5A56);

  if (mod_sigscan(&air_dash_sig) == SIG_ERR_OK && air_dash_sig.addr) {
    cheats->air_dash_offset = *(int32_t *)((uint8_t *)air_dash_sig.addr + 9);
  }

  if (mod_sigscan(&dash_cooldown_sig) == SIG_ERR_OK && dash_cooldown_sig.addr) {
    cheats->dash_cooldown_offset = *(int32_t *)((uint8_t *)dash_cooldown_sig.addr + 6);
  }

  mod_register_cmd(h, STR_LIT("cheats.gears"), STR_LIT("[<amount> | +<amount>]: show or change the gears"), cheats_cmd_gears, NULL);
  mod_register_cmd(h, STR_LIT("cheats.give"), STR_LIT("<item tag> [count]: give an item, e.g. Variable.LifeTank"), cheats_cmd_give, NULL);

  if (!cheats->air_dash_offset || !cheats->dash_cooldown_offset) {
    LOG_WARN("cheats: the dash code was not found, infinite dash is off");
  }

  void *jbox_play_music_addr = NULL;
  sigscan_entry_t jbox_play_music_sig = SIG_ENTRY_DIRECT(jbox_play_music_addr,
                                                          "48 89 5C 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 4C 8B F2 48 8B F9");
  mod_sigscan(&jbox_play_music_sig);
  cheats->jbox.play_music = jbox_play_music_addr;

  cheats->jbox.state_path = path_join(cheats->perm, mod_get_mod_dir(h), STR_LIT("jukebox_state.ini"));
  cheats_load_jbox_state(&cheats->jbox.saved_state, cheats->jbox.state_path);

  cheats->jbox.controller_cls_name = unreal_fname_from_str(STR_LIT("JukeBoxController_BP_C"), FNAME_FIND);

  mod_register_uobject_listener(h, UOBJECT_LISTENER_KIND_CREATE, cheats_on_jbox_controller_create, NULL);
  mod_register_uobject_listener(h, UOBJECT_LISTENER_KIND_DELETE, cheats_on_jbox_controller_delete, NULL);

  uclass_t *jbox_controller_cls = unreal_uclass_find(STR_LIT("JukeBoxController_BP_C"), false, true);
  if (jbox_controller_cls) {
    cheats->jbox.controller = (jbox_controller_t *)unreal_uobject_find_first_of(jbox_controller_cls);
    if (cheats->jbox.controller) {
      cheats->jbox.should_restore_state = true;
    }
  }

  cheats->inited = true;
  return true;
}

static void MOD_CALL
cheats_deinit(mod_handle_t h)
{
  (void)h;

  cheats_t *cheats = &g_cheats;
  mem_zero(cheats, sizeof(*cheats));
}

static bool
cheats_process_event_pre(mod_handle_t h, uobject_t *self, ufunc_t *func, void *params)
{
  (void)h;
  (void)params;
  (void)self;

  bool should_consume = false;

  cheats_t *cheats = &g_cheats;

  if (should_disable_tutorials(cheats) && ufunc_is(func, STR_LIT("WidgetAnimationEvt_OpenAnimation_K2Node_WidgetAnimationEvent"))) {
    should_consume = true;
  }

  if (mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.enable_infinite_jumps_h)) {
    if (func && unreal_fname_match_text(func->name, STR_LIT("CanJumpInternal"), false, true)) {
      *((uint8_t *)params) = 1;
      should_consume = true;
    }
  }

  return should_consume;
}

static void
cheats_process_event_post(mod_handle_t h, uobject_t *self, ufunc_t *func, void *params, bool consumed)
{
  (void)h;
  (void)params;
  (void)consumed;

  cheats_t *cheats = &g_cheats;

  if (should_disable_tutorials(cheats) && uobject_class_is(self, STR_LIT("DrawNewTutorial_UI_C")) && ufunc_is(func, STR_LIT("Construct"))) {
    cheats->tutorial_close_ticks = 2;
  }

  if (should_restore_jbox_music(cheats)) {
    if (uobject_class_is(self, STR_LIT("SM_Arcade_BP_C"))) {
      if (ufunc_is(func, STR_LIT("OnNotifyBegin_59884126489B8A771076CBBF9EB59637"))) {
        cheats->jbox.in_arcade_mode = true;
      } else if (ufunc_is(func, STR_LIT("OnCloseArcadeMenu_855C3BCC4FA558B3F4D02C834A70C17B"))) {
        cheats->jbox.in_arcade_mode       = false;
        cheats->jbox.should_restore_state = true;
      }
    }
  }
}

static void
cheats_ufunction_invoke_post(mod_handle_t h, ufunc_t *func, uobject_t *self, fframe_t *stack, void *result, bool consumed)
{
  (void)h;
  (void)stack;
  (void)result;
  (void)consumed;

  cheats_t *cheats = &g_cheats;
  if (should_disable_tutorials(cheats)) {
    if (uobject_class_is(self, STR_LIT("HbkShowNewTutorialTask")) && ufunc_is(func, STR_LIT("Activate"))) {
      if (self->cls) {
        ufunc_t *end_task_func = unreal_ustruct_find_func((ustruct_t *)self->cls, STR_LIT("OnDisplayEndClear"));
        if (end_task_func) {
          unreal_process_event(self, end_task_func, NULL);
        }
      }
      return;
    }
  }
}

/* Showing a wheel that is already open breaks its input, so the shortcut toggles it.
 * It is looked up by class name: after a level change the class is a new object. */
static uobject_t *
cheats_find_debug_widget(void)
{
  uobject_t *fallback = NULL;
  int        count    = unreal_uobject_array_count();

  for (int idx = 0; idx < count; ++idx) {
    uobject_t *obj = unreal_uobject_array_get_obj(idx);
    if (!obj || !obj->cls || (obj->obj_flags & RF_CLASS_DEFAULT_OBJECT) ||
        !unreal_fname_match_text(((uobject_t *)obj->cls)->name, STR_LIT("DebugWidgetSelect_UI_C"), false, true) ||
        !unreal_uobject_is_valid(obj)) {
      continue;
    }

    // the live wheel is the one in the viewport
    ufunc_t *is_in_viewport = unreal_ustruct_find_func((ustruct_t *)obj->cls, STR_LIT("IsInViewport"));
    struct {
      bool result;
    } params = {0};

    if (is_in_viewport) {
      unreal_process_event(obj, is_in_viewport, &params);
    }

    if (params.result) {
      return obj;
    }

    if (!fallback) {
      fallback = obj;
    }
  }

  return fallback;
}

static void
cheats_toggle_debug_widget(void)
{
  uobject_t *wheel = cheats_find_debug_widget();
  if (!wheel) {
    LOG_WARN("debug wheel: the shortcut was pressed but there is no wheel to show (not in a level?)");
    return;
  }

  ustruct_t *cls            = (ustruct_t *)wheel->cls;
  ufunc_t   *show           = unreal_ustruct_find_func(cls, STR_LIT("Show"));
  ufunc_t   *hide           = unreal_ustruct_find_func(cls, STR_LIT("Hide"));
  ufunc_t   *get_visibility = unreal_ustruct_find_func(cls, STR_LIT("GetVisibility"));

  if (!show || !hide || !get_visibility) {
    LOG_WARN("debug wheel: the wheel has no Show / Hide to call");
    return;
  }

  struct {
    uint8_t result; // ESlateVisibility, 0 is Visible
  } visibility = {1};

  unreal_process_event(wheel, get_visibility, &visibility);
  unreal_process_event(wheel, visibility.result == 0 ? hide : show, NULL);
  LOG_INFO("debug wheel: %s", visibility.result == 0 ? "closed" : "opened");
}

/* Some tutorials are started from the game's code, where Activate is not seen, but every
 * pop-up builds a DrawNewTutorial_UI_C. Then the tasks showing a tutorial are ended a tick
 * later, as if the player closed it. A task shows something while this byte is set. */
#define CHEATS_TUTORIAL_TASK_SHOWING_OFFSET 0xAC

static void
cheats_close_shown_tutorials(void)
{
  int closed = 0;
  int count  = unreal_uobject_array_count();

  for (int idx = 0; idx < count; ++idx) {
    uobject_t *obj = unreal_uobject_array_get_obj(idx);
    if (!obj || !obj->cls || (obj->obj_flags & RF_CLASS_DEFAULT_OBJECT) || !uobject_class_is(obj, STR_LIT("HbkShowNewTutorialTask")) ||
        !unreal_uobject_is_valid(obj)) {
      continue;
    }

    if (((ustruct_t *)obj->cls)->props_size <= CHEATS_TUTORIAL_TASK_SHOWING_OFFSET ||
        ((const uint8_t *)obj)[CHEATS_TUTORIAL_TASK_SHOWING_OFFSET] == 0) {
      continue;
    }

    ufunc_t *end_task_func = unreal_ustruct_find_func((ustruct_t *)obj->cls, STR_LIT("OnDisplayEndClear"));
    if (end_task_func) {
      unreal_process_event(obj, end_task_func, NULL);
      closed += 1;
    }
  }

  LOG_INFO("tutorial pop-up: %d task(s) ended", closed);
}

static uobject_t *
cheats_player_manager(cheats_t *cheats)
{
  if (cheats->player_manager && unreal_uobject_is_valid(cheats->player_manager)) {
    return cheats->player_manager;
  }

  cheats->player_manager = NULL;

  /* the object array is long: it is looked through at most once a second */
  double now = (double)time_now_us() / 1e6;
  if (now - cheats->player_manager_search_time < 1.0) {
    return NULL;
  }

  cheats->player_manager_search_time = now;

  uclass_t *cls          = unreal_uclass_find(STR_LIT("HbkPlayerCharacterManager"), false, true);
  cheats->player_manager = cls ? unreal_uobject_find_first_of(cls) : NULL;
  return cheats->player_manager;
}

typedef struct cheats_call_s cheats_call_t;
struct cheats_call_s {
  uobject_t *self;
  ufunc_t   *func;
  uint8_t    params[128];
};

static bool
cheats_call_begin(cheats_call_t *call, uobject_t *self, str_t name)
{
  mem_zero(call, sizeof(*call));
  call->self = self;
  call->func = self && self->cls ? unreal_ustruct_find_func((ustruct_t *)self->cls, name) : NULL;
  return call->func && call->func->params_size <= sizeof(call->params);
}

static void *
cheats_call_param(cheats_call_t *call, str_t name)
{
  fprop_t *prop = unreal_ustruct_find_prop((ustruct_t *)call->func, name);
  return prop ? call->params + prop->offset_internal : NULL;
}

static void
cheats_call(cheats_call_t *call)
{
  unreal_process_event(call->self, call->func, call->params);
}

static uobject_t *
cheats_player_character(cheats_t *cheats)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, cheats_player_manager(cheats), STR_LIT("GetCurrentPlayerCharacter"))) {
    return NULL;
  }

  cheats_call(&call);

  uobject_t **player = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return player && *player && unreal_uobject_is_valid(*player) ? *player : NULL;
}

/* -1 when there is no player (title screen, loading) */
static int
cheats_get_money(cheats_t *cheats)
{
  if (!cheats_player_character(cheats)) {
    return -1;
  }

  cheats_call_t call;
  if (!cheats_call_begin(&call, cheats_player_manager(cheats), STR_LIT("GetMoney"))) {
    return -1;
  }

  cheats_call(&call);

  int32_t *money = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return money ? *money : -1;
}

static void
cheats_set_money(cheats_t *cheats, int64_t money)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, cheats_player_manager(cheats), STR_LIT("SetMoney"))) {
    return;
  }

  int32_t *new_money = cheats_call_param(&call, STR_LIT("NewMoney"));
  if (new_money) {
    *new_money = (int32_t)CLAMP(money, 0, 99999999);
    cheats_call(&call);
    LOG_INFO("cheats: gears set to %d", *new_money);
  }
}

static bool
cheats_give_item(cheats_t *cheats, cheats_item_t *item, int count)
{
  uobject_t    *player = cheats_player_character(cheats);
  cheats_call_t call;

  if (!player || !cheats_call_begin(&call, cheats_player_manager(cheats), STR_LIT("PlayerTakeItem"))) {
    return false;
  }

  uobject_t      **context = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  fgameplay_tag_t *id      = cheats_call_param(&call, STR_LIT("InventoryId"));
  int32_t         *amount  = cheats_call_param(&call, STR_LIT("Count"));
  bool            *taken   = cheats_call_param(&call, STR_LIT("ReturnValue"));

  if (!context || !id || !amount) {
    return false;
  }

  *context = player;
  *id      = item->id;
  *amount  = count;
  cheats_call(&call);

  LOG_INFO("cheats: gave %d x %.*s (%.*s)", count, STR_ARG(item->name), STR_ARG(item->tag));
  return !taken || *taken;
}

/* the dodge cooldown never runs, and the air dash is never used up */
static void
cheats_infinite_dash(cheats_t *cheats)
{
  if (cheats->dash_cooldown_offset <= 0 || cheats->air_dash_offset <= 0) {
    return;
  }

  uobject_t *player = cheats_player_character(cheats);
  if (!player || ((ustruct_t *)player->cls)->props_size <= MAX_VAL(cheats->dash_cooldown_offset, cheats->air_dash_offset)) {
    return;
  }

  uint8_t *bytes    = (uint8_t *)player;
  float   *cooldown = (float *)(bytes + cheats->dash_cooldown_offset);

  if (*cooldown > 0.0f) {
    *cooldown = 0.0f;
  }

  bytes[cheats->air_dash_offset] &= (uint8_t)~1u;
}

static str_t
cheats_text(ftext_t *text, arena_t *arena)
{
  if (!text || !text->text_data.obj) {
    return STR_NULL;
  }

  itext_data_t *data = text->text_data.obj;
  if (!data->vtable || !data->vtable->get_display_string) {
    return STR_NULL;
  }

  fstring_t *display = data->vtable->get_display_string(data);
  return display ? unreal_fstring_to_str(*display, arena) : STR_NULL;
}

static int
cheats_item_compare(const void *a, const void *b)
{
  const cheats_item_t *x = a;
  const cheats_item_t *y = b;

  if (x->category != y->category) {
    return x->category - y->category;
  }

  return str_compare(x->tag, y->tag, 0);
}

/* every item the game knows: the inventory data asset maps each tag to its class, which names it */
static void
cheats_load_items(cheats_t *cheats)
{
  if (cheats->items) {
    return;
  }

  uclass_t  *cls   = unreal_uclass_find(STR_LIT("HbkInventoryDataAsset"), false, true);
  uobject_t *asset = cls ? unreal_uobject_find_first_of(cls) : NULL;
  if (!asset) {
    return;
  }

  fprop_map_t *prop = (fprop_map_t *)unreal_ustruct_find_prop((ustruct_t *)asset->cls, STR_LIT("InventoryMap"));
  if (!prop) {
    return;
  }

  void   *map   = (uint8_t *)asset + prop->base.offset_internal;
  int32_t count = unreal_map_num(map, prop);
  int32_t max   = unreal_map_max_index(map, prop);
  if (count <= 0 || max <= 0) {
    return;
  }

  cheats->items      = ARENA_PUSH_ARRAY_ZERO(cheats->perm, cheats_item_t, count);
  cheats->item_count = 0;

  for (int32_t i = 0; i < max && cheats->item_count < count; ++i) {
    const fgameplay_tag_t *key   = unreal_map_get_key(map, prop, i);
    uclass_t             **value = unreal_map_get_value(map, prop, i);
    if (!key || !value) {
      continue;
    }

    cheats_item_t item = {.id = *key, .tag = unreal_fname_to_str(key->tag_name, cheats->perm), .category = -1};

    for (int c = 0; c < COUNTOF(g_item_categories); ++c) {
      if (str_has_prefix(item.tag, str_from_cstr(g_item_categories[c].prefix), 0)) {
        item.category = c;
      }
    }

    if (item.category < 0) {
      continue;
    }

    uobject_t *cdo  = *value ? (*value)->cdo : NULL;
    fprop_t   *name = cdo ? unreal_ustruct_find_prop((ustruct_t *)cdo->cls, STR_LIT("InventoryObjectNameText")) : NULL;

    item.name = name ? cheats_text((ftext_t *)((uint8_t *)cdo + name->offset_internal), cheats->perm) : STR_NULL;
    if (str_is_empty(item.name)) {
      item.name = item.tag;
    }

    cheats->items[cheats->item_count++] = item;
  }

  qsort(cheats->items, (size_t)cheats->item_count, sizeof(cheats->items[0]), cheats_item_compare);
  LOG_INFO("cheats: %d items", cheats->item_count);
}

static bool
cheats_item_matches(cheats_t *cheats, cheats_item_t *item)
{
  if (cheats->item_category > 0 && item->category != cheats->item_category - 1) {
    return false;
  }

  str_t filter = str_from_cstr(cheats->item_filter);
  return str_is_empty(filter) || str_find(item->name, filter, STR_CMP_FLAG_IGNORE_CASE, NULL) ||
         str_find(item->tag, filter, STR_CMP_FLAG_IGNORE_CASE, NULL);
}

static void
cheats_draw_money(cheats_t *cheats, struct nk_context *ctx)
{
  int money = cheats_get_money(cheats);

  nk_layout_row_dynamic(ctx, 22.0f, 1);

  if (money < 0) {
    nk_label(ctx, "Load a save to change the gears", NK_TEXT_LEFT);
    return;
  }

  nk_labelf(ctx, NK_TEXT_LEFT, "Gears: %d", money);

  nk_layout_row_dynamic(ctx, 26.0f, 4);
  nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, cheats->money_text, (int)sizeof(cheats->money_text), nk_filter_decimal);

  if (nk_button_label(ctx, "Set") && cheats->money_text[0]) {
    cheats_set_money(cheats, atoll(cheats->money_text));
  }

  if (nk_button_label(ctx, "+10,000")) {
    cheats_set_money(cheats, (int64_t)money + 10000);
  }

  if (nk_button_label(ctx, "+100,000")) {
    cheats_set_money(cheats, (int64_t)money + 100000);
  }
}

static void
cheats_draw_items(cheats_t *cheats, struct nk_context *ctx)
{
  cheats_load_items(cheats);

  if (!cheats->items) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "The game has not loaded its item list yet", NK_TEXT_LEFT);
    return;
  }

  const char *labels[COUNTOF(g_item_categories) + 1] = {"All"};
  for (int c = 0; c < COUNTOF(g_item_categories); ++c) {
    labels[c + 1] = g_item_categories[c].label;
  }

  nk_layout_row_dynamic(ctx, 26.0f, 3);
  cheats->item_category = nk_combo(ctx, labels, COUNTOF(labels), cheats->item_category, 24, nk_vec2(200.0f, 280.0f));
  nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, cheats->item_filter, (int)sizeof(cheats->item_filter), nk_filter_default);
  nk_property_int(ctx, "Count:", 1, &cheats->item_give_count, 99, 1, 1.0f);

  bool has_player = cheats_player_character(cheats) != NULL;

  if (!has_player) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Load a save to give items", NK_TEXT_LEFT);
  }

  nk_layout_row_dynamic(ctx, 320.0f, 1);

  if (nk_group_begin(ctx, "cheats.items", 0)) {
    float give_w = ui_text_width(ctx, STR_LIT("Give")) + 24.0f;

    for (int i = 0; i < cheats->item_count; ++i) {
      cheats_item_t *item = &cheats->items[i];
      if (!cheats_item_matches(cheats, item)) {
        continue;
      }

      nk_layout_row_template_begin(ctx, 22.0f);
      nk_layout_row_template_push_dynamic(ctx);
      nk_layout_row_template_push_dynamic(ctx);
      nk_layout_row_template_push_static(ctx, give_w);
      nk_layout_row_template_end(ctx);

      nk_text(ctx, (const char *)item->name.data, (int)item->name.len, NK_TEXT_LEFT);
      nk_text_colored(ctx, (const char *)item->tag.data + 10, (int)item->tag.len - 10, NK_TEXT_LEFT, UI_C_TEXT_DIM);

      if (nk_button_label(ctx, "Give") && has_player) {
        cheats_give_item(cheats, item, cheats->item_give_count);
      }
    }

    nk_group_end(ctx);
  }
}

static void MOD_CALL
cheats_cmd_gears(mod_handle_t h, str_t name, str_t args, void *user)
{
  (void)h;
  (void)name;
  (void)user;

  cheats_t *cheats = &g_cheats;
  int       money  = cheats_get_money(cheats);

  if (money < 0) {
    LOG_ERROR("cheats: load a save first");
    return;
  }

  args = str_trim_space(args);

  if (str_is_empty(args)) {
    LOG_INFO("cheats: gears: %d", money);
    return;
  }

  int64_t value = 0;
  bool    add   = args.data[0] == '+';

  if (!str_parse_s64(add ? str_make(args.data + 1, args.len - 1) : args, &value)) {
    LOG_ERROR("Usage: cheats.gears [<amount> | +<amount>]");
    return;
  }

  cheats_set_money(cheats, add ? (int64_t)money + value : value);
}

static void MOD_CALL
cheats_cmd_give(mod_handle_t h, str_t name, str_t args, void *user)
{
  (void)h;
  (void)name;
  (void)user;

  cheats_t *cheats = &g_cheats;
  cheats_load_items(cheats);

  str_t    tag   = str_trim_space(args);
  int64_t  count = 1;
  uint64_t space = 0;

  if (str_find(tag, STR_LIT(" "), 0, &space)) {
    str_parse_s64(str_trim_space(str_make(tag.data + space, tag.len - space)), &count);
    tag = str_make(tag.data, space);
  }

  for (int i = 0; i < cheats->item_count; ++i) {
    if (str_equal(cheats->items[i].tag, tag, STR_CMP_FLAG_IGNORE_CASE) ||
        str_equal(str_make(cheats->items[i].tag.data + 10, cheats->items[i].tag.len - 10), tag, STR_CMP_FLAG_IGNORE_CASE)) {
      if (!cheats_give_item(cheats, &cheats->items[i], (int)CLAMP(count, 1, 99))) {
        LOG_ERROR("cheats: could not give %.*s (load a save first)", STR_ARG(tag));
      }

      return;
    }
  }

  LOG_ERROR("Usage: cheats.give <item tag, e.g. Variable.LifeTank> [count]");
}

static void MOD_CALL
cheats_draw_panel(mod_handle_t h, struct nk_context *ctx)
{
  (void)h;

  static enum nk_collapse_states money_open = NK_MAXIMIZED;
  static enum nk_collapse_states items_open = NK_MINIMIZED;

  cheats_t *cheats = &g_cheats;
  if (!cheats->inited) {
    return;
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Gears", &money_open)) {
    cheats_draw_money(cheats, ctx);
    nk_tree_pop(ctx);
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Items", &items_open)) {
    cheats_draw_items(cheats, ctx);
    nk_tree_pop(ctx);
  }
}

static void
cheats_tick(mod_handle_t h, float delta)
{
  (void)h;
  (void)delta;

  cheats_t *cheats = &g_cheats;
  if (!cheats->inited) {
    return;
  }

  if (cheats->tutorial_close_ticks > 0 && --cheats->tutorial_close_ticks == 0 && should_disable_tutorials(cheats)) {
    cheats_close_shown_tutorials();
  }

  if (should_restore_jbox_music(cheats)) {
    if (cheats->jbox.controller && unreal_uobject_is_valid((uobject_t *)cheats->jbox.controller)) {
      if (cheats->jbox.should_restore_state) {
        cheats->jbox.should_restore_state = false;
        if (cheats->jbox.saved_state.playback_state == JBOX_PLAYBACK_PLAYING) {
          bool ok                           = jbox_play_music(cheats->jbox.controller, cheats->jbox.saved_state);
          cheats->jbox.should_restore_state = !ok;
        }
      }

      jbox_state_t state = {
        .product_id      = cheats->jbox.controller->playing_music_data.product_id.tag_name,
        .repeat_mode     = cheats->jbox.controller->repeat_mode,
        .shuffle_enabled = cheats->jbox.controller->shuffle_enabled,
        .playback_state  = cheats->jbox.controller->playback_state,
      };

      /* save valid state changes made outside arcade mode */
      if (memcmp(&cheats->jbox.saved_state, &state, sizeof(state)) != 0 && state.product_id.cmp_idx != 0 && !cheats->jbox.in_arcade_mode) {
#if 0
        tmp_arena_t tmp = scratch_begin(NULL);
        {
          LOG_DEBUG("%llu: %s: jukebox state changed: repeat mode: %d -> %d, shuffle: %d -> %d, playback state: %d (%s) -> %d (%s), product id: %.*s -> %.*s",
                    globals.frame_counter,
                    MOD_ID,
                    cheats->jbox.saved_state.repeat_mode,
                    state.repeat_mode,
                    cheats->jbox.saved_state.shuffle_enabled,
                    state.shuffle_enabled,
                    cheats->jbox.saved_state.playback_state,
                    jbox_playback_state_to_str(cheats->jbox.saved_state.playback_state),
                    state.playback_state,
                    jbox_playback_state_to_str(state.playback_state),
                    STR_ARG(unreal_fname_to_str(cheats->jbox.saved_state.product_id, tmp.arena)),
                    STR_ARG(unreal_fname_to_str(state.product_id, tmp.arena)));
        }
        scratch_end(tmp);
#endif

        cheats->jbox.saved_state = state;
        cheats_save_jbox_state(&cheats->jbox.saved_state, cheats->jbox.state_path);
      }
    }
  }

  if (keybind_is_pressed(mod_manager_cfg_get_keybind(&globals.mod_manager, cheats->cfg.debug_widget_keybind_h))) {
    cheats_toggle_debug_widget();
  }

  if (mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.infinite_dash_h)) {
    cheats_infinite_dash(cheats);
  }
}

/* the mod used to be called Tweaks: its settings move with it */
static void
cheats_move_old_settings(mod_manager_t *manager)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t root = mod_cfg_string_as_str(&manager->cfg.root_mod_dir);
    if (!path_is_abs(root)) {
      root = path_join(tmp.arena, manager->game_dir, root);
    }

    str_t old_dir = path_join(tmp.arena, root, STR_LIT("builtin.tweaks"));
    str_t new_dir = path_join(tmp.arena, root, STR_LIT("builtin.cheats"));

    if (dir_exists(old_dir) && !dir_exists(new_dir)) {
      if (MoveFileW(str16_from_str(tmp.arena, old_dir).data, str16_from_str(tmp.arena, new_dir).data)) {
        LOG_INFO("cheats: settings moved from builtin.tweaks");
      } else {
        LOG_WARN("cheats: could not move builtin.tweaks to builtin.cheats (%lu)", GetLastError());
      }
    }
  }
  scratch_end(tmp);
}

void
register_builtin_cheats(mod_manager_t *manager)
{
  mod_option_info_t options[] = {
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_DISABLE_TUTORIALS),
      .label       = STR_CLIT("Disable tutorial pop-ups"),
      .description = STR_CLIT("Prevents tutorial pop-ups from appearing"),
      .val = {
        .boolean = {
          .default_val = true,
        },
      },
    },
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_RESTORE_JUKEBOX),
      .label       = STR_CLIT("Remember jukebox playback"),
      .description = STR_CLIT("Restores the jukebox track, playback state, repeat mode, and shuffle setting after stage transitions and game restarts"),
      .val = {
        .boolean = {
          .default_val = true,
        },
      },
    },
    {
      .type        = MOD_OPTION_KEYBIND,
      .id          = STR_CLIT(CFG_DEBUG_WIDGET_KEYBIND),
      .label       = STR_CLIT("Open Debug Widget"),
      .description = STR_CLIT("Opens Debug Widget that is inaccessible by default."),
      .val = {
        .keybind = {
          .default_val = keybind_parse(STR_LIT("Gamepad_LeftThumbstick+Gamepad_RightShoulder"), KEYBIND_NULL),
        },
      },
    },
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_ENABLE_INFINITE_JUMPS),
      .label       = STR_CLIT("Enable infinite jumps"),
      .description = STR_CLIT("Jump as high as you can, lol"),
      .val = {
        .boolean = {
          .default_val = false,
        },
      },
    },
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_INFINITE_DASH),
      .label       = STR_CLIT("Infinite dash"),
      .description = STR_CLIT("Dash as often as you like, on the ground and in the air"),
      .val = {
        .boolean = {
          .default_val = false,
        },
      },
    },
  };

  mod_manifest_t manifest = {
    .info = {
      .id          = STR_CLIT(MOD_ID),
      .name        = STR_CLIT("Cheats"),
      .author      = STR_CLIT("akmubi"),
      .description = STR_CLIT("Cheats and quality-of-life changes"),
      .kind        = MOD_KIND_MOD,
      .version     = MAKE_VERSION(0, 1, 0),
    },
    .options      = options,
    .option_count = COUNTOF(options),
  };

  mod_dll_runtime_funcs_t funcs = {
    .init             = cheats_init,
    .deinit           = cheats_deinit,
    .tick             = cheats_tick,
    .pe_pre           = cheats_process_event_pre,
    .pe_post          = cheats_process_event_post,
    .func_invoke_post = cheats_ufunction_invoke_post,
    .draw_panel       = cheats_draw_panel,
  };

  cheats_move_old_settings(manager);

  mod_handle_t h = mod_register_builtin(manager, &manifest, funcs);
  if (h == MOD_HANDLE_INVALID) {
    CONSOLE_ERROR("Failed to register the built-in Cheats mod");
  }
}
