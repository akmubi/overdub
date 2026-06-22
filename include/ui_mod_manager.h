#ifndef UI_MOD_MANAGER_H
#define UI_MOD_MANAGER_H

#include "input.h"
#include "mod_manager.h"
#include "ui_keybind_capture.h"
#include "ui_nuklear.h"

typedef int ui_dialog_kind_t;
enum {
  UI_DIALOG_NONE = 0,
  UI_DIALOG_CONFIG,
  UI_DIALOG_REORDER,
};

typedef struct ui_dialog_window_s ui_dialog_window_t;
struct ui_dialog_window_s {
  struct nk_rect bounds;
  bool           inited;
};

typedef struct ui_dialog_state_s ui_dialog_state_t;
struct ui_dialog_state_s {
  ui_dialog_kind_t   kind;
  ui_dialog_window_t config_win;
  ui_dialog_window_t reorder_win;
};

typedef struct ui_reorder_state_s ui_reorder_state_t;
struct ui_reorder_state_s {
  mod_handle_t order[CONFIG_MOD_MANAGER_MAX_MODS];
  int          order_count;
  int          selected_idx;
  bool         dirty;
};

typedef struct ui_inspector_state_s ui_inspector_state_t;
struct ui_inspector_state_s {
  enum nk_collapse_states info_open;
  enum nk_collapse_states code_open;
  enum nk_collapse_states code_dll_info_open;
  enum nk_collapse_states code_runtime_open;

  enum nk_collapse_states blueprints_open;
  enum nk_collapse_states blueprints_info_open;
  enum nk_collapse_states blueprints_runtime_open;

  enum nk_collapse_states assets_open;
  enum nk_collapse_states assets_info_open;
  enum nk_collapse_states assets_runtime_open;

  enum nk_collapse_states options_open;
  enum nk_collapse_states options_code_open;
  enum nk_collapse_states options_blueprints_open;
  enum nk_collapse_states options_assets_open;

  enum nk_collapse_states config_manager;
  enum nk_collapse_states config_console;
};

typedef struct ui_mod_manager_s ui_mod_manager_t;
struct ui_mod_manager_s {
  ui_keybind_capture_t  *keybind_capture;
  ui_nk_splitter_state_t split;
  ui_dialog_state_t      dialog;
  ui_reorder_state_t     reorder;
  ui_inspector_state_t   inspector;
  mod_handle_t           selected;
  struct nk_font        *font_body;
  struct nk_font        *font_title;
  struct nk_rect         bounds;
  bool                   inited;
  bool                   closed;
  unsigned int           vw;
  unsigned int           vh;
};

void
ui_mod_manager_init(ui_mod_manager_t     *ui,
                    ui_keybind_capture_t *cap,
                    struct nk_font       *font_body,
                    struct nk_font       *font_title);

void
ui_mod_manager_draw(ui_mod_manager_t  *ui,
                    struct nk_context *ctx,
                    mod_manager_t     *manager,
                    unsigned int       viewport_width,
                    unsigned int       viewport_height);

void
ui_mod_manager_toggle(ui_mod_manager_t *ui);
void
ui_mod_manager_open(ui_mod_manager_t *ui);
void
ui_mod_manager_close(ui_mod_manager_t *ui);
bool
ui_mod_manager_is_open(ui_mod_manager_t *ui);

#endif /* UI_MOD_MANAGER_H */
