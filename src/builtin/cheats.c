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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CFG_DISABLE_TUTORIALS     "disable-tutorials"
#define CFG_RESTORE_JUKEBOX       "restore-jukebox-state"
#define CFG_DEBUG_WIDGET_KEYBIND  "debug-widget-keybind"
#define CFG_ENABLE_INFINITE_JUMPS "enable-infinite-jumps"
#define CFG_INFINITE_DASH         "infinite-dash"
#define CFG_GOD_MODE              "god-mode"
#define CFG_INFINITE_HEALTH       "infinite-health"
#define CFG_INFINITE_REVERB       "infinite-reverb-gauge"
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
  mod_cfg_handle_t infinite_health_h;
  mod_cfg_handle_t infinite_reverb_h;
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
  int32_t         store; // EHbkInventoryCategory of the item's class
};

/* the Items sections in the order shown; the costumes are grouped by character under one node */
static const struct {
  const char *prefix;
  const char *label;
  bool        costume;
} g_item_categories[] = {
  {"Inventory.Variable.",      "Variable",      false},
  {"Inventory.Chip.",          "Chip",          false},
  {"Inventory.SpecialAttack.", "SpecialAttack", false},
  {"Inventory.Ability.",       "Ability",       false},
  {"Inventory.Attack.",        "Attack",        false},
  {"Inventory.Combo.",         "Combo",         false},
  {"Inventory.Partner.",       "Partner",       false},
  {"Inventory.Costume.808.",   "808",           true },
  {"Inventory.Costume.CHA.",   "Chai",          true },
  {"Inventory.Costume.CNM.",   "CNMN",          true },
  {"Inventory.Costume.KOR.",   "Korsica",       true },
  {"Inventory.Costume.MAC.",   "Macaron",       true },
  {"Inventory.Costume.PEP.",   "Peppermint",    true },
  {"Inventory.File.",          "File",          false},
};

/* EHbkInventoryCategory::LatestVariableItem: used up when taken, like life gauges and reverb cores */
#define CHEATS_STORE_USED_UP 3

/* where an item the player has is in the inventory lists */
typedef struct cheats_owned_s cheats_owned_t;
struct cheats_owned_s {
  tarray_uint8_t *list;
  int32_t         index;
};

/* how the inventory lists are laid out, read with the lists */
typedef struct cheats_inventory_s cheats_inventory_t;
struct cheats_inventory_s {
  fprop_t *item_prop; // FHbkInventoryItem
  int32_t  id_offset;
  int32_t  count_offset;
};

/* the game flag manager's own setter: it adds the category and the flag when they are missing */
typedef void(__fastcall *cheats_set_flag_fn_t)(uobject_t *manager, uint8_t container, fname_t category, fname_t name, int32_t value, bool unknown);

/* the flag manager's containers: what is saved with the game, and with its stage progress */
#define CHEATS_FLAGS_GAME     0
#define CHEATS_FLAGS_PROGRESS 1

/* EHbkProductStateType::Purchased */
#define CHEATS_PRODUCT_PURCHASED 4

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
  uobject_t *gauge;
  double     gauge_search_time;
  uobject_t *health_bar;
  double     health_bar_search_time;
  uobject_t *hud_player; // the player the gauge and the health bar were found for

  cheats_item_t     *items;
  int                item_count;
  int32_t           *item_slots; // the item index + 1 by tag hash, a power of two long
  int32_t            item_slot_count;
  int32_t           *owned;      // how many of each item the player has
  cheats_owned_t    *owned_at;
  bool              *not_kept;   // given, and not kept by the game (the costume sets)
  cheats_inventory_t inventory;

  enum nk_collapse_states item_open[COUNTOF(g_item_categories)];
  enum nk_collapse_states costume_open;
  enum nk_collapse_states abilities_open;
  enum nk_collapse_states partners_open;

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
  bool                 infinite_health_on;
  cheats_set_flag_fn_t set_flag;

  /* the player whose reactions that break super armor are put away, and how many there were */
  uobject_t *god_player;
  int32_t    god_reaction_tags;
  int32_t    god_reaction_parents;
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
  cheats->cfg.infinite_health_h       = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_INFINITE_HEALTH));
  cheats->cfg.infinite_reverb_h       = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_INFINITE_REVERB));

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

  void           *set_flag     = NULL;
  sigscan_entry_t set_flag_sig = SIG_ENTRY_DIRECT(set_flag, "4C 8B DC 4D 89 43 18 53 41 57 48 83 EC 68 0F B6 C2 49 8B D9 45 0F B7 D0 4C 8B F9 49 89 6B 08", 0x020495B0);

  if (mod_sigscan(&set_flag_sig) == SIG_ERR_OK && set_flag_sig.addr) {
    cheats->set_flag = (cheats_set_flag_fn_t)set_flag_sig.addr;
  } else {
    LOG_WARN("cheats: the game flag setter was not found, some unlocks are off");
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

    uobject_t *cdo   = *value ? (*value)->cdo : NULL;
    fprop_t   *name  = cdo ? unreal_ustruct_find_prop((ustruct_t *)cdo->cls, STR_LIT("InventoryObjectNameText")) : NULL;
    fprop_t   *store = cdo ? unreal_ustruct_find_prop((ustruct_t *)cdo->cls, STR_LIT("InventoryCategory")) : NULL;

    item.store = store ? *((uint8_t *)cdo + store->offset_internal) : 0;

    item.name = name ? cheats_text((ftext_t *)((uint8_t *)cdo + name->offset_internal), cheats->perm) : STR_NULL;
    if (str_is_empty(item.name)) {
      item.name = item.tag;
    }

    cheats->items[cheats->item_count++] = item;
  }

  qsort(cheats->items, (size_t)cheats->item_count, sizeof(cheats->items[0]), cheats_item_compare);

  cheats->item_slot_count = 1;
  while (cheats->item_slot_count < cheats->item_count * 2) {
    cheats->item_slot_count *= 2;
  }

  cheats->item_slots = ARENA_PUSH_ARRAY_ZERO(cheats->perm, int32_t, cheats->item_slot_count);
  cheats->owned      = ARENA_PUSH_ARRAY_ZERO(cheats->perm, int32_t, cheats->item_count);
  cheats->owned_at   = ARENA_PUSH_ARRAY_ZERO(cheats->perm, cheats_owned_t, cheats->item_count);
  cheats->not_kept   = ARENA_PUSH_ARRAY_ZERO(cheats->perm, bool, cheats->item_count);

  uint32_t mask = (uint32_t)cheats->item_slot_count - 1;
  for (int i = 0; i < cheats->item_count; ++i) {
    uint32_t at = unreal_fname_hash(cheats->items[i].id.tag_name) & mask;
    while (cheats->item_slots[at]) {
      at = (at + 1) & mask;
    }

    cheats->item_slots[at] = i + 1;
  }

  LOG_INFO("cheats: %d items", cheats->item_count);
}

/* -1 when the game has no such item */
static int
cheats_item_find(cheats_t *cheats, fname_t tag)
{
  if (!cheats->item_slots) {
    return -1;
  }

  uint32_t mask = (uint32_t)cheats->item_slot_count - 1;
  for (uint32_t at = unreal_fname_hash(tag) & mask;; at = (at + 1) & mask) {
    int32_t slot = cheats->item_slots[at];

    if (!slot) {
      return -1;
    }

    if (unreal_fname_equal(cheats->items[slot - 1].id.tag_name, tag, false)) {
      return slot - 1;
    }
  }
}

static int
cheats_item_find_tag(cheats_t *cheats, str_t tag)
{
  cheats_load_items(cheats);
  return cheats_item_find(cheats, unreal_fname_from_str(tag, FNAME_FIND));
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

static bool
cheats_call_bool(uobject_t *self, str_t func)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, self, func)) {
    return false;
  }

  cheats_call(&call);

  bool *value = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return value && *value;
}

/* Super armor keeps the player from reacting to a hit. The reactions that break it (being blown off) are
 * emptied from the player's list while god mode is on, and put back after. */
static void
cheats_god_reactions(cheats_t *cheats, uobject_t *player, bool enable)
{
  cheats_call_t call;
  if (cheats_call_bool(player, STR_LIT("IsSuperArmor")) != enable && cheats_call_begin(&call, player, STR_LIT("SetSuperArmor"))) {
    bool *armor = cheats_call_param(&call, STR_LIT("SuperArmor"));

    if (armor) {
      *armor = enable;
      cheats_call(&call);
    }
  }

  bool                       same = cheats->god_player == player;
  fgameplay_tag_container_t *tags = cheats_prop_ptr(player, (ustruct_t *)player->cls, STR_LIT("OverReactionTags"), NULL);

  if (!tags) {
    return;
  }

  if (enable && (!same || tags->gameplay_tags.num > 0 || tags->parent_tags.num > 0)) {
    cheats->god_player           = player;
    cheats->god_reaction_tags    = tags->gameplay_tags.num;
    cheats->god_reaction_parents = tags->parent_tags.num;
    tags->gameplay_tags.num      = 0;
    tags->parent_tags.num        = 0;
  } else if (!enable && same) {
    /* the arrays still hold them */
    tags->gameplay_tags.num = MIN_VAL(cheats->god_reaction_tags, tags->gameplay_tags.max);
    tags->parent_tags.num   = MIN_VAL(cheats->god_reaction_parents, tags->parent_tags.max);
    cheats->god_player      = NULL;
  }
}

/* The damage component's correction list scales every hit to nothing (one entry, no tags, rate 0). The hits
 * are taken as usual otherwise, so parries work, and with super armor the player does not react to them. The
 * player also can not die and is kept at full health. */
static void
cheats_god_mode(cheats_t *cheats, bool enable)
{
  uobject_t *player = cheats_player_character(cheats);
  uobject_t *damage = cheats_object_prop(player, STR_LIT("HbkDamage"));
  if (!damage) {
    return;
  }

  /* a player from a level since left has nothing to put back */
  if (cheats->god_player && !unreal_uobject_is_valid(cheats->god_player)) {
    cheats->god_player = NULL;
  }

  cheats_god_reactions(cheats, player, enable);

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

/* A new level has a new player: the gauge and the health bar of the last one stay valid for a while */
static void
cheats_check_hud_player(cheats_t *cheats)
{
  uobject_t *player = cheats_player_character(cheats);

  if (player != cheats->hud_player) {
    cheats->hud_player             = player;
    cheats->gauge                  = NULL;
    cheats->health_bar             = NULL;
    cheats->gauge_search_time      = 0.0;
    cheats->health_bar_search_time = 0.0;
  }
}

/* The HUD's health bar, which also draws the reverb gauge. Only the one on screen has a player: the others
 * (templates, widgets not shown) can not be updated. It is looked for at most once a second. */
static uobject_t *
cheats_health_bar(cheats_t *cheats)
{
  cheats_check_hud_player(cheats);

  if (cheats->health_bar && unreal_uobject_is_valid(cheats->health_bar)) {
    return cheats->health_bar;
  }

  cheats->health_bar = NULL;

  double now = (double)time_now_us() / 1e6;
  if (now - cheats->health_bar_search_time < 1.0) {
    return NULL;
  }

  cheats->health_bar_search_time = now;

  uclass_t *cls = unreal_uclass_find(STR_LIT("HbkHealthBarWidget"), false, true);
  for (int i = 0, count = cls ? unreal_uobject_array_count() : 0; i < count; ++i) {
    uobject_t    *obj = unreal_uobject_array_get_obj(i);
    cheats_call_t call;

    if (!obj || !unreal_uobject_is_a(obj, cls) || unreal_uobject_is_default(obj) || !unreal_uobject_is_valid(obj) ||
        !cheats_call_begin(&call, obj, STR_LIT("GetOwningPlayer"))) {
      continue;
    }

    cheats_call(&call);

    uobject_t **player = cheats_call_param(&call, STR_LIT("ReturnValue"));
    if (player && *player) {
      cheats->health_bar = obj;
      break;
    }
  }

  return cheats->health_bar;
}

/* the bar draws its own count of reverb bars, and fills them up to it once started */
static void
cheats_redraw_reverb(cheats_t *cheats, int bars)
{
  uobject_t    *bar   = cheats_health_bar(cheats);
  int32_t      *count = bar ? cheats_prop_ptr(bar, (ustruct_t *)bar->cls, STR_LIT("DeathblowGaugeNum"), NULL) : NULL;
  cheats_call_t call;

  if (count && cheats_call_begin(&call, bar, STR_LIT("StartDeathblowGauge"))) {
    *count = bars;
    cheats_call(&call);
  }
}

/* The player's health is the player controller's: the character's own damage component does not change */
static uobject_t *
cheats_player_damage(cheats_t *cheats)
{
  uobject_t    *player = cheats_player_character(cheats);
  cheats_call_t call;

  if (player && cheats_call_begin(&call, player, STR_LIT("GetController"))) {
    cheats_call(&call);

    uobject_t **controller = cheats_call_param(&call, STR_LIT("ReturnValue"));
    uobject_t  *damage     = controller && *controller ? cheats_object_prop(*controller, STR_LIT("HbkDamage")) : NULL;

    if (damage) {
      return damage;
    }
  }

  return cheats_object_prop(player, STR_LIT("HbkDamage"));
}

/* Hits land and hurt as usual, but the player can not die and the health is filled up again at once */
static void
cheats_infinite_health(cheats_t *cheats, bool enable)
{
  uobject_t *damage = cheats_player_damage(cheats);
  if (!damage) {
    return;
  }

  ustruct_t *type         = (ustruct_t *)damage->cls;
  fprop_t   *undead       = NULL;
  void      *undead_value = cheats_prop_ptr(damage, type, STR_LIT("bUnDeadMode"), &undead);
  float     *health       = cheats_prop_ptr(damage, type, STR_LIT("CurrentHealth"), NULL);
  float     *maximum      = cheats_prop_ptr(damage, type, STR_LIT("MaxHealth"), NULL);

  /* god mode keeps the player alive on its own */
  if (undead_value && (enable || !mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.god_mode_h))) {
    unreal_fprop_write_bool(undead, undead_value, enable);
  }

  cheats_call_t call;
  if (enable && health && maximum && *health < *maximum && cheats_call_begin(&call, damage, STR_LIT("AddHealth"))) {
    float *value = cheats_call_param(&call, STR_LIT("Value"));

    if (value) {
      *value = *maximum - *health;
      cheats_call(&call);
    }
  }

  cheats->infinite_health_on = enable;
}

/* the reverb gauge is on the player controller; it is looked for at most once a second */
static uobject_t *
cheats_reverb_gauge(cheats_t *cheats)
{
  cheats_check_hud_player(cheats);

  if (cheats->gauge && unreal_uobject_is_valid(cheats->gauge)) {
    return cheats->gauge;
  }

  cheats->gauge = NULL;

  double now = (double)time_now_us() / 1e6;
  if (now - cheats->gauge_search_time < 1.0 || !cheats_player_character(cheats)) {
    return NULL;
  }

  cheats->gauge_search_time = now;
  cheats->gauge             = cheats_find_live(STR_LIT("HbkPlayerGaugeComponent"));
  return cheats->gauge;
}

static int
cheats_call_int(uobject_t *self, str_t func)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, self, func)) {
    return 0;
  }

  cheats_call(&call);

  int32_t *value = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return value ? *value : 0;
}

static float
cheats_call_float(uobject_t *self, str_t func)
{
  cheats_call_t call;
  if (!cheats_call_begin(&call, self, func)) {
    return 0.0f;
  }

  cheats_call(&call);

  float *value = cheats_call_param(&call, STR_LIT("ReturnValue"));
  return value ? *value : 0.0f;
}

static void
cheats_infinite_reverb(cheats_t *cheats)
{
  uobject_t *gauge = cheats_reverb_gauge(cheats);
  if (!gauge) {
    return;
  }

  /* the capacity is one bar of the gauge */
  float         full  = cheats_call_float(gauge, STR_LIT("GetGaugeCapacity")) * (float)cheats_call_int(gauge, STR_LIT("GetGaugeMax"));
  float         value = cheats_call_float(gauge, STR_LIT("GetAccumulatedGaugeValue"));
  cheats_call_t call;

  if (value < full && cheats_call_begin(&call, gauge, STR_LIT("AccumulateGaugeValue"))) {
    float *add = cheats_call_param(&call, STR_LIT("AddValue"));

    if (add) {
      *add = full - value;
      cheats_call(&call);
      cheats_redraw_reverb(cheats, cheats_call_int(gauge, STR_LIT("GetAccumulatedGaugeCount")));
    }
  }
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

static void *
cheats_state_prop(str_t name)
{
  uobject_t *manager = cheats_player_manager(&g_cheats);
  fprop_t   *info    = NULL;
  void      *value   = manager ? cheats_prop_ptr(manager, (ustruct_t *)manager->cls, STR_LIT("PlayerStateInfo"), &info) : NULL;

  if (!value || unreal_fprop_get_kind(info) != UNREAL_PROP_KIND_STRUCT) {
    return NULL;
  }

  return cheats_prop_ptr(value, (ustruct_t *)((fprop_struct_t *)info)->script_struct, name, NULL);
}

/* the player state's inventory: a map from the inventory category to a list of items and their counts */
static fprop_map_t *
cheats_inventory_map(void **out_map)
{
  fprop_t    *inventory_prop = NULL;
  uobject_t  *manager        = cheats_player_character(&g_cheats) ? cheats_player_manager(&g_cheats) : NULL;
  fprop_t    *state_prop     = NULL;
  void       *state          = manager ? cheats_prop_ptr(manager, (ustruct_t *)manager->cls, STR_LIT("PlayerStateInfo"), &state_prop) : NULL;

  if (!state || unreal_fprop_get_kind(state_prop) != UNREAL_PROP_KIND_STRUCT) {
    return NULL;
  }

  void *inventory = cheats_prop_ptr(state, (ustruct_t *)((fprop_struct_t *)state_prop)->script_struct, STR_LIT("PlayerInventory"), &inventory_prop);
  if (!inventory || unreal_fprop_get_kind(inventory_prop) != UNREAL_PROP_KIND_STRUCT) {
    return NULL;
  }

  uobject_t **data = cheats_prop_ptr(inventory, (ustruct_t *)((fprop_struct_t *)inventory_prop)->script_struct, STR_LIT("InventoryCategoryData"), NULL);
  if (!data || !*data || !unreal_uobject_is_valid(*data)) {
    return NULL;
  }

  fprop_t *map_prop = NULL;
  void    *map      = cheats_prop_ptr(*data, (ustruct_t *)(*data)->cls, STR_LIT("Map"), &map_prop);

  if (!map || unreal_fprop_get_kind(map_prop) != UNREAL_PROP_KIND_MAP) {
    return NULL;
  }

  *out_map = map;
  return (fprop_map_t *)map_prop;
}

/* Counts what the player has of each item. An item can be in its category's list and in the one of what was
 * taken since the last save: the counts add up, and the first list it is in is the one changed. */
static bool
cheats_scan_inventory(cheats_t *cheats)
{
  cheats_load_items(cheats);

  if (!cheats->items) {
    return false;
  }

  mem_zero(cheats->owned, sizeof(cheats->owned[0]) * (size_t)cheats->item_count);
  mem_zero(cheats->owned_at, sizeof(cheats->owned_at[0]) * (size_t)cheats->item_count);

  void        *map  = NULL;
  fprop_map_t *prop = cheats_inventory_map(&map);

  if (!prop || !prop->val_prop || unreal_fprop_get_kind(prop->val_prop) != UNREAL_PROP_KIND_STRUCT) {
    return false;
  }

  fprop_t *list_prop = unreal_ustruct_find_prop((ustruct_t *)((fprop_struct_t *)prop->val_prop)->script_struct, STR_LIT("List"));
  fprop_t *item_prop = list_prop && unreal_fprop_get_kind(list_prop) == UNREAL_PROP_KIND_ARRAY ? ((fprop_array_t *)list_prop)->inner : NULL;

  if (!item_prop || unreal_fprop_get_kind(item_prop) != UNREAL_PROP_KIND_STRUCT) {
    return false;
  }

  ustruct_t *item_type = (ustruct_t *)((fprop_struct_t *)item_prop)->script_struct;
  fprop_t   *id_prop   = unreal_ustruct_find_prop(item_type, STR_LIT("InventoryId"));
  fprop_t   *count     = unreal_ustruct_find_prop(item_type, STR_LIT("Count"));

  if (!id_prop || !count) {
    return false;
  }

  cheats->inventory = (cheats_inventory_t){
    .item_prop    = item_prop,
    .id_offset    = id_prop->offset_internal,
    .count_offset = count->offset_internal,
  };

  for (int32_t i = 0, max = unreal_map_max_index(map, prop); i < max; ++i) {
    uint8_t *value = unreal_map_is_valid_index(map, prop, i) ? unreal_map_get_value(map, prop, i) : NULL;
    if (!value) {
      continue;
    }

    tarray_uint8_t *list = (tarray_uint8_t *)(value + list_prop->offset_internal);

    for (int32_t j = 0; j < list->num; ++j) {
      uint8_t *entry = list->data + (size_t)j * (size_t)item_prop->elem_size;
      int      item  = cheats_item_find(cheats, ((fgameplay_tag_t *)(entry + cheats->inventory.id_offset))->tag_name);

      if (item < 0) {
        continue;
      }

      cheats->owned[item] += *(int32_t *)(entry + cheats->inventory.count_offset);

      if (!cheats->owned_at[item].list) {
        cheats->owned_at[item] = (cheats_owned_t){.list = list, .index = j};
      }
    }
  }

  return true;
}

/* More are taken through the game; fewer are written to the list, and none takes the item away */
static void
cheats_set_item_count(cheats_t *cheats, int item, int count)
{
  cheats_owned_t *at   = &cheats->owned_at[item];
  int             have = cheats->owned[item];

  if (count > have) {
    cheats_give_item(cheats, &cheats->items[item], count - have);
    cheats_scan_inventory(cheats);

    cheats->not_kept[item] = cheats->owned[item] <= have;
    return;
  } else if (count < have && at->list && at->index < at->list->num) {
    int32_t  size  = cheats->inventory.item_prop->elem_size;
    uint8_t *entry = at->list->data + (size_t)at->index * (size_t)size;
    int32_t *first = (int32_t *)(entry + cheats->inventory.count_offset);
    int32_t  left  = count - (have - *first);

    if (left > 0) {
      *first = left;
    } else {
      unreal_fprop_destroy_value(cheats->inventory.item_prop, entry);
      memmove(entry, entry + size, (size_t)(at->list->num - at->index - 1) * (size_t)size);
      at->list->num -= 1;
    }

    LOG_INFO("cheats: %.*s set to %d", STR_ARG(cheats->items[item].tag), count);
  }

  cheats_scan_inventory(cheats);
}

static bool
cheats_count_row(struct nk_context *ctx, const char *label, int *value, int max)
{
  nk_layout_row_template_begin(ctx, 26.0f);
  nk_layout_row_template_push_dynamic(ctx);
  nk_layout_row_template_push_static(ctx, 180.0f);
  nk_layout_row_template_end(ctx);

  nk_label(ctx, label, NK_TEXT_LEFT);

  int before = *value;
  nk_property_int(ctx, "#", 0, value, max, 1, 1.0f);
  return *value != before;
}

static void
cheats_draw_general(cheats_t *cheats, struct nk_context *ctx)
{
  int money = cheats_get_money(cheats);

  if (money < 0 || !cheats_scan_inventory(cheats)) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Load a save to change these", NK_TEXT_LEFT);
    return;
  }

  if (cheats_count_row(ctx, "Gears", &money, 99999999)) {
    cheats_set_money(cheats, money);
  }

  int circuits = cheats_get_count(STR_LIT("GetCircuitNum"));
  int before   = circuits;

  if (circuits >= 0 && cheats_count_row(ctx, "Circuits", &circuits, 9999)) {
    cheats_add_count(STR_LIT("AddCircuitNum"), circuits - before);
  }

  int ticket = cheats_item_find_tag(cheats, STR_LIT("Inventory.Variable.Ticket"));
  if (ticket >= 0) {
    int tickets = cheats->owned[ticket];

    if (cheats_count_row(ctx, "Tickets", &tickets, 9999)) {
      cheats_set_item_count(cheats, ticket, tickets);
    }
  }
}

static void
cheats_draw_item_table(cheats_t *cheats, struct nk_context *ctx, int category)
{
  nk_layout_row_dynamic(ctx, 26.0f, 1);

  if (nk_button_label(ctx, "Give every item")) {
    for (int i = 0; i < cheats->item_count; ++i) {
      if (cheats->items[i].category != category) {
        continue;
      }

      if (cheats->items[i].store == CHEATS_STORE_USED_UP) {
        cheats_give_item(cheats, &cheats->items[i], 1);
      } else if (cheats->owned[i] <= 0) {
        cheats_set_item_count(cheats, i, 1);
      }
    }
  }

  for (int i = 0; i < cheats->item_count; ++i) {
    cheats_item_t *item = &cheats->items[i];
    if (item->category != category) {
      continue;
    }

    nk_layout_row_template_begin(ctx, 24.0f);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 140.0f);
    nk_layout_row_template_end(ctx);

    /* the columns are narrow: the whole name and tag are in a tooltip */
    if (nk_widget_is_hovered(ctx)) {
      nk_tooltip_text(ctx, (const char *)item->name.data, (int)item->name.len);
    }

    nk_text(ctx, (const char *)item->name.data, (int)item->name.len, NK_TEXT_LEFT);

    if (nk_widget_is_hovered(ctx)) {
      nk_tooltip_text(ctx, (const char *)item->tag.data, (int)item->tag.len);
    }

    nk_text_colored(ctx, (const char *)item->tag.data + 10, (int)item->tag.len - 10, NK_TEXT_LEFT, UI_C_TEXT_DIM);

    if (cheats->not_kept[i] && cheats->owned[i] <= 0) {
      if (nk_widget_is_hovered(ctx)) {
        nk_tooltip(ctx, "The game takes this one but does not keep it in the inventory");
      }

      nk_label_colored(ctx, "Not kept", NK_TEXT_CENTERED, UI_C_TEXT_DIM);
      continue;
    }

    /* the gamepad focus is on the count: its tooltip names the item too */
    if (nk_widget_is_hovered(ctx)) {
      char text[256];
      snprintf(text, sizeof(text), "%.*s (%.*s)", STR_ARG(item->name), STR_ARG(item->tag));
      nk_tooltip(ctx, text);
    }

    /* used up when taken: there is no count to show */
    if (item->store == CHEATS_STORE_USED_UP) {
      if (nk_button_label(ctx, "Give")) {
        cheats_give_item(cheats, item, 1);
      }

      continue;
    }

    int count = cheats->owned[i];
    nk_property_int(ctx, "#", 0, &count, 9999, 1, 1.0f);

    if (count != cheats->owned[i]) {
      cheats_set_item_count(cheats, i, count);
    }
  }
}

static void
cheats_draw_items(cheats_t *cheats, struct nk_context *ctx)
{
  if (!cheats_scan_inventory(cheats)) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, cheats->items ? "Load a save to change the items" : "The game has not loaded its item list yet", NK_TEXT_LEFT);
    return;
  }

  for (int c = 0; c < COUNTOF(g_item_categories); ++c) {
    if (!g_item_categories[c].costume) {
      if (nk_tree_state_push(ctx, NK_TREE_NODE, g_item_categories[c].label, &cheats->item_open[c])) {
        cheats_draw_item_table(cheats, ctx, c);
        nk_tree_pop(ctx);
      }

      continue;
    }

    /* the costume nodes are drawn with the first of them */
    if (c > 0 && g_item_categories[c - 1].costume) {
      continue;
    }

    if (nk_tree_state_push(ctx, NK_TREE_NODE, "Costume", &cheats->costume_open)) {
      for (int k = c; k < COUNTOF(g_item_categories) && g_item_categories[k].costume; ++k) {
        if (nk_tree_state_push(ctx, NK_TREE_NODE, g_item_categories[k].label, &cheats->item_open[k])) {
          cheats_draw_item_table(cheats, ctx, k);
          nk_tree_pop(ctx);
        }
      }

      nk_tree_pop(ctx);
    }
  }
}

/* the tags of an ability, from the player manager's map of them by EHbkPlayerAppendAbilityType */
static fgameplay_tag_t *
cheats_ability_tag(int ability, fgameplay_tag_t **out_item)
{
  uobject_t *manager = cheats_player_manager(&g_cheats);
  fprop_t   *prop    = NULL;
  void      *map     = manager ? cheats_prop_ptr(manager, (ustruct_t *)manager->cls, STR_LIT("PlayerAbilityInfoMap"), &prop) : NULL;

  if (!map || unreal_fprop_get_kind(prop) != UNREAL_PROP_KIND_MAP) {
    return NULL;
  }

  fprop_map_t *map_prop = (fprop_map_t *)prop;
  uint8_t      key      = (uint8_t)ability;
  void        *info     = unreal_map_find_value_ptr(map, map_prop, &key);

  if (!info || unreal_fprop_get_kind(map_prop->val_prop) != UNREAL_PROP_KIND_STRUCT) {
    return NULL;
  }

  ustruct_t *type = (ustruct_t *)((fprop_struct_t *)map_prop->val_prop)->script_struct;

  *out_item = cheats_prop_ptr(info, type, STR_LIT("InventoryTag"), NULL);
  return cheats_prop_ptr(info, type, STR_LIT("AbilityTag"), NULL);
}

/* the ability's tag leaves the player's tags, and the item that teaches it the inventory */
static void
cheats_forget_ability(cheats_t *cheats, int ability)
{
  fgameplay_tag_t           *item_tag = NULL;
  fgameplay_tag_t           *tag      = cheats_ability_tag(ability, &item_tag);
  fgameplay_tag_container_t *tags     = cheats_state_prop(STR_LIT("AbilityTags"));

  if (!tag || !tags) {
    return;
  }

  for (int32_t i = 0; i < tags->gameplay_tags.num; ++i) {
    if (unreal_fname_equal(tags->gameplay_tags.data[i].tag_name, tag->tag_name, false)) {
      memmove(&tags->gameplay_tags.data[i], &tags->gameplay_tags.data[i + 1], sizeof(tags->gameplay_tags.data[0]) * (size_t)(tags->gameplay_tags.num - i - 1));
      tags->gameplay_tags.num -= 1;
      break;
    }
  }

  int item = item_tag && !unreal_fname_is_none(item_tag->tag_name) ? cheats_item_find(cheats, item_tag->tag_name) : -1;
  if (item >= 0 && cheats_scan_inventory(cheats) && cheats->owned[item] > 0) {
    cheats_set_item_count(cheats, item, 0);
  }

  LOG_INFO("cheats: ability %d forgotten", ability);
}

static void
cheats_unlock_all_skills(void)
{
  cheats_call_simple(STR_LIT("DebugOpenAllSkills"));

  cheats_call_t call;
  if (cheats_call_begin(&call, cheats_find_live(STR_LIT("HbkInventoryManager")), STR_LIT("GetSkillAll"))) {
    cheats_call(&call);
  }
}

/* the health bar and the reverb gauge grow with the items the game gives for them */
static void
cheats_max_growth(cheats_t *cheats, bool health)
{
  uobject_t *growth = cheats_find_live(STR_LIT("HbkPlayerGrowthComponent"));
  int32_t   *max    = growth ? cheats_prop_ptr(growth, (ustruct_t *)growth->cls, health ? STR_LIT("HealthGrowthMaxNum") : STR_LIT("GaugeGrowthMaxNum"), NULL) : NULL;
  int32_t   *step   = growth ? cheats_prop_ptr(growth, (ustruct_t *)growth->cls, STR_LIT("GaugeGrowthAmount"), NULL) : NULL;
  int        item   = cheats_item_find_tag(cheats, health ? STR_LIT("Inventory.Variable.LifeUp") : STR_LIT("Inventory.Variable.GaugeUp"));

  if (!max || item < 0) {
    return;
  }

  int now     = cheats_call_int(growth, health ? STR_LIT("GetCurrentHealthGrowthNum") : STR_LIT("GetCurrentGaugeGrowthAmount"));
  int missing = health ? *max - now : *max - now / MAX_VAL(step ? *step : 1, 1);

  if (missing > 0) {
    cheats_give_item(cheats, &cheats->items[item], missing);
  }
}

static void
cheats_draw_progress(cheats_t *cheats, struct nk_context *ctx)
{
  if (!cheats_player_character(cheats)) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Load a save to change the progress", NK_TEXT_LEFT);
    return;
  }

  nk_layout_row_dynamic(ctx, 26.0f, 3);

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "Unlocks all abilities, partners, combos, partner combos, special attacks, chips, additional chip slots and the special attack slot");
  }

  if (nk_button_label(ctx, "Unlock all skills")) {
    cheats_unlock_all_skills();
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "Unlocks the maximum possible health bar");
  }

  if (nk_button_label(ctx, "Unlock max healthbar")) {
    cheats_max_growth(cheats, true);
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "Unlocks the maximum possible reverb gauge");
  }

  if (nk_button_label(ctx, "Unlock max reverb gauge")) {
    cheats_max_growth(cheats, false);
  }

  if (nk_tree_state_push(ctx, NK_TREE_NODE, "Abilities", &cheats->abilities_open)) {
    for (int i = 1; i < COUNTOF(g_ability_labels); ++i) {
      nk_layout_row_template_begin(ctx, 24.0f);
      nk_layout_row_template_push_dynamic(ctx);
      nk_layout_row_template_push_static(ctx, 120.0f);
      nk_layout_row_template_end(ctx);

      nk_label(ctx, g_ability_labels[i], NK_TEXT_LEFT);

      int has = cheats_has_ability(i) ? 1 : 0;
      int now = has;

      nk_checkbox_label(ctx, "Unlocked", &now);

      if (now != has) {
        if (now) {
          cheats_learn_ability(i);
        } else {
          cheats_forget_ability(cheats, i);
        }
      }
    }

    nk_tree_pop(ctx);
  }

  if (nk_tree_state_push(ctx, NK_TREE_NODE, "Partners", &cheats->partners_open)) {
    uint8_t *joined = cheats_state_prop(STR_LIT("JoinedPartnerFlags"));

    for (int i = 0; i < COUNTOF(g_partners) && joined; ++i) {
      nk_layout_row_template_begin(ctx, 24.0f);
      nk_layout_row_template_push_dynamic(ctx);
      nk_layout_row_template_push_static(ctx, 120.0f);
      nk_layout_row_template_end(ctx);

      nk_label(ctx, g_partners[i].label, NK_TEXT_LEFT);

      int has = (*joined & g_partners[i].flag) ? 1 : 0;
      int now = has;

      nk_checkbox_label(ctx, "Joined", &now);

      if (now && !has) {
        cheats_join_partner(g_partners[i].flag);
      } else if (!now && has) {
        *joined &= (uint8_t)~g_partners[i].flag;
        LOG_INFO("cheats: partner %d left", g_partners[i].flag);
      }
    }

    nk_tree_pop(ctx);
  }
}

static void
cheats_set_hideout_flag(cheats_t *cheats, str_t flag, int value)
{
  uobject_t    *player = cheats_player_character(cheats);
  uclass_t     *cls    = unreal_uclass_find(STR_LIT("HbkGameFlagFunction"), false, true);
  cheats_call_t call;

  if (!player || !cls || !cls->cdo || !cheats_call_begin(&call, cls->cdo, STR_LIT("SetHideOutFlag"))) {
    return;
  }

  uobject_t **context = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  fname_t    *name    = cheats_call_param(&call, STR_LIT("FlagName"));
  int32_t    *set     = cheats_call_param(&call, STR_LIT("SetFlag"));

  if (!context || !name || !set) {
    return;
  }

  *context = player;
  *name    = unreal_fname_from_str(flag, FNAME_FIND_OR_ADD);
  *set     = value;
  cheats_call(&call);

  LOG_INFO("cheats: hideout flag %.*s set to %d", STR_ARG(flag), value);
}

/* every challenge on the wall as reached; their rewards are still taken at the wall */
static void
cheats_unlock_wall_of_fame(void)
{
  uobject_t *wall          = cheats_find_live(STR_LIT("HbkHideOutChallenge"));
  fprop_t   *progress_prop = NULL;
  void      *progress      = wall ? cheats_prop_ptr(wall, (ustruct_t *)wall->cls, STR_LIT("Progress"), &progress_prop) : NULL;

  if (!progress || unreal_fprop_get_kind(progress_prop) != UNREAL_PROP_KIND_STRUCT) {
    return;
  }

  fprop_t *table_prop = NULL;
  void    *table      = cheats_prop_ptr(progress, (ustruct_t *)((fprop_struct_t *)progress_prop)->script_struct, STR_LIT("ChallengeProgressTbl"), &table_prop);

  if (!table || unreal_fprop_get_kind(table_prop) != UNREAL_PROP_KIND_MAP) {
    return;
  }

  fprop_map_t *map = (fprop_map_t *)table_prop;
  if (unreal_fprop_get_kind(map->val_prop) != UNREAL_PROP_KIND_STRUCT) {
    return;
  }

  ustruct_t *type   = (ustruct_t *)((fprop_struct_t *)map->val_prop)->script_struct;
  fprop_t   *status = unreal_ustruct_find_prop(type, STR_LIT("Status"));
  fprop_t   *now    = unreal_ustruct_find_prop(type, STR_LIT("NowValue"));
  fprop_t   *target = unreal_ustruct_find_prop(type, STR_LIT("TargetValue"));
  fprop_t   *fresh  = unreal_ustruct_find_prop(type, STR_LIT("bNewUnlocked"));

  if (!status || !now || !target) {
    return;
  }

  int unlocked = 0;
  for (int32_t i = 0, max = unreal_map_max_index(table, map); i < max; ++i) {
    uint8_t *param = unreal_map_is_valid_index(table, map, i) ? unreal_map_get_value(table, map, i) : NULL;

    /* EHbkHideOutChallengeStatus::Unlocked */
    if (!param || param[status->offset_internal] == 2) {
      continue;
    }

    param[status->offset_internal]               = 2;
    *(int32_t *)(param + now->offset_internal)    = *(int32_t *)(param + target->offset_internal);

    if (fresh) {
      unreal_fprop_write_bool(fresh, param + fresh->offset_internal, true);
    }

    unlocked += 1;
  }

  LOG_INFO("cheats: %d challenges unlocked", unlocked);
}

static bool
cheats_set_flag(cheats_t *cheats, uint8_t container, str_t category, fname_t name, int32_t value)
{
  uobject_t *manager = cheats->set_flag ? cheats_find_live(STR_LIT("HbkGameFlagManager")) : NULL;
  if (!manager) {
    return false;
  }

  cheats->set_flag(manager, container, unreal_fname_from_str(category, FNAME_FIND_OR_ADD), name, value, false);
  return true;
}

typedef struct cheats_products_s cheats_products_t;
struct cheats_products_s {
  str_t   prefix;
  fname_t names[512];
  int     count;
};

/* a product's tag is the category, the group and the product under Product.Gallery: the shorter ones are groups */
static bool
cheats_gather_product(fname_t name, fname_entry_t *entry, void *user)
{
  cheats_products_t *products = user;

  if (!unreal_fname_entry_match_text(entry, products->prefix, false, false)) {
    return true;
  }

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t text  = unreal_fname_to_str(name, tmp.arena);
    int   parts = 1;

    for (uint64_t i = 0; i < text.len; ++i) {
      parts += text.data[i] == '.';
    }

    if (parts == 5 && str_has_prefix(text, products->prefix, 0) && products->count < COUNTOF(products->names)) {
      products->names[products->count++] = name;
    }
  }
  scratch_end(tmp);

  return true;
}

/* Songs in the jukebox and models in the model viewer are bought with gears: they are set as bought. The
 * names are gathered first, the setter may add names to the pool. */
static void
cheats_purchase_gallery(cheats_t *cheats, str_t prefix)
{
  static cheats_products_t products;
  products = (cheats_products_t){.prefix = prefix};

  if (!cheats->set_flag) {
    LOG_ERROR("cheats: the game flag setter was not found");
    return;
  }

  unreal_fname_pool_iterate(cheats_gather_product, &products);

  int bought = 0;
  for (int i = 0; i < products.count; ++i) {
    bought += cheats_set_flag(cheats, CHEATS_FLAGS_GAME, STR_LIT("Product"), products.names[i], CHEATS_PRODUCT_PURCHASED);
  }

  LOG_INFO("cheats: %d of %.*s bought", bought, STR_ARG(prefix));
}

/* A graffiti is found when its row's flag is in the stage progress' GraffitiCollectFound. The last one is
 * the one given at the end, when all the others are found. */
static void
cheats_find_graffiti(cheats_t *cheats, bool last)
{
  uobject_t              *settings = cheats_find_live(STR_LIT("HbkGraffitiCollectSettingsDataAsset"));
  udata_table_t         **table    = settings ? cheats_prop_ptr(settings, (ustruct_t *)settings->cls, STR_LIT("DataTable"), NULL) : NULL;
  tmap_fname_uint8ptr_t  *rows     = table && *table ? unreal_udata_table_get_row_map(*table) : NULL;
  fprop_t                *flag     = rows ? unreal_ustruct_find_prop((ustruct_t *)(*table)->row_struct, STR_LIT("FoundGameFlagItemName")) : NULL;

  if (!flag || !cheats->set_flag) {
    LOG_ERROR("cheats: the graffiti list or the game flag setter was not found");
    return;
  }

  int found = 0;
  TMAP_FOR_EACH_INDEX(rows, idx) {
    uint8_t *row = TMAP_VALUE_AT(rows, idx);
    if (!row) {
      continue;
    }

    fname_t name    = *(fname_t *)(row + flag->offset_internal);
    bool    is_last = unreal_fname_match_text(name, STR_LIT("Graffiti.LastGraffiti"), false, true);

    if (is_last != last || unreal_fname_match_text(name, STR_LIT("Default"), false, true)) {
      continue;
    }

    if (cheats_set_flag(cheats, CHEATS_FLAGS_PROGRESS, STR_LIT("GraffitiCollectFound"), name, 1)) {
      found += 1;
    }
  }

  if (last) {
    cheats_set_hideout_flag(cheats, STR_LIT("LastGraffiti"), 1);
  }

  LOG_INFO("cheats: %d graffiti found", found);
}

/* got from the game studio, a stat of the save; the hideout flag puts it in the hideout */
static void
cheats_give_gift_basket(cheats_t *cheats)
{
  uobject_t    *player = cheats_player_character(cheats);
  uclass_t     *cls    = unreal_uclass_find(STR_LIT("HbkPlayerMetricsFunction"), false, true);
  cheats_call_t call;

  if (!player || !cls || !cls->cdo || !cheats_call_begin(&call, cls->cdo, STR_LIT("ModifyStat"))) {
    return;
  }

  uobject_t **context  = cheats_call_param(&call, STR_LIT("WorldContextObject"));
  fname_t    *name     = cheats_call_param(&call, STR_LIT("StatName"));
  int32_t    *value    = cheats_call_param(&call, STR_LIT("Value"));
  uint8_t    *modifier = cheats_call_param(&call, STR_LIT("Modifier"));

  if (!context || !name || !value || !modifier) {
    return;
  }

  *context  = player;
  *name     = unreal_fname_from_str(STR_LIT("GotGameStudioGiftBasket"), FNAME_FIND_OR_ADD);
  *value    = 1;
  *modifier = 3; // EHbkPlayerMetricsStatModifier::Max
  cheats_call(&call);
  cheats_set_hideout_flag(cheats, STR_LIT("Giftbox"), 1);

  LOG_INFO("cheats: gift basket got");
}

static void
cheats_draw_misc(cheats_t *cheats, struct nk_context *ctx)
{
  if (!cheats_player_character(cheats)) {
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, "Load a save to unlock these", NK_TEXT_LEFT);
    return;
  }

  nk_layout_row_dynamic(ctx, 26.0f, 1);

  if (nk_button_label(ctx, "Unlock every song in jukebox")) {
    cheats_purchase_gallery(cheats, STR_LIT("Product.Gallery.SoundGallery."));
  }

  if (nk_button_label(ctx, "Unlock every model in model viewer")) {
    cheats_purchase_gallery(cheats, STR_LIT("Product.Gallery.ModelViewer."));
  }

  if (nk_button_label(ctx, "Unlock every costume")) {
    cheats_call_t call;
    if (cheats_call_begin(&call, cheats_find_live(STR_LIT("HbkInventoryManager")), STR_LIT("OpenCostumeAll"))) {
      cheats_call(&call);
      LOG_INFO("cheats: every costume unlocked");
    }
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "Unlocks every achievement in Wall of Fame");
  }

  if (nk_button_label(ctx, "Unlock everything in Wall of Fame")) {
    cheats_unlock_wall_of_fame();
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "The arcade machine in the hideout");
  }

  if (nk_button_label(ctx, "Unlock BPM RUSH and Power Up! Tower Up!")) {
    cheats_set_hideout_flag(cheats, STR_LIT("OpenArcadeMode"), 1);
    cheats_set_hideout_flag(cheats, STR_LIT("OpenTowerTalk"), 1);
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "They do not show up right away: go through a checkpoint in a stage and come back to the hideout");
  }

  if (nk_button_label(ctx, "Unlock every graffiti")) {
    cheats_find_graffiti(cheats, false);
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "The gift basket in the hideout. It does not show up right away: go to a stage and come back to the hideout");
  }

  if (nk_button_label(ctx, "Unlock Gift Basket")) {
    cheats_give_gift_basket(cheats);
  }

  if (nk_widget_is_hovered(ctx)) {
    nk_tooltip(ctx, "The Chai banana head graffiti in the hideout. It does not show up right away: go through a checkpoint in a stage and come back to the hideout");
  }

  if (nk_button_label(ctx, "Unlock the graffiti")) {
    cheats_find_graffiti(cheats, true);
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

  static enum nk_collapse_states general_open  = NK_MAXIMIZED;
  static enum nk_collapse_states items_open    = NK_MINIMIZED;
  static enum nk_collapse_states progress_open = NK_MINIMIZED;
  static enum nk_collapse_states teleport_open = NK_MINIMIZED;
  static enum nk_collapse_states misc_open     = NK_MINIMIZED;

  cheats_t *cheats = &g_cheats;
  if (!cheats->inited) {
    return;
  }

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "General", &general_open)) {
    cheats_draw_general(cheats, ctx);
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

  if (nk_tree_state_push(ctx, NK_TREE_TAB, "Misc", &misc_open)) {
    cheats_draw_misc(cheats, ctx);
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

  bool infinite_health = mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.infinite_health_h);
  if (infinite_health || cheats->infinite_health_on) {
    cheats_infinite_health(cheats, infinite_health);
  }

  if (mod_manager_cfg_get_bool(&globals.mod_manager, cheats->cfg.infinite_reverb_h)) {
    cheats_infinite_reverb(cheats);
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
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_INFINITE_HEALTH),
      .label       = STR_CLIT("Infinite health"),
      .description = STR_CLIT("Hits hurt as usual, but the health is filled up again at once and the player can not die"),
      .val = {
        .boolean = {
          .default_val = false,
        },
      },
    },
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_INFINITE_REVERB),
      .label       = STR_CLIT("Infinite reverb gauge"),
      .description = STR_CLIT("The reverb gauge is always full"),
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
