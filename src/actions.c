#include "actions.h"

#include "arena.h"
#include "config.h"
#include "globals.h"
#include "log.h"
#include "string_pool.h"

static void actions_send_on_held(actions_t *registry, bool replay);
static void actions_fire(actions_t *registry, uint64_t pressed_frame);

void
actions_init(actions_t *registry, actions_replay_fn_t replay, actions_dispatch_fn_t dispatch, actions_can_hold_fn_t can_hold)
{
  *registry = (actions_t){
    .items        = ARENA_PUSH_ARRAY_ZERO(&globals.perm, action_t, CONFIG_ACTIONS_MAX),
    .replay       = replay,
    .dispatch     = dispatch,
    .can_hold     = can_hold,
    .hold_ms      = CONFIG_ACTIONS_DEFAULT_HOLD_MS,
    .match        = -1,
  };

  ASSERT(registry->items != NULL);
}

void
actions_set_hold_ms(actions_t *registry, uint32_t ms)
{
  registry->hold_ms = ms;
}

uint32_t
actions_get_hold_ms(actions_t *registry)
{
  return registry->hold_ms;
}

/* ==================================================== REGISTRY ==================================================== */

static int
actions_index_of(actions_t *registry, keybind_t *bind)
{
  for (int i = 0; i < registry->count; ++i) {
    if (registry->items[i].bind == bind) {
      return i;
    }
  }
  return -1;
}

static void
actions_release_strings(action_t *a)
{
  string_pool_release(&globals.strings, a->id);
  string_pool_release(&globals.strings, a->label);
  string_pool_release(&globals.strings, a->source);
}

static void
actions_gesture_reset(actions_t *registry)
{
  registry->active      = false;
  registry->fired       = false;
  registry->held        = KEYBIND_NULL;
  registry->passed      = KEYBIND_NULL;
  registry->held_events = 0;
  registry->match       = -1;
  registry->fired_part  = KEYBIND_NULL;
}

/* indices may have moved */
static void
actions_gesture_restart(actions_t *registry)
{
  if (registry->active && !registry->fired) {
    actions_send_on_held(registry, true);
  }

  actions_gesture_reset(registry);
}

action_t *
actions_register(actions_t *registry, str_t id, str_t label, str_t source, mod_handle_t owner, keybind_t *bind, action_fire_fn_t on_fire, void *user)
{
  if (!bind || !registry->items) {
    return NULL;
  }

  int idx = actions_index_of(registry, bind);

  if (idx >= 0) {
    actions_release_strings(&registry->items[idx]);
  } else if (registry->count < CONFIG_ACTIONS_MAX) {
    idx = registry->count++;
  } else {
    LOG_WARN("Too many keybinds: '%.*s' (%.*s) is not registered", STR_ARG(label), STR_ARG(source));
    return NULL;
  }

  action_t *a = &registry->items[idx];

  *a = (action_t){
    .id      = string_pool_push(&globals.strings, id),
    .label   = string_pool_push(&globals.strings, label),
    .source  = string_pool_push(&globals.strings, source),
    .owner   = owner,
    .bind    = bind,
    .on_fire = on_fire,
    .user    = user,
  };

  actions_gesture_restart(registry);
  return a;
}

void
actions_unregister(actions_t *registry, keybind_t *bind)
{
  int idx = actions_index_of(registry, bind);

  if (idx < 0) {
    return;
  }

  actions_release_strings(&registry->items[idx]);

  registry->count     -= 1;
  registry->items[idx] = registry->items[registry->count];
  mem_zero(&registry->items[registry->count], sizeof(action_t));

  actions_gesture_restart(registry);
}

/* ==================================================== GAMEPAD ===================================================== */

static bool
held_contains(keybind_t set, input_key_kind_t key)
{
  for (int i = 0; i < set.count; ++i) {
    if (set.keys[i] == key) {
      return true;
    }
  }
  return false;
}

static bool
held_subset(keybind_t a, keybind_t b)
{
  for (int i = 0; i < a.count; ++i) {
    if (!held_contains(b, a.keys[i])) {
      return false;
    }
  }
  return true;
}

static keybind_t
action_gamepad_part(action_t *a)
{
  return a->bind ? keybind_gamepad_part(*a->bind) : KEYBIND_NULL;
}

/* some part has all of the held buttons, pressed in any order */
static bool
held_fits(actions_t *registry, keybind_t held)
{
  for (int i = 0; i < registry->count; ++i) {
    keybind_t part = action_gamepad_part(&registry->items[i]);

    if (part.count > 0 && held_subset(held, part)) {
      return true;
    }
  }
  return false;
}

static int
held_match(actions_t *registry, keybind_t held)
{
  for (int i = 0; i < registry->count; ++i) {
    keybind_t part = action_gamepad_part(&registry->items[i]);

    if (part.count == held.count && part.count > 0 && held_subset(held, part)) {
      return i;
    }
  }
  return -1;
}

static bool
held_has_longer(actions_t *registry, keybind_t held)
{
  for (int i = 0; i < registry->count; ++i) {
    keybind_t part = action_gamepad_part(&registry->items[i]);

    if (part.count > held.count && held_subset(held, part)) {
      return true;
    }
  }
  return false;
}

static void
actions_send_on_held(actions_t *registry, bool replay)
{
  for (int i = 0; i < registry->held_events; ++i) {
    if (registry->replay) {
      registry->replay(replay);
    }
  }

  registry->held_events = 0;
}

static void
actions_gesture_abort(actions_t *registry)
{
  actions_send_on_held(registry, true);
  actions_gesture_reset(registry);
}

static void
actions_fired_release(actions_t *registry)
{
  for (int i = 0; i < registry->count && registry->fired_part.count > 0; ++i) {
    action_t *a = &registry->items[i];

    if (keybind_equal(action_gamepad_part(a), registry->fired_part)) {
      a->down           = false;
      a->released_frame = registry->frame + 1;
    }
  }

  actions_gesture_reset(registry);
}

static actions_event_result_t
actions_on_key_down(actions_t *registry, input_event_t *ev)
{
  if (ev->is_repeat) {
    bool hidden = registry->active && held_contains(registry->held, ev->key) && !held_contains(registry->passed, ev->key);
    return hidden ? ACTIONS_EVENT_CONSUME : ACTIONS_EVENT_PASS;
  }

  if (registry->fired) {
    return ACTIONS_EVENT_PASS;
  }

  keybind_t candidate = registry->active ? registry->held : KEYBIND_NULL;

  if (candidate.count >= KEYBIND_MAX_KEYS) {
    actions_gesture_abort(registry);
    return ACTIONS_EVENT_PASS;
  }

  candidate.keys[candidate.count++] = ev->key;

  if (!held_fits(registry, candidate)) {
    if (registry->active) {
      actions_gesture_abort(registry);
    }
    return ACTIONS_EVENT_PASS;
  }

  int match = held_match(registry, candidate);

  if (!registry->active || match != registry->match) {
    registry->match_since_us = time_now_us();
  }

  registry->active = true;
  registry->held   = candidate;
  registry->match  = match;

  /* a chord fires at once; between frames, so its press shows next frame */
  if (match >= 0 && candidate.count >= 2 && !held_has_longer(registry, candidate)) {
    actions_fire(registry, registry->frame + 1);
    return ACTIONS_EVENT_CONSUME;
  }

  /* a press the game needs on the beat is not held back; the combo still completes */
  if (registry->can_hold && !registry->can_hold(ev->key)) {
    registry->passed.keys[registry->passed.count++] = ev->key;
    return ACTIONS_EVENT_PASS;
  }

  registry->held_events += 1;
  return ACTIONS_EVENT_HOLD;
}

static actions_event_result_t
actions_on_key_up(actions_t *registry, input_event_t *ev)
{
  if (!registry->active || !held_contains(registry->held, ev->key)) {
    return ACTIONS_EVENT_PASS;
  }

  if (!registry->fired) {
    actions_gesture_abort(registry);
    return ACTIONS_EVENT_PASS;
  }

  /* the buttons of a fired part stay hidden until all are up, except the ones the game got */
  bool      passed = held_contains(registry->passed, ev->key);
  keybind_t rest   = KEYBIND_NULL;

  for (int i = 0; i < registry->held.count; ++i) {
    if (registry->held.keys[i] != ev->key) {
      rest.keys[rest.count++] = registry->held.keys[i];
    }
  }

  registry->held = rest;

  if (rest.count == 0) {
    actions_fired_release(registry);
  }
  return passed ? ACTIONS_EVENT_PASS : ACTIONS_EVENT_CONSUME;
}

actions_event_result_t
actions_on_input_event(actions_t *registry, input_event_t *ev)
{
  if (ev->kind == INPUT_EVENT_APP_ACTIVATION && !ev->app_activated) {
    actions_send_on_held(registry, false);
    actions_fired_release(registry);
    return ACTIONS_EVENT_PASS;
  }

  if (!input_key_is_gamepad(ev->key)) {
    return ACTIONS_EVENT_PASS;
  }

  if (ev->kind == INPUT_EVENT_KEY_DOWN) {
    return actions_on_key_down(registry, ev);
  }

  if (ev->kind == INPUT_EVENT_KEY_UP) {
    return actions_on_key_up(registry, ev);
  }
  return ACTIONS_EVENT_PASS;
}

/* every action of the matching part fires */
static void
actions_fire(actions_t *registry, uint64_t pressed_frame)
{
  actions_send_on_held(registry, false);
  registry->fired      = true;
  registry->fired_part = action_gamepad_part(&registry->items[registry->match]);

  for (int i = 0; i < registry->count; ++i) {
    action_t *a = &registry->items[i];

    if (!keybind_equal(action_gamepad_part(a), registry->fired_part)) {
      continue;
    }

    a->down          = true;
    a->pressed_frame = pressed_frame;

    if (a->on_fire) {
      a->on_fire(a->user);
    }
  }

  if (registry->dispatch) {
    input_event_t ev = {
      .kind = INPUT_EVENT_KEY_DOWN,
      .key  = registry->held.keys[0],
    };

    registry->dispatching = true;
    registry->dispatch(&ev);
    registry->dispatching = false;
  }
}

void
actions_tick(actions_t *registry)
{
  registry->frame += 1;

  if (!registry->active || registry->fired || time_now_us() - registry->match_since_us < (uint64_t)registry->hold_ms * 1000) {
    return;
  }

  /* held for the hold time: a single-button bind fires, otherwise no combo followed and the buttons go on to the game */
  if (registry->match >= 0) {
    actions_fire(registry, registry->frame);
  } else {
    actions_gesture_abort(registry);
  }
}

static action_t *
actions_find_gamepad(actions_t *registry, keybind_t gamepad_part)
{
  if (gamepad_part.count == 0) {
    return NULL;
  }

  for (int i = 0; i < registry->count; ++i) {
    action_t *a = &registry->items[i];

    if (a->bind && keybind_equal(keybind_gamepad_part(*a->bind), gamepad_part)) {
      return a;
    }
  }
  return NULL;
}

bool
actions_gamepad_state(actions_t *registry, keybind_t gamepad_part, bool *down, bool *pressed, bool *released)
{
  action_t *a = actions_find_gamepad(registry, gamepad_part);

  if (!a) {
    return false;
  }

  *down     = a->down;
  *pressed  = a->pressed_frame == registry->frame && a->pressed_frame != 0;
  *released = a->released_frame == registry->frame && a->released_frame != 0;
  return true;
}

bool
actions_is_registered_gamepad(actions_t *registry, keybind_t gamepad_part)
{
  return actions_find_gamepad(registry, gamepad_part) != NULL;
}

bool
actions_gamepad_firing(actions_t *registry, keybind_t gamepad_part)
{
  return registry->dispatching && gamepad_part.count > 0 && keybind_equal(registry->fired_part, gamepad_part);
}
