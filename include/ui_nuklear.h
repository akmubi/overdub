#ifndef UI_NUKLEAR_H
#define UI_NUKLEAR_H

#include "str.h"
#include "ui_keybind_capture.h"
#include "vendor_nuklear.h"

#if 0
#define UI_ITEM(C) nk_style_item_color(C)

/* Core surfaces */
#define UI_C_BG_WINDOW      nk_rgba( 18,  16,  15, 170)
#define UI_C_BG_PANEL       nk_rgba( 29,  25,  23, 245)
#define UI_C_BG_PANEL_ALT   nk_rgba( 38,  31,  27, 245)
#define UI_C_BG_HOVER       nk_rgba( 51,  39,  32, 250)
#define UI_C_BG_ACTIVE      nk_rgba( 64,  45,  34, 255)

/* Header */
#define UI_C_HEADER         nk_rgba( 82,  43,  34, 255)
#define UI_C_HEADER_HOVER   nk_rgba(101,  50,  37, 255)
#define UI_C_HEADER_ACTIVE  nk_rgba(124,  59,  40, 255)

/* Borders */
#define UI_C_BORDER         nk_rgba( 91,  71,  56, 220)
#define UI_C_BORDER_SOFT    nk_rgba( 69,  55,  47, 170)
#define UI_C_BORDER_FOCUS   nk_rgba(190, 118,  52, 245)

/* Text */
#define UI_C_TEXT           nk_rgba(239, 230, 214, 255)
#define UI_C_TEXT_MUTED     nk_rgba(191, 171, 149, 255)
#define UI_C_TEXT_DIM       nk_rgba(144, 127, 110, 255)
#define UI_C_TEXT_DISABLED  nk_rgba(102,  91,  82, 255)

/* Generic controls */
#define UI_C_CONTROL        nk_rgba( 43,  35,  31, 255)
#define UI_C_CONTROL_HOVER  nk_rgba( 57,  43,  36, 255)
#define UI_C_CONTROL_ACTIVE nk_rgba( 72,  50,  39, 255)

/* Main accent */
#define UI_C_ACCENT         nk_rgba(160,  68,  45, 255)
#define UI_C_ACCENT_HOVER   nk_rgba(191,  84,  52, 255)
#define UI_C_ACCENT_ACTIVE  nk_rgba(218, 156,  62, 255)

/* Selection */
#define UI_C_SELECT         nk_rgba( 78,  50,  38, 230)
#define UI_C_SELECT_HOVER   nk_rgba(103,  62,  43, 240)
#define UI_C_SELECT_ACTIVE  nk_rgba(131,  74,  45, 248)

/* Inputs */
#define UI_C_EDIT           nk_rgba( 20,  18,  17, 255)
#define UI_C_EDIT_HOVER     nk_rgba( 27,  24,  22, 255)
#define UI_C_EDIT_ACTIVE    nk_rgba( 35,  30,  27, 255)

/* Extra colors */
#define UI_C_BLACK          nk_rgba( 12,  10,   9, 255)
//#define UI_C_GREEN          nk_rgba( 80, 200, 120, 255)
//#define UI_C_RED            nk_rgba( 220, 80,  80, 255)
//#define UI_C_RED_HOVER      nk_rgba(170,  52,  41, 255)
#define UI_C_ORANGE         nk_rgba(203,  91,  43, 255)
#define UI_C_GOLD           nk_rgba(218, 156,  62, 255)
#define UI_C_GOLD_HOVER     nk_rgba(235, 181,  82, 255)

#define UI_C_GREEN         nk_rgba( 80, 200, 120, 255)
#define UI_C_GREEN_HOVER   nk_rgba( 96, 216, 136, 255)
#define UI_C_GREEN_TEXT    nk_rgba( 18,  16,  15, 255)
#define UI_C_GREEN_BORDER  nk_rgba( 45, 145,  78, 255)

#define UI_C_RED           nk_rgba(220,  80,  80, 255)
#define UI_C_RED_HOVER     nk_rgba(236,  96,  94, 255)
#define UI_C_RED_TEXT      nk_rgba( 18,  16,  15, 255)
#define UI_C_RED_BORDER    nk_rgba(166,  48,  48, 255)

#define UI_C_SLIDER_BAR     nk_rgba(218, 156,  62, 255)

#define UI_C_TRANSPARENT    nk_rgba(  0,   0,   0,   0)
#endif

#define UI_ITEM(C) nk_style_item_color(C)

/* Core surfaces */
#define UI_C_BG_WINDOW      nk_rgba( 17,  19,  23, 180)
#define UI_C_BG_PANEL       nk_rgba( 23,  27,  33, 248)
#define UI_C_BG_PANEL_ALT   nk_rgba( 29,  34,  41, 250)
#define UI_C_BG_HOVER       nk_rgba( 37,  44,  53, 252)
#define UI_C_BG_ACTIVE      nk_rgba( 46,  55,  67, 255)

/* Header */
#define UI_C_HEADER         nk_rgba( 30,  35,  43, 255)
#define UI_C_HEADER_HOVER   nk_rgba( 38,  45,  55, 255)
#define UI_C_HEADER_ACTIVE  nk_rgba( 47,  56,  68, 255)

/* Borders */
#define UI_C_BORDER         nk_rgba( 72,  84,  99, 220)
#define UI_C_BORDER_SOFT    nk_rgba( 54,  64,  77, 180)
#define UI_C_BORDER_FOCUS   nk_rgba(218, 105,  88, 245)

/* Text */
#define UI_C_TEXT           nk_rgba(235, 238, 243, 255)
#define UI_C_TEXT_MUTED     nk_rgba(177, 187, 199, 255)
#define UI_C_TEXT_DIM       nk_rgba(130, 142, 156, 255)
#define UI_C_TEXT_DISABLED  nk_rgba( 89,  99, 111, 255)

/* Generic controls */
#define UI_C_CONTROL        nk_rgba( 31,  37,  45, 255)
#define UI_C_CONTROL_HOVER  nk_rgba( 40,  48,  58, 255)
#define UI_C_CONTROL_ACTIVE nk_rgba( 50,  60,  72, 255)

/* Main accent */
#define UI_C_ACCENT         nk_rgba(202,  79,  68, 255)
#define UI_C_ACCENT_HOVER   nk_rgba(221,  95,  80, 255)
#define UI_C_ACCENT_ACTIVE  nk_rgba(235, 116,  92, 255)
#define UI_C_ACCENT_TEXT    nk_rgba( 20,  22,  26, 255)

/* Selection */
#define UI_C_SELECT         nk_rgba( 69,  45,  48, 235)
#define UI_C_SELECT_HOVER   nk_rgba( 89,  53,  53, 242)
#define UI_C_SELECT_ACTIVE  nk_rgba(112,  62,  58, 250)

/* Inputs */
#define UI_C_EDIT           nk_rgba( 14,  17,  21, 255)
#define UI_C_EDIT_HOVER     nk_rgba( 19,  23,  28, 255)
#define UI_C_EDIT_ACTIVE    nk_rgba( 24,  29,  35, 255)

/* Scrollbars */
#define UI_C_SCROLLBAR              nk_rgba( 19,  23,  29, 180)
#define UI_C_SCROLLBAR_HOVER        nk_rgba( 25,  30,  37, 215)
#define UI_C_SCROLLBAR_ACTIVE       nk_rgba( 31,  37,  45, 235)
#define UI_C_SCROLLBAR_CURSOR       nk_rgba( 73,  86, 102, 220)
#define UI_C_SCROLLBAR_CURSOR_HOVER nk_rgba( 91, 106, 125, 240)

/* Semantic colors */
#define UI_C_GREEN         nk_rgba( 74, 177, 116, 255)
#define UI_C_GREEN_HOVER   nk_rgba( 88, 199, 132, 255)
#define UI_C_GREEN_TEXT    nk_rgba( 15,  22,  18, 255)
#define UI_C_GREEN_BORDER  nk_rgba( 48, 127,  81, 255)

#define UI_C_RED           nk_rgba(210,  76,  80, 255)
#define UI_C_RED_HOVER     nk_rgba(229,  93,  96, 255)
#define UI_C_RED_TEXT      nk_rgba( 20,  20,  23, 255)
#define UI_C_RED_BORDER    nk_rgba(151,  48,  53, 255)

#define UI_C_ORANGE        nk_rgba(225, 128,  67, 255)
#define UI_C_GOLD          nk_rgba(231, 177,  83, 255)
#define UI_C_GOLD_HOVER    nk_rgba(243, 197, 105, 255)

#define UI_C_BLACK         nk_rgba( 10,  12,  15, 255)
#define UI_C_SLIDER_BAR    UI_C_ACCENT
#define UI_C_TRANSPARENT   nk_rgba(  0,   0,   0,   0)

static inline bool
ui_button_str(struct nk_context *ctx, str_t str)
{
  return nk_button_text(ctx, (const char *)str.data, (int)str.len);
}

static inline void
ui_label_str(struct nk_context *ctx, str_t str, nk_flags flags)
{
  nk_text(ctx, (const char *)str.data, (int)str.len, flags);
}

static inline void
ui_label_str_colored(struct nk_context *ctx, str_t str, nk_flags flags, struct nk_color color)
{
  nk_text_colored(ctx, (const char *)str.data, (int)str.len, flags, color);
}

static inline struct nk_color
ui_nk_style_item_color_or(struct nk_style_item item, struct nk_color fallback)
{
  if (item.type == NK_STYLE_ITEM_COLOR) {
    return item.data.color;
  }
  return fallback;
}

typedef struct ui_nk_item_s ui_nk_item_t;
struct ui_nk_item_s {
  struct nk_rect               bounds;
  enum nk_widget_layout_states layout_state;
  struct nk_input             *in;
  nk_flags                     widget_state;
};
bool
ui_nk_item_begin(struct nk_context *ctx, ui_nk_item_t *item);
bool
ui_nk_item_behavior(struct nk_context *ctx, ui_nk_item_t *item);
bool
ui_nk_selectable_behavior(struct nk_context *ctx, ui_nk_item_t *item);
struct nk_color
ui_nk_select_bg(struct nk_context *ctx, nk_flags state, bool selected);
struct nk_color
ui_nk_select_fg(struct nk_context *ctx, nk_flags state, bool selected);
bool
ui_nk_current_panel_accepts_input(struct nk_context *ctx);

bool
ui_text_fits_width(struct nk_context *ctx, str_t text, float max_w);
float
ui_text_width(struct nk_context *ctx, str_t text);

typedef struct ui_text_opts_s ui_text_opts_t;
struct ui_text_opts_s {
  float           text_width;
  nk_flags        text_alignment;
  struct nk_vec2  pad;
  struct nk_color bg;
  struct nk_color fg;
};
void
ui_text_draw(struct nk_context *ctx, struct nk_command_buffer *out, struct nk_rect bounds, str_t text, ui_text_opts_t opts);
void
ui_text(struct nk_context *ctx, str_t text, float text_width, nk_flags alignment, struct nk_color color);

enum ui_nk_axis_e {
  UI_NK_AXIS_X,
  UI_NK_AXIS_Y,
};
typedef enum ui_nk_axis_e ui_nk_axis_t;

typedef struct ui_nk_splitter_state_s ui_nk_splitter_state_t;
struct ui_nk_splitter_state_s {
  float ratio;
  bool  dragging;
  bool  hovering;
  float drag_offset;
};

typedef struct ui_nk_splitter_opts_s ui_nk_splitter_opts_t;
struct ui_nk_splitter_opts_s {
  ui_nk_axis_t axis;

  /*
    Screen-space track rect

    For vertical splitter:
      track.x = left/right split area x
      track.w = total usable split width

    For horizontal splitter:
      track.y = top/bottom split area y
      track.h = total usable split height
  */
  struct nk_rect track;

  float gap_before;
  float min_before;
  float min_after;

  float line_thickness;
  bool  disabled;
};

bool
ui_splitter(struct nk_context *ctx, ui_nk_splitter_state_t *state, ui_nk_splitter_opts_t *opts);

typedef struct ui_nk_select_row_opts_s ui_nk_select_row_opts_t;
struct ui_nk_select_row_opts_s {
  str_t           text;
  bool            selected;
  bool            show_dot;
  struct nk_color dot_color;
  float           pad_x;
  float           dot_d;
};

bool
ui_select_row(struct nk_context *ctx, ui_nk_select_row_opts_t *opts);

float
ui_label_wrap(struct nk_context *ctx, str_t text);
float
ui_label_wrap_colored(struct nk_context *ctx, str_t text, struct nk_color color);

void
ui_win_clamp_bounds(struct nk_context *ctx, const char *name, struct nk_vec2 min_size, struct nk_vec2 max_size);
void
ui_win_make_centered(struct nk_context *ctx, const char *name, unsigned int vw, unsigned int vh);

bool
ui_tree_push(struct nk_context *ctx, enum nk_tree_type type, str_t title, bool maximized);
bool
ui_tree_state_push(struct nk_context *ctx, enum nk_tree_type type, str_t title, bool *state);

typedef struct ui_text_span_s ui_text_span_t;
struct ui_text_span_s {
  str_t str;   // text to render
  float width; // cached text width
};

ui_text_span_t
ui_text_span_make(struct nk_context *ctx, str_t str);
void
ui_text_span_align(ui_text_span_t *spans, int count);

#define UI_TEXT_CELL(TEXT, COLOR, ...)                                \
  (ui_text_cell_t) {                                                  \
    .text = (TEXT), .fg = (COLOR), .align = NK_TEXT_LEFT, __VA_ARGS__ \
  }

typedef struct ui_text_cell_s ui_text_cell_t;
struct ui_text_cell_s {
  ui_text_span_t  text;
  struct nk_color fg;
  nk_flags        align;
};

void
ui_text_cell_draw(struct nk_context *ctx, struct nk_command_buffer *out, struct nk_rect bounds, ui_text_cell_t cell, struct nk_color bg);
void
ui_text_cell(struct nk_context *ctx, ui_text_cell_t cell);

#define UI_MAX_TEXT_COLS (8)

typedef struct ui_text_cols_s ui_text_cols_t;
struct ui_text_cols_s {
  float width[UI_MAX_TEXT_COLS];
  int   count;
};

static inline void
ui_text_cols_reset(ui_text_cols_t *cols, int count)
{
  ASSERT(cols != NULL);
  ASSERT(count >= 0);
  ASSERT(count <= UI_MAX_TEXT_COLS);

  mem_zero(cols, sizeof(*cols));
  cols->count = count;
}

static inline void
ui_text_cols_include(ui_text_cols_t *cols, int col, ui_text_span_t text)
{
  ASSERT(cols != NULL);
  ASSERT(col >= 0);
  ASSERT(col < cols->count);
  ASSERT(col < UI_MAX_TEXT_COLS);

  cols->width[col] = MAX_VAL(cols->width[col], text.width);
}

#endif /* UI_NUKLEAR_H */
