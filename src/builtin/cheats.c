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
#define CFG_GOD_MODE              "god-mode"
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
  mod_cfg_handle_t god_mode_h;
};

typedef struct cheats_checkpoint_s cheats_checkpoint_t;
struct cheats_checkpoint_s {
  str_t       level;
  fname_t     spawn;
  const char *label;
  bool        hideout;
};

typedef struct cheats_stage_s cheats_stage_t;
struct cheats_stage_s {
  const char *label;
  int         first;
  int         count;
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

  /* where the dodge cooldown, the on-beat dodge chain and the "air dash used" bit are on the player character */
  int32_t dash_cooldown_offset;
  int32_t dash_chain_offset;
  int32_t air_dash_offset;

  uobject_t *player_manager;
  double     player_manager_search_time;

  cheats_item_t *items;
  int            item_count;
  int            item_category;
  int            item_give_count;
  char           item_filter[64];
  char           money_text[16];

  cheats_checkpoint_t *checkpoints;
  int                  checkpoint_count;
  cheats_stage_t      *stages;
  int                  stage_count;
  int                  stage_index;
  int                  checkpoint_index;
  int                  teleport_request; // checkpoint + 1, taken on the next tick
  bool                 teleport_saving;
  bool                 teleport_saw_save;
  int                  teleport_frames;
  bool                 god_mode_on;
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
static void MOD_CALL
cheats_cmd_teleport(mod_handle_t h, str_t name, str_t args, void *user);

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
  cheats->cfg.god_mode_h              = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_GOD_MODE));
  cheats->item_give_count             = 1;

  /* the offsets are read from the code that uses them: or byte [rdi+air], 1, comiss xmm7, [rbx+cooldown] and inc dword [rbx+chain] */
  void           *air_dash          = NULL;
  void           *dash_cooldown     = NULL;
  void           *dash_chain        = NULL;
  sigscan_entry_t air_dash_sig      = SIG_ENTRY_DIRECT(air_dash, "0F B6 87 ? ? ? ? 80 8F ? ? ? ? 01 24 CF 0C 4C", 0x02143C4C, 0x020C752C);
  sigscan_entry_t dash_cooldown_sig = SIG_ENTRY_DIRECT(dash_cooldown, "0F 57 FF 0F 2F BB ? ? ? ? 0F 82 ? ? ? ? 80 A3", 0x02142176, 0x020C5A56);
  sigscan_entry_t dash_chain_sig    = SIG_ENTRY_DIRECT(dash_chain, "FF 83 ? ? ? ? 48 8B CB E8 ? ? ? ? 48 8B 16 48 8B C8 E8 ? ? ? ? 84 C0 74 ? 8B 83 ? ? ? ? 83 F8 04",
                                                       0x0214297A, 0x020C625A);

  if (mod_sigscan(&air_dash_sig) == SIG_ERR_OK && air_dash_sig.addr) {
    cheats->air_dash_offset = *(int32_t *)((uint8_t *)air_dash_sig.addr + 9);
  }

  if (mod_sigscan(&dash_cooldown_sig) == SIG_ERR_OK && dash_cooldown_sig.addr) {
    cheats->dash_cooldown_offset = *(int32_t *)((uint8_t *)dash_cooldown_sig.addr + 6);
  }

  mod_register_cmd(h, STR_LIT("cheats.gears"), STR_LIT("[<amount> | +<amount>]: show or change the gears"), cheats_cmd_gears, NULL);
  mod_register_cmd(h, STR_LIT("cheats.give"), STR_LIT("<item tag> [count]: give an item, e.g. Variable.LifeTank"), cheats_cmd_give, NULL);
  mod_register_cmd(h, STR_LIT("cheats.teleport"), STR_LIT("<checkpoint>: load the save at a checkpoint, e.g. ST02_CP0010"), cheats_cmd_teleport, NULL);

  if (mod_sigscan(&dash_chain_sig) == SIG_ERR_OK && dash_chain_sig.addr) {
    cheats->dash_chain_offset = *(int32_t *)((uint8_t *)dash_chain_sig.addr + 2);
  }

  if (!cheats->air_dash_offset || !cheats->dash_cooldown_offset || !cheats->dash_chain_offset) {
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
cheats_find_live(str_t class_name)
{
  uclass_t *cls = unreal_uclass_find(class_name, false, true);
  if (!cls) {
    return NULL;
  }

  for (int i = 0, count = unreal_uobject_array_count(); i < count; ++i) {
    uobject_t *obj = unreal_uobject_array_get_obj(i);

    if (obj && unreal_uobject_is_a(obj, cls) && !unreal_uobject_is_default(obj) && unreal_uobject_is_valid(obj)) {
      return obj;
    }
  }

  return NULL;
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

  cheats->player_manager = cheats_find_live(STR_LIT("HbkPlayerCharacterManager"));
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

/* The dodge cooldown never runs and the air dash is never used up. On the beat, the third dodge of a
 * chain is refused: the chain is kept at its start. */
static void
cheats_infinite_dash(cheats_t *cheats)
{
  if (cheats->dash_cooldown_offset <= 0 || cheats->dash_chain_offset <= 0 || cheats->air_dash_offset <= 0) {
    return;
  }

  uobject_t *player = cheats_player_character(cheats);
  int32_t end = MAX_VAL(MAX_VAL(cheats->dash_cooldown_offset, cheats->dash_chain_offset) + 4, cheats->air_dash_offset + 1);
  if (!player || ((ustruct_t *)player->cls)->props_size < end) {
    return;
  }

  uint8_t *bytes    = (uint8_t *)player;
  float   *cooldown = (float *)(bytes + cheats->dash_cooldown_offset);

  int32_t *chain    = (int32_t *)(bytes + cheats->dash_chain_offset);

  if (*cooldown > 0.0f) {
    *cooldown = 0.0f;
  }

  if (*chain > 0) {
    *chain = 0;
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

  uobject_t *asset = cheats_find_live(STR_LIT("HbkInventoryDataAsset"));
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

static void *
cheats_prop_ptr(void *container, ustruct_t *type, str_t name, fprop_t **out_prop)
{
  fprop_t *prop = container && type ? unreal_ustruct_find_prop(type, name) : NULL;

  if (out_prop) {
    *out_prop = prop;
  }

  return prop ? (uint8_t *)container + prop->offset_internal : NULL;
}

static uobject_t *
cheats_object_prop(uobject_t *obj, str_t name)
{
  uobject_t **value = obj ? cheats_prop_ptr(obj, (ustruct_t *)obj->cls, name, NULL) : NULL;
  return value && *value && unreal_uobject_is_valid(*value) ? *value : NULL;
}

/* The damage component's correction list scales every hit to nothing (one entry, no tags, rate 0). The hits
 * are taken as usual otherwise, so parries work. The player also can not die and is kept at full health. */
static void
cheats_god_mode(cheats_t *cheats, bool enable)
{
  uobject_t *damage = cheats_object_prop(cheats_player_character(cheats), STR_LIT("HbkDamage"));
  if (!damage) {
    return;
  }

  ustruct_t *type         = (ustruct_t *)damage->cls;
  fprop_t   *undead       = NULL;
  fprop_t   *list_prop    = NULL;
  void      *undead_value = cheats_prop_ptr(damage, type, STR_LIT("bUnDeadMode"), &undead);
  float     *health       = cheats_prop_ptr(damage, type, STR_LIT("CurrentHealth"), NULL);
  float     *maximum      = cheats_prop_ptr(damage, type, STR_LIT("MaxHealth"), NULL);

  TARRAY(uint8_t) *list = cheats_prop_ptr(damage, type, STR_LIT("DamageCorrectionList"), &list_prop);

  if (undead_value) {
    unreal_fprop_write_bool(undead, undead_value, enable);
  }

  /* set through the game's own setter, which copies the list */
  cheats_call_t call;
  if (list && unreal_fprop_get_kind(list_prop) == UNREAL_PROP_KIND_ARRAY && (enable ? list->num == 0 : list->num > 0) &&
      cheats_call_begin(&call, damage, STR_LIT("SetDamageCorrectionList"))) {
    fprop_t *inner = ((fprop_array_t *)list_prop)->inner;
    TARRAY(uint8_t) *param = cheats_call_param(&call, STR_LIT("InDamageCorrectionList"));
    uint8_t entry[128] = {0};

    if (param && inner && unreal_fprop_get_kind(inner) == UNREAL_PROP_KIND_STRUCT && inner->elem_size <= (int32_t)sizeof(entry)) {
      ustruct_t *entry_type = (ustruct_t *)((fprop_struct_t *)inner)->script_struct;
      uint8_t   *kind       = cheats_prop_ptr(entry, entry_type, STR_LIT("CorrectionType"), NULL);
      float     *rate       = cheats_prop_ptr(entry, entry_type, STR_LIT("CorrectionValue"), NULL);

      if (kind && rate) {
        *kind = 0; // EHbkDamageCorrectionType::Rate
        *rate = 0.0f;

        param->data = enable ? entry : NULL;
        param->num  = enable ? 1 : 0;
        param->max  = param->num;
        cheats_call(&call);
      }
    }
  }

  /* healed through the game, so the health bar follows */
  if (enable && health && maximum && *health < *maximum && cheats_call_begin(&call, damage, STR_LIT("AddHealth"))) {
    float *value = cheats_call_param(&call, STR_LIT("Value"));

    if (value) {
      *value = *maximum - *health;
      cheats_call(&call);
    }
  }

  cheats->god_mode_on = enable;
}

/* a parameter of an enum or a small integer type, by its own size */
static void
cheats_write_small_int(fprop_t *prop, void *value, int64_t input)
{
  switch (prop->elem_size) {
    case 1: {
      *(uint8_t *)value = (uint8_t)input;
      break;
    }

    case 2: {
      *(uint16_t *)value = (uint16_t)input;
      break;
    }

    default: {
      *(int32_t *)value = (int32_t)input;
      break;
    }
  }
}

static bool
cheats_call_with_context(cheats_call_t *call, str_t name)
{
  uobject_t *player = cheats_player_character(&g_cheats);

  if (!player || !cheats_call_begin(call, cheats_player_manager(&g_cheats), name)) {
    return false;
  }

  uobject_t **context = cheats_call_param(call, STR_LIT("WorldContextObject"));
  if (context) {
    *context = player;
  }

  return true;
}

static bool
cheats_has_ability(int ability)
{
  cheats_call_t call;
  if (!cheats_call_with_context(&call, STR_LIT("HasPlayerAbility"))) {
    return false;
  }

  fprop_t *prop  = unreal_ustruct_find_prop((ustruct_t *)call.func, STR_LIT("AbilityType"));
  void    *value = cheats_call_param(&call, STR_LIT("AbilityType"));
  bool    *has   = cheats_call_param(&call, STR_LIT("ReturnValue"));

  if (!prop || !value || !has) {
    return false;
  }

  cheats_write_small_int(prop, value, ability);
  cheats_call(&call);
  return *has;
}

static void
cheats_learn_ability(int ability)
{
  cheats_call_t call;
  if (!cheats_call_with_context(&call, STR_LIT("LearnPlayerAbility"))) {
    return;
  }

  fprop_t *prop  = unreal_ustruct_find_prop((ustruct_t *)call.func, STR_LIT("AbilityType"));
  void    *value = cheats_call_param(&call, STR_LIT("AbilityType"));

  if (prop && value) {
    cheats_write_small_int(prop, value, ability);
    cheats_call(&call);
    LOG_INFO("cheats: ability %d learned", ability);
  }
}

static int
cheats_joined_partners(void)
{
  uobject_t *manager = cheats_player_manager(&g_cheats);
  fprop_t   *info    = NULL;
  void      *value   = manager ? cheats_prop_ptr(manager, (ustruct_t *)manager->cls, STR_LIT("PlayerStateInfo"), &info) : NULL;

  if (!value || unreal_fprop_get_kind(info) != UNREAL_PROP_KIND_STRUCT) {
    return 0;
  }

  uint8_t *flags = cheats_prop_ptr(value, (ustruct_t *)((fprop_struct_t *)info)->script_struct, STR_LIT("JoinedPartnerFlags"), NULL);
  return flags ? *flags : 0;
}

static void
cheats_join_partner(int flag)
{
  cheats_call_t call;
  if (!cheats_call_with_context(&call, STR_LIT("JoinedNewPartner"))) {
    return;
  }

  fprop_t *prop  = unreal_ustruct_find_prop((ustruct_t *)call.func, STR_LIT("NewPartner"));
  void    *value = cheats_call_param(&call, STR_LIT("NewPartner"));

  if (prop && value) {
    cheats_write_small_int(prop, value, flag);
    cheats_call(&call);
    LOG_INFO("cheats: partner %d joined", flag);
  }
}

/* GetCircuitNum / GetViewerTicketNum, -1 without a player */
static int
cheats_get_count(str_t func)
{
  if (!cheats_player_character(&g_cheats)) {
    return -1;
  }

  cheats_call_t call;
  if (!cheats_call_begin(&call, cheats_player_manager(&g_cheats), func)) {
    return -1;
  }

  cheats_call(&call);

  int32_t *count = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return count ? *count : -1;
}

/* AddCircuitNum (the game's AddViewerTicketNum adds circuits too) */
static void
cheats_add_count(str_t func, int add)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, cheats_player_manager(&g_cheats), func)) {
    return;
  }

  int32_t *value = cheats_call_param(&call, STR_LIT("Add"));
  if (value) {
    *value = add;
    cheats_call(&call);
  }
}

/* tickets are the Bonus Ticket item */
static void
cheats_give_tickets(int count)
{
  cheats_t *cheats = &g_cheats;
  cheats_load_items(cheats);

  for (int i = 0; i < cheats->item_count; ++i) {
    if (str_equal(cheats->items[i].tag, STR_LIT("Inventory.Variable.Ticket"), 0)) {
      cheats_give_item(cheats, &cheats->items[i], count);
      return;
    }
  }
}

static void
cheats_call_simple(str_t func)
{
  cheats_call_t call;
  if (cheats_player_character(&g_cheats) && cheats_call_begin(&call, cheats_player_manager(&g_cheats), func)) {
    cheats_call(&call);
    LOG_INFO("cheats: %.*s", STR_ARG(func));
  }
}

/* The debug jump table lists every checkpoint. A teleport does not use the debug jump, which also puts a
 * preset over the player's data: the game saves as if the player went to the checkpoint's stage, and the
 * save is loaded. */
static void
cheats_load_checkpoints(cheats_t *cheats)
{
  if (cheats->checkpoints) {
    return;
  }

  uobject_t *asset = cheats_find_live(STR_LIT("HbkDebugJumpDataAsset"));
  TARRAY(udata_table_t *) *tables = asset ? cheats_prop_ptr(asset, (ustruct_t *)asset->cls, STR_LIT("DataTables"), NULL) : NULL;

  if (!tables || tables->num <= 0) {
    return;
  }

  int total = 0;
  for (int32_t t = 0; t < tables->num; ++t) {
    tmap_fname_uint8ptr_t *rows = tables->data[t] ? unreal_udata_table_get_row_map(tables->data[t]) : NULL;
    total += rows ? TMAP_NUM(rows) : 0;
  }

  cheats->checkpoints      = ARENA_PUSH_ARRAY_ZERO(cheats->perm, cheats_checkpoint_t, total + 1);
  cheats->stages           = ARENA_PUSH_ARRAY_ZERO(cheats->perm, cheats_stage_t, tables->num + 1);
  cheats->checkpoint_count = 0;
  cheats->stage_count      = 0;

  /* the hideout is not in the table: its level is the game mode's, and the game picks the spawn point */
  uclass_t *mode_cls = unreal_uclass_find(STR_LIT("HbkGameModeBase"), false, true);
  fname_t  *hideout  = mode_cls && mode_cls->cdo ? cheats_prop_ptr(mode_cls->cdo, (ustruct_t *)mode_cls, STR_LIT("LevelName_HideOut"), NULL) : NULL;

  if (hideout && hideout->cmp_idx != 0) {
    cheats->stages[cheats->stage_count++] = (cheats_stage_t){.label = "Hideout", .first = 0, .count = 1};
    cheats->checkpoints[cheats->checkpoint_count++] = (cheats_checkpoint_t){
      .level   = unreal_fname_to_str(*hideout, cheats->perm),
      .label   = "Hideout",
      .hideout = true,
    };
  }

  for (int32_t t = 0; t < tables->num; ++t) {
    udata_table_t         *table = tables->data[t];
    tmap_fname_uint8ptr_t *rows  = table ? unreal_udata_table_get_row_map(table) : NULL;
    ustruct_t             *type  = table ? (ustruct_t *)table->row_struct : NULL;
    str_t                  name  = table ? unreal_fname_to_str(((uobject_t *)table)->name, cheats->perm) : STR_NULL;

    /* the stages only: not the rhythm tower waves or BPM rush */
    if (!rows || !type || !str_has_prefix(name, STR_LIT("Stage"), 0)) {
      continue;
    }

    fprop_t *text_prop  = unreal_ustruct_find_prop(type, STR_LIT("Text"));
    fprop_t *level_prop = unreal_ustruct_find_prop(type, STR_LIT("LevelName"));
    fprop_t *start_prop = unreal_ustruct_find_prop(type, STR_LIT("PlayerStartName"));
    if (!level_prop || !start_prop) {
      continue;
    }

    cheats_stage_t *stage = &cheats->stages[cheats->stage_count++];
    stage->label          = str_push_cstr(cheats->perm, str_has_suffix(name, STR_LIT("_DT"), 0) ? str_make(name.data, name.len - 3) : name);
    stage->first          = cheats->checkpoint_count;

    TMAP_FOR_EACH_INDEX(rows, idx) {
      uint8_t *row = TMAP_VALUE_AT(rows, idx);
      if (!row || cheats->checkpoint_count >= total) {
        continue;
      }

      str_t level = unreal_fname_to_str(*(fname_t *)(row + level_prop->offset_internal), cheats->perm);
      for (uint64_t i = level.len; i > 0; --i) {
        if (level.data[i - 1] == '/') {
          level = str_make(level.data + i, level.len - i);
          break;
        }
      }

      cheats_checkpoint_t *checkpoint = &cheats->checkpoints[cheats->checkpoint_count++];
      checkpoint->level               = level;
      checkpoint->spawn               = *(fname_t *)(row + start_prop->offset_internal);

      str_t label = text_prop ? cheats_text((ftext_t *)(row + text_prop->offset_internal), cheats->perm) : STR_NULL;
      if (str_is_empty(label)) {
        label = unreal_fname_to_str(checkpoint->spawn, cheats->perm);
      }

      checkpoint->label = str_push_cstr(cheats->perm, label);

      stage->count += 1;
    }

  }
}

/* the game mode of the world the player is in */
static uobject_t *
cheats_game_mode(cheats_t *cheats)
{
  uobject_t    *player = cheats_player_character(cheats);
  uclass_t     *cls    = unreal_uclass_find(STR_LIT("HbkGameModeBase"), false, true);
  cheats_call_t call;

  if (!player || !cls || !cls->cdo || !cheats_call_begin(&call, cls->cdo, STR_LIT("GetHbkGameMode"))) {
    return NULL;
  }

  uobject_t **context = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  if (context) {
    *context = player;
  }

  cheats_call(&call);

  uobject_t **mode = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return mode && *mode && unreal_uobject_is_valid(*mode) ? *mode : NULL;
}

/* The save is loaded by the main game mode only: the hideout runs another one, and leaving it for a stage
 * any other way (a plain level load) puts a stage preset over the player's data, which the game then saves. */
static bool
cheats_in_hideout(cheats_t *cheats)
{
  uobject_t    *player = cheats_player_character(cheats);
  uclass_t     *cls    = unreal_uclass_find(STR_LIT("HbkGameModeBase"), false, true);
  cheats_call_t call;

  if (!player || !cls || !cls->cdo || !cheats_call_begin(&call, cls->cdo, STR_LIT("IsHideOutLevel"))) {
    return false;
  }

  uobject_t **context = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  if (context) {
    *context = player;
  }

  cheats_call(&call);

  bool *hideout = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return hideout && *hideout;
}

static bool
cheats_save_busy(void)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, cheats_find_live(STR_LIT("HbkSaveGameManager")), STR_LIT("IsWorkingJob"))) {
    return false;
  }

  cheats_call(&call);

  bool *busy = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return busy && *busy;
}

/* LoadGame reads the save from disk again, so the spawn point goes in through the save the game makes when
 * the player moves to another stage. It moves the global save only: the hideout's "Continue stage" still
 * leads to the last stage checkpoint, and the game saves that one once the player is there. */
static void
cheats_teleport(cheats_t *cheats, cheats_checkpoint_t *checkpoint)
{
  if (cheats_in_hideout(cheats)) {
    LOG_ERROR("cheats: teleport does not work in the hideout: leave it for a stage first");
    return;
  }

  /* only in game: loaded from a menu, the save fails to load */
  uobject_t    *player = cheats_player_character(cheats);
  uclass_t     *cls    = unreal_uclass_find(STR_LIT("HbkSaveGameBlueprintAsyncSaveGame"), false, true);
  cheats_call_t call;

  if (!player || !cheats_game_mode(cheats) || cheats->teleport_saving || cheats_save_busy()) {
    LOG_ERROR("cheats: teleport needs a loaded save, with no save in progress");
    return;
  }

  if (!cls || !cls->cdo || !cheats_call_begin(&call, cls->cdo, STR_LIT("RequestAutoSaveGameOnTransferStageDatas"))) {
    LOG_ERROR("cheats: the game's stage save was not found");
    return;
  }

  uobject_t **context = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  fstring_t  *level   = cheats_call_param(&call, STR_LIT("LevelName"));
  fstring_t  *spawn   = cheats_call_param(&call, STR_LIT("SpawnSpotName"));
  uobject_t **action  = cheats_call_param(&call, STR_LIT("ReturnValue"));

  if (!context || !level || !spawn || !action) {
    LOG_ERROR("cheats: the game's stage save was not found");
    return;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t spawn_name = checkpoint->spawn.cmp_idx ? unreal_fname_to_str(checkpoint->spawn, tmp.arena) : STR_NULL;

    /* an empty spawn point is the stage's default start */
    *context = player;
    *level   = unreal_fstring_from_str(checkpoint->level, tmp.arena);
    if (!str_is_empty(spawn_name)) {
      *spawn = unreal_fstring_from_str(spawn_name, tmp.arena);
    }

    LOG_INFO("cheats: teleporting to %s (%.*s, %.*s)", checkpoint->label, STR_ARG(checkpoint->level), STR_ARG(spawn_name));

    cheats_call(&call);
  }
  scratch_end(tmp);

  if (!*action || !cheats_call_begin(&call, *action, STR_LIT("Activate"))) {
    LOG_ERROR("cheats: the game did not start its stage save");
    return;
  }

  cheats_call(&call);

  cheats->teleport_saving   = true;
  cheats->teleport_saw_save = false;
  cheats->teleport_frames   = 0;
}

/* the save runs a few jobs one after another: the save is loaded once none has run for a while */
static void
cheats_teleport_load(cheats_t *cheats)
{
  cheats->teleport_frames += 1;

  if (cheats_save_busy()) {
    cheats->teleport_saw_save = true;
    cheats->teleport_frames   = 0;
    return;
  }

  if (!cheats->teleport_saw_save) {
    if (cheats->teleport_frames > 300) {
      LOG_ERROR("cheats: the game did not save, teleport canceled");
      cheats->teleport_saving = false;
    }
    return;
  }

  if (cheats->teleport_frames < 10) {
    return;
  }

  cheats->teleport_saving = false;

  uobject_t    *game_mode = cheats_game_mode(cheats);
  cheats_call_t call;

  if (!game_mode || !cheats_call_begin(&call, game_mode, STR_LIT("LoadGame"))) {
    LOG_ERROR("cheats: teleport needs a loaded save");
    return;
  }

  uobject_t **context = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  if (context) {
    *context = game_mode;
  }

  cheats_call(&call);
}

static void
cheats_draw_teleport(cheats_t *cheats, struct nk_context *ctx)
{
  cheats_load_checkpoints(cheats);

  if (!cheats->stage_count) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "The game has not loaded its checkpoint list yet", NK_TEXT_LEFT);
    return;
  }

  if (!cheats_player_character(cheats)) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Load a save to teleport", NK_TEXT_LEFT);
    return;
  }

  if (cheats_in_hideout(cheats)) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Teleport does not work in the hideout: leave it for a stage first", NK_TEXT_LEFT);
    return;
  }

  cheats->stage_index = CLAMP(cheats->stage_index, 0, cheats->stage_count - 1);
  cheats_stage_t *stage = &cheats->stages[cheats->stage_index];

  const char *stage_labels[64];
  int         stage_count = MIN_VAL(cheats->stage_count, (int)COUNTOF(stage_labels));
  for (int i = 0; i < stage_count; ++i) {
    stage_labels[i] = cheats->stages[i].label;
  }

  nk_layout_row_dynamic(ctx, 26.0f, 3);

  /* popups as tall as their entries: a tall one opens upwards from the bottom of the window, far away */
  float stage_h = MIN_VAL(300.0f, (float)stage_count * 28.0f + 16.0f);
  int   picked  = nk_combo(ctx, stage_labels, stage_count, cheats->stage_index, 24, nk_vec2(220.0f, stage_h));
  if (picked != cheats->stage_index) {
    cheats->stage_index      = picked;
    cheats->checkpoint_index = 0;
    stage                    = &cheats->stages[picked];
  }

  cheats->checkpoint_index = CLAMP(cheats->checkpoint_index, 0, MAX_VAL(stage->count - 1, 0));

  if (nk_combo_begin_label(ctx, stage->count ? cheats->checkpoints[stage->first + cheats->checkpoint_index].label : "",
                           nk_vec2(320.0f, MIN_VAL(360.0f, (float)stage->count * 28.0f + 16.0f)))) {
    nk_layout_row_dynamic(ctx, 24.0f, 1);

    for (int i = 0; i < stage->count; ++i) {
      if (nk_combo_item_label(ctx, cheats->checkpoints[stage->first + i].label, NK_TEXT_LEFT)) {
        cheats->checkpoint_index = i;
      }
    }

    nk_combo_end(ctx);
  }

  if (nk_button_label(ctx, "Teleport") && stage->count) {
    cheats->teleport_request = stage->first + cheats->checkpoint_index + 1;
  }

  nk_layout_row_dynamic(ctx, 20.0f, 1);
  nk_label_colored(ctx, "Loads the save at the checkpoint: progress since the last save is lost", NK_TEXT_LEFT, UI_C_TEXT_DIM);
}

static const char *g_ability_labels[] = {
  "", "Beat hit", "Special attack", "Dodge", "Air dodge", "Rhythm dodge", "Magnet", "Magnet backstab", "Parry", "Air parry",
  "Directional parry", "Partner call",
};

static const struct {
  int         flag;
  const char *label;
} g_partners[] = {
  {1,  "808"       },
  {2,  "Peppermint"},
  {4,  "Macaron"   },
  {8,  "Korsica"   },
  {16, "CNMN"      },
};

static void
cheats_draw_progress(cheats_t *cheats, struct nk_context *ctx)
{
  int circuits = cheats_get_count(STR_LIT("GetCircuitNum"));

  if (circuits < 0) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Load a save to change the progress", NK_TEXT_LEFT);
    return;
  }

  nk_layout_row_dynamic(ctx, 26.0f, 1);
  if (nk_button_label(ctx, "Unlock all skills")) {
    cheats_call_simple(STR_LIT("DebugOpenAllSkills"));
  }

  int tickets = cheats_get_count(STR_LIT("GetViewerTicketNum"));

  nk_layout_row_dynamic(ctx, 26.0f, 3);
  nk_labelf(ctx, NK_TEXT_LEFT, "Circuits: %d", circuits);
  if (nk_button_label(ctx, "+1")) {
    cheats_add_count(STR_LIT("AddCircuitNum"), 1);
  }

  if (nk_button_label(ctx, "+10")) {
    cheats_add_count(STR_LIT("AddCircuitNum"), 10);
  }

  nk_labelf(ctx, NK_TEXT_LEFT, "Tickets: %d", tickets);
  if (nk_button_label(ctx, "+1")) {
    cheats_give_tickets(1);
  }

  if (nk_button_label(ctx, "+10")) {
    cheats_give_tickets(10);
  }

  nk_layout_row_dynamic(ctx, 20.0f, 1);
  nk_label(ctx, "Abilities", NK_TEXT_LEFT);

  nk_layout_row_dynamic(ctx, 24.0f, 3);
  /* the ones the player has are shown dim, the others can be learned */
  for (int i = 1; i < COUNTOF(g_ability_labels); ++i) {
    if (cheats_has_ability(i)) {
      nk_label_colored(ctx, g_ability_labels[i], NK_TEXT_CENTERED, UI_C_TEXT_DIM);
    } else if (nk_button_label(ctx, g_ability_labels[i])) {
      cheats_learn_ability(i);
    }
  }

  int joined = cheats_joined_partners();

  nk_layout_row_dynamic(ctx, 20.0f, 1);
  nk_label(ctx, "Partners", NK_TEXT_LEFT);

  nk_layout_row_dynamic(ctx, 24.0f, 5);
  for (int i = 0; i < COUNTOF(g_partners); ++i) {
    if (joined & g_partners[i].flag) {
      nk_label_colored(ctx, g_partners[i].label, NK_TEXT_CENTERED, UI_C_TEXT_DIM);
    } else if (nk_button_label(ctx, g_partners[i].label)) {
      cheats_join_partner(g_partners[i].flag);
    }
  }
}

static void MOD_CALL
cheats_cmd_teleport(mod_handle_t h, str_t name, str_t args, void *user)
{
  (void)h;
  (void)name;
  (void)user;

  cheats_t *cheats = &g_cheats;
  cheats_load_checkpoints(cheats);

  str_t wanted = str_trim_space(args);

  for (int i = 0; i < cheats->checkpoint_count && !str_is_empty(wanted); ++i) {
    cheats_checkpoint_t *checkpoint = &cheats->checkpoints[i];

    if (str_equal(str_from_cstr(checkpoint->label), wanted, STR_CMP_FLAG_IGNORE_CASE) ||
        (checkpoint->spawn.cmp_idx && unreal_fname_match_text(checkpoint->spawn, wanted, true, true))) {
      cheats->teleport_request = i + 1;
      return;
    }
  }

  LOG_ERROR("Usage: cheats.teleport <checkpoint, e.g. ST02_CP0010>");
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
  static enum nk_collapse_states items_open    = NK_MINIMIZED;
  static enum nk_collapse_states progress_open = NK_MINIMIZED;
  static enum nk_collapse_states teleport_open = NK_MINIMIZED;

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

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Progress", &progress_open)) {
    cheats_draw_progress(cheats, ctx);
    nk_tree_pop(ctx);
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Teleport", &teleport_open)) {
    cheats_draw_teleport(cheats, ctx);
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

  bool god_mode = mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.god_mode_h);
  if (god_mode || cheats->god_mode_on) {
    cheats_god_mode(cheats, god_mode);
  }

  if (cheats->teleport_request > 0) {
    int checkpoint           = cheats->teleport_request - 1;
    cheats->teleport_request = 0;

    if (checkpoint < cheats->checkpoint_count) {
      cheats_teleport(cheats, &cheats->checkpoints[checkpoint]);
    }
  }

  if (cheats->teleport_saving) {
    cheats_teleport_load(cheats);
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
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_GOD_MODE),
      .label       = STR_CLIT("God mode"),
      .description = STR_CLIT("Hits do no damage and the player can not die; hits still land, so parries work"),
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
