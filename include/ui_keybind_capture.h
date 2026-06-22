#ifndef UI_KEYBIND_CAPTURE_H
#define UI_KEYBIND_CAPTURE_H

#include "input.h"
#include "types.h"

#include "vendor_nuklear.h"

typedef struct ui_keybind_capture_s ui_keybind_capture_t;
struct ui_keybind_capture_s {
  bool     active; // currently capturing
  bool     combo_active;
  uint64_t owner;

  keybind_t original;
  keybind_t live;
  keybind_t pending;

  bool      result_ready;
  bool      result_accepted;
  uint64_t  result_hash;
  keybind_t result;
};

bool
ui_keybind_capture_is_open(ui_keybind_capture_t *cap);
bool
ui_keybind_capture_input(ui_keybind_capture_t *cap, input_event_t *ev);
bool
ui_keybind_capture(ui_keybind_capture_t *cap, struct nk_context *ctx, str_t name, keybind_t *bind);
void
ui_keybind_capture_draw(ui_keybind_capture_t *cap,
                        struct nk_context    *ctx,
                        unsigned int          viewport_width,
                        unsigned int          viewport_height);

#endif /* UI_KEYBIND_CAPTURE_H */
