#ifndef UI_CONSOLE_H
#define UI_CONSOLE_H

#include "input.h"
#include "log.h"
#include "mod_manager.h"
#include "str.h"
#include "types.h"
#include "ui_nuklear.h"

#include "vendor_nuklear.h"

typedef int ui_console_log_level_t;
enum {
  UI_CONSOLE_LOG_DEBUG,
  UI_CONSOLE_LOG_INFO,
  UI_CONSOLE_LOG_WARN,
  UI_CONSOLE_LOG_ERROR,

  UI_CONSOLE_LOG_LEVEL_MAX,
};

typedef int ui_console_position_t;
enum {
  UI_CONSOLE_POSITION_BOTTOM,
  UI_CONSOLE_POSITION_TOP,
  UI_CONSOLE_POSITION_MAX,
};

static inline ui_console_log_level_t
ui_console_level_from_log(log_level_t level)
{
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return UI_CONSOLE_LOG_DEBUG;
  case LOG_LEVEL_INFO:
    return UI_CONSOLE_LOG_INFO;
  case LOG_LEVEL_WARN:
    return UI_CONSOLE_LOG_WARN;
  case LOG_LEVEL_ERROR:
    return UI_CONSOLE_LOG_ERROR;
  default:
    return UI_CONSOLE_LOG_INFO;
  }
}

str_array_t
ui_console_log_level_str_array(void);
str_array_t
ui_console_position_str_array(void);

typedef struct ui_console_line_s ui_console_line_t;
struct ui_console_line_s {
  str_t                  text;
  ui_console_log_level_t level;
  float                  row_w;
};

typedef struct ui_console_drag_state_s ui_console_drag_state_t;
struct ui_console_drag_state_s {
  float height_ratio;
  bool  dragging;
  bool  hovering;
  float drag_offset_y;
};

typedef struct ui_console_cfg_s ui_console_cfg_t;
struct ui_console_cfg_s {
  keybind_t              toggle_bind;
  bool                   auto_scroll;
  ui_console_position_t  position;
  ui_console_log_level_t min_level;
};

typedef struct ui_console_s ui_console_t;
struct ui_console_s {
  ui_console_line_t *lines;
  uint8_t           *line_text_storage;
  int                line_count;
  int                max_lines;
  int                max_line_len;

  uint8_t *input_buf;
  int      input_max;
  str_t    input;

  str_t   *history;
  uint8_t *history_storage;
  int      history_count;
  int      history_max;
  int      history_pos; // -1 = not browsing

  struct nk_scroll scroll;
  bool             scroll_to_bottom;

  ui_console_cfg_t cfg;
  ui_console_cfg_t saved_cfg;

  ui_console_drag_state_t drag;
  bool                    focus_input;
  bool                    inited;
  bool                    closed;
};

void
ui_console_init(ui_console_t *console, arena_t *arena);
void
ui_console_logv(ui_console_t *console, ui_console_log_level_t level, const char *fmt, va_list ap);
void
ui_console_draw(ui_console_t      *console,
                struct nk_context *ctx,
                mod_manager_t     *mod_manager,
                unsigned int       viewport_width,
                unsigned int       viewport_height);

void
ui_console_toggle(ui_console_t *console);
void
ui_console_open(ui_console_t *console);
void
ui_console_close(ui_console_t *console);
bool
ui_console_is_open(ui_console_t *console);

bool
ui_console_load_cfg(ui_console_t *console, str_list_t lines);
void
ui_console_commit_cfg(ui_console_t *console);
void
ui_console_save_cfg(ui_console_t *console, arena_t *arena, str_list_t *lines);
bool
ui_console_cfg_is_dirty(ui_console_t *console);

#endif /* NK_CONSOLE_H */
