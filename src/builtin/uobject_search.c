#include "builtin.h"
#include "common.h"
#include "string_pool.h"
#include "ui_nuklear.h"
#include "uobject_search_internal.h"

#include "arena.h"
#include "globals.h"
#include "input.h"
#include "log.h"
#include "mod_host.h"
#include "mod_manager.h"
#include "nk_d3d12.h"
#include "scratch.h"
#include "str.h"
#include "unreal.h"

#include "vendor_nuklear.h"
#include "vendor_stb.h"

#include <string.h> // memcmp

#define CFG_PAGE_SIZE_ID   "page-size"
#define CFG_WORK_BUDGET_ID "work-budget"
#define CFG_OPEN_WINDOW_ID "open-window"

static search_tool_t g_tool = {0};

static uint32_t
progress_pct(uint32_t current, uint32_t max)
{
  if (max == 0) {
    return 100;
  }

  uint64_t pct = (current * 100ull) / max;
  return (uint32_t)MIN_VAL(pct, 100);
}

bool
work_budget_exhausted(search_tool_t *tool, uint64_t start_us)
{
  uint64_t work_budget_us = (uint64_t)mod_cfg_get_int(&globals.mod_manager, tool->cfg.work_budget_h);
  return time_now_us() - start_us >= work_budget_us;
}

static uint32_t
get_page_count(slot_list_t *results, uint32_t page_size)
{
  if (!results || page_size == 0) {
    return 1;
  }

  return NK_MAX(1u, (results->count + page_size - 1) / page_size);
}

static void
clamp_page(search_tool_t *tool, slot_list_t *results)
{
  uint32_t max_results = (uint32_t)mod_cfg_get_int(&globals.mod_manager, tool->cfg.max_results_h);
  uint32_t page_count  = get_page_count(results, max_results);
  if (tool->ui.page_index >= page_count) {
    tool->ui.page_index = page_count - 1;
  }
}

static bool
ui_checkbox(struct nk_context *ctx, const char *label, bool *v)
{
  nk_bool check   = *v;
  bool    changed = nk_checkbox_label(ctx, label, &check);
  if (changed) {
    *v = check;
  }
  return changed;
}

static float
ui_uobject_row_width(struct nk_context *ctx, str_t text)
{
  if (!ctx) {
    return 0.0f;
  }

  float gutter_w = 3.0f;
  float pad_x    = 10.0f;
  float text_w   = ui_text_width(ctx, text);

  return gutter_w + pad_x + text_w;
}

static bool
ui_uobject_row(struct nk_context *ctx, float row_h, ui_text_cols_t *cols, ui_text_cell_t type_cell, ui_text_cell_t full_name_cell)
{
  ASSERT(ctx != NULL);
  ASSERT(cols != NULL);
  ASSERT(cols->count == 2);

  float pad_x       = 10.0f;
  float gap_x       = 6.0f;
  float type_w      = cols->width[0];
  float full_name_w = cols->width[1];
  float row_w       = pad_x + type_w + gap_x + full_name_w;
  float max_row_w   = NK_MAX(ctx->current->layout->bounds.w, row_w);

  bool clicked = false;
  nk_layout_row_begin(ctx, NK_STATIC, row_h, 1);
  {
    ui_nk_item_t item = {0};
    nk_layout_row_push(ctx, row_w);
    if (ui_nk_item_begin(ctx, &item)) {
      clicked = ui_nk_selectable_behavior(ctx, &item);

      struct nk_command_buffer *out = nk_window_get_canvas(ctx);

      struct nk_color bg = ui_nk_select_bg(ctx, item.widget_state, false);

      float x = item.bounds.x;
      float y = item.bounds.y;
      float h = item.bounds.h;

      nk_fill_rect(out, nk_rect(x, y, max_row_w, h), ctx->style.selectable.rounding, bg);

      ui_text_cell_draw(ctx, out, nk_rect(x, y, type_w, h), type_cell, bg);
      x += type_w + gap_x;

      ui_text_cell_draw(ctx, out, nk_rect(x, y, full_name_w, h), full_name_cell, bg);
    }
  }
  nk_layout_row_end(ctx);
  return clicked;
}

static void
draw_window(search_tool_t *tool, unsigned int vw, unsigned int vh)
{
  struct nk_context *ctx = tool->ctx;
  ASSERT(ctx != NULL);

  const char *name = "uobject_search";

  if (tool->ui.closed) {
    if (!nk_window_is_closed(ctx, name)) {
      nk_window_close(ctx, name);
    }
    return;
  }

  nk_window_set_maximize_bounds(ctx, nk_rect(0.0f, 0.0f, (float)vw, (float)vh));

  nk_flags flags =
    NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MAXIMIZABLE;

  static char title[256] = {0};

  if (build_in_progress(tool)) {
    uint32_t current = build_in_progress(tool) ? tool->cache.build_cursor : tool->cache.build_total;
    uint32_t total   = tool->cache.build_total;
    uint32_t pct     = progress_pct(current, total);

    stbsp_snprintf(title, sizeof(title), "UObject Search - Indexing: %u%% (%u / %u)", pct, current, total);
  } else if (query_is_empty(&tool->search.query)) {
    stbsp_snprintf(title, sizeof(title), "UObject Search");
  } else if (tool->search.search_cursor < tool->search.search_total) {
    MASSERT(tool->search.search_cursor <= tool->search.search_total, "count: %u, max: %u", tool->search.search_cursor, tool->search.search_total);
    uint32_t current = tool->search.search_cursor;
    uint32_t total   = tool->search.search_total;
    uint32_t pct     = progress_pct(current, total);
    stbsp_snprintf(title, sizeof(title), "UObject Search - Searching: %u%% (%u / %u)", pct, current, total);
  } else if (tool->search.matches.count == 0) {
    stbsp_snprintf(title, sizeof(title), "UObject Search - No results");
  } else {
    stbsp_snprintf(title, sizeof(title), "UObject Search - %u matches (%u visible)", tool->search.matches.count, tool->search.visible.count);
  }

  const float min_w = 700.0f;
  const float min_h = 500.0f;

  if (nk_begin_titled(ctx, name, title, nk_rect(120.0f, 80.0f, min_w, min_h), flags)) {
    float search_h  = 28.0f;
    float filters_h = 20.0f;
    float gap_y     = ctx->style.window.spacing.y;

    float splitter_h    = 8.0f;
    float min_results_h = 120.0f;
    float min_details_h = 120.0f;

    nk_layout_row_dynamic(ctx, search_h, 1);
    {
      char before[TOOL_MAX_QUERY_LEN];
      mem_copy(before, tool->ui.input_query, sizeof(before));

      nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, tool->ui.input_query, sizeof(tool->ui.input_query), nk_filter_default);

      str_t before_str  = str_from_cstr_with_cap(before, sizeof(before));
      str_t current_str = str_from_cstr_with_cap(tool->ui.input_query, sizeof(tool->ui.input_query));

      if (!str_equal(before_str, current_str, 0)) {
        start_search(tool, current_str);
      }
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Search settings", NK_MINIMIZED)) {
      bool should_restart_search = false;

      nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 4);
      {
        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Search outer chain", &tool->search.match_outer)) {
          should_restart_search = true;
        }

        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Search type instances", &tool->search.match_type_instances)) {
          should_restart_search = true;
        }

        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Search child types", &tool->search.match_type_children)) {
          should_restart_search = true;
        }

        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Search properties", &tool->search.match_props)) {
          should_restart_search = true;
        }
      }
      nk_layout_row_end(ctx);

      nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 4);
      {
        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Ignore case", &tool->search.ignore_case)) {
          should_restart_search = true;
        }

        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Exact match", &tool->search.exact_match)) {
          should_restart_search = true;
        }

        nk_layout_row_push(ctx, 0.25f);
        if (ui_checkbox(ctx, "Has byte code", &tool->search.has_byte_code)) {
          should_restart_search = true;
        }

        nk_layout_row_push(ctx, 0.2f);
        nk_spacer(ctx);
      }
      nk_layout_row_end(ctx);

      if (should_restart_search) {
        start_search(tool, str_from_cstr_with_cap(tool->ui.input_query, sizeof(tool->ui.input_query)));
      }

      nk_tree_pop(ctx);
    }

    if (nk_tree_push(ctx, NK_TREE_TAB, "Filters", NK_MINIMIZED)) {
      bool should_rebuild_visible = false;
      {
        nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 5);
        {
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_PACKAGE);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_CLASS);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_STRUCT);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_ENUM);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_FUNC);
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 5);
        {
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_WORLD);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_LEVEL);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_ACTOR);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_ACTOR_COMPONENT);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_GAME_INSTANCE);
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 5);
        {
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_LOCAL_PLAYER);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_PLAYER_CONTROLLER);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_PAWN);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_HUD);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_WIDGET);
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 5);
        {
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_BLUEPRINT);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_DATA_ASSET);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_DATA_TABLE);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_CDO);
          nk_layout_row_push(ctx, 0.2f);
          should_rebuild_visible |= uobject_kind_checkbox(ctx, &tool->search.enabled_kind_mask, UOBJECT_KIND_UNKNOWN);
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_DYNAMIC, filters_h, 2);
        {
          uint32_t all_selected_mask = ((1 << UOBJECT_KIND_MAX) - 1);

          nk_layout_row_push(ctx, 0.5f);
          if (ui_button_str(ctx, STR_LIT("Select all"))) {
            should_rebuild_visible         = (tool->search.enabled_kind_mask != all_selected_mask);
            tool->search.enabled_kind_mask = all_selected_mask;
          }

          nk_layout_row_push(ctx, 0.5f);
          if (ui_button_str(ctx, STR_LIT("Deselect all"))) {
            should_rebuild_visible         = (tool->search.enabled_kind_mask != 0);
            tool->search.enabled_kind_mask = 0;
          }
        }
        nk_layout_row_end(ctx);
      }

      if (should_rebuild_visible) {
        rebuild_visible(tool);
      }

      nk_tree_pop(ctx);
    }

    clamp_page(tool, &tool->search.visible);

    uint32_t max_results = (uint32_t)mod_cfg_get_int(&globals.mod_manager, tool->cfg.max_results_h);
    uint32_t page_size   = (uint32_t)NK_MAX(max_results, 1);
    uint32_t page_count  = get_page_count(&tool->search.visible, page_size);
    uint32_t start_idx   = tool->ui.page_index * page_size;
    uint32_t end_idx     = NK_MIN(start_idx + page_size, tool->search.visible.count);

    if (page_count > 1) {
      nk_layout_row_begin(ctx, NK_DYNAMIC, 20.0f, 3);
      {
        nk_layout_row_push(ctx, 0.20f);
        if (ui_button_str(ctx, STR_LIT("<")) && tool->ui.page_index > 0) {
          tool->ui.page_index -= 1;
        }

        nk_layout_row_push(ctx, 0.60f);
        nk_labelf(ctx, NK_TEXT_CENTERED, "Page %u / %u", tool->ui.page_index + 1, page_count);

        nk_layout_row_push(ctx, 0.20f);
        if (ui_button_str(ctx, STR_LIT(">")) && tool->ui.page_index + 1 < page_count) {
          tool->ui.page_index += 1;
        }
      }
      nk_layout_row_end(ctx);
    }

    float split_area_h     = nk_layout_get_remaining_height(ctx);
    float min_split_area_h = min_results_h + gap_y + splitter_h + gap_y + min_details_h;

    split_area_h = NK_MAX(split_area_h, min_split_area_h);

    float panel_area_h = 0.0f;
    float results_h    = 0.0f;
    float details_h    = 0.0f;

    if (tool->details.count > 0) {
      panel_area_h = split_area_h - (gap_y + splitter_h + gap_y);
      results_h    = panel_area_h * tool->ui.results_split.ratio;
      results_h    = NK_CLAMP(min_results_h, results_h, panel_area_h - min_details_h);
      details_h    = panel_area_h - results_h;

      if (panel_area_h > 0.0f) {
        tool->ui.results_split.ratio = results_h / panel_area_h;
      }
    } else {
      panel_area_h = split_area_h;
      results_h    = panel_area_h;
      details_h    = 0.0f;
    }

    nk_layout_row_dynamic(ctx, results_h, 1);
    struct nk_rect split_track = nk_widget_bounds(ctx);
    split_track.h              = panel_area_h;

    if (nk_group_begin(ctx, "uobject_search.results_scroll", 0)) {
      ui_text_cols_t cols = {0};
      ui_text_cols_reset(&cols, 2);

      for (uint32_t i = start_idx; i < end_idx; ++i) {
        uint32_t slot = tool->search.visible.slots[i];
        if (slot >= tool->cache.record_cap) {
          continue;
        }

        record_t *record = record_from_slot(tool, slot);
        if (!record_has_flag(record, RECORD_FLAG_LIVE) || str_is_empty(record->full_name.str)) {
          continue;
        }

        record_t *type_record = record_from_slot(tool, record->type_slot);
        ASSERT(type_record != NULL);
        ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));

        ui_text_cols_include(&cols, 0, type_record->name);
        ui_text_cols_include(&cols, 1, record->full_name);
      }

      for (uint32_t i = start_idx; i < end_idx; ++i) {
        uint32_t slot = tool->search.visible.slots[i];
        if (slot >= tool->cache.record_cap) {
          continue;
        }

        record_t *record = &tool->cache.records[slot];
        if (!record_has_flag(record, RECORD_FLAG_LIVE) || str_is_empty(record->full_name.str)) {
          continue;
        }

        record_t *type_record = record_from_slot(tool, record->type_slot);
        ASSERT(type_record != NULL);
        ASSERT(record_has_flag(type_record, RECORD_FLAG_LIVE));

        if (ui_uobject_row(ctx, 20.0f, &cols, UI_TEXT_CELL(type_record->name, uobject_kind_color(record->kind)), UI_TEXT_CELL(record->full_name, UI_C_TEXT))) {
          detail_tab_open(tool, record, false);
        }
      }
      nk_group_end(ctx);
    }

    if (tool->details.count > 0) {
      nk_layout_row_dynamic(ctx, splitter_h, 1);
      ui_nk_splitter_opts_t split_opts = {
        .axis           = UI_NK_AXIS_Y,
        .track          = split_track,
        .gap_before     = gap_y,
        .min_before     = min_results_h,
        .min_after      = min_details_h,
        .line_thickness = 2.0f,
      };
      ui_splitter(ctx, &tool->ui.results_split, &split_opts);

      draw_details_panel(tool, details_h);
    }
  }
  nk_end(ctx);

  if (nk_window_is_closed(ctx, name)) {
    tool->ui.closed = true;
  }
}

static void
tool_work_tick(search_tool_t *tool)
{
  uint64_t start_us = time_now_us();

  if (!build_cache_tick(tool, start_us)) {
    return;
  }

  if (!process_pending_slots_tick(tool, start_us)) {
    return;
  }

  if (!search_matching_tick(tool, start_us)) {
    return;
  }

  if (!select_visible_tick(tool, start_us)) {
    return;
  }
}

static bool MOD_CALL
tool_init(const mod_host_api_t *host, mod_handle_t h)
{
  search_tool_t *tool = &g_tool;
  mem_zero(tool, sizeof(*tool));

  tool->perm             = mod_arena_handle_resolve(mod_get_perm_arena(&globals.mod_manager, h));
  tool->cache.record_cap = (uint32_t)unreal_uobject_array_capacity();
  if (tool->cache.record_cap > 0) {
    tool->cache.records = ARENA_PUSH_ARRAY_ZERO(tool->perm, record_t, tool->cache.record_cap);

    bool ok = (tool->cache.records != NULL);

    ok = ok && slot_list_init(tool->perm, &tool->cache.live_slots, tool->cache.record_cap);
    ok = ok && pending_queue_init(tool->perm, &tool->cache.pending_slots, tool->cache.record_cap);

    ok = ok && slot_list_init(tool->perm, &tool->search.matches, tool->cache.record_cap);
    ok = ok && slot_list_init(tool->perm, &tool->search.visible, tool->cache.record_cap);

    ok = ok && pending_queue_init(tool->perm, &tool->search.pending_search_types, tool->cache.record_cap);
    ok = ok && pending_queue_init(tool->perm, &tool->search.pending_search_instances, tool->cache.record_cap);
    ok = ok && pending_queue_init(tool->perm, &tool->search.pending_select, tool->cache.record_cap);

    if (!ok) {
      LOG_ERROR("UObject Search: failed to allocate internal data structures");
      return false;
    }
  }

  tool->cache.build_cursor = 0;
  tool->cache.build_total  = (uint32_t)unreal_uobject_array_count();
  tool->cache.world        = (globals.gworld_ptr) ? *globals.gworld_ptr : NULL;

  arena_t *string_arena = mod_arena_handle_resolve(host->arena_create(h, 128 * MB, 1 * MB));
  if (!string_arena) {
    LOG_ERROR("UObject Search: failed to create the string arena");
    return false;
  }

  tool->cache.strings = string_pool_create(string_arena, 1024);

  tool->search.search_cursor     = 0;
  tool->search.search_total      = 0;
  tool->search.gen               = 0;
  tool->search.has_byte_code     = false;
  tool->search.ignore_case       = false;
  tool->search.exact_match       = false;
  tool->search.enabled_kind_mask = (1u << UOBJECT_KIND_MAX) - 1u;

  tool->search.match_outer          = true;
  tool->search.match_type_instances = true;
  tool->search.match_type_children  = true;
  tool->search.match_props          = false;

  tool->cfg.max_results_h = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_PAGE_SIZE_ID));
  tool->cfg.work_budget_h = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_WORK_BUDGET_ID));
  tool->cfg.open_window_h = mod_cfg_get_by_id(&globals.mod_manager, h, STR_LIT(CFG_OPEN_WINDOW_ID));

  tool->ui.closed                    = true;
  tool->ui.results_split.ratio       = 0.62f;
  tool->ui.results_split.dragging    = false;
  tool->ui.results_split.hovering    = false;
  tool->ui.results_split.drag_offset = 0.0f;
  tool->ui.page_index                = 0;

  tool->details.count        = 0;
  tool->details.first        = NULL;
  tool->details.last         = NULL;
  tool->details.selected_tab = NULL;
  tool->details.preview_tab  = NULL;

  for (int i = 0; i < COUNTOF(tool->details.tabs); ++i) {
    tool->details.tabs[i].info_arena  = mod_arena_handle_resolve(host->arena_create(h, 64 * MB, 64 * KB));
    tool->details.tabs[i].value_arena = mod_arena_handle_resolve(host->arena_create(h, 64 * MB, 64 * KB));

    ASSERT(tool->details.tabs[i].info_arena != NULL);
    ASSERT(tool->details.tabs[i].value_arena != NULL);
  }

  tool->details.free = &tool->details.tabs[0];
  for (int i = 0; i < COUNTOF(tool->details.tabs) - 1; ++i) {
    tool->details.tabs[i].next = &tool->details.tabs[i + 1];
  }

  ASSERT(host->register_uobject_listener(h, UOBJECT_LISTENER_KIND_CREATE, on_uobject_created, tool));
  ASSERT(host->register_uobject_listener(h, UOBJECT_LISTENER_KIND_DELETE, on_uobject_deleted, tool));

  tool->ctx    = globals.ui_manager.ctx;
  tool->handle = h;
  tool->inited = true;

  return true;
}

static void MOD_CALL
tool_deinit(mod_handle_t h)
{
  (void)h;

  search_tool_t *tool = &g_tool;
  mem_zero(tool, sizeof(*tool));
}

static void MOD_CALL
tool_tick(mod_handle_t h, float delta)
{
  (void)h;
  (void)delta;

  search_tool_t *tool = &g_tool;
  if (!tool->inited) {
    return;
  }

  uworld_t *world = (globals.gworld_ptr) ? *globals.gworld_ptr : NULL;
  if (tool->cache.world != world) {
    build_cache_all(tool);
    tool->cache.world = world;
  }

  tool_work_tick(tool);

  if (globals.ui_manager.inited && globals.ui_manager.ctx) {
    draw_window(tool, globals.ui_manager.vw, globals.ui_manager.vh);
  }
}

static bool MOD_CALL
tool_input(mod_handle_t h, input_event_t *ev)
{
  (void)h;

  search_tool_t *tool = &g_tool;
  if (!tool->inited) {
    return false;
  }

  keybind_t toggle_keybind = mod_cfg_get_keybind(&globals.mod_manager, tool->cfg.open_window_h);
  if (input_event_covers_keybind(ev, toggle_keybind)) {
    if (keybind_is_pressed(toggle_keybind)) {
      tool->ui.closed = !tool->ui.closed;
    }
    return true;
  }

  return false;
}

static void MOD_CALL
tool_draw_panel(mod_handle_t h, struct nk_context *ctx)
{
  (void)h;

  search_tool_t *tool = &g_tool;
  if (!tool->inited) {
    return;
  }

  static bool panel_maximized = true;
  if (ui_tree_state_push(ctx, NK_TREE_TAB, STR_LIT("Tool"), &panel_maximized)) {
    float info_h = 18.0f;
    float prog_h = 16.0f;

    {
      uint32_t current = build_in_progress(tool) ? tool->cache.build_cursor : tool->cache.build_total;
      uint32_t total   = tool->cache.build_total;
      uint32_t pct     = progress_pct(current, total);

      if (!build_in_progress(tool) || total == 0) {
        pct = 100;
      }

      nk_layout_row_dynamic(ctx, info_h, 1);
      nk_labelf(ctx, NK_TEXT_LEFT, "Initial indexing: %u%% (%u / %u)", pct, current, total);

      nk_layout_row_dynamic(ctx, prog_h, 1);
      nk_prog(ctx, pct, 100, false);
    }

    {
      ASSERT(tool->search.search_cursor <= tool->search.search_total);
      uint32_t current = tool->search.search_cursor;
      uint32_t total   = tool->search.search_total;
      uint32_t pct     = progress_pct(current, total);

      if (total == 0) {
        pct = 100;
      }

      nk_layout_row_dynamic(ctx, info_h, 1);
      nk_labelf(ctx, NK_TEXT_LEFT, "Searching: %u%% (%u / %u)", pct, current, total);

      nk_layout_row_dynamic(ctx, prog_h, 1);
      nk_prog(ctx, pct, 100, false);
    }

    nk_layout_row_dynamic(ctx, 28.0f, 1);
    if (ui_button_str(ctx, tool->ui.closed ? STR_LIT("Open window") : STR_LIT("Close window"))) {
      tool->ui.closed = !tool->ui.closed;
    }
    nk_tree_pop(ctx);
  }
}

void
register_builtin_uobject_search(mod_manager_t *manager)
{
  keybind_t open_window_toggle_bind = keybind_parse(STR_LIT("F2"), KEYBIND_NULL);

  mod_option_info_t options[] = {
    {
      .type        = MOD_OPTION_INT,
      .id          = STR_CLIT(CFG_WORK_BUDGET_ID),
      .label       = STR_CLIT("Work budget (us)"),
      .description = STR_CLIT("Maximum time spent indexing and searching per frame"),
      .val =
        {
          .integer =
            {
              .min_val     = 100,
              .default_val = 2000,
              .max_val     = 10000,
              .step        = 100,
            },
        },
    },
    {
      .type        = MOD_OPTION_INT,
      .id          = STR_CLIT(CFG_PAGE_SIZE_ID),
      .label       = STR_CLIT("Results per page"),
      .description = STR_CLIT("Maximum number of results shown per page"),
      .val =
        {
          .integer =
            {
              .min_val     = 10,
              .default_val = 100,
              .max_val     = 1000,
              .step        = 1,
            },
        },
    },
    {
      .type        = MOD_OPTION_KEYBIND,
      .id          = STR_CLIT(CFG_OPEN_WINDOW_ID),
      .label       = STR_CLIT("Window shortcut"),
      .description = STR_CLIT("Shortcut to open the search window"),
      .val =
        {
          .keybind =
            {
              .default_val = open_window_toggle_bind,
            },
        },
    },
  };

  mod_manifest_t manifest = {
    .info =
      {
        .id          = STR_CLIT("uobject-search"),
        .name        = STR_CLIT("UObject Search"),
        .author      = STR_CLIT("akmubi"),
        .description = STR_CLIT("Searches and inspects loaded UObjects in real time"),
        .kind        = MOD_KIND_TOOL,
        .version     = MAKE_VERSION(0, 1, 0),
      },
    .options      = options,
    .option_count = COUNTOF(options),
  };

  mod_dll_runtime_funcs_t funcs = {
    .init       = tool_init,
    .deinit     = tool_deinit,
    .tick       = tool_tick,
    .input      = tool_input,
    .draw_panel = tool_draw_panel,
  };

  mod_handle_t h = mod_register_builtin(manager, &manifest, funcs);
  if (h == MOD_HANDLE_INVALID) {
    CONSOLE_ERROR("failed to register builtin UObject Search tool");
  }

  mod_t *m = mod_handle_resolve(&globals.mod_manager, h);
  ASSERT(m != NULL);
}
