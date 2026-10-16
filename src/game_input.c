#include "game_input.h"

#include "log.h"
#include "scratch.h"
#include "str.h"
#include "unreal.h"
#include "unreal_prop.h"

static const str_t g_waiting_actions[] = {
  STR_CLIT("GameOption"),
  STR_CLIT("Gamepause"),
  STR_CLIT("FastForwardEvent"),
  STR_CLIT("SkipEvent"),
  STR_CLIT("SkipTalkLine"),
  STR_CLIT("HintButton"),
  STR_CLIT("RestartSound"),
  STR_CLIT("Decide"),
};

static const str_t g_waiting_prefixes[] = {
  STR_CLIT("Menu"),
  STR_CLIT("Debug"),
  STR_CLIT("PhotoCamera"),
};

static struct {
  bool           resolved;
  uobject_t     *settings;
  fprop_array_t *mappings;
  int32_t        action_offset;
  int32_t        key_offset;
} g_game_input;

static bool
game_input_resolve(void)
{
  if (g_game_input.resolved) {
    return g_game_input.mappings != NULL;
  }

  uclass_t *cls = unreal_uclass_find(STR_LIT("InputSettings"), false, true);
  if (!cls || !cls->cdo) {
    return false;
  }

  g_game_input.resolved = true;

  fprop_array_t *mappings = (fprop_array_t *)unreal_ustruct_find_prop((ustruct_t *)cls, STR_LIT("ActionMappings"));
  if (!mappings || !mappings->inner) {
    LOG_WARN("game input: InputSettings has no ActionMappings; every gamepad button may wait for a combo");
    return false;
  }

  ustruct_t *element = (ustruct_t *)((fprop_struct_t *)mappings->inner)->script_struct;
  fprop_t   *action  = element ? unreal_ustruct_find_prop(element, STR_LIT("ActionName")) : NULL;
  fprop_t   *key     = element ? unreal_ustruct_find_prop(element, STR_LIT("Key")) : NULL;

  if (!action || !key) {
    LOG_WARN("game input: unknown action mapping layout; every gamepad button may wait for a combo");
    return false;
  }

  g_game_input.settings      = cls->cdo;
  g_game_input.mappings      = mappings;
  g_game_input.action_offset = action->offset_internal;
  g_game_input.key_offset    = key->offset_internal; // FKey starts with its name
  return true;
}

static void
game_input_log_waiting(void)
{
  tmp_arena_t tmp     = scratch_begin(NULL);
  str_list_t  waiting = {0};

  for (input_key_kind_t key = INPUT_KEY_GAMEPAD_FACE_BOTTOM; key <= INPUT_KEY_GAMEPAD_DPAD_RIGHT; ++key) {
    if (game_input_key_can_wait(key)) {
      str_list_push(tmp.arena, &waiting, input_key_to_str(key));
    }
  }

  str_t list = str_list_join(tmp.arena, waiting, STR_LIT(""), STR_LIT(", "), STR_LIT(""));
  LOG_INFO("game input: these buttons may wait for a combo, the others reach the game at once: %.*s", STR_ARG(list));
  scratch_end(tmp);
}

static bool
game_input_action_can_wait(str_t action)
{
  for (int i = 0; i < COUNTOF(g_waiting_actions); ++i) {
    if (str_equal(action, g_waiting_actions[i], 0)) {
      return true;
    }
  }

  for (int i = 0; i < COUNTOF(g_waiting_prefixes); ++i) {
    if (str_has_prefix(action, g_waiting_prefixes[i], 0)) {
      return true;
    }
  }
  return false;
}

bool
game_input_key_can_wait(input_key_kind_t key)
{
  if (!game_input_resolve()) {
    return true;
  }

  fname_t name    = input_key_to_fname(key);
  void   *array   = (uint8_t *)g_game_input.settings + g_game_input.mappings->base.offset_internal;
  int32_t count   = unreal_array_num(array, g_game_input.mappings);
  bool    waiting = true;

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    for (int32_t i = 0; i < count && waiting; ++i) {
      uint8_t *mapping = unreal_array_get(array, g_game_input.mappings, i);

      if (!mapping || !unreal_fname_equal(*(fname_t *)(mapping + g_game_input.key_offset), name, true)) {
        continue;
      }

      waiting = game_input_action_can_wait(unreal_fname_to_str(*(fname_t *)(mapping + g_game_input.action_offset), tmp.arena));
    }
  }
  scratch_end(tmp);

  return waiting;
}

void
game_input_init(void)
{
  if (game_input_resolve()) {
    game_input_log_waiting();
  }
}
