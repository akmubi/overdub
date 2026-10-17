#ifndef ACTIONS_H
#define ACTIONS_H

#include "input.h"
#include "mod.h"
#include "str.h"
#include "types.h"

MOD_EXTERN_C_BEGIN

/* Keybind registry: the keybinds a user can change (Overdub's and the mods' manifest keybinds).
 * Binds may be shared; the mod manager shows the conflicts.
 *
 * A gamepad part is pressed in its order, from its first button (the leader). Its buttons are held
 * back from the game while a part may follow, except the ones the game needs at once. A part of two
 * or more buttons fires once all of them are down, unless a longer part starts with it. Others fire
 * after the hold time. A press that turns out not to be a bind is sent on to the game. */

typedef void (*action_fire_fn_t)(void *user);

typedef struct action_s action_t;

struct action_s {
  str_t            id;     // "overdub.mod_manager", "<mod id>.<option id>", "<mod id>.<blueprint>.spawn"
  str_t            label;
  str_t            source; // "Overdub" or the mod's name
  mod_handle_t     owner;  // MOD_HANDLE_INVALID for Overdub
  keybind_t       *bind;   // owned by its config
  action_fire_fn_t on_fire;
  void            *user;

  bool     down;
  uint64_t pressed_frame;
  uint64_t released_frame;
};

typedef uint8_t actions_event_result_t;

enum {
  ACTIONS_EVENT_PASS,
  ACTIONS_EVENT_CONSUME,
  ACTIONS_EVENT_HOLD, // dropped now, maybe sent on later by the replay callback
};

/* sends the oldest held-back key down event on to the game, or drops it */
typedef void (*actions_replay_fn_t)(bool replay);
typedef bool (*actions_can_hold_fn_t)(input_key_kind_t key);

/* gives the mods the key down event of a fired gamepad part */
typedef void (*actions_dispatch_fn_t)(input_event_t *ev);

typedef struct actions_s actions_t;

struct actions_s {
  action_t             *items;
  int                   count;
  actions_replay_fn_t   replay;
  actions_dispatch_fn_t dispatch;
  actions_can_hold_fn_t can_hold;
  uint32_t              hold_ms;
  uint64_t              frame;

  /* gamepad gesture */
  bool      active;
  bool      fired;
  keybind_t held;   // leader first
  keybind_t passed; // of those, the ones the game got at once
  int       held_events;
  int       match;
  keybind_t fired_part;
  bool      dispatching;
  uint64_t  match_since_us;
};

/* can_hold: whether a press may wait to see if it starts a combo; NULL: always */
void
actions_init(actions_t *registry, actions_replay_fn_t replay, actions_dispatch_fn_t dispatch, actions_can_hold_fn_t can_hold);
void
actions_set_hold_ms(actions_t *registry, uint32_t ms);
uint32_t
actions_get_hold_ms(actions_t *registry);

/* registering the same bind again updates its entry */
action_t *
actions_register(actions_t *registry, str_t id, str_t label, str_t source, mod_handle_t owner, keybind_t *bind, action_fire_fn_t on_fire, void *user);
void
actions_unregister(actions_t *registry, keybind_t *bind);

actions_event_result_t
actions_on_input_event(actions_t *registry, input_event_t *ev);

/* once a frame, before the mods tick */
void
actions_tick(actions_t *registry);

/* false when the gamepad part is not registered */
bool
actions_gamepad_state(actions_t *registry, keybind_t gamepad_part, bool *down, bool *pressed, bool *released);
bool
actions_is_registered_gamepad(actions_t *registry, keybind_t gamepad_part);
bool
actions_gamepad_firing(actions_t *registry, keybind_t gamepad_part);

MOD_EXTERN_C_END

#endif /* ACTIONS_H */
