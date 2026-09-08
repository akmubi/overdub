#include "lua_overdub_ui.h"

#include "config.h"
#include "globals.h"
#include "log.h"
#include "lua_overdub.h"
#include "scratch.h"
#include "ui_keybind_capture.h"
#include "ui_nuklear.h"

#include "lua/src/lauxlib.h"
#include "lua/src/lua.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define LUA_OVERDUB_UI_WINDOW_META   "overdub.ui.Window"
#define LUA_OVERDUB_UI_CONTEXT_META  "overdub.ui.Context"
#define LUA_OVERDUB_UI_ID_CAP        (128)
#define LUA_OVERDUB_UI_TITLE_CAP     (256)
#define LUA_OVERDUB_UI_NAME_CAP      (384)
#define LUA_OVERDUB_UI_TEXT_CAP      (4096)
#define LUA_OVERDUB_UI_COMBO_CAP     (256)
#define LUA_OVERDUB_UI_KEYBIND_CAP   (256)
#define LUA_OVERDUB_UI_WIDGET_ID_CAP (768)

typedef struct lua_overdub_ui_grid_state_s lua_overdub_ui_grid_state_t;
struct lua_overdub_ui_grid_state_s {
  struct nk_grid_state state;
  nk_hash              scope;
  size_t               key_len;
  char                 key[LUA_OVERDUB_UI_ID_CAP];
};

typedef struct lua_overdub_ui_window_s lua_overdub_ui_window_t;
struct lua_overdub_ui_window_s {
  uint64_t       id;
  int            callback_ref;
  int            handle_ref;
  struct nk_rect bounds;
  size_t         public_id_len;
  char           public_id[LUA_OVERDUB_UI_ID_CAP];
  char           name[LUA_OVERDUB_UI_NAME_CAP];
  char           title[LUA_OVERDUB_UI_TITLE_CAP];
  bool           active;
  bool           open;
  bool           created;
  bool           show_requested;
  bool           drawing;
  bool           remove_requested;
};

typedef struct lua_overdub_ui_window_handle_s lua_overdub_ui_window_handle_t;
struct lua_overdub_ui_window_handle_s {
  lua_overdub_ui_context_t *context;
  uint64_t                  id;
};

typedef struct lua_overdub_ui_draw_context_s lua_overdub_ui_draw_context_t;
struct lua_overdub_ui_draw_context_s {
  lua_overdub_ui_context_t *context;
  struct nk_context        *nk;
  struct nk_grid           *grid;
  uint64_t                  token;
  int                       grid_column;
  int                       row_remaining;
  float                     row_values[NK_GRID_MAX_COLUMNS];
  bool                      grid_row_active;
  bool                      valid;
};

struct lua_overdub_ui_context_s {
  lua_runtime_t              *runtime;
  str_t                       mod_id;
  uint32_t                    owner_thread_id;
  uint64_t                    next_window_id;
  uint64_t                    draw_token;
  unsigned int                viewport_width;
  unsigned int                viewport_height;
  struct nk_context          *nk;
  lua_overdub_ui_window_t     windows[CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS];
  lua_overdub_ui_grid_state_t grids[CONFIG_LUA_OVERDUB_MAX_UI_GRIDS];
  bool                        drawing;
};

static char g_lua_overdub_ui_context_key;

static bool
lua_overdub_ui_name_valid(const char *value, size_t len, size_t cap);

static bool
lua_overdub_ui_ref_valid(int ref)
{
  return ref != LUA_NOREF && ref != LUA_REFNIL;
}

static bool
lua_overdub_ui_on_owner_thread(lua_overdub_ui_context_t *context)
{
  if (!context) {
    return false;
  }

  if (context->owner_thread_id == 0) {
    return false;
  }

  return context->owner_thread_id == thread_current_id();
}

static bool
lua_overdub_ui_draw_begin(lua_overdub_ui_context_t *ui, struct nk_context *nk, unsigned int vw, unsigned int vh)
{
  if (!ui || !nk || ui->drawing || !lua_overdub_ui_on_owner_thread(ui)) {
    return false;
  }

  ui->nk              = nk;
  ui->viewport_width  = vw;
  ui->viewport_height = vh;
  ui->drawing         = true;
  return true;
}

static void
lua_overdub_ui_draw_end(lua_overdub_ui_context_t *ui)
{
  if (!ui) {
    return;
  }

  ui->draw_token += 1;
  ui->drawing     = false;
  ui->nk          = NULL;
}

static void
lua_overdub_ui_push_draw_context(lua_overdub_ui_context_t *ui)
{
  lua_State *state = ui->runtime->state;
  ui->draw_token += 1;

  lua_overdub_ui_draw_context_t *draw = lua_newuserdatauv(state, sizeof(*draw), 0);
  *draw = (lua_overdub_ui_draw_context_t){
    .context = ui,
    .nk      = ui->nk,
    .token   = ui->draw_token,
    .valid   = true,
  };
  luaL_setmetatable(state, LUA_OVERDUB_UI_CONTEXT_META);
}

static lua_overdub_ui_context_t *
lua_overdub_ui_context_get(lua_State *state)
{
  lua_pushlightuserdata(state, &g_lua_overdub_ui_context_key);
  lua_rawget(state, LUA_REGISTRYINDEX);
  lua_overdub_ui_context_t *context = lua_touserdata(state, -1);
  lua_pop(state, 1);
  return context;
}

static lua_overdub_ui_context_t *
lua_overdub_ui_context_check(lua_State *state)
{
  lua_overdub_context_check(state);

  lua_overdub_ui_context_t *context = lua_overdub_ui_context_get(state);
  if (!context) {
    luaL_error(state, "Overdub UI context is unavailable");
  }

  return context;
}

static lua_overdub_ui_window_t *
lua_overdub_ui_window_find(lua_overdub_ui_context_t *context, uint64_t id)
{
  if (!context || id == 0) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS; ++i) {
    lua_overdub_ui_window_t *window = &context->windows[i];
    if (window->active && window->id == id) {
      return window;
    }
  }

  return NULL;
}

static lua_overdub_ui_window_t *
lua_overdub_ui_window_find_public_id(lua_overdub_ui_context_t *context, const char *id, size_t id_len)
{
  if (!context || !id || id_len == 0) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS; ++i) {
    lua_overdub_ui_window_t *window = &context->windows[i];
    if (!window->active || window->public_id_len != id_len) {
      continue;
    }

    if (memcmp(window->public_id, id, id_len) == 0) {
      return window;
    }
  }

  return NULL;
}

static lua_overdub_ui_window_t *
lua_overdub_ui_window_find_free(lua_overdub_ui_context_t *context, int *out_index)
{
  if (!context) {
    return NULL;
  }

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS; ++i) {
    if (context->windows[i].active) {
      continue;
    }

    if (out_index) {
      *out_index = i;
    }
    return &context->windows[i];
  }

  return NULL;
}

static void
lua_overdub_ui_window_release(lua_overdub_ui_context_t *context, lua_overdub_ui_window_t *window)
{
  if (!context || !window || !window->active) {
    return;
  }

  lua_State *state = context->runtime->state;
  if (lua_overdub_ui_ref_valid(window->handle_ref)) {
    luaL_unref(state, LUA_REGISTRYINDEX, window->handle_ref);
  }

  if (lua_overdub_ui_ref_valid(window->callback_ref)) {
    luaL_unref(state, LUA_REGISTRYINDEX, window->callback_ref);
  }

  *window = (lua_overdub_ui_window_t){
    .callback_ref = LUA_NOREF,
    .handle_ref   = LUA_NOREF,
  };
}

static void
lua_overdub_ui_window_invalidate_handle(lua_overdub_ui_context_t *context, lua_overdub_ui_window_t *window)
{
  if (!context || !window) {
    return;
  }

  if (!lua_overdub_ui_ref_valid(window->handle_ref)) {
    return;
  }

  lua_State *state = context->runtime->state;
  lua_rawgeti(state, LUA_REGISTRYINDEX, window->handle_ref);

  lua_overdub_ui_window_handle_t *handle = luaL_testudata(state, -1, LUA_OVERDUB_UI_WINDOW_META);
  if (handle && handle->id == window->id) {
    handle->context = NULL;
  }

  lua_pop(state, 1);
}

static lua_overdub_ui_window_handle_t *
lua_overdub_ui_window_handle_check(lua_State *state)
{
  lua_overdub_ui_window_handle_t *handle = luaL_checkudata(state, 1, LUA_OVERDUB_UI_WINDOW_META);

  lua_overdub_ui_context_t *context = lua_overdub_ui_context_check(state);
  if (!handle->context || handle->context != context) {
    luaL_error(state, "UI Window handle is invalid");
  }

  return handle;
}

static bool
lua_overdub_ui_window_handle_active(lua_overdub_ui_window_handle_t *handle)
{
  lua_overdub_ui_window_t *window = lua_overdub_ui_window_find(handle->context, handle->id);
  return window && !window->remove_requested;
}

static int
lua_overdub_ui_window_remove_lua(lua_State *state)
{
  lua_overdub_ui_window_handle_t *handle = lua_overdub_ui_window_handle_check(state);
  lua_overdub_ui_window_t        *window = lua_overdub_ui_window_find(handle->context, handle->id);
  if (!window || window->remove_requested) {
    lua_pushboolean(state, false);
    return 1;
  }

  if (window->drawing) {
    window->remove_requested = true;
  } else {
    lua_overdub_ui_window_release(handle->context, window);
  }

  lua_pushboolean(state, true);
  return 1;
}

static int
lua_overdub_ui_window_is_active_lua(lua_State *state)
{
  lua_overdub_ui_window_handle_t *handle = lua_overdub_ui_window_handle_check(state);
  lua_pushboolean(state, lua_overdub_ui_window_handle_active(handle));
  return 1;
}

static int
lua_overdub_ui_window_set_open_lua(lua_State *state)
{
  lua_overdub_ui_window_handle_t *handle = lua_overdub_ui_window_handle_check(state);
  lua_overdub_ui_window_t        *window = lua_overdub_ui_window_find(handle->context, handle->id);
  if (!window || window->remove_requested) {
    lua_pushboolean(state, false);
    return 1;
  }

  bool open = lua_toboolean(state, 2);
  if (open && !window->open) {
    window->show_requested = true;
  }
  window->open = open;

  lua_pushboolean(state, true);
  return 1;
}

static int
lua_overdub_ui_window_is_open_lua(lua_State *state)
{
  lua_overdub_ui_window_handle_t *handle = lua_overdub_ui_window_handle_check(state);
  lua_overdub_ui_window_t        *window = lua_overdub_ui_window_find(handle->context, handle->id);

  bool open = false;
  if (window && !window->remove_requested) {
    open = window->open;
  }

  lua_pushboolean(state, open);
  return 1;
}

static int
lua_overdub_ui_window_gc_lua(lua_State *state)
{
  lua_overdub_ui_window_handle_t *handle = luaL_checkudata(state, 1, LUA_OVERDUB_UI_WINDOW_META);
  handle->context = NULL;
  handle->id      = 0;
  return 0;
}

static lua_overdub_ui_draw_context_t *
lua_overdub_ui_draw_context_check(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw    = luaL_checkudata(state, 1, LUA_OVERDUB_UI_CONTEXT_META);
  lua_overdub_ui_context_t      *context = lua_overdub_ui_context_check(state);

  if (!draw->valid || draw->context != context) {
    luaL_error(state, "UI context is only valid during its draw callback");
  }

  if (!context->drawing || context->draw_token != draw->token) {
    luaL_error(state, "UI context is only valid during its draw callback");
  }

  if (!draw->nk || draw->nk != context->nk) {
    luaL_error(state, "UI context is only valid during its draw callback");
  }

  return draw;
}

static void
lua_overdub_ui_grid_end_row(lua_overdub_ui_draw_context_t *draw)
{
  if (draw->grid && draw->grid_row_active) {
    nk_grid_row_end(draw->grid);
    draw->grid_row_active = false;
    draw->grid_column     = 0;
  }
}

static void
lua_overdub_ui_grid_begin_row(lua_overdub_ui_draw_context_t *draw)
{
  if (draw->grid && !draw->grid_row_active) {
    nk_grid_row_begin(draw->grid);
    draw->grid_row_active = true;
    draw->grid_column     = 0;
  }
}

static void
lua_overdub_ui_prepare_widget_size(lua_overdub_ui_draw_context_t *draw, float width, float height)
{
  if (draw->grid) {
    if (draw->grid_row_active && draw->grid_column >= draw->grid->column_count) {
      lua_overdub_ui_grid_end_row(draw);
    }

    lua_overdub_ui_grid_begin_row(draw);
    nk_grid_push(draw->grid, draw->grid_column, nk_vec2(width, height));
    draw->grid_column += 1;
    return;
  }

  if (draw->row_remaining > 0) {
    draw->row_remaining -= 1;
    return;
  }

  nk_layout_row_dynamic(draw->nk, height, 1);
}

static void
lua_overdub_ui_prepare_widget(lua_overdub_ui_draw_context_t *draw, float height)
{
  lua_overdub_ui_prepare_widget_size(draw, 0.0f, height);
}

static float
lua_overdub_ui_text_width(lua_overdub_ui_draw_context_t *draw, const char *text, size_t text_len)
{
  return nk_text_width(draw->nk, draw->nk->style.font, draw->nk->style.font_size, text, (int)text_len);
}

static bool
lua_overdub_ui_scoped_name(lua_overdub_ui_draw_context_t *draw, const char *kind, const char *id, size_t id_len, char *name, size_t name_cap)
{
  nk_hash scope    = draw->nk->current->layout->text_selection_scope;
  str_t   mod_id   = draw->context->mod_id;
  int     name_len = snprintf(name, name_cap, "%.*s::%08X::%s::%.*s", STR_ARG(mod_id), (unsigned int)scope, kind, (int)id_len, id);
  return name_len >= 0 && (size_t)name_len < name_cap;
}

static void
lua_overdub_ui_prepare_labeled_control(lua_overdub_ui_draw_context_t *draw, const char *label, size_t label_len)
{
  if (draw->grid) {
    float label_width = lua_overdub_ui_text_width(draw, label, label_len);
    lua_overdub_ui_prepare_widget_size(draw, label_width, draw->nk->style.font_size);
    nk_text(draw->nk, label, (int)label_len, NK_TEXT_LEFT);
    lua_overdub_ui_prepare_widget_size(draw, 120.0f, 24.0f);
    return;
  }

  draw->row_remaining = 0;
  nk_layout_row_dynamic(draw->nk, 24.0f, 2);
  nk_text(draw->nk, label, (int)label_len, NK_TEXT_LEFT);
}

static nk_flags
lua_overdub_ui_text_alignment(lua_State *state, int arg)
{
  if (lua_isnoneornil(state, arg)) {
    return NK_TEXT_LEFT;
  }

  size_t      alignment_len = 0;
  const char *alignment     = luaL_checklstring(state, arg, &alignment_len);
  if (alignment_len == sizeof("Left") - 1 && memcmp(alignment, "Left", alignment_len) == 0) {
    return NK_TEXT_LEFT;
  }

  if (alignment_len == sizeof("Center") - 1 && memcmp(alignment, "Center", alignment_len) == 0) {
    return NK_TEXT_CENTERED;
  }

  if (alignment_len == sizeof("Right") - 1 && memcmp(alignment, "Right", alignment_len) == 0) {
    return NK_TEXT_RIGHT;
  }

  luaL_argerror(state, arg, "alignment must be 'Left', 'Center', or 'Right'");
  return NK_TEXT_LEFT;
}

static int
lua_overdub_ui_text_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      text_len = 0;
  const char *text     = luaL_checklstring(state, 2, &text_len);
  if (text_len > INT_MAX) {
    return luaL_argerror(state, 2, "text is too long");
  }

  nk_flags alignment = lua_overdub_ui_text_alignment(state, 3);
  float    width     = lua_overdub_ui_text_width(draw, text, text_len);
  lua_overdub_ui_prepare_widget_size(draw, width, 22.0f);
  nk_text(draw->nk, text, (int)text_len, alignment);
  return 0;
}

static int
lua_overdub_ui_wrapped_text_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      text_len = 0;
  const char *text     = luaL_checklstring(state, 2, &text_len);
  if (text_len > INT_MAX) {
    return luaL_argerror(state, 2, "text is too long");
  }

  lua_Number requested_height = luaL_optnumber(state, 3, 48.0);
  if (!isfinite((double)requested_height) || requested_height <= 0.0 || requested_height > 4000.0) {
    return luaL_argerror(state, 3, "height must be finite and between 0 and 4000");
  }

  lua_overdub_ui_prepare_widget(draw, (float)requested_height);
  nk_text_wrap(draw->nk, text, (int)text_len);
  return 0;
}

static int
lua_overdub_ui_button_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "button label is too long");
  }

  float width = lua_overdub_ui_text_width(draw, label, label_len) + draw->nk->style.button.padding.x * 2.0f;
  lua_overdub_ui_prepare_widget_size(draw, width, 24.0f);
  bool pressed = nk_button_text(draw->nk, label, (int)label_len);
  lua_pushboolean(state, pressed);
  return 1;
}

static int
lua_overdub_ui_checkbox_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "checkbox label is too long");
  }

  int value = lua_toboolean(state, 3);
  float width = lua_overdub_ui_text_width(draw, label, label_len) + draw->nk->style.font_size + draw->nk->style.checkbox.spacing;
  lua_overdub_ui_prepare_widget_size(draw, width, 24.0f);
  nk_checkbox_text(draw->nk, label, (int)label_len, &value);
  lua_pushboolean(state, value);
  return 1;
}

static bool
lua_overdub_ui_integer_fits_int(lua_Integer value)
{
  return value >= INT_MIN && value <= INT_MAX;
}

static int
lua_overdub_ui_slider_int_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "slider label is too long");
  }

  lua_Integer value_arg = luaL_checkinteger(state, 3);
  lua_Integer min_arg   = luaL_checkinteger(state, 4);
  lua_Integer max_arg   = luaL_checkinteger(state, 5);
  lua_Integer step_arg  = luaL_optinteger(state, 6, 1);

  if (!lua_overdub_ui_integer_fits_int(value_arg)) {
    return luaL_argerror(state, 3, "value is outside the supported integer range");
  }

  if (!lua_overdub_ui_integer_fits_int(min_arg)) {
    return luaL_argerror(state, 4, "minimum is outside the supported integer range");
  }

  if (!lua_overdub_ui_integer_fits_int(max_arg)) {
    return luaL_argerror(state, 5, "maximum is outside the supported integer range");
  }

  if (!lua_overdub_ui_integer_fits_int(step_arg) || step_arg <= 0) {
    return luaL_argerror(state, 6, "step must be a positive supported integer");
  }

  if (min_arg > max_arg) {
    return luaL_error(state, "slider minimum cannot exceed its maximum");
  }

  int value = (int)value_arg;
  if (value < (int)min_arg) {
    value = (int)min_arg;
  }

  if (value > (int)max_arg) {
    value = (int)max_arg;
  }

  lua_overdub_ui_prepare_labeled_control(draw, label, label_len);
  nk_slider_int(draw->nk, (int)min_arg, &value, (int)max_arg, (int)step_arg);
  lua_pushinteger(state, value);
  return 1;
}

static bool
lua_overdub_ui_number_valid(lua_Number value)
{
  float converted = (float)value;
  return isfinite((double)value) && isfinite(converted);
}

static int
lua_overdub_ui_slider_float_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "slider label is too long");
  }

  lua_Number value_arg = luaL_checknumber(state, 3);
  lua_Number min_arg   = luaL_checknumber(state, 4);
  lua_Number max_arg   = luaL_checknumber(state, 5);
  lua_Number step_arg  = luaL_optnumber(state, 6, 0.1);

  if (!lua_overdub_ui_number_valid(value_arg)) {
    return luaL_argerror(state, 3, "value must be finite");
  }

  if (!lua_overdub_ui_number_valid(min_arg)) {
    return luaL_argerror(state, 4, "minimum must be finite");
  }

  if (!lua_overdub_ui_number_valid(max_arg)) {
    return luaL_argerror(state, 5, "maximum must be finite");
  }

  if (!lua_overdub_ui_number_valid(step_arg) || step_arg <= 0.0) {
    return luaL_argerror(state, 6, "step must be finite and positive");
  }

  if (min_arg > max_arg) {
    return luaL_error(state, "slider minimum cannot exceed its maximum");
  }

  float value = (float)value_arg;
  if (value < (float)min_arg) {
    value = (float)min_arg;
  }

  if (value > (float)max_arg) {
    value = (float)max_arg;
  }

  lua_overdub_ui_prepare_labeled_control(draw, label, label_len);
  nk_slider_float(draw->nk, (float)min_arg, &value, (float)max_arg, (float)step_arg);
  lua_pushnumber(state, value);
  return 1;
}

static int
lua_overdub_ui_input_int_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (!lua_overdub_ui_name_valid(label, label_len, LUA_OVERDUB_UI_TITLE_CAP)) {
    return luaL_argerror(state, 2, "integer input label must be non-empty, short, and contain no null bytes");
  }

  lua_Integer value_arg  = luaL_checkinteger(state, 3);
  lua_Integer min_arg    = luaL_checkinteger(state, 4);
  lua_Integer max_arg    = luaL_checkinteger(state, 5);
  lua_Integer step_arg   = luaL_optinteger(state, 6, 1);
  bool        value_fits = lua_overdub_ui_integer_fits_int(value_arg);
  bool        min_fits   = lua_overdub_ui_integer_fits_int(min_arg);
  bool        max_fits   = lua_overdub_ui_integer_fits_int(max_arg);

  bool values_fit = value_fits && min_fits && max_fits;
  if (!values_fit) {
    return luaL_error(state, "integer input values are outside the supported integer range");
  }

  if (!lua_overdub_ui_integer_fits_int(step_arg) || step_arg <= 0) {
    return luaL_argerror(state, 6, "step must be a positive supported integer");
  }

  if (min_arg > max_arg) {
    return luaL_error(state, "integer input minimum cannot exceed its maximum");
  }

  int value = (int)value_arg;
  value = MAX_VAL(value, (int)min_arg);
  value = MIN_VAL(value, (int)max_arg);

  float width = lua_overdub_ui_text_width(draw, label, label_len) + 120.0f;
  lua_overdub_ui_prepare_widget_size(draw, width, 24.0f);
  bool changed = nk_property_int(draw->nk, label, (int)min_arg, &value, (int)max_arg, (int)step_arg, 1.0f);
  lua_pushinteger(state, value);
  lua_pushboolean(state, changed);
  return 2;
}

static int
lua_overdub_ui_input_float_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (!lua_overdub_ui_name_valid(label, label_len, LUA_OVERDUB_UI_TITLE_CAP)) {
    return luaL_argerror(state, 2, "float input label must be non-empty, short, and contain no null bytes");
  }

  lua_Number value_arg   = luaL_checknumber(state, 3);
  lua_Number min_arg     = luaL_checknumber(state, 4);
  lua_Number max_arg     = luaL_checknumber(state, 5);
  lua_Number step_arg    = luaL_optnumber(state, 6, 0.1);
  bool       value_valid = lua_overdub_ui_number_valid(value_arg);
  bool       min_valid   = lua_overdub_ui_number_valid(min_arg);
  bool       max_valid   = lua_overdub_ui_number_valid(max_arg);

  bool values_valid = value_valid && min_valid && max_valid;
  if (!values_valid) {
    return luaL_error(state, "float input values must be finite");
  }

  if (!lua_overdub_ui_number_valid(step_arg) || step_arg <= 0.0) {
    return luaL_argerror(state, 6, "step must be finite and positive");
  }

  if (min_arg > max_arg) {
    return luaL_error(state, "float input minimum cannot exceed its maximum");
  }

  float value = (float)value_arg;
  value = MAX_VAL(value, (float)min_arg);
  value = MIN_VAL(value, (float)max_arg);

  float width = lua_overdub_ui_text_width(draw, label, label_len) + 120.0f;
  lua_overdub_ui_prepare_widget_size(draw, width, 24.0f);

  bool changed = nk_property_float(draw->nk, label, (float)min_arg, &value, (float)max_arg, (float)step_arg, 1.0f);
  lua_pushnumber(state, value);
  lua_pushboolean(state, changed);
  return 2;
}

static int
lua_overdub_ui_text_input_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "text input label is too long");
  }

  size_t      value_len  = 0;
  const char *value      = luaL_checklstring(state, 3, &value_len);
  lua_Integer max_length = luaL_optinteger(state, 4, 1024);
  if (max_length < 1 || max_length >= LUA_OVERDUB_UI_TEXT_CAP) {
    return luaL_argerror(state, 4, "maximum length must be between 1 and 4095");
  }

  if (value_len > (size_t)max_length) {
    return luaL_argerror(state, 3, "text exceeds the configured maximum length");
  }

  char buffer[LUA_OVERDUB_UI_TEXT_CAP];
  memcpy(buffer, value, value_len);
  buffer[value_len] = 0;

  int edit_len   = (int)value_len;
  int buffer_cap = (int)max_length + 1;
  lua_overdub_ui_prepare_labeled_control(draw, label, label_len);

  nk_flags events  = nk_edit_string(draw->nk, (nk_flags)NK_EDIT_FIELD | (nk_flags)NK_EDIT_SIG_ENTER, buffer, &edit_len, buffer_cap, nk_filter_default);
  bool     changed = edit_len != (int)value_len || memcmp(buffer, value, value_len) != 0;

  lua_pushlstring(state, buffer, (size_t)edit_len);
  lua_pushboolean(state, changed);
  lua_pushboolean(state, (events & NK_EDIT_COMMITED) != 0);
  return 3;
}

static bool
lua_overdub_ui_combo_items_valid(lua_State *state, int table_index, size_t count)
{
  table_index = lua_absindex(state, table_index);

  for (size_t i = 1; i <= count; ++i) {
    lua_rawgeti(state, table_index, (lua_Integer)i);

    size_t item_len = 0;
    bool   valid    = lua_type(state, -1) == LUA_TSTRING;
    if (valid) {
      lua_tolstring(state, -1, &item_len);
      valid = item_len <= INT_MAX;
    }

    lua_pop(state, 1);
    if (!valid) {
      return false;
    }
  }

  return true;
}

static int
lua_overdub_ui_combo_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "combo label is too long");
  }

  luaL_checktype(state, 3, LUA_TTABLE);
  int    items_index = lua_absindex(state, 3);
  size_t item_count  = lua_rawlen(state, items_index);
  if (item_count == 0 || item_count > LUA_OVERDUB_UI_COMBO_CAP) {
    return luaL_argerror(state, 3, "combo must contain between 1 and 256 items");
  }

  if (!lua_overdub_ui_combo_items_valid(state, items_index, item_count)) {
    return luaL_argerror(state, 3, "combo items must be strings");
  }

  lua_Integer selected_arg = luaL_checkinteger(state, 4);
  if (selected_arg < 1 || (size_t)selected_arg > item_count) {
    return luaL_argerror(state, 4, "selected index is outside the combo item range");
  }

  lua_overdub_ui_prepare_labeled_control(draw, label, label_len);

  lua_rawgeti(state, items_index, selected_arg);
  size_t      selected_len  = 0;
  const char *selected_text = lua_tolstring(state, -1, &selected_len);

  float popup_height = 24.0f * (float)item_count;
  if (popup_height > 240.0f) {
    popup_height = 240.0f;
  }

  int            selected   = (int)selected_arg;
  struct nk_vec2 popup_size = nk_vec2(240.0f, popup_height);
  bool           open       = nk_combo_begin_text(draw->nk, selected_text, (int)selected_len, popup_size);
  lua_pop(state, 1);

  if (open) {
    nk_layout_row_dynamic(draw->nk, 22.0f, 1);
    for (size_t i = 1; i <= item_count; ++i) {
      lua_rawgeti(state, items_index, (lua_Integer)i);

      size_t      item_len = 0;
      const char *item     = lua_tolstring(state, -1, &item_len);
      bool        chosen   = nk_combo_item_text(draw->nk, item, (int)item_len, NK_TEXT_LEFT);
      lua_pop(state, 1);

      if (chosen) {
        selected = (int)i;
        nk_combo_close(draw->nk);
      }
    }
    nk_combo_end(draw->nk);
  }

  lua_pushinteger(state, selected);
  return 1;
}

static uint8_t
lua_overdub_ui_check_color_component(lua_State *state, int table_idx, const char *name, uint8_t default_value, bool required)
{
  lua_getfield(state, table_idx, name);
  if (lua_isnil(state, -1) && !required) {
    lua_pop(state, 1);
    return default_value;
  }

  lua_Integer value = luaL_checkinteger(state, -1);
  lua_pop(state, 1);
  if (value < 0 || value > 255) {
    luaL_error(state, "color component %s must be between 0 and 255", name);
  }

  return (uint8_t)value;
}

static void
lua_overdub_ui_push_color(lua_State *state, struct nk_color color)
{
  lua_createtable(state, 0, 4);
  lua_pushinteger(state, color.r);
  lua_setfield(state, -2, "R");
  lua_pushinteger(state, color.g);
  lua_setfield(state, -2, "G");
  lua_pushinteger(state, color.b);
  lua_setfield(state, -2, "B");
  lua_pushinteger(state, color.a);
  lua_setfield(state, -2, "A");
}

static struct nk_color
lua_overdub_ui_check_color(lua_State *state, int table_idx)
{
  luaL_checktype(state, table_idx, LUA_TTABLE);
  table_idx = lua_absindex(state, table_idx);
  return (struct nk_color){
    .r = lua_overdub_ui_check_color_component(state, table_idx, "R", 0, true),
    .g = lua_overdub_ui_check_color_component(state, table_idx, "G", 0, true),
    .b = lua_overdub_ui_check_color_component(state, table_idx, "B", 0, true),
    .a = lua_overdub_ui_check_color_component(state, table_idx, "A", 255, false),
  };
}

static int
lua_overdub_ui_text_colored_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      text_len = 0;
  const char *text     = luaL_checklstring(state, 2, &text_len);
  if (text_len > INT_MAX) {
    return luaL_argerror(state, 2, "text is too long");
  }

  struct nk_color color     = lua_overdub_ui_check_color(state, 3);
  nk_flags        alignment = lua_overdub_ui_text_alignment(state, 4);
  float           width     = lua_overdub_ui_text_width(draw, text, text_len);
  lua_overdub_ui_prepare_widget_size(draw, width, 22.0f);
  nk_text_colored(draw->nk, text, (int)text_len, alignment, color);
  return 0;
}

static int
lua_overdub_ui_wrapped_text_colored_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      text_len = 0;
  const char *text     = luaL_checklstring(state, 2, &text_len);
  if (text_len > INT_MAX) {
    return luaL_argerror(state, 2, "text is too long");
  }

  lua_Number height = luaL_optnumber(state, 4, 48.0);
  if (!isfinite((double)height) || height <= 0.0 || height > 4000.0) {
    return luaL_argerror(state, 4, "height must be finite and between 0 and 4000");
  }

  struct nk_color color = lua_overdub_ui_check_color(state, 3);
  lua_overdub_ui_prepare_widget(draw, (float)height);
  nk_text_wrap_colored(draw->nk, text, (int)text_len, color);
  return 0;
}

static int
lua_overdub_ui_color_editor_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len > INT_MAX) {
    return luaL_argerror(state, 2, "color editor label is too long");
  }

  struct nk_color color = lua_overdub_ui_check_color(state, 3);

  lua_overdub_ui_prepare_labeled_control(draw, label, label_len);
  if (nk_combo_begin_color(draw->nk, color, nk_vec2(nk_widget_width(draw->nk), 320.0f))) {
    nk_layout_row_dynamic(draw->nk, 120.0f, 1);
    struct nk_colorf picked = nk_color_picker(draw->nk, nk_color_cf(color), NK_RGBA);
    color                   = nk_rgba_cf(picked);

    nk_layout_row_dynamic(draw->nk, 24.0f, 1);
    int red   = color.r;
    int green = color.g;
    int blue  = color.b;
    int alpha = color.a;
    nk_property_int(draw->nk, "#R", 0, &red,   255, 1, 1);
    nk_property_int(draw->nk, "#G", 0, &green, 255, 1, 1);
    nk_property_int(draw->nk, "#B", 0, &blue,  255, 1, 1);
    nk_property_int(draw->nk, "#A", 0, &alpha, 255, 1, 1);

    color = nk_rgba((nk_byte)red, (nk_byte)green, (nk_byte)blue, (nk_byte)alpha);
    nk_combo_end(draw->nk);
  }

  lua_overdub_ui_push_color(state, color);
  return 1;
}

static int
lua_overdub_ui_keybind_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      label_len = 0;
  const char *label     = luaL_checklstring(state, 2, &label_len);
  if (label_len == 0 || label_len >= LUA_OVERDUB_UI_KEYBIND_CAP) {
    return luaL_argerror(state, 2, "keybind label must be between 1 and 255 bytes");
  }

  size_t      value_len = 0;
  const char *value     = luaL_checklstring(state, 3, &value_len);
  keybind_t   bind      = KEYBIND_NULL;
  if (value_len > 0) {
    bind = keybind_parse(str_make((void *)value, value_len), KEYBIND_NULL);
    if (!keybind_is_valid(bind)) {
      return luaL_argerror(state, 3, "keybind is invalid");
    }
  }

  char name[LUA_OVERDUB_UI_WIDGET_ID_CAP];
  if (!lua_overdub_ui_scoped_name(draw, "Keybind", label, label_len, name, sizeof(name))) {
    return luaL_error(state, "keybind widget id is too long");
  }

  lua_overdub_ui_prepare_labeled_control(draw, label, label_len);
  ui_keybind_capture(&globals.ui_manager.keybind_capture, draw->nk, str_make(name, strlen(name)), &bind);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t text = keybind_to_str(bind, tmp.arena);
    lua_pushlstring(state, (const char *)text.data, (size_t)text.len);
  }
  scratch_end(tmp);
  return 1;
}

static int
lua_overdub_ui_group_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      id_len = 0;
  const char *id     = luaL_checklstring(state, 2, &id_len);
  if (!lua_overdub_ui_name_valid(id, id_len, LUA_OVERDUB_UI_ID_CAP)) {
    return luaL_argerror(state, 2, "group id must be non-empty, short, and contain no null bytes");
  }

  lua_Number height = luaL_checknumber(state, 3);
  if (!isfinite((double)height) || height <= 0.0 || height > 4000.0) {
    return luaL_argerror(state, 3, "group height must be finite and between 0 and 4000");
  }

  luaL_checktype(state, 4, LUA_TFUNCTION);
  char name[LUA_OVERDUB_UI_WIDGET_ID_CAP];
  if (!lua_overdub_ui_scoped_name(draw, "Group", id, id_len, name, sizeof(name))) {
    return luaL_error(state, "namespaced group id is too long");
  }

  lua_overdub_ui_prepare_widget(draw, (float)height);
  struct nk_grid *outer_grid       = draw->grid;
  int             outer_column     = draw->grid_column;
  int             outer_remaining  = draw->row_remaining;
  bool            outer_row_active = draw->grid_row_active;
  if (nk_group_begin(draw->nk, name, NK_WINDOW_BORDER)) {
    draw->grid            = NULL;
    draw->grid_column     = 0;
    draw->grid_row_active = false;
    draw->row_remaining   = 0;

    lua_pushvalue(state, 4);
    lua_pushvalue(state, 1);

    int status = lua_pcall(state, 1, 0, 0);
    nk_group_end(draw->nk);
    draw->grid            = outer_grid;
    draw->grid_column     = outer_column;
    draw->grid_row_active = outer_row_active;
    draw->row_remaining   = outer_remaining;

    if (status != LUA_OK) {
      return lua_error(state);
    }
  }

  nk_uint scroll_x = 0;
  nk_uint scroll_y = 0;
  nk_group_get_scroll(draw->nk, name, &scroll_x, &scroll_y, NULL, NULL, NULL);
  lua_pushinteger(state, scroll_x);
  lua_pushinteger(state, scroll_y);
  return 2;
}

static int
lua_overdub_ui_group_scroll_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      id_len = 0;
  const char *id     = luaL_checklstring(state, 2, &id_len);
  if (!lua_overdub_ui_name_valid(id, id_len, LUA_OVERDUB_UI_ID_CAP)) {
    return luaL_argerror(state, 2, "group id must be non-empty, short, and contain no null bytes");
  }

  char name[LUA_OVERDUB_UI_WIDGET_ID_CAP];
  if (!lua_overdub_ui_scoped_name(draw, "Group", id, id_len, name, sizeof(name))) {
    return luaL_error(state, "namespaced group id is too long");
  }

  nk_uint scroll_x = 0;
  nk_uint scroll_y = 0;
  nk_group_get_scroll(draw->nk, name, &scroll_x, &scroll_y, NULL, NULL, NULL);
  lua_pushinteger(state, scroll_x);
  lua_pushinteger(state, scroll_y);
  return 2;
}

static int
lua_overdub_ui_set_group_scroll_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      id_len = 0;
  const char *id     = luaL_checklstring(state, 2, &id_len);
  if (!lua_overdub_ui_name_valid(id, id_len, LUA_OVERDUB_UI_ID_CAP)) {
    return luaL_argerror(state, 2, "group id must be non-empty, short, and contain no null bytes");
  }

  lua_Integer scroll_x = luaL_checkinteger(state, 3);
  lua_Integer scroll_y = luaL_checkinteger(state, 4);
  if (scroll_x < 0 || (uint64_t)scroll_x > UINT_MAX) {
    return luaL_argerror(state, 3, "horizontal scroll must fit an unsigned 32-bit integer");
  }

  if (scroll_y < 0 || (uint64_t)scroll_y > UINT_MAX) {
    return luaL_argerror(state, 4, "vertical scroll must fit an unsigned 32-bit integer");
  }

  char name[LUA_OVERDUB_UI_WIDGET_ID_CAP];
  if (!lua_overdub_ui_scoped_name(draw, "Group", id, id_len, name, sizeof(name))) {
    return luaL_error(state, "namespaced group id is too long");
  }
  nk_group_set_scroll(draw->nk, name, (nk_uint)scroll_x, (nk_uint)scroll_y);
  return 0;
}

static int
lua_overdub_ui_tree_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  if (draw->grid) {
    return luaL_error(state, "Tree cannot be used directly inside Grid; place it inside a Group");
  }

  size_t      title_len = 0;
  const char *title     = luaL_checklstring(state, 2, &title_len);
  if (title_len > INT_MAX) {
    return luaL_argerror(state, 2, "tree title is too long");
  }

  enum nk_collapse_states expanded = NK_MINIMIZED;
  if (lua_toboolean(state, 3)) {
    expanded = NK_MAXIMIZED;
  }
  luaL_checktype(state, 4, LUA_TFUNCTION);
  draw->row_remaining = 0;

  int status = LUA_OK;
  if (nk_tree_state_push_text(draw->nk, NK_TREE_NODE, title, (int)title_len, &expanded)) {
    lua_pushvalue(state, 4);
    lua_pushvalue(state, 1);
    status = lua_pcall(state, 1, 0, 0);
    nk_tree_pop(draw->nk);
  }

  draw->row_remaining = 0;
  if (status != LUA_OK) {
    return lua_error(state);
  }

  lua_pushboolean(state, expanded == NK_MAXIMIZED);
  return 1;
}

static int
lua_overdub_ui_tooltip_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  size_t      text_len = 0;
  const char *text     = luaL_checklstring(state, 2, &text_len);
  if (text_len > INT_MAX) {
    return luaL_argerror(state, 2, "tooltip text is too long");
  }

  if (nk_widget_is_hovered(draw->nk)) {
    nk_tooltip_text(draw->nk, text, (int)text_len);
  }
  return 0;
}

static int
lua_overdub_ui_separator_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  lua_overdub_ui_prepare_widget(draw, 8.0f);
  nk_rule_horizontal(draw->nk, draw->nk->style.window.border_color, true);
  return 0;
}

static int
lua_overdub_ui_row_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  if (draw->grid) {
    return luaL_error(state, "Row cannot be used inside Grid");
  }

  lua_Integer columns = luaL_checkinteger(state, 2);
  if (columns < 1 || columns > 16) {
    return luaL_argerror(state, 2, "column count must be between 1 and 16");
  }

  lua_Number height = luaL_optnumber(state, 3, 24.0);
  if (!isfinite((double)height) || height <= 0.0 || height > 1000.0) {
    return luaL_argerror(state, 3, "row height must be finite and between 0 and 1000");
  }

  nk_layout_row_dynamic(draw->nk, (float)height, (int)columns);
  draw->row_remaining = (int)columns;
  return 0;
}

static int
lua_overdub_ui_row_static_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  if (draw->grid) {
    return luaL_error(state, "RowStatic cannot be used inside Grid");
  }

  lua_Integer columns = luaL_checkinteger(state, 2);
  lua_Number  width   = luaL_checknumber(state, 3);
  lua_Number  height  = luaL_optnumber(state, 4, 24.0);
  if (columns < 1 || columns > NK_GRID_MAX_COLUMNS) {
    return luaL_argerror(state, 2, "column count must be between 1 and 16");
  }

  if (!isfinite((double)width) || width <= 0.0 || width > 4000.0) {
    return luaL_argerror(state, 3, "column width must be finite and between 0 and 4000");
  }

  if (!isfinite((double)height) || height <= 0.0 || height > 1000.0) {
    return luaL_argerror(state, 4, "row height must be finite and between 0 and 1000");
  }

  nk_layout_row_static(draw->nk, (float)height, (int)width, (int)columns);
  draw->row_remaining = (int)columns;
  return 0;
}

static int
lua_overdub_ui_row_values_lua(lua_State *state, enum nk_layout_format format)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  if (draw->grid) {
    const char *name = "RowWidths";
    if (format == NK_DYNAMIC) {
      name = "RowRatios";
    }

    return luaL_error(state, "%s cannot be used inside Grid", name);
  }

  luaL_checktype(state, 2, LUA_TTABLE);
  int    table_idx = lua_absindex(state, 2);
  size_t count     = lua_rawlen(state, table_idx);
  if (count < 1 || count > NK_GRID_MAX_COLUMNS) {
    return luaL_argerror(state, 2, "row must contain between 1 and 16 columns");
  }

  lua_Number height = luaL_optnumber(state, 3, 24.0);
  if (!isfinite((double)height) || height <= 0.0 || height > 1000.0) {
    return luaL_argerror(state, 3, "row height must be finite and between 0 and 1000");
  }

  float sum = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    lua_rawgeti(state, table_idx, (lua_Integer)i + 1);
    lua_Number value = luaL_checknumber(state, -1);
    lua_pop(state, 1);

    if (!isfinite((double)value) || value <= 0.0 || value > 4000.0) {
      return luaL_argerror(state, 2, "row column values must be finite and between 0 and 4000");
    }

    draw->row_values[i] = (float)value;
    sum += (float)value;
  }

  if (format == NK_DYNAMIC) {
    for (size_t i = 0; i < count; ++i) {
      draw->row_values[i] /= sum;
    }
  }

  nk_layout_row(draw->nk, format, (float)height, (int)count, draw->row_values);
  draw->row_remaining = (int)count;
  return 0;
}

static int
lua_overdub_ui_row_ratios_lua(lua_State *state)
{
  return lua_overdub_ui_row_values_lua(state, NK_DYNAMIC);
}

static int
lua_overdub_ui_row_widths_lua(lua_State *state)
{
  return lua_overdub_ui_row_values_lua(state, NK_STATIC);
}

static int
lua_overdub_ui_spacing_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  lua_Integer                    count = luaL_optinteger(state, 2, 1);
  if (count < 1 || count > NK_GRID_MAX_COLUMNS) {
    return luaL_argerror(state, 2, "spacing count must be between 1 and 16");
  }

  for (lua_Integer i = 0; i < count; ++i) {
    lua_overdub_ui_prepare_widget(draw, 1.0f);
    nk_spacing(draw->nk, 1);
  }

  return 0;
}

static uint32_t
lua_overdub_ui_hash_u32(uint32_t hash, uint32_t value)
{
  for (int i = 0; i < 4; ++i) {
    hash ^= (uint8_t)(value >> (i * 8));
    hash *= 16777619u;
  }

  return hash;
}

static bool
lua_overdub_ui_grid_columns(lua_State *state, int table_idx, struct nk_grid_column *columns, int count, uint32_t *out_signature)
{
  uint32_t signature = lua_overdub_ui_hash_u32(2166136261u, (uint32_t)count);
  for (int i = 0; i < count; ++i) {
    lua_rawgeti(state, table_idx, i + 1);
    int type = lua_type(state, -1);

    struct nk_grid_column column = {0};
    bool                  valid  = false;
    if (type == LUA_TNUMBER) {
      lua_Number width = lua_tonumber(state, -1);
      if (isfinite((double)width) && width > 0.0 && width <= 4000.0) {
        column.sizing = NK_GRID_COLUMN_FIXED;
        column.width  = (float)width;
        valid         = true;
      }
    } else if (type == LUA_TSTRING) {
      size_t      sizing_len = 0;
      const char *sizing     = lua_tolstring(state, -1, &sizing_len);
      if (sizing_len == sizeof("Content") - 1 && memcmp(sizing, "Content", sizing_len) == 0) {
        column.sizing    = NK_GRID_COLUMN_CONTENT;
        column.min_width = 8.0f;
        valid            = true;
      } else if (sizing_len == sizeof("Flex") - 1 && memcmp(sizing, "Flex", sizing_len) == 0) {
        column.sizing    = NK_GRID_COLUMN_FLEX;
        column.width     = 1.0f;
        column.min_width = 20.0f;
        valid            = true;
      }
    }
    lua_pop(state, 1);

    if (!valid) {
      return false;
    }

    uint32_t width_bits = 0;
    mem_copy(&width_bits, &column.width, sizeof(width_bits));
    signature  = lua_overdub_ui_hash_u32(signature, (uint32_t)column.sizing);
    signature  = lua_overdub_ui_hash_u32(signature, width_bits);
    columns[i] = column;
  }

  if (signature == 0) {
    signature = 1;
  }
  *out_signature = signature;
  return true;
}

static lua_overdub_ui_grid_state_t *
lua_overdub_ui_grid_state(lua_overdub_ui_context_t *context, nk_hash scope, const char *id, size_t id_len)
{
  lua_overdub_ui_grid_state_t *free_slot = NULL;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_GRIDS; ++i) {
    lua_overdub_ui_grid_state_t *slot = &context->grids[i];
    if (slot->scope == scope && slot->key_len == id_len && memcmp(slot->key, id, id_len) == 0) {
      return slot;
    }

    if (!free_slot && slot->key_len == 0) {
      free_slot = slot;
    }
  }

  if (free_slot) {
    memcpy(free_slot->key, id, id_len);
    free_slot->key[id_len] = 0;
    free_slot->key_len     = id_len;
    free_slot->scope       = scope;
  }

  return free_slot;
}

static int
lua_overdub_ui_grid_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  if (draw->grid) {
    return luaL_error(state, "Grid cannot be nested inside Grid");
  }

  size_t      id_len = 0;
  const char *id     = luaL_checklstring(state, 2, &id_len);
  if (!lua_overdub_ui_name_valid(id, id_len, LUA_OVERDUB_UI_ID_CAP)) {
    return luaL_argerror(state, 2, "grid id must be non-empty, short, and contain no null bytes");
  }

  luaL_checktype(state, 3, LUA_TTABLE);
  int    columns_idx  = lua_absindex(state, 3);
  size_t column_count = lua_rawlen(state, columns_idx);
  if (column_count < 1 || column_count > NK_GRID_MAX_COLUMNS) {
    return luaL_argerror(state, 3, "grid must contain between 1 and 16 columns");
  }

  int        callback_idx = 4;
  lua_Number row_height   = 24.0;
  if (lua_gettop(state) == 5) {
    row_height   = luaL_checknumber(state, 4);
    callback_idx = 5;
  } else if (lua_gettop(state) != 4) {
    return luaL_error(state, "expected Grid(id, columns, callback) or Grid(id, columns, height, callback)");
  }

  if (!isfinite((double)row_height) || row_height <= 0.0 || row_height > 1000.0) {
    return luaL_argerror(state, 4, "grid row height must be finite and between 0 and 1000");
  }
  luaL_checktype(state, callback_idx, LUA_TFUNCTION);

  struct nk_grid_column columns[NK_GRID_MAX_COLUMNS] = {0};
  uint32_t              signature                    = 0;
  if (!lua_overdub_ui_grid_columns(state, columns_idx, columns, (int)column_count, &signature)) {
    return luaL_argerror(state, 3, "grid columns must be positive pixel widths, 'Content', or 'Flex'");
  }

  nk_hash                      scope = draw->nk->current->layout->text_selection_scope;
  lua_overdub_ui_grid_state_t *slot  = lua_overdub_ui_grid_state(draw->context, scope, id, id_len);
  if (!slot) {
    return luaL_error(state, "Overdub UI grid limit reached or grid id is too long");
  }

  struct nk_grid_options options = {
    .row_height = (float)row_height,
    .column_gap = draw->nk->style.window.spacing.x,
    .generation = signature,
  };
  struct nk_grid grid;
  draw->row_remaining = 0;
  nk_grid_begin(draw->nk, &grid, &slot->state, columns, (int)column_count, &options);
  draw->grid = &grid;

  lua_pushvalue(state, callback_idx);
  lua_pushvalue(state, 1);
  int status = lua_pcall(state, 1, 0, 0);

  lua_overdub_ui_grid_end_row(draw);
  nk_grid_end(&grid);
  draw->grid = NULL;

  if (status != LUA_OK) {
    return lua_error(state);
  }

  return 0;
}

static int
lua_overdub_ui_next_row_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);
  if (!draw->grid) {
    return luaL_error(state, "NextRow is only valid inside Grid");
  }

  lua_overdub_ui_grid_end_row(draw);
  return 0;
}

static int
lua_overdub_ui_viewport_size_lua(lua_State *state)
{
  lua_overdub_ui_draw_context_t *draw = lua_overdub_ui_draw_context_check(state);

  lua_pushinteger(state, draw->context->viewport_width);
  lua_pushinteger(state, draw->context->viewport_height);
  return 2;
}

static bool
lua_overdub_ui_name_valid(const char *value, size_t len, size_t cap)
{
  if (!value || len == 0 || len >= cap) {
    return false;
  }

  return memchr(value, 0, len) == NULL;
}

static int
lua_overdub_ui_register_window_lua(lua_State *state)
{
  lua_overdub_ui_context_t *context = lua_overdub_ui_context_check(state);

  int argument_count = lua_gettop(state);
  int callback_index = 2;
  if (argument_count == 3) {
    callback_index = 3;
  } else if (argument_count != 2) {
    return luaL_error(state, "expected RegisterWindow(id, callback) or RegisterWindow(id, title, callback)");
  }

  size_t      id_len = 0;
  const char *id     = luaL_checklstring(state, 1, &id_len);
  if (!lua_overdub_ui_name_valid(id, id_len, LUA_OVERDUB_UI_ID_CAP)) {
    return luaL_argerror(state, 1, "window id must be non-empty, short, and contain no null bytes");
  }

  size_t      title_len = id_len;
  const char *title     = id;
  if (argument_count == 3) {
    title = luaL_checklstring(state, 2, &title_len);
  }

  if (!lua_overdub_ui_name_valid(title, title_len, LUA_OVERDUB_UI_TITLE_CAP)) {
    return luaL_argerror(state, 2, "window title must be non-empty, short, and contain no null bytes");
  }

  luaL_checktype(state, callback_index, LUA_TFUNCTION);
  if (lua_overdub_ui_window_find_public_id(context, id, id_len)) {
    return luaL_error(state, "UI window '%s' is already registered", id);
  }

  int                       slot_index = 0;
  lua_overdub_ui_window_t *window     = lua_overdub_ui_window_find_free(context, &slot_index);
  if (!window) {
    return luaL_error(state, "Overdub UI window limit reached");
  }

  int name_len = snprintf(window->name, sizeof(window->name), "%.*s::%.*s", (int)context->mod_id.len, (const char *)context->mod_id.data, (int)id_len, id);
  if (name_len < 0 || name_len >= (int)sizeof(window->name)) {
    mem_zero(window, sizeof(*window));
    return luaL_error(state, "namespaced UI window name is too long");
  }

  memcpy(window->public_id, id, id_len);
  window->public_id[id_len] = 0;
  window->public_id_len = id_len;

  memcpy(window->title, title, title_len);
  window->title[title_len] = 0;

  lua_pushvalue(state, callback_index);
  int callback_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  uint64_t id_number = context->next_window_id;
  context->next_window_id += 1;
  if (id_number == 0) {
    id_number = context->next_window_id;
    context->next_window_id += 1;
  }

  lua_overdub_ui_window_handle_t *handle = lua_newuserdatauv(state, sizeof(*handle), 0);
  *handle = (lua_overdub_ui_window_handle_t){
    .context = context,
    .id      = id_number,
  };
  luaL_setmetatable(state, LUA_OVERDUB_UI_WINDOW_META);

  lua_pushvalue(state, -1);
  int handle_ref = luaL_ref(state, LUA_REGISTRYINDEX);

  float offset = (float)(slot_index % 8) * 24.0f;
  window->id             = id_number;
  window->callback_ref   = callback_ref;
  window->handle_ref     = handle_ref;
  window->bounds         = nk_rect(40.0f + offset, 40.0f + offset, 420.0f, 300.0f);
  window->active         = true;
  window->open           = true;
  window->show_requested = true;
  return 1;
}

static int
luaopen_overdub_ui(lua_State *state)
{
  static const luaL_Reg functions[] = {
    {"RegisterWindow", lua_overdub_ui_register_window_lua},
    {NULL, NULL},
  };

  luaL_newlib(state, functions);
  return 1;
}

lua_overdub_ui_context_t *
lua_overdub_ui_context_create(arena_t *arena, lua_runtime_t *runtime, str_t mod_id, uint32_t owner_thread_id)
{
  if (!arena || !arena->backing || !runtime || !runtime->state) {
    return NULL;
  }

  if (str_is_empty(mod_id) || owner_thread_id == 0) {
    return NULL;
  }

  lua_overdub_ui_context_t *context = ARENA_PUSH_ZERO(arena, lua_overdub_ui_context_t);
  if (!context) {
    return NULL;
  }

  context->runtime         = runtime;
  context->mod_id          = mod_id;
  context->owner_thread_id = owner_thread_id;
  context->next_window_id  = 1;

  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS; ++i) {
    context->windows[i].callback_ref = LUA_NOREF;
    context->windows[i].handle_ref   = LUA_NOREF;
  }

  return context;
}

void
lua_overdub_ui_context_destroy(lua_overdub_ui_context_t *context)
{
  if (!context) {
    return;
  }

  lua_State *state = context->runtime->state;
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS; ++i) {
    lua_overdub_ui_window_invalidate_handle(context, &context->windows[i]);
    lua_overdub_ui_window_release(context, &context->windows[i]);
  }

  if (state) {
    lua_pushlightuserdata(state, &g_lua_overdub_ui_context_key);
    lua_pushnil(state);
    lua_rawset(state, LUA_REGISTRYINDEX);
  }

}

bool
lua_overdub_ui_register(lua_State *state, lua_overdub_ui_context_t *context)
{
  if (!state || !context || context->runtime->state != state) {
    return false;
  }

  lua_pushlightuserdata(state, &g_lua_overdub_ui_context_key);
  lua_pushlightuserdata(state, context);
  lua_rawset(state, LUA_REGISTRYINDEX);

  if (luaL_newmetatable(state, LUA_OVERDUB_UI_WINDOW_META)) {
    static const luaL_Reg methods[] = {
      {"Remove",   lua_overdub_ui_window_remove_lua},
      {"IsActive", lua_overdub_ui_window_is_active_lua},
      {"SetOpen",  lua_overdub_ui_window_set_open_lua},
      {"IsOpen",   lua_overdub_ui_window_is_open_lua},
      {NULL, NULL},
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushcfunction(state, lua_overdub_ui_window_gc_lua);
    lua_setfield(state, -2, "__gc");
    lua_pushliteral(state, "Overdub UI Window");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  if (luaL_newmetatable(state, LUA_OVERDUB_UI_CONTEXT_META)) {
    static const luaL_Reg methods[] = {
      {"Text",               lua_overdub_ui_text_lua                },
      {"TextColored",        lua_overdub_ui_text_colored_lua        },
      {"WrappedText",        lua_overdub_ui_wrapped_text_lua        },
      {"WrappedTextColored", lua_overdub_ui_wrapped_text_colored_lua},
      {"Button",             lua_overdub_ui_button_lua              },
      {"Checkbox",           lua_overdub_ui_checkbox_lua            },
      {"SliderInt",          lua_overdub_ui_slider_int_lua          },
      {"SliderFloat",        lua_overdub_ui_slider_float_lua        },
      {"InputInt",           lua_overdub_ui_input_int_lua           },
      {"InputFloat",         lua_overdub_ui_input_float_lua         },
      {"InputText",          lua_overdub_ui_text_input_lua          },
      {"TextInput",          lua_overdub_ui_text_input_lua          },
      {"Dropdown",           lua_overdub_ui_combo_lua               },
      {"Combo",              lua_overdub_ui_combo_lua               },
      {"ColorEditor",        lua_overdub_ui_color_editor_lua        },
      {"Keybind",            lua_overdub_ui_keybind_lua             },
      {"Group",              lua_overdub_ui_group_lua               },
      {"GroupScroll",        lua_overdub_ui_group_scroll_lua        },
      {"SetGroupScroll",     lua_overdub_ui_set_group_scroll_lua    },
      {"Tree",               lua_overdub_ui_tree_lua                },
      {"Tooltip",            lua_overdub_ui_tooltip_lua             },
      {"Separator",          lua_overdub_ui_separator_lua           },
      {"Spacing",            lua_overdub_ui_spacing_lua             },
      {"Row",                lua_overdub_ui_row_lua                 },
      {"RowStatic",          lua_overdub_ui_row_static_lua          },
      {"RowRatios",          lua_overdub_ui_row_ratios_lua          },
      {"RowWidths",          lua_overdub_ui_row_widths_lua          },
      {"Grid",               lua_overdub_ui_grid_lua                },
      {"NextRow",            lua_overdub_ui_next_row_lua            },
      {"ViewportSize",       lua_overdub_ui_viewport_size_lua       },
      {NULL,                 NULL                                   },
    };

    luaL_setfuncs(state, methods, 0);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    lua_pushliteral(state, "Overdub UI Context");
    lua_setfield(state, -2, "__metatable");
  }
  lua_pop(state, 1);

  return lua_overdub_preload_module(state, "overdub.ui", luaopen_overdub_ui);
}

static void
lua_overdub_ui_draw_window(lua_overdub_ui_context_t *context, lua_overdub_ui_window_t *window, bool *in_callback)
{
  if (!window->active || !window->open || window->remove_requested) {
    return;
  }

  if (window->created && window->show_requested) {
    nk_window_show(context->nk, window->name, NK_SHOWN);
  }
  window->show_requested = false;

  nk_flags flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE;
  flags |= NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE;
  flags |= (nk_flags)NK_WINDOW_CLOSE_BUTTON_HIDES;

  bool visible = nk_begin_titled(context->nk, window->name, window->title, window->bounds, flags);
  window->created = true;
  window->drawing = true;

  bool called = true;
  if (visible) {
    lua_overdub_ui_push_draw_context(context);

    *in_callback = true;
    called = lua_runtime_call_ref(context->runtime, window->callback_ref, 1, 0);
    *in_callback = false;
    context->draw_token += 1;
  }

  nk_end(context->nk);
  window->drawing = false;

  if (nk_window_is_hidden(context->nk, window->name)) {
    window->open = false;
  }

  if (!called) {
    LOG_ERROR("Overdub Lua mod '%.*s': removing failed UI window '%s'", STR_ARG(context->mod_id), window->public_id);
    window->remove_requested = true;
  }

  if (window->remove_requested) {
    lua_overdub_ui_window_release(context, window);
  }
}

uint64_t
lua_overdub_ui_draw_windows(lua_overdub_ui_context_t *ui, struct nk_context *nk, bool *in_cb, unsigned int vw, unsigned int vh)
{
  if (!ui || !nk || !in_cb || *in_cb) {
    return 0;
  }

  if (!lua_overdub_ui_draw_begin(ui, nk, vw, vh)) {
    return 0;
  }

  uint64_t start_us = time_now_us();
  for (int i = 0; i < CONFIG_LUA_OVERDUB_MAX_UI_WINDOWS; ++i) {
    lua_overdub_ui_draw_window(ui, &ui->windows[i], in_cb);
  }

  lua_overdub_ui_draw_end(ui);
  return time_now_us() - start_us;
}

bool
lua_overdub_ui_begin_callback(lua_overdub_ui_context_t *ui, struct nk_context *nk, unsigned int vw, unsigned int vh)
{
  if (!lua_overdub_ui_draw_begin(ui, nk, vw, vh)) {
    return false;
  }

  lua_overdub_ui_push_draw_context(ui);
  return true;
}

void
lua_overdub_ui_end_callback(lua_overdub_ui_context_t *ui)
{
  lua_overdub_ui_draw_end(ui);
}
