#include "builtin.h"
#include "common.h"

#include "arena.h"
#include "file.h"
#include "globals.h"
#include "input.h"
#include "log.h"
#include "mod_manager.h"
#include "path.h"
#include "scratch.h"
#include "str.h"
#include "types.h"
#include "ui_nuklear.h"
#include "unreal.h"
#include "vendor_nuklear.h"
#include "vendor_stb.h"

#include <stdio.h>

#define CFG_MAX_CALLS_ID   "max-calls"
#define CFG_OPEN_WINDOW_ID "open-window"
#define CFG_PAGE_SIZE_ID   "page-size"

#define DETAIL_C_ROW_COLLAPSIBLE_BG     nk_rgba(34, 43, 48, 210)
#define DETAIL_C_ROW_COLLAPSIBLE_HOVER  nk_rgba(95, 178, 145, 255)
#define DETAIL_C_ROW_COLLAPSIBLE_ACTIVE nk_rgba(50, 65, 72, 245)

#define TRACE_RAW_ALIGN       (16)
#define TRACE_SAVE_PATH_MAX   (260)
#define TRACE_FILTER_TEXT_CAP (4 * KB)
#define TRACE_FILTER_RULE_MAX (128)

#define TRACE_CALL_COL_COUNT    (8)
#define TRACE_CALL_COL_SEQ      "SEQ"
#define TRACE_CALL_COL_TIME     "TIME"
#define TRACE_CALL_COL_DURATION "DURATION (us)"
#define TRACE_CALL_COL_DEPTH    "DEPTH"
#define TRACE_CALL_COL_KIND     "SOURCE"
#define TRACE_CALL_COL_CLASS    "CLASS"
#define TRACE_CALL_COL_OBJECT   "OBJECT"
#define TRACE_CALL_COL_FUNCTION "FUNCTION"

#define TRACE_CALL_HOOK_KIND_PE            "PE"
#define TRACE_CALL_HOOK_KIND_INVOKE        "I"
#define TRACE_CALL_HOOK_KIND_PE_AND_INVOKE "PE+I"

typedef uint8_t trace_hook_kind_t;
enum {
  TRACE_HOOK_PROCESS_EVENT = 0,
  TRACE_HOOK_INVOKE        = 1,
};

typedef uint8_t trace_filter_sign_t;
enum {
  TRACE_FILTER_SIGN_NONE,
  TRACE_FILTER_SIGN_INCLUDE,
  TRACE_FILTER_SIGN_EXCLUDE,
};

typedef uint8_t trace_filter_target_t;
enum {
  TRACE_FILTER_TARGET_NONE,
  TRACE_FILTER_TARGET_SELF_NAME,
  TRACE_FILTER_TARGET_SELF_OUTER_NAME,
  TRACE_FILTER_TARGET_CLASS_NAME,
  TRACE_FILTER_TARGET_CLASS_OUTER_NAME,
  TRACE_FILTER_TARGET_CLASS_INHERIT,
  TRACE_FILTER_TARGET_FUNC_NAME,
  TRACE_FILTER_TARGET_FUNC_OUTER_NAME,

  TRACE_FILTER_TARGET_MAX,
};

typedef struct trace_filter_rule_s trace_filter_rule_t;
struct trace_filter_rule_s {
  str_t                 text;
  trace_filter_target_t target;
  trace_filter_sign_t   sign;
};

typedef struct trace_filter_set_s trace_filter_set_t;
struct trace_filter_set_s {
  trace_filter_rule_t *rules;
  uint32_t             rule_count;
  char                *rule_input;
  uint32_t             rule_input_len;
  char                *preview;
  uint32_t             preview_len;
  bool                 ignore_case;
  bool                 exact_match;
};

typedef TARRAY(void) tarray_view_t;

typedef uint8_t trace_filter_dialog_kind_t;
enum {
  TRACE_FILTER_DIALOG_NONE,
  TRACE_FILTER_DIALOG_CAPTURE,
};

typedef struct trace_call_s trace_call_t;
struct trace_call_s {
  /* capture order */
  trace_call_t *next;

  /* captured-call tree */
  trace_call_t *parent;
  trace_call_t *first_child;
  trace_call_t *last_child;
  trace_call_t *next_sibling;

  uint64_t start_us;
  uint64_t end_us;
  uint32_t seq;
  uint32_t depth;

  uobject_t *self;
  ufunc_t   *func;

  fname_t self_fname;
  fname_t class_fname;
  fname_t func_fname;

  ui_text_span_t class_name;
  ui_text_span_t self_name;
  ui_text_span_t func_name;

  uint8_t        source_flags;
  uint8_t        consumed_flags;
  uobject_kind_t self_kind;
  bool           maximized;
};

typedef struct trace_frame_s trace_frame_t;
struct trace_frame_s {
  trace_frame_t *prev;
  trace_frame_t *free_next;

  uobject_t *self;
  ufunc_t   *func;

  trace_call_t     *call;
  trace_hook_kind_t kind;

  bool owns_logical_call;
  bool invoke_seen;
};

typedef struct trace_ui_s trace_ui_t;
struct trace_ui_s {
  bool           closed;
  bool           ignore_case;
  bool           exact_match;
  bool           capture_nested_calls;
  char          *capture_filter_input;
  uint32_t       capture_filter_input_len;
  char          *save_path;
  uint32_t       save_path_len;
  bool           filter_dialog_is_open;
  struct nk_rect filter_dialog_bounds;
  uint32_t       page_idx;
  arena_t        page_arena;

  ui_text_span_t seq;
  ui_text_span_t time;
  ui_text_span_t duration;
  ui_text_span_t depth;
  ui_text_span_t kind;
  ui_text_span_t class_name;
  ui_text_span_t self_name;
  ui_text_span_t func_name;
  ui_text_span_t kind_pe;
  ui_text_span_t kind_invoke;
  ui_text_span_t kind_pe_and_invoke;
};

typedef struct trace_cfg_s trace_cfg_t;
struct trace_cfg_s {
  mod_cfg_handle_t max_calls_h;
  mod_cfg_handle_t open_window_h;
  mod_cfg_handle_t page_size_h;
};

typedef struct trace_tool_s trace_tool_t;
struct trace_tool_s {
  arena_t *perm;
  uint64_t perm_size;

  trace_call_t *first_call;
  trace_call_t *last_call;

  trace_call_t *first_root;
  trace_call_t *last_root;

  trace_call_t *active_captured_call;

  trace_frame_t *frame_top;
  trace_frame_t *frame_free;

  uint32_t logical_depth;
  uint32_t dropped_frame_depth;

  uint32_t call_count;
  uint32_t max_call_count;
  uint32_t seq_counter;

  bool capture_full;
  bool clear_pending;
  bool capturing;

  trace_filter_set_t capture_filter;
  trace_ui_t         ui;
  trace_cfg_t        cfg;

  mod_handle_t       handle;
  struct nk_context *ctx;
  bool               inited;
};

static trace_tool_t g_trace_tool = {0};

static trace_frame_t *
trace_frame_alloc(trace_tool_t *tool)
{
  trace_frame_t *frame = tool->frame_free;
  if (frame) {
    tool->frame_free = frame->free_next;
    mem_zero(frame, sizeof(*frame));
    return frame;
  }

  return ARENA_PUSH_ZERO(tool->perm, trace_frame_t);
}

static void
trace_frame_release(trace_tool_t *tool, trace_frame_t *frame)
{
  ASSERT(tool != NULL);
  ASSERT(frame != NULL);

  frame->free_next = tool->frame_free;
  tool->frame_free = frame;
}

static trace_frame_t *
trace_frame_push(trace_tool_t *tool, trace_hook_kind_t kind, uobject_t *self, ufunc_t *func)
{
  if (tool->dropped_frame_depth > 0) {
    tool->dropped_frame_depth += 1;
    return NULL;
  }

  trace_frame_t *frame = trace_frame_alloc(tool);
  if (!frame) {
    tool->capture_full        = true;
    tool->dropped_frame_depth = 1;
    return NULL;
  }

  frame->kind = kind;
  frame->self = self;
  frame->func = func;
  frame->prev = tool->frame_top;

  tool->frame_top = frame;
  return frame;
}

static trace_frame_t *
trace_frame_pop(trace_tool_t *tool, trace_hook_kind_t kind, uobject_t *self, ufunc_t *func)
{
  trace_frame_t *frame = tool->frame_top;

  ASSERT(frame != NULL);
  ASSERT(frame->kind == kind);
  ASSERT(frame->self == self);
  ASSERT(frame->func == func);

  tool->frame_top = frame->prev;
  return frame;
}

static trace_call_t *
trace_call_alloc(trace_tool_t *tool)
{
  if (tool->call_count >= tool->max_call_count) {
    return NULL;
  }

  trace_call_t *call = ARENA_PUSH_ZERO(tool->perm, trace_call_t);
  if (!call) {
    tool->capture_full = true;
    return NULL;
  }

  call->seq = ++tool->seq_counter;

  if (tool->last_call) {
    tool->last_call->next = call;
  } else {
    tool->first_call = call;
  }

  tool->last_call   = call;
  tool->call_count += 1;

  return call;
}

static void
trace_call_link(trace_tool_t *tool, trace_call_t *call, trace_call_t *parent)
{
  ASSERT(tool != NULL);
  ASSERT(call != NULL);

  call->parent = parent;

  if (parent) {
    if (parent->last_child) {
      parent->last_child->next_sibling = call;
    } else {
      parent->first_child = call;
    }

    parent->last_child = call;
  } else {
    if (tool->last_root) {
      tool->last_root->next_sibling = call;
    } else {
      tool->first_root = call;
    }

    tool->last_root = call;
  }
}

static void
trace_rebuild_capture_rules(trace_tool_t *tool)
{
  ASSERT(tool != NULL);

  trace_filter_set_t *set         = &tool->capture_filter;
  str_t               input       = str_make(tool->ui.capture_filter_input, tool->ui.capture_filter_input_len);
  bool                ignore_case = tool->ui.ignore_case;
  bool                exact_match = tool->ui.exact_match;

  set->rule_count     = 0;
  set->ignore_case    = ignore_case;
  set->exact_match    = exact_match;
  set->preview_len    = 0;
  set->rule_input_len = (uint32_t)str_write_data(input, set->rule_input, TRACE_FILTER_TEXT_CAP);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t splits[] = {
      STR_LIT(";"),
      STR_LIT("\n"),
      STR_LIT("\r"),
    };

    str_list_t tokens = str_split(tmp.arena, str_make(set->rule_input, set->rule_input_len), COUNTOF(splits), splits);
    for (str_node_t *node = tokens.first; node && set->rule_count < TRACE_FILTER_RULE_MAX; node = node->next) {
      str_t raw   = str_trim_space(node->str);
      str_t token = raw;
      if (str_is_empty(raw)) {
        continue;
      }

      trace_filter_sign_t sign = TRACE_FILTER_SIGN_NONE;
      if (token.data[0] == '+') {
        sign = TRACE_FILTER_SIGN_INCLUDE;
      } else if (token.data[0] == '-') {
        sign = TRACE_FILTER_SIGN_EXCLUDE;
      } else {
        continue;
      }

      token = str_slice(token, 1, token.len);
      if (str_is_empty(token)) {
        continue;
      }

      str_t      split  = STR_LIT(":");
      str_list_t fields = str_split(tmp.arena, token, 1, &split);

      str_t target_str = STR_NULL;
      str_t filter_str = STR_NULL;

      if (fields.count == 2) {
        target_str = str_trim_space(fields.first->str);
        filter_str = str_trim_space(fields.last->str);
      } else if (fields.count == 1) {
        target_str = STR_LIT("func");
        filter_str = str_trim_space(fields.first->str);
      } else {
        continue;
      }

      if (str_is_empty(target_str) || str_is_empty(filter_str)) {
        continue;
      }

      trace_filter_target_t target = TRACE_FILTER_TARGET_NONE;
      if (str_equal_icase(target_str, STR_LIT("self_name")) || str_equal_icase(target_str, STR_LIT("self")) || str_equal_icase(target_str, STR_LIT("obj")) ||
          str_equal_icase(target_str, STR_LIT("object"))) {
        target = TRACE_FILTER_TARGET_SELF_NAME;
      } else if (str_equal_icase(target_str, STR_LIT("self_outer_name")) || str_equal_icase(target_str, STR_LIT("self_outer")) ||
                 str_equal_icase(target_str, STR_LIT("obj_outer")) || str_equal_icase(target_str, STR_LIT("object_outer"))) {
        target = TRACE_FILTER_TARGET_SELF_OUTER_NAME;
      } else if (str_equal_icase(target_str, STR_LIT("class_name")) || str_equal_icase(target_str, STR_LIT("class")) || str_equal_icase(target_str, STR_LIT("self_class"))) {
        target = TRACE_FILTER_TARGET_CLASS_NAME;
      } else if (str_equal_icase(target_str, STR_LIT("class_outer_name")) || str_equal_icase(target_str, STR_LIT("class_outer")) ||
                 str_equal_icase(target_str, STR_LIT("self_class_outer"))) {
        target = TRACE_FILTER_TARGET_CLASS_OUTER_NAME;
      } else if (str_equal_icase(target_str, STR_LIT("class_inherit")) || str_equal_icase(target_str, STR_LIT("class_super")) ||
                 str_equal_icase(target_str, STR_LIT("self_super"))) {
        target = TRACE_FILTER_TARGET_CLASS_INHERIT;
      } else if (str_equal_icase(target_str, STR_LIT("func_name")) || str_equal_icase(target_str, STR_LIT("func")) || str_equal_icase(target_str, STR_LIT("function"))) {
        target = TRACE_FILTER_TARGET_FUNC_NAME;
      } else if (str_equal_icase(target_str, STR_LIT("func_name_outer")) || str_equal_icase(target_str, STR_LIT("func_outer_name")) ||
                 str_equal_icase(target_str, STR_LIT("func_outer")) || str_equal_icase(target_str, STR_LIT("function_outer"))) {
        target = TRACE_FILTER_TARGET_FUNC_OUTER_NAME;
      } else {
        continue;
      }

      trace_filter_rule_t *rule = &set->rules[set->rule_count++];
      rule->sign                = sign;
      rule->target              = target;
      rule->text                = filter_str;

      if (set->preview_len < TRACE_FILTER_TEXT_CAP) {
        if (set->preview_len > 0) {
          set->preview_len += stbsp_snprintf(set->preview + set->preview_len, TRACE_FILTER_TEXT_CAP - set->preview_len, ";%.*s", STR_ARG(raw));
        } else {
          set->preview_len += stbsp_snprintf(set->preview + set->preview_len, TRACE_FILTER_TEXT_CAP - set->preview_len, "%.*s", STR_ARG(raw));
        }
      }
    }
  }
  scratch_end(tmp);
}

static bool
trace_uobject_name_matches(uobject_t *obj, str_t text, bool ignore_case, bool exact_match)
{
  return obj && unreal_fname_match_text(obj->name, text, ignore_case, exact_match);
}

static bool
trace_uobject_outer_chain_matches(uobject_t *obj, str_t text, bool ignore_case, bool exact_match)
{
  return unreal_outer_chain_contains(obj ? obj->outer : NULL, text, ignore_case, exact_match);
}

static bool
trace_filter_rule_matches(trace_filter_set_t *set, trace_filter_rule_t *rule, uobject_t *self, ufunc_t *func)
{
  uobject_t *class_obj = (uobject_t *)(self ? self->cls : NULL);
  uobject_t *func_obj  = (uobject_t *)func;

  switch (rule->target) {
  case TRACE_FILTER_TARGET_SELF_NAME:
    return trace_uobject_name_matches(self, rule->text, set->ignore_case, set->exact_match);
  case TRACE_FILTER_TARGET_SELF_OUTER_NAME:
    return trace_uobject_outer_chain_matches(self, rule->text, set->ignore_case, set->exact_match);
  case TRACE_FILTER_TARGET_CLASS_NAME:
    return trace_uobject_name_matches(class_obj, rule->text, set->ignore_case, set->exact_match);
  case TRACE_FILTER_TARGET_CLASS_OUTER_NAME:
    return trace_uobject_outer_chain_matches(class_obj, rule->text, set->ignore_case, set->exact_match);
  case TRACE_FILTER_TARGET_CLASS_INHERIT:
    return unreal_super_chain_contains(self ? self->cls : NULL, rule->text, set->ignore_case, set->exact_match);
  case TRACE_FILTER_TARGET_FUNC_NAME:
    return trace_uobject_name_matches(func_obj, rule->text, set->ignore_case, set->exact_match);
  case TRACE_FILTER_TARGET_FUNC_OUTER_NAME:
    return trace_uobject_outer_chain_matches(func_obj, rule->text, set->ignore_case, set->exact_match);
  }
  return false;
}

static bool
trace_filter_set_passes(trace_filter_set_t *set, uobject_t *self, ufunc_t *func)
{
  bool has_include = false;
  bool any_matched = false;

  for (uint32_t i = 0; i < set->rule_count; ++i) {
    trace_filter_rule_t *rule = &set->rules[i];

    if (rule->sign == TRACE_FILTER_SIGN_INCLUDE) {
      has_include = true;

      if (trace_filter_rule_matches(set, rule, self, func)) {
        any_matched = true;
        break;
      }
    }
  }

  if (has_include && !any_matched) {
    return false;
  }

  for (uint32_t i = 0; i < set->rule_count; ++i) {
    trace_filter_rule_t *rule = &set->rules[i];

    if (rule->sign == TRACE_FILTER_SIGN_EXCLUDE) {
      if (trace_filter_rule_matches(set, rule, self, func)) {
        return false;
      }
    }
  }

  return true;
}

static trace_call_t *
trace_call_root_at(trace_tool_t *tool, uint32_t index)
{
  trace_call_t *call = tool->first_call;
  for (uint32_t i = 0; call && i < index; ++i) {
    call = call->next_sibling;
  }
  return call;
}

static uint32_t
trace_call_root_count(trace_tool_t *tool)
{
  uint32_t count = 0;
  for (trace_call_t *call = tool->first_call; call; call = call->next_sibling) {
    count += 1;
  }
  return count;
}

static void
trace_call_cache_names(trace_tool_t *tool, trace_call_t *call)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(call != NULL);

  if (str_is_empty(call->self_name.str)) {
    call->self_name = ui_text_span_make(tool->ctx, unreal_fname_to_str(call->self_fname, &tool->ui.page_arena));
  }

  if (str_is_empty(call->class_name.str)) {
    call->class_name = ui_text_span_make(tool->ctx, unreal_fname_to_str(call->class_fname, &tool->ui.page_arena));
  }

  if (str_is_empty(call->func_name.str)) {
    call->func_name = ui_text_span_make(tool->ctx, unreal_fname_to_str(call->func_fname, &tool->ui.page_arena));
  }
}

static trace_call_t *
trace_logical_call_begin(trace_tool_t *tool, uobject_t *self, ufunc_t *func, uint8_t source)
{
  uint32_t depth       = tool->logical_depth;
  tool->logical_depth += 1;

  if (!tool->capturing || !self || !func) {
    return NULL;
  }

  bool in_capture_scope = tool->ui.capture_nested_calls && tool->active_captured_call != NULL;

  if (!in_capture_scope && !trace_filter_set_passes(&tool->capture_filter, self, func)) {
    return NULL;
  }

  trace_call_t *call = trace_call_alloc(tool);
  if (!call) {
    return NULL;
  }

  call->start_us     = time_now_us();
  call->end_us       = call->start_us;
  call->depth        = depth;
  call->self         = self;
  call->func         = func;
  call->self_fname   = self->name;
  call->class_fname  = self->cls ? self->cls->base.base.base.name : (fname_t){0};
  call->func_fname   = func->base.base.base.name;
  call->self_kind    = uobject_kind(self);
  call->source_flags = source;

  trace_call_link(tool, call, tool->active_captured_call);

  tool->active_captured_call = call;
  return call;
}

static void
trace_logical_call_end(trace_tool_t *tool, trace_call_t *call)
{
  ASSERT(tool->logical_depth > 0);

  if (call) {
    ASSERT(tool->active_captured_call == call);

    call->end_us = time_now_us();

    tool->active_captured_call = call->parent;
  }

  tool->logical_depth -= 1;
}

static void
trace_clear_now(trace_tool_t *tool)
{
  ASSERT(tool->frame_top == NULL);
  ASSERT(tool->dropped_frame_depth == 0);

  tool->first_call           = NULL;
  tool->last_call            = NULL;
  tool->first_root           = NULL;
  tool->last_root            = NULL;
  tool->active_captured_call = NULL;

  tool->frame_free    = NULL;
  tool->logical_depth = 0;
  tool->call_count    = 0;
  tool->seq_counter   = 0;
  tool->capture_full  = false;
  tool->clear_pending = false;
  tool->ui.page_idx   = 0;
  arena_reset(&tool->ui.page_arena);

  arena_set_used(tool->perm, tool->perm_size);
}

static void
trace_clear(trace_tool_t *tool)
{
  if (tool->frame_top || tool->dropped_frame_depth > 0) {
    tool->clear_pending = true;
    return;
  }

  trace_clear_now(tool);
}

typedef enum detail_tree_row_view_e {
  DETAIL_TREE_ROW_TEXT,
  DETAIL_TREE_ROW_CLOSED,
  DETAIL_TREE_ROW_OPEN,
} detail_tree_row_view_t;

typedef struct ui_detail_row_s ui_detail_row_t;
struct ui_detail_row_s {
  ui_text_cell_t *parts;
  int             count;
  bool            has_children;
  int             depth;
  str_t           copy_text;
};

static str_t
detail_row_push_line(arena_t *arena, ui_detail_row_t row)
{
  ASSERT(arena != NULL);

  str_list_t list = {0};
  for (int i = 0; i < row.count; ++i) {
    if (!str_is_empty(row.parts[i].text.str)) {
      str_list_push(arena, &list, row.parts[i].text.str);
    }
  }
  return str_list_join(arena, list, STR_NULL, STR_LIT("  "), STR_NULL);
}

static void
detail_copy_text(struct nk_context *ctx, str_t text)
{
  if (!ctx || str_is_empty(text) || !ctx->clip.copy) {
    return;
  }
  ctx->clip.copy(ctx->clip.userdata, (const char *)text.data, (int)text.len);
}

static bool
ui_detail_row(trace_tool_t *tool, ui_text_cols_t *cols, ui_detail_row_t row, bool *maximized)
{
  ASSERT(tool != NULL);
  ASSERT(tool->ctx != NULL);
  ASSERT(row.parts != NULL);
  ASSERT(row.count > 0);
  if (cols) {
    ASSERT(cols->count == row.count);
  }

  if (row.has_children) {
    ASSERT(maximized != NULL);
  }

  struct nk_context *ctx = tool->ctx;
  if (!ctx->current || !ctx->current->layout) {
    return false;
  }

  detail_tree_row_view_t view = DETAIL_TREE_ROW_TEXT;
  if (row.has_children) {
    view = *maximized ? DETAIL_TREE_ROW_OPEN : DETAIL_TREE_ROW_CLOSED;
  }

  struct nk_style *style  = &ctx->style;
  struct nk_panel *layout = ctx->current->layout;

  float row_h = style->font->height + 2.0f * style->tab.padding.y;
  float pad_x = style->tab.padding.x;
  float gap_w = ui_text_width(ctx, STR_LIT(" "));

  float indent_w = style->tab.indent;
  if (indent_w <= 0.0f) {
    indent_w = ui_text_width(ctx, STR_LIT("  "));
  }

  if (indent_w <= 0.0f) {
    indent_w = 12.0f;
  }

  float symbol_w = NK_MAX(style->font->height, 12.0f);
  float parts_w  = 0.0f;

  if (cols) {
    for (int i = 0; i < cols->count; ++i) {
      parts_w += cols->width[i];
      if (i + 1 < cols->count) {
        parts_w += gap_w;
      }
    }
  } else {
    for (int i = 0; i < row.count; ++i) {
      parts_w += row.parts[i].text.width;
      if (i + 1 < row.count) {
        parts_w += gap_w;
      }
    }
  }

  row.depth = NK_MAX(row.depth, 0);

  float text_cell_w = pad_x + (float)row.depth * indent_w + symbol_w + gap_w + parts_w + gap_w;
  float row_w       = text_cell_w;

  float visible_w  = NK_MAX(layout->bounds.w - 2.0f * style->window.padding.x, 1.0f);
  float filler_w   = NK_MAX(visible_w - row_w, 0.0f);
  bool  has_filler = AS_BOOL(filler_w > 0.5f);
  bool  expanded   = (row.has_children) ? *maximized : false;

  struct nk_rect row_bounds = {0};

  nk_layout_set_min_row_height(ctx, row_h);
  nk_layout_row_begin(ctx, NK_STATIC, row_h, 1 + has_filler);
  {
    nk_layout_row_push(ctx, text_cell_w);

    struct nk_rect               bounds       = {0};
    enum nk_widget_layout_states layout_state = nk_widget(&bounds, ctx);

    if (layout_state != NK_WIDGET_INVALID) {
      row_bounds   = bounds;
      row_bounds.w = row_w;

      struct nk_input *in = NULL;
      if (layout_state != NK_WIDGET_ROM && layout_state != NK_WIDGET_DISABLED && !(layout->flags & NK_WINDOW_ROM) && !(layout->flags & NK_WINDOW_NO_INPUT) &&
          ui_nk_current_panel_accepts_input(ctx)) {
        in = &ctx->input;
      }

      nk_flags row_state = 0;
      if (in) {
        if (row.has_children) {
          bool clicked = nk_button_behavior(&row_state, row_bounds, in, NK_BUTTON_DEFAULT);
          if (clicked) {
            *maximized = !*maximized;
            expanded   = *maximized;
            view       = *maximized ? DETAIL_TREE_ROW_OPEN : DETAIL_TREE_ROW_CLOSED;
          }

          ctx->last_widget_state = row_state;
        }
      }

      struct nk_command_buffer *out = nk_window_get_canvas(ctx);
      struct nk_color           bg  = UI_C_TRANSPARENT;

      if (row.has_children) {
        bg = DETAIL_C_ROW_COLLAPSIBLE_BG;
      }

      if (row_state & NK_WIDGET_STATE_HOVER) {
        if (row.has_children) {
          bg = DETAIL_C_ROW_COLLAPSIBLE_HOVER;
        }
      }

      if (row_state & NK_WIDGET_STATE_ACTIVE) {
        if (row.has_children) {
          bg = DETAIL_C_ROW_COLLAPSIBLE_ACTIVE;
        }
      }

      if (bg.a > 0) {
        nk_fill_rect(out, row_bounds, style->selectable.rounding, bg);
      }

      float x = bounds.x + pad_x + (float)row.depth * indent_w;
      float y = bounds.y + (bounds.h - symbol_w) * 0.5f;

      struct nk_rect sym = nk_rect(x, y, symbol_w, symbol_w);

      if (view != DETAIL_TREE_ROW_TEXT) {
        enum nk_symbol_type           symbol = NK_SYMBOL_NONE;
        const struct nk_style_button *button = NULL;

        if (view == DETAIL_TREE_ROW_OPEN) {
          symbol = style->tab.sym_maximize;
          button = &style->tab.node_maximize_button;
        } else {
          symbol = style->tab.sym_minimize;
          button = &style->tab.node_minimize_button;
        }

        nk_flags draw_state = 0;
        nk_do_button_symbol(&draw_state, out, sym, symbol, NK_BUTTON_DEFAULT, button, NULL, style->font);
      }

      x += symbol_w + gap_w;

      for (int i = 0; i < row.count; ++i) {
        float max_w = (cols) ? cols->width[i] : row.parts[i].text.width;
        ui_text_cell_draw(ctx, out, nk_rect(x, bounds.y, max_w, bounds.h), row.parts[i], bg);
        x += max_w;

        if (i + 1 < row.count) {
          x += gap_w;
        }
      }
    }

    if (has_filler) {
      nk_layout_row_push(ctx, filler_w);
      nk_spacer(ctx);
    }
  }
  nk_layout_row_end(ctx);
  nk_layout_reset_min_row_height(ctx);

  if (nk_contextual_begin(ctx, NK_WINDOW_BORDER, nk_vec2(190.0f, 92.0f), row_bounds)) {
    if (!str_is_empty(row.copy_text)) {
      nk_layout_row_dynamic(ctx, 20.0f, 1);
      if (nk_contextual_item_label(ctx, "Copy value", NK_TEXT_LEFT)) {
        detail_copy_text(ctx, row.copy_text);
      }
    }

    nk_layout_row_dynamic(ctx, 20.0f, 1);
    if (nk_contextual_item_label(ctx, "Copy row", NK_TEXT_LEFT)) {
      tmp_arena_t tmp = scratch_begin(NULL);
      {
        str_t line = detail_row_push_line(tmp.arena, row);
        detail_copy_text(ctx, line);
      }
      scratch_end(tmp);
    }

    nk_contextual_end(ctx);
  }
  return expanded;
}

static inline int
u32_count_digits(uint32_t v)
{
  int count = 1;
  while (v >= 10) {
    v     /= 10;
    count += 1;
  }
  return count;
}

static ui_text_span_t
trace_call_hook_kind_text(trace_tool_t *tool, trace_call_t *call)
{
  ASSERT(call != NULL);

  bool has_pe     = (call->source_flags & FLAG(TRACE_HOOK_PROCESS_EVENT));
  bool has_invoke = (call->source_flags & FLAG(TRACE_HOOK_INVOKE));

  if (has_pe && has_invoke) {
    return tool->ui.kind_pe_and_invoke;
  } else if (has_pe) {
    return tool->ui.kind_pe;
  } else if (has_invoke) {
    return tool->ui.kind_invoke;
  }
  return ui_text_span_make(tool->ctx, STR_NULL);
}

static str_t
trace_timestamp_push(arena_t *arena, uint64_t time_us)
{
  uint64_t total_ms = (time_us / 1000);
  uint64_t millis   = (total_ms % 1000);
  uint64_t seconds  = (total_ms / 1000) % 60;
  uint64_t minutes  = (total_ms / (60 * 1000)) % 60;
  uint64_t hours    = (total_ms / (60 * 60 * 1000));

  return str_push_fmt(arena, "%02llu:%02llu:%02llu.%03llu", hours, minutes, seconds, millis);
}

static void
draw_detail_call(trace_tool_t *tool, ui_text_cols_t *cols, trace_call_t *call, int depth)
{
  ASSERT(tool != NULL);
  ASSERT(call != NULL);
  ASSERT(cols != NULL);
  ASSERT(cols->count == TRACE_CALL_COL_COUNT);

  uint64_t capture_start_us = (tool->first_call) ? tool->first_call->start_us : 0ULL;

  bool        maximized = false;
  tmp_arena_t tmp       = scratch_begin(NULL);
  {
    ui_text_span_t seq_text      = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%u", call->seq));
    ui_text_span_t duration_text = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%llu", call->end_us - call->start_us));
    ui_text_span_t time_text     = ui_text_span_make(tool->ctx, trace_timestamp_push(tmp.arena, call->start_us - capture_start_us));
    ui_text_span_t depth_text    = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%u", call->depth));
    ui_text_span_t kind_text     = trace_call_hook_kind_text(tool, call);

    trace_call_cache_names(tool, call);

    ui_text_cell_t parts[] = {
      UI_TEXT_CELL(seq_text, UI_C_TEXT),
      UI_TEXT_CELL(time_text, UI_C_TEXT_MUTED),
      UI_TEXT_CELL(duration_text, UI_C_TEXT_MUTED),
      UI_TEXT_CELL(depth_text, UI_C_TEXT_MUTED),
      UI_TEXT_CELL(kind_text, UI_C_TEXT),
      UI_TEXT_CELL(call->class_name, uobject_kind_color(call->self_kind)),
      UI_TEXT_CELL(call->self_name, UI_C_TEXT),
      UI_TEXT_CELL(call->func_name, uobject_kind_color(UOBJECT_KIND_FUNC)),
    };

    ui_detail_row_t row = {
      .parts        = parts,
      .count        = COUNTOF(parts),
      .depth        = depth,
      .has_children = (call->first_child != NULL),
      .copy_text    = STR_NULL,
    };

    maximized = ui_detail_row(tool, cols, row, row.has_children ? &call->maximized : NULL);
  }
  scratch_end(tmp);

  if (maximized) {
    ui_text_cols_t child_cols = {0};
    ui_text_cols_reset(&child_cols, TRACE_CALL_COL_COUNT);

    ui_text_cols_include(&child_cols, 0, tool->ui.seq);
    ui_text_cols_include(&child_cols, 1, tool->ui.time);
    ui_text_cols_include(&child_cols, 2, tool->ui.duration);
    ui_text_cols_include(&child_cols, 3, tool->ui.depth);
    ui_text_cols_include(&child_cols, 4, tool->ui.kind);
    ui_text_cols_include(&child_cols, 5, tool->ui.class_name);
    ui_text_cols_include(&child_cols, 6, tool->ui.self_name);
    ui_text_cols_include(&child_cols, 7, tool->ui.func_name);

    uint32_t max_seq      = 0;
    uint64_t max_duration = 0;
    uint32_t max_depth    = 0;
    for (trace_call_t *child = call->first_child; child; child = child->next_sibling) {
      max_seq      = MAX_VAL(max_seq, child->seq);
      max_duration = MAX_VAL(max_duration, child->end_us - child->start_us);
      max_depth    = MAX_VAL(max_depth, child->depth);

      trace_call_cache_names(tool, child);

      ui_text_cols_include(&child_cols, 4, trace_call_hook_kind_text(tool, child));
      ui_text_cols_include(&child_cols, 5, child->class_name);
      ui_text_cols_include(&child_cols, 6, child->self_name);
      ui_text_cols_include(&child_cols, 7, child->func_name);
    }

    tmp_arena_t tmp = scratch_begin(NULL);
    {
      ui_text_span_t seq_text      = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%u", max_seq));
      ui_text_span_t duration_text = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%llu", max_duration));
      ui_text_span_t time_text     = ui_text_span_make(tool->ctx, STR_LIT("00:00:00.000"));
      ui_text_span_t depth_text    = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%u", max_depth));

      ui_text_cols_include(&child_cols, 0, seq_text);
      ui_text_cols_include(&child_cols, 1, time_text);
      ui_text_cols_include(&child_cols, 2, duration_text);
      ui_text_cols_include(&child_cols, 3, depth_text);
    }
    scratch_end(tmp);

    for (trace_call_t *child = call->first_child; child; child = child->next_sibling) {
      draw_detail_call(tool, &child_cols, child, depth + 1);
    }
  }
}

static uint32_t
trace_page_count(uint32_t item_count, uint32_t page_size)
{
  if (page_size == 0) {
    return 1;
  }
  return MAX_VAL(1u, (item_count + page_size - 1u) / page_size);
}

static bool
trace_checkbox_label(struct nk_context *ctx, const char *label, bool *v)
{
  nk_bool value   = *v ? nk_true : nk_false;
  bool    changed = nk_checkbox_label(ctx, label, &value) != 0;
  if (changed) {
    *v = value != nk_false;
  }
  return changed;
}

static void
trace_filter_dialog_open(trace_tool_t *tool, unsigned int vw, unsigned int vh)
{
  if (!tool) {
    return;
  }

  float w = 620.0f;
  float h = 420.0f;
  float x = ((float)vw - w) * 0.5f;
  float y = ((float)vh - h) * 0.5f;

  tool->ui.filter_dialog_is_open = true;
  tool->ui.filter_dialog_bounds  = nk_rect(NK_MAX(x, 0.0f), NK_MAX(y, 0.0f), w, h);
}

static void
trace_filter_dialog_close(trace_tool_t *tool)
{
  if (tool) {
    tool->ui.filter_dialog_is_open = false;
  }
}

static void
trace_filter_tooltip(struct nk_context *ctx, char *text, uint32_t len)
{
  if (!ctx || !nk_widget_is_hovered(ctx)) {
    return;
  }

  if (text && len > 0) {
    nk_tooltip_text(ctx, text, (int)len);
  }
}

static void
trace_draw_filter_dialog(trace_tool_t *tool, struct nk_context *ctx)
{
  if (!tool || !ctx || !tool->ui.filter_dialog_is_open) {
    return;
  }

  const char *name  = "ufunction_tracer.capture_filter";
  const char *title = "Filter rules for capturing";
  nk_flags    flags = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE | NK_WINDOW_NO_SCROLLBAR;

  if (nk_begin_titled(ctx, name, title, tool->ui.filter_dialog_bounds, flags)) {
    tool->ui.filter_dialog_bounds = nk_window_get_bounds(ctx);

    struct nk_rect content = nk_window_get_content_region(ctx);
    float          edit_h  = content.h - 2.0f * ctx->style.window.padding.y;

    nk_layout_row_dynamic(ctx, edit_h, 1);
    {
      int len = (int)tool->ui.capture_filter_input_len;
      nk_edit_string(ctx, NK_EDIT_BOX, tool->ui.capture_filter_input, &len, TRACE_FILTER_TEXT_CAP, nk_filter_default);
      tool->ui.capture_filter_input_len = (uint32_t)len;

      str_t before = str_make(tool->capture_filter.rule_input, tool->capture_filter.rule_input_len);
      str_t after  = str_make(tool->ui.capture_filter_input, tool->ui.capture_filter_input_len);

      if (!str_equal(before, after, 0)) {
        trace_rebuild_capture_rules(tool);
      }
    }
  }
  nk_end(ctx);

  if (nk_window_is_closed(ctx, name)) {
    trace_filter_dialog_close(tool);
  }
}

static void
trace_save(trace_tool_t *tool)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_t save_path = str_make(tool->ui.save_path, tool->ui.save_path_len);
    dir_create_recursive(path_dir(save_path));

    FILE *file = fopen(str_push_cstr(tmp.arena, save_path), "wb");
    if (file) {
      int max_seq   = 0;
      int max_depth = 0;

      uint64_t first_start_us = (tool->first_call) ? tool->first_call->start_us : 0ULL;
      uint32_t first_depth    = (tool->first_call) ? tool->first_call->depth : 0;

      int max_cls_name_len = (int)STR_LIT(TRACE_CALL_COL_CLASS).len;
      int max_obj_name_len = (int)STR_LIT(TRACE_CALL_COL_OBJECT).len;

      for (trace_call_t *call = tool->first_call; call; call = call->next) {
        max_seq   = MAX_VAL(max_seq, (int)call->seq);
        max_depth = MAX_VAL(max_depth, (int)call->depth);

        max_cls_name_len = MAX_VAL(max_cls_name_len, (int)unreal_fname_utf8_len(call->class_fname));
        max_obj_name_len = MAX_VAL(max_obj_name_len, (int)unreal_fname_utf8_len(call->self_fname));
      }

      int max_seq_len         = MAX_VAL(u32_count_digits(max_seq), (int)STR_LIT(TRACE_CALL_COL_SEQ).len);
      int max_depth_len       = MAX_VAL(u32_count_digits(max_depth), (int)STR_LIT(TRACE_CALL_COL_DEPTH).len);
      int max_duration_us_len = MAX_VAL(12, (int)STR_LIT(TRACE_CALL_COL_DURATION).len);
      int max_time_len        = MAX_VAL(12, (int)STR_LIT(TRACE_CALL_COL_TIME).len);
      int max_kind_len        = (int)STR_LIT(TRACE_CALL_COL_KIND).len;

      str_t indent = str_push_fill_byte(tmp.arena, 2 * first_depth, ' ');

      fprintf(file, "%-*s  %-*s  %-*s  %-*s  %-*s  %.*s%-*s  %-*s  %s\n",
              max_seq_len,         TRACE_CALL_COL_SEQ,
              max_time_len,        TRACE_CALL_COL_TIME,
              max_duration_us_len, TRACE_CALL_COL_DURATION,
              max_depth_len,       TRACE_CALL_COL_DEPTH,
              max_kind_len,        TRACE_CALL_COL_KIND,
              STR_ARG(indent),
              max_cls_name_len,    TRACE_CALL_COL_CLASS,
              max_obj_name_len,    TRACE_CALL_COL_OBJECT,
              TRACE_CALL_COL_FUNCTION);

      for (trace_call_t *call = tool->first_call; call; call = call->next) {
        tmp_arena_t tmp2 = scratch_begin(tmp.arena);
        {
          uint64_t duration_us = 0;
          if (call->end_us >= call->start_us) {
            duration_us = call->end_us - call->start_us;
          }

          str_t indent = str_push_fill_byte(tmp.arena, 2 * call->depth, ' ');

          str_t time_str   = trace_timestamp_push(tmp.arena, call->start_us - first_start_us);
          str_t class_name = unreal_fname_to_str(call->class_fname, tmp.arena);
          str_t self_name  = unreal_fname_to_str(call->self_fname, tmp.arena);
          str_t func_name  = unreal_fname_to_str(call->func_fname, tmp.arena);

          fprintf(file, "%*u  %-*.*s  %*llu  %*u  %-*.*s  %.*s%-*.*s  %-*.*s  %.*s\n",
                  max_seq_len,         call->seq,
                  max_time_len,        STR_ARG(time_str),
                  max_duration_us_len, duration_us,
                  max_depth_len,       call->depth,
                  max_kind_len,        STR_ARG(trace_call_hook_kind_text(tool, call).str),
                  STR_ARG(indent),
                  max_cls_name_len,    STR_ARG(class_name),
                  max_obj_name_len,    STR_ARG(self_name),
                  STR_ARG(func_name));
        }
        scratch_end(tmp2);
      }
      fclose(file);
    }
  }
  scratch_end(tmp);
}

static void
trace_draw_window(trace_tool_t *tool, unsigned int vw, unsigned int vh)
{
  struct nk_context *ctx  = tool->ctx;
  const char        *name = "ufunction_tracer";

  if (tool->ui.closed) {
    trace_filter_dialog_close(tool);
    if (!nk_window_is_closed(ctx, name)) {
      nk_window_close(ctx, name);
    }
    return;
  }

  nk_window_set_maximize_bounds(ctx, nk_rect(0.0f, 0.0f, (float)vw, (float)vh));

  nk_flags flags =
    NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MAXIMIZABLE;

  char title[256];
  stbsp_snprintf(title, sizeof(title), "UFunction Tracer - %s - %u calls", tool->capturing ? "capturing" : "stopped", tool->call_count);

  if (nk_begin_titled(ctx, name, title, nk_rect(130.0f, 90.0f, 900.0f, 650.0f), flags)) {
    nk_layout_row_begin(ctx, NK_DYNAMIC, 24.0f, 5);
    {
      nk_layout_row_push(ctx, 0.2f);
      if (ui_button_str(ctx, tool->capturing ? STR_LIT("Stop capture") : STR_LIT("Start capture"))) {
        if (!tool->capturing) {
          trace_rebuild_capture_rules(tool);
        }
        tool->capturing = !tool->capturing;
      }

      nk_layout_row_push(ctx, 0.2f);
      if (ui_button_str(ctx, STR_LIT("Clear calls"))) {
        trace_clear(tool);
      }

      nk_layout_row_push(ctx, 0.2f);
      nk_label(ctx, "Output file:", NK_TEXT_RIGHT);

      nk_layout_row_push(ctx, 0.2f);
      {
        int len = (int)tool->ui.save_path_len;
        nk_edit_string(ctx, NK_EDIT_FIELD, tool->ui.save_path, &len, TRACE_SAVE_PATH_MAX, nk_filter_default);
        tool->ui.save_path_len = len;
      }

      nk_layout_row_push(ctx, 0.2f);
      if (ui_button_str(ctx, STR_LIT("Save trace"))) {
        trace_save(tool);
      }
    }
    nk_layout_row_end(ctx);

    if (nk_tree_push(ctx, NK_TREE_TAB, "Capture filters", NK_MINIMIZED)) {
      nk_layout_row_dynamic(ctx, 24.0f, 1);
      {
        trace_filter_tooltip(ctx, tool->capture_filter.preview, tool->capture_filter.preview_len);
        if (ui_button_str(ctx, STR_LIT("Edit filters"))) {
          trace_filter_dialog_open(tool, vw, vh);
        }
      }

      nk_layout_row_begin(ctx, NK_DYNAMIC, 24.0f, 4);
      {
        nk_layout_row_push(ctx, 0.25f);
        trace_checkbox_label(ctx, "Capture nested calls", &tool->ui.capture_nested_calls);
        if (nk_widget_is_hovered(ctx)) {
          nk_tooltip(ctx, "Also capture calls made by a captured call before it returns");
        }

        nk_layout_row_push(ctx, 0.25f);
        trace_checkbox_label(ctx, "Ignore case", &tool->ui.ignore_case);

        nk_layout_row_push(ctx, 0.25f);
        trace_checkbox_label(ctx, "Exact match", &tool->ui.exact_match);

        nk_layout_row_push(ctx, 0.25f);
        nk_spacer(ctx);
      }
      nk_layout_row_end(ctx);

      nk_tree_pop(ctx);
    }

    uint32_t call_root_count = trace_call_root_count(tool);
    uint32_t page_size       = (uint32_t)MAX_VAL(1, mod_cfg_get_int(&globals.mod_manager, tool->cfg.page_size_h));
    uint32_t page_count      = trace_page_count(call_root_count, page_size);
    if (tool->ui.page_idx >= page_count) {
      tool->ui.page_idx = page_count - 1;
    }

    if (page_count > 1) {
      nk_layout_row_begin(ctx, NK_DYNAMIC, 22.0f, 3);
      {
        nk_layout_row_push(ctx, 0.20f);
        if (ui_button_str(ctx, STR_LIT("<")) && tool->ui.page_idx > 0) {
          tool->ui.page_idx -= 1;
        }

        nk_layout_row_push(ctx, 0.60f);
        nk_labelf(ctx, NK_TEXT_CENTERED, "Page %u / %u", tool->ui.page_idx + 1, page_count);

        nk_layout_row_push(ctx, 0.20f);
        if (ui_button_str(ctx, STR_LIT(">")) && tool->ui.page_idx + 1 < page_count) {
          tool->ui.page_idx += 1;
        }
      }
      nk_layout_row_end(ctx);
    }

    float results_h = nk_layout_get_remaining_height(ctx);

    nk_layout_row_dynamic(ctx, results_h, 1);
    if (nk_group_begin(ctx, "ufunction_trace_calls", NK_WINDOW_BORDER)) {
      if (call_root_count > 0) {
        ui_text_cols_t cols = {0};
        ui_text_cols_reset(&cols, TRACE_CALL_COL_COUNT);

        ui_text_cols_include(&cols, 0, tool->ui.seq);
        ui_text_cols_include(&cols, 1, tool->ui.time);
        ui_text_cols_include(&cols, 2, tool->ui.duration);
        ui_text_cols_include(&cols, 3, tool->ui.depth);
        ui_text_cols_include(&cols, 4, tool->ui.kind);
        ui_text_cols_include(&cols, 5, tool->ui.class_name);
        ui_text_cols_include(&cols, 6, tool->ui.self_name);
        ui_text_cols_include(&cols, 7, tool->ui.func_name);

        uint32_t first_idx = tool->ui.page_idx * page_size;
        uint32_t last_idx  = MIN_VAL(first_idx + page_size, call_root_count);

        trace_call_t *first_call = trace_call_root_at(tool, first_idx);
        trace_call_t *last_call  = trace_call_root_at(tool, last_idx);

        uint32_t max_seq      = 0;
        uint64_t max_duration = 0;
        uint32_t max_depth    = 0;
        for (trace_call_t *call = first_call; call != NULL && call != last_call; call = call->next_sibling) {
          max_seq      = MAX_VAL(max_seq, call->seq);
          max_duration = MAX_VAL(max_duration, call->end_us - call->start_us);
          max_depth    = MAX_VAL(max_depth, call->depth);

          trace_call_cache_names(tool, call);

          ui_text_cols_include(&cols, 4, trace_call_hook_kind_text(tool, call));
          ui_text_cols_include(&cols, 5, call->class_name);
          ui_text_cols_include(&cols, 6, call->self_name);
          ui_text_cols_include(&cols, 7, call->func_name);
        }

        tmp_arena_t tmp = scratch_begin(NULL);
        {
          ui_text_span_t seq_text      = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%u", max_seq));
          ui_text_span_t time_text     = ui_text_span_make(tool->ctx, STR_LIT("00:00:00.000"));
          ui_text_span_t duration_text = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%llu", max_duration));
          ui_text_span_t depth_text    = ui_text_span_make(tool->ctx, str_push_fmt(tmp.arena, "%u", max_depth));

          ui_text_cols_include(&cols, 0, seq_text);
          ui_text_cols_include(&cols, 1, time_text);
          ui_text_cols_include(&cols, 2, duration_text);
          ui_text_cols_include(&cols, 3, depth_text);
        }
        scratch_end(tmp);

        ui_text_cell_t header_parts[] = {
          UI_TEXT_CELL(tool->ui.seq, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.time, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.duration, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.depth, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.kind, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.class_name, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.self_name, UI_C_TEXT),
          UI_TEXT_CELL(tool->ui.func_name, UI_C_TEXT),
        };

        ui_detail_row_t row = {
          .parts        = header_parts,
          .count        = COUNTOF(header_parts),
          .depth        = 0,
          .has_children = false,
          .copy_text    = STR_NULL,
        };
        ui_detail_row(tool, &cols, row, NULL);

        for (trace_call_t *call = first_call; call != NULL && call != last_call; call = call->next_sibling) {
          draw_detail_call(tool, &cols, call, 0);
        }
      }
      nk_group_end(ctx);
    }
  }
  nk_end(ctx);

  if (nk_window_is_closed(ctx, name)) {
    tool->ui.closed = true;
    trace_filter_dialog_close(tool);
  }

  trace_draw_filter_dialog(tool, ctx);
}

static bool MOD_CALL
trace_init(const mod_host_api_t *host, mod_handle_t h)
{
  UNUSED_VAR(host);

  trace_tool_t *tool = &g_trace_tool;
  mem_zero(tool, sizeof(*tool));

  tool->perm              = mod_arena_handle_resolve(mod_get_perm_arena(&globals.mod_manager, h));
  tool->cfg.max_calls_h   = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_MAX_CALLS_ID));
  tool->cfg.open_window_h = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_OPEN_WINDOW_ID));
  tool->cfg.page_size_h   = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_PAGE_SIZE_ID));

  tool->max_call_count = (uint32_t)MAX_VAL(1, mod_cfg_get_int(&globals.mod_manager, tool->cfg.max_calls_h));

  tool->ui.capture_filter_input     = ARENA_PUSH_ARRAY_ZERO(tool->perm, char, TRACE_FILTER_TEXT_CAP);
  tool->ui.capture_filter_input_len = 0;
  tool->ui.save_path                = ARENA_PUSH_ARRAY_ZERO(tool->perm, char, TRACE_SAVE_PATH_MAX);
  tool->ui.save_path_len            = 0;

  tool->capture_filter.rules       = ARENA_PUSH_ARRAY_ZERO(tool->perm, trace_filter_rule_t, TRACE_FILTER_RULE_MAX);
  tool->capture_filter.rule_count  = 0;
  tool->capture_filter.rule_input  = ARENA_PUSH_ARRAY_ZERO(tool->perm, char, TRACE_FILTER_TEXT_CAP);
  tool->capture_filter.preview     = ARENA_PUSH_ARRAY_ZERO(tool->perm, char, TRACE_FILTER_TEXT_CAP);
  tool->capture_filter.preview_len = 0;

  if (!tool->ui.capture_filter_input || !tool->ui.save_path || !tool->capture_filter.rules || !tool->capture_filter.rule_input || !tool->capture_filter.preview) {
    LOG_ERROR("failed to allocate memory for internal structures");
    return false;
  }

  tool->ui.page_arena = arena_new_dynamic(64 * MB, 64 * KB);
  if (!tool->ui.page_arena.backing) {
    LOG_ERROR("failed to create arena");
    return false;
  }

  tool->perm_size = arena_get_used(tool->perm);

  tool->ui.closed               = true;
  tool->ui.ignore_case          = true;
  tool->ui.exact_match          = false;
  tool->ui.capture_nested_calls = false;
  tool->ui.page_idx             = 0;
  tool->ui.seq                  = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_SEQ));
  tool->ui.time                 = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_TIME));
  tool->ui.duration             = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_DURATION));
  tool->ui.depth                = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_DEPTH));
  tool->ui.kind                 = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_KIND));
  tool->ui.class_name           = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_CLASS));
  tool->ui.self_name            = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_OBJECT));
  tool->ui.func_name            = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_COL_FUNCTION));
  tool->ui.kind_pe              = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_HOOK_KIND_PE));
  tool->ui.kind_invoke          = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_HOOK_KIND_INVOKE));
  tool->ui.kind_pe_and_invoke   = ui_text_span_make(globals.ui_manager.ctx, STR_LIT(TRACE_CALL_HOOK_KIND_PE_AND_INVOKE));

  tool->ctx    = globals.ui_manager.ctx;
  tool->handle = h;
  tool->inited = true;

  tool->ui.save_path_len = (uint32_t)str_write_data(STR_LIT("trace.txt"), tool->ui.save_path, TRACE_SAVE_PATH_MAX);
  return true;
}

static void MOD_CALL
trace_deinit(mod_handle_t h)
{
  UNUSED_VAR(h);

  trace_tool_t *tool = &g_trace_tool;
  arena_destroy(&tool->ui.page_arena);
  mem_zero(tool, sizeof(*tool));
}

static void MOD_CALL
trace_tick(mod_handle_t h, float delta)
{
  UNUSED_VAR(h);
  UNUSED_VAR(delta);

  trace_tool_t *tool = &g_trace_tool;
  if (!tool->inited) {
    return;
  }

  if (globals.ui_manager.inited && globals.ui_manager.ctx) {
    tool->ctx = globals.ui_manager.ctx;
    trace_draw_window(tool, globals.ui_manager.vw, globals.ui_manager.vh);
  }
}

static bool MOD_CALL
trace_input(mod_handle_t h, input_event_t *ev)
{
  UNUSED_VAR(h);

  trace_tool_t *tool = &g_trace_tool;
  if (!tool->inited) {
    return false;
  }

  keybind_t toggle = mod_cfg_get_keybind(&globals.mod_manager, tool->cfg.open_window_h);
  if (input_event_covers_keybind(ev, toggle)) {
    if (keybind_is_pressed(toggle)) {
      tool->ui.closed = !tool->ui.closed;
    }
    return true;
  }

  return false;
}

static bool MOD_CALL
trace_process_event_pre(mod_handle_t h, uobject_t *self, ufunc_t *func, void *params)
{
  UNUSED_VAR(h);
  UNUSED_VAR(params);

  trace_tool_t *tool = &g_trace_tool;
  if (!tool->inited) {
    return false;
  }

  trace_frame_t *frame = trace_frame_push(tool, TRACE_HOOK_PROCESS_EVENT, self, func);
  if (!frame) {
    return false;
  }

  frame->owns_logical_call = true;
  frame->call              = trace_logical_call_begin(tool, self, func, FLAG(TRACE_HOOK_PROCESS_EVENT));
  return false;
}

static bool MOD_CALL
trace_ufunction_invoke_pre(mod_handle_t h, ufunc_t *func, uobject_t *self, fframe_t *stack, void *result)
{
  UNUSED_VAR(h);
  UNUSED_VAR(stack);
  UNUSED_VAR(result);

  trace_tool_t *tool = &g_trace_tool;
  if (!tool->inited) {
    return false;
  }

  trace_frame_t *parent = tool->frame_top;
  trace_frame_t *frame  = trace_frame_push(tool, TRACE_HOOK_INVOKE, self, func);
  if (!frame) {
    return false;
  }

  bool merge = parent && parent->kind == TRACE_HOOK_PROCESS_EVENT && parent->self == self && parent->func == func && !parent->invoke_seen;
  if (merge) {
    parent->invoke_seen = true;

    frame->owns_logical_call = false;
    frame->call              = parent->call;

    if (frame->call) {
      frame->call->source_flags |= FLAG(TRACE_HOOK_INVOKE);
    }
  } else {
    frame->owns_logical_call = true;
    frame->call              = trace_logical_call_begin(tool, self, func, FLAG(TRACE_HOOK_INVOKE));
  }

  return false;
}

static void
trace_hook_post(trace_tool_t *tool, trace_hook_kind_t kind, uobject_t *self, ufunc_t *func, uint8_t consumed_source, bool consumed)
{
  if (tool->dropped_frame_depth > 0) {
    tool->dropped_frame_depth -= 1;

    if (tool->dropped_frame_depth == 0 && !tool->frame_top && tool->clear_pending) {
      trace_clear_now(tool);
    }

    return;
  }

  trace_frame_t *frame = trace_frame_pop(tool, kind, self, func);

  if (frame->owns_logical_call) {
    trace_logical_call_end(tool, frame->call);
  }

  if (frame->call && consumed) {
    frame->call->consumed_flags |= consumed_source;
  }

  trace_frame_release(tool, frame);

  if (!tool->frame_top && tool->clear_pending) {
    trace_clear_now(tool);
  }
}

static void MOD_CALL
trace_process_event_post(mod_handle_t h, uobject_t *self, ufunc_t *func, void *params, bool consumed)
{
  UNUSED_VAR(h);
  UNUSED_VAR(params);

  trace_hook_post(&g_trace_tool, TRACE_HOOK_PROCESS_EVENT, self, func, FLAG(TRACE_HOOK_PROCESS_EVENT), consumed);
}

static void MOD_CALL
trace_ufunction_invoke_post(mod_handle_t h, ufunc_t *func, uobject_t *self, fframe_t *stack, void *result, bool consumed)
{
  UNUSED_VAR(h);
  UNUSED_VAR(stack);
  UNUSED_VAR(result);

  trace_hook_post(&g_trace_tool, TRACE_HOOK_INVOKE, self, func, FLAG(TRACE_HOOK_INVOKE), consumed);
}

static void MOD_CALL
trace_draw_panel(mod_handle_t h, struct nk_context *ctx)
{
  UNUSED_VAR(h);

  trace_tool_t *tool = &g_trace_tool;
  if (!tool->inited) {
    return;
  }

  static bool panel_maximized = true;
  if (ui_tree_state_push(ctx, NK_TREE_TAB, STR_LIT("Tool"), &panel_maximized)) {
    nk_layout_row_dynamic(ctx, 28.0f, 1);
    if (ui_button_str(ctx, tool->ui.closed ? STR_LIT("Open window") : STR_LIT("Close window"))) {
      tool->ui.closed = !tool->ui.closed;
    }
    nk_tree_pop(ctx);
  }
}

void
register_builtin_ufunction_tracer(mod_manager_t *manager)
{
  keybind_t open_window_toggle_bind = keybind_parse(STR_LIT("F3"), KEYBIND_NULL);

  mod_option_info_t options[] = {
    {
      .type        = MOD_OPTION_INT,
      .id          = STR_CLIT(CFG_MAX_CALLS_ID),
      .label       = STR_CLIT("Maximum captured calls"),
      .description = STR_CLIT("Maximum number of calls retained in memory"),
      .val =
        {
          .integer =
            {
              .min_val     = 256,
              .default_val = 64 * KB,
              .max_val     = 1 * MB,
              .step        = 256,
            },
        },
    },
    {
      .type        = MOD_OPTION_INT,
      .id          = STR_CLIT(CFG_PAGE_SIZE_ID),
      .label       = STR_CLIT("Lines per page"),
      .description = STR_CLIT("Maximum number of root trace calls per page"),
      .val =
        {
          .integer =
            {
              .min_val     = 10,
              .default_val = 250,
              .max_val     = 5000,
              .step        = 10,
            },
        },
    },
    {.type        = MOD_OPTION_KEYBIND,
     .id          = STR_CLIT(CFG_OPEN_WINDOW_ID),
     .label       = STR_CLIT("Window shortcut"),
     .description = STR_CLIT("Shortcut to open the UFunction Tracer window"),
     .val =
       {
         .keybind =
           {
             .default_val = open_window_toggle_bind,
           },
       }},
  };

  mod_manifest_t manifest = {
    .info =
      {
        .id          = STR_CLIT("ufunction-tracer"),
        .name        = STR_CLIT("UFunction Tracer"),
        .author      = STR_CLIT("akmubi"),
        .description = STR_CLIT("Records UFunction calls from UObject->ProcessEvent/UFunction->Invoke."),
        .kind        = MOD_KIND_TOOL,
        .version     = MAKE_VERSION(0, 1, 0),
      },
    .options      = options,
    .option_count = COUNTOF(options),
  };

  mod_dll_runtime_funcs_t funcs = {
    .init             = trace_init,
    .deinit           = trace_deinit,
    .tick             = trace_tick,
    .input            = trace_input,
    .pe_pre           = trace_process_event_pre,
    .pe_post          = trace_process_event_post,
    .func_invoke_pre  = trace_ufunction_invoke_pre,
    .func_invoke_post = trace_ufunction_invoke_post,
    .draw_panel       = trace_draw_panel,
  };

  mod_handle_t h = mod_register_builtin(manager, &manifest, funcs);
  if (h == MOD_HANDLE_INVALID) {
    CONSOLE_ERROR("failed to register builtin UFunction Tracer tool");
  }
}
