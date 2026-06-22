#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "input.h"
#include "mod_manager.h"
#include "ui_console.h"
#include "ui_keybind_capture.h"
#include "ui_mod_manager.h"

#include "vendor_nuklear.h"

typedef struct ui_manager_s ui_manager_t;
struct ui_manager_s {
  struct nk_context *ctx;
  struct nk_color    color_table[NK_COLOR_COUNT];

  struct nk_font *font_body;
  struct nk_font *font_title;

  bool inited;

  bool     input_begun;
  uint64_t input_frame;

  uint32_t mouse_button_mask;
  bool     block_keyboard_when_open;
  bool     block_gamepad_when_open;

  unsigned int vw;
  unsigned int vh;

  ui_keybind_capture_t keybind_capture;
  ui_console_t         console;
  ui_mod_manager_t     main; // NOTE: did't come up with a better name
  mod_manager_t       *mod_manager;
};

void
ui_manager_preinit(ui_manager_t *manager, mod_manager_t *mod_manager, arena_t *arena);
void
ui_manager_init(ui_manager_t *manager, struct nk_context *ctx, struct nk_font *font_body, struct nk_font *font_title, unsigned int vw, unsigned int vh);
void
ui_manager_shutdown(ui_manager_t *manager);

void
ui_manager_console_toggle(ui_manager_t *manager);
void
ui_manager_console_open(ui_manager_t *manager);
void
ui_manager_console_close(ui_manager_t *manager);
bool
ui_manager_console_is_open(ui_manager_t *manager);

void
ui_manager_main_toggle(ui_manager_t *manager);
void
ui_manager_main_open(ui_manager_t *manager);
void
ui_manager_main_close(ui_manager_t *manager);
bool
ui_manager_main_is_open(ui_manager_t *manager);

bool
ui_manager_keybind_capture_is_open(ui_manager_t *manager);

bool
ui_manager_on_input_event_pre(ui_manager_t *manager, uint64_t frame_counter, input_event_t *ev);
bool
ui_manager_on_input_event_post(ui_manager_t *manager, input_event_t *ev);
void
ui_manager_on_frame_begin(ui_manager_t *manager, uint64_t frame_counter);
void
ui_manager_on_frame_end(ui_manager_t *manager, unsigned int viewport_width, unsigned int viewport_height);

bool
ui_manager_should_show_cursor(ui_manager_t *manager);

#endif /* UI_MANAGER_H */
