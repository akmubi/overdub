#include "builtin.h"

#include "debug.h"
#include "file.h"
#include "globals.h"
#include "log.h"
#include "mod_host.h"
#include "path.h"
#include "scratch.h"

#include <string.h>

#define CFG_DISABLE_TUTORIALS "disable-tutorials"
#define CFG_RESTORE_JUKEBOX   "restore-jukebox-state"
#define MOD_ID                "tweaks"

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

typedef struct tweaks_jbox_s tweaks_jbox_t;
struct tweaks_jbox_s {
  jbox_state_t         saved_state;
  jbox_controller_t   *controller;
  bool                 should_restore_state;
  jbox_play_music_fn_t play_music;
  str_t                state_path;
  bool                 in_arcade_mode;
  fname_t              controller_cls_name;
};

typedef struct tweaks_cfg_s tweaks_cfg_t;
struct tweaks_cfg_s {
  mod_cfg_handle_t disable_tutorials_h;
  mod_cfg_handle_t restore_jbox_h;
};

typedef struct tweaks_s tweaks_t;
struct tweaks_s {
  tweaks_cfg_t  cfg;
  tweaks_jbox_t jbox;
  arena_t      *perm;
  bool          inited;
};

static tweaks_t g_tweaks = {0};

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

  return fname_is(obj->cls->base.base.base.name, cls_name);
}

static bool
ufunc_is(ufunc_t *func, str_t func_name)
{
  if (!func) {
    return false;
  }

  return fname_is(func->base.base.base.name, func_name);
}

static bool
should_disable_tutorials(tweaks_t *tweaks)
{
  return tweaks && tweaks->inited && mod_cfg_get_bool(&globals.mod_manager, tweaks->cfg.disable_tutorials_h);
}

static bool
should_restore_jbox_music(tweaks_t *tweaks)
{
  return tweaks && tweaks->inited && mod_cfg_get_bool(&globals.mod_manager, tweaks->cfg.restore_jbox_h);
}

static bool
jbox_play_music(jbox_controller_t *controller, jbox_state_t state)
{
  ASSERT(controller != NULL);
  ASSERT(unreal_uobject_is_valid((uobject_t *)controller));

  if (!g_tweaks.jbox.play_music) {
    return false;
  }

  controller->repeat_mode     = state.repeat_mode;
  controller->shuffle_enabled = state.shuffle_enabled;

  fgameplay_tag_t product_id = {
    .tag_name = state.product_id,
  };

  return g_tweaks.jbox.play_music(controller, &product_id, false);
}

static void
tweaks_save_jbox_state(jbox_state_t *state, str_t state_path)
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
tweaks_load_jbox_state(jbox_state_t *state, str_t state_path)
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
tweaks_on_jbox_controller_create(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  tweaks_t *tweaks = &g_tweaks;

  if (obj && obj->cls && unreal_fname_equal(obj->cls->base.base.base.name, tweaks->jbox.controller_cls_name, true) && !unreal_uobject_is_default(obj)) {
    tweaks->jbox.controller           = (jbox_controller_t *)obj;
    tweaks->jbox.should_restore_state = true;
  }
}

static void
tweaks_on_jbox_controller_delete(uobject_t *obj, int32_t idx, void *user)
{
  (void)idx;
  (void)user;

  tweaks_t *tweaks = &g_tweaks;

  if (tweaks->jbox.controller && tweaks->jbox.controller == (jbox_controller_t *)obj) {
    tweaks->jbox.controller           = NULL;
    tweaks->jbox.in_arcade_mode       = false;
    tweaks->jbox.should_restore_state = false;
  }
}

static bool MOD_CALL
tweaks_init(const mod_host_api_t *host, mod_handle_t h)
{
  tweaks_t *tweaks = &g_tweaks;
  mem_zero(tweaks, sizeof(*tweaks));

  tweaks->perm = mod_arena_handle_resolve(host->get_perm(h));
  ASSERT(tweaks->perm != NULL);

  tweaks->cfg.disable_tutorials_h = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_DISABLE_TUTORIALS));
  tweaks->cfg.restore_jbox_h      = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_RESTORE_JUKEBOX));

  void *jbox_play_music_addr = NULL;
  host->sigscan("JukeboxPlayMusic", "48 89 5C 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 4C 8B F2 48 8B F9", 0, 0, NULL, &jbox_play_music_addr);
  tweaks->jbox.play_music = jbox_play_music_addr;

  tweaks->jbox.state_path = path_join(tweaks->perm, host->get_mod_dir(h), STR_LIT("jukebox_state.ini"));
  tweaks_load_jbox_state(&tweaks->jbox.saved_state, tweaks->jbox.state_path);

  tweaks->jbox.controller_cls_name = unreal_fname_from_str(STR_LIT("JukeBoxController_BP_C"), FNAME_FIND);

  host->register_uobject_listener(h, UOBJECT_LISTENER_KIND_CREATE, tweaks_on_jbox_controller_create, NULL);
  host->register_uobject_listener(h, UOBJECT_LISTENER_KIND_DELETE, tweaks_on_jbox_controller_delete, NULL);

  uclass_t *jbox_controller_cls = unreal_uclass_find(STR_LIT("JukeBoxController_BP_C"), false, true);
  if (jbox_controller_cls) {
    tweaks->jbox.controller = (jbox_controller_t *)unreal_uobject_find_first_of(jbox_controller_cls);
    if (tweaks->jbox.controller) {
      tweaks->jbox.should_restore_state = true;
    }
  }

  tweaks->inited = true;
  return true;
}

static void MOD_CALL
tweaks_deinit(mod_handle_t h)
{
  (void)h;

  tweaks_t *tweaks = &g_tweaks;
  mem_zero(tweaks, sizeof(*tweaks));
}

static bool
tweaks_process_event_pre(mod_handle_t h, uobject_t *self, ufunc_t *func, void *params)
{
  (void)h;
  (void)params;
  (void)self;

  bool should_consume = false;

  tweaks_t *tweaks = &g_tweaks;

  if (should_disable_tutorials(tweaks) && ufunc_is(func, STR_LIT("WidgetAnimationEvt_OpenAnimation_K2Node_WidgetAnimationEvent"))) {
    should_consume = true;
  }

  return should_consume;
}

static void
tweaks_process_event_post(mod_handle_t h, uobject_t *self, ufunc_t *func, void *params, bool consumed)
{
  (void)h;
  (void)params;
  (void)consumed;

  tweaks_t *tweaks = &g_tweaks;

  if (should_restore_jbox_music(tweaks)) {
    if (uobject_class_is(self, STR_LIT("SM_Arcade_BP_C"))) {
      if (ufunc_is(func, STR_LIT("OnNotifyBegin_59884126489B8A771076CBBF9EB59637"))) {
        tweaks->jbox.in_arcade_mode = true;
      } else if (ufunc_is(func, STR_LIT("OnCloseArcadeMenu_855C3BCC4FA558B3F4D02C834A70C17B"))) {
        tweaks->jbox.in_arcade_mode       = false;
        tweaks->jbox.should_restore_state = true;
      }
    }
  }
}

static void
tweaks_ufunction_invoke_post(mod_handle_t h, ufunc_t *func, uobject_t *self, fframe_t *stack, void *result, bool consumed)
{
  (void)h;
  (void)stack;
  (void)result;
  (void)consumed;

  tweaks_t *tweaks = &g_tweaks;
  if (should_disable_tutorials(tweaks)) {
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

static void
tweaks_tick(mod_handle_t h, float delta)
{
  (void)h;
  (void)delta;

  tweaks_t *tweaks = &g_tweaks;
  if (!tweaks->inited) {
    return;
  }

  if (should_restore_jbox_music(tweaks)) {
    if (tweaks->jbox.controller && unreal_uobject_is_valid((uobject_t *)tweaks->jbox.controller)) {
      if (tweaks->jbox.should_restore_state) {
        tweaks->jbox.should_restore_state = false;
        if (tweaks->jbox.saved_state.playback_state == JBOX_PLAYBACK_PLAYING) {
          bool ok                           = jbox_play_music(tweaks->jbox.controller, tweaks->jbox.saved_state);
          tweaks->jbox.should_restore_state = !ok;
        }
      }

      jbox_state_t state = {
        .product_id      = tweaks->jbox.controller->playing_music_data.product_id.tag_name,
        .repeat_mode     = tweaks->jbox.controller->repeat_mode,
        .shuffle_enabled = tweaks->jbox.controller->shuffle_enabled,
        .playback_state  = tweaks->jbox.controller->playback_state,
      };

      /* save valid state changes made outside arcade mode */
      if (memcmp(&tweaks->jbox.saved_state, &state, sizeof(state)) != 0 && state.product_id.cmp_idx != 0 && !tweaks->jbox.in_arcade_mode) {
#if 0
        tmp_arena_t tmp = scratch_begin(NULL);
        {
          LOG_DEBUG("%llu: %s: jukebox state changed: repeat mode: %d -> %d, shuffle: %d -> %d, playback state: %d (%s) -> %d (%s), product id: %.*s -> %.*s",
                    globals.frame_counter,
                    MOD_ID,
                    tweaks->jbox.saved_state.repeat_mode,
                    state.repeat_mode,
                    tweaks->jbox.saved_state.shuffle_enabled,
                    state.shuffle_enabled,
                    tweaks->jbox.saved_state.playback_state,
                    jbox_playback_state_to_str(tweaks->jbox.saved_state.playback_state),
                    state.playback_state,
                    jbox_playback_state_to_str(state.playback_state),
                    STR_ARG(unreal_fname_to_str(tweaks->jbox.saved_state.product_id, tmp.arena)),
                    STR_ARG(unreal_fname_to_str(state.product_id, tmp.arena)));
        }
        scratch_end(tmp);
#endif

        tweaks->jbox.saved_state = state;
        tweaks_save_jbox_state(&tweaks->jbox.saved_state, tweaks->jbox.state_path);
      }
    }
  }
}

void
register_builtin_tweaks(mod_manager_t *manager)
{
  mod_option_info_t options[] = {
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_DISABLE_TUTORIALS),
      .label       = STR_CLIT("Disable tutorial pop-ups"),
      .description = STR_CLIT("Prevents tutorial pop-ups from appearing"),
      .val =
        {
          .boolean =
            {
              .default_val = true,
            },
        },
    },
    {
      .type        = MOD_OPTION_BOOL,
      .id          = STR_CLIT(CFG_RESTORE_JUKEBOX),
      .label       = STR_CLIT("Remember jukebox playback"),
      .description = STR_CLIT("Restores the jukebox track, playback state, repeat mode, and shuffle setting after stage transitions and game restarts"),
      .val =
        {
          .boolean =
            {
              .default_val = true,
            },
        },
    },
  };

  mod_manifest_t manifest = {
    .info =
      {
        .id          = STR_CLIT(MOD_ID),
        .name        = STR_CLIT("Tweaks"),
        .author      = STR_CLIT("akmubi"),
        .description = STR_CLIT("Quality-of-life tweaks"),
        .kind        = MOD_KIND_MOD,
        .version     = MAKE_VERSION(0, 1, 0),
      },
    .options      = options,
    .option_count = COUNTOF(options),
  };

  mod_dll_runtime_funcs_t funcs = {
    .init             = tweaks_init,
    .deinit           = tweaks_deinit,
    .tick             = tweaks_tick,
    .pe_pre           = tweaks_process_event_pre,
    .pe_post          = tweaks_process_event_post,
    .func_invoke_post = tweaks_ufunction_invoke_post,
  };

  mod_handle_t h = mod_register_builtin(manager, &manifest, funcs);
  if (h == MOD_HANDLE_INVALID) {
    CONSOLE_ERROR("Failed to register the built-in Tweaks mod");
  }
}
