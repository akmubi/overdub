#ifndef BUILTIN_UOBJECT_SEARCH_INTERNAL_H
#define BUILTIN_UOBJECT_SEARCH_INTERNAL_H

#include "mod_manager.h"
#include "string_pool.h"
#include "ui_nuklear.h"

#include "common.h"

#define TOOL_MAX_QUERY_LEN    (256)
#define TOOL_MAX_DETAILS_TABS (64)

typedef struct search_tool_s search_tool_t;

typedef struct disassember_ctx_s disassember_ctx_t;
struct disassember_ctx_s {
  ustruct_script_t script;
  uint32_t         pos;
  arena_t         *arena;
  str_list_t       list;
  int32_t          depth;
  bool             failed;
  char             error_msg[1024];
};
str_list_t
unreal_disassemble_structure(ustruct_t *s, arena_t *arena);

typedef struct slot_list_s slot_list_t;
struct slot_list_s {
  uint32_t *slots;
  uint32_t  count;
  uint32_t  cap;
};

typedef struct pending_list_s pending_queue_t;
struct pending_list_s {
  uint8_t  *set;
  uint32_t *slots;
  uint32_t  head;
  uint32_t  tail;
  uint32_t  count;
  uint32_t  cap;
};

#define RECORD_SLOT_INVALID (UINT32_MAX)

typedef uint8_t record_flags_t;
enum {
  RECORD_FLAG_LIVE      = FLAG(0),
  RECORD_FLAG_VISIBLE   = FLAG(1),
  RECORD_FLAG_MATCHED   = FLAG(2),
  RECORD_FLAG_MAXIMIZED = FLAG(3),
};

typedef struct record_s record_t;
struct record_s {
  uobject_t     *obj;
  fname_t        fname;
  ui_text_span_t full_name;
  ui_text_span_t outer_name;
  ui_text_span_t name;

  uint32_t live_list_idx;
  uint32_t match_idx;
  uint32_t visible_idx;

  uint32_t       checked_gen;
  record_flags_t flags;
  uobject_kind_t kind;

  uint32_t type_slot;
  uint32_t parent_slot;
  uint32_t first_child_slot;
  uint32_t last_child_slot;
  uint32_t prev_sibling_slot;
  uint32_t next_sibling_slot;

  uint32_t first_instance_slot;
  uint32_t last_instance_slot;
  uint32_t prev_instance_slot;
  uint32_t next_instance_slot;
  uint32_t instance_count;
};

bool
record_slot_valid(uint32_t slot);
record_t *
record_from_slot(search_tool_t *tool, uint32_t slot);
uint32_t
record_to_slot(search_tool_t *tool, record_t *record);
bool
record_is_type_record(record_t *record);
bool
record_is_instance_record(record_t *record);
bool
record_has_flag(record_t *record, record_flags_t flag);
void
record_set_flag(record_t *record, record_flags_t flag, bool enabled);

typedef enum {
  DETAIL_PROP_CHILDREN_NONE = 0,
  DETAIL_PROP_CHILDREN_SCHEMA,
  DETAIL_PROP_CHILDREN_ARRAY_ELEMS,
  DETAIL_PROP_CHILDREN_SET_ELEMS,
  DETAIL_PROP_CHILDREN_MAP_PAIRS,
  DETAIL_PROP_CHILDREN_MCAST_DELEGATE,
} detail_prop_children_kind_t;

typedef struct detail_prop_s detail_prop_t;
struct detail_prop_s {
  detail_prop_t *next;
  detail_prop_t *first_child;
  detail_prop_t *last_child;

  detail_prop_t *free_next;
  bool           runtime_node;
  record_t      *record_to_open;

  fprop_t       *prop;
  ui_text_span_t offset_text;
  ui_text_span_t size_text;
  ui_text_span_t name;
  ui_text_span_t type;
  ui_text_span_t summary;

  int32_t offset;
  int32_t size;
  int32_t array_dim;

  bool is_param;
  bool maximized;

  bool     has_value_addr;
  uint8_t *value_addr;

  detail_prop_children_kind_t children_kind;
  int32_t                     live_child_count;
  int32_t                     live_slot_count;
  int32_t                     live_slot_idx;
};

typedef struct detail_func_s detail_func_t;
struct detail_func_s {
  detail_func_t *next;
  record_t      *record;

  uint32_t index;
  uint32_t flags;
  uint16_t params_size;

  ui_text_span_t flags_text;
  ui_text_span_t params_size_text;
  ui_text_span_t native_func_text;
  ui_text_span_t event_graph_func_text;
  ui_text_span_t event_graph_call_offset_text;

  detail_prop_t *first_param;
  detail_prop_t *last_param;
  bool           maximized;
};

typedef struct detail_owner_s detail_owner_t;
struct detail_owner_s {
  detail_owner_t *next;
  record_t       *record;
  detail_prop_t  *first_prop;
  detail_prop_t  *last_prop;
  detail_func_t  *first_func;
  detail_func_t  *last_func;
};

typedef struct detail_enum_entry_s detail_enum_entry_t;
struct detail_enum_entry_s {
  detail_enum_entry_t *next;
  ui_text_span_t       name;
  ui_text_span_t       value_text;
  int64_t              value;
};

typedef struct detail_enum_s detail_enum_t;
struct detail_enum_s {
  record_t      *record;
  ui_text_span_t cpp_type;
  ui_text_span_t enum_name_col;
  ui_text_span_t enum_value_col;
  ui_text_cols_t cols;

  detail_enum_entry_t *first_entry;
  detail_enum_entry_t *last_entry;
  bool                 maximized;
};

typedef struct detail_tab_s detail_tab_t;
struct detail_tab_s {
  detail_tab_t *next;
  detail_tab_t *prev;

  arena_t *info_arena;
  arena_t *value_arena;

  record_t *record;
  bool      pinned;

  struct nk_scroll body_scroll;

  ui_text_span_t outer_name;
  ui_text_span_t flags;
  ui_text_span_t internal_idx;
  ui_text_span_t addr;
  uint8_t       *value_base;

  ui_text_span_t offset_col;
  ui_text_span_t size_col;
  ui_text_span_t type_col;
  ui_text_span_t name_col;
  ui_text_span_t val_col;
  ui_text_span_t flags_col;
  ui_text_span_t no_instances;
  ui_text_span_t no_props;
  ui_text_span_t no_funcs;
  ui_text_span_t no_entries;
  ui_text_span_t no_params;
  ui_text_span_t owner_hbar;

  bool layout_prop_maximized;
  bool layout_func_maximized;
  bool layout_instances_maximized;

  bool force_expand_values;

  str_list_t disasm_lines;
  bool       disasm_built;
  bool       disasm_maximized;

  detail_owner_t *first_owner;
  detail_owner_t *last_owner;
  detail_func_t  *function;
  detail_enum_t  *enumeration;
  detail_prop_t  *free_prop;
};

void
detail_tab_open(search_tool_t *tool, record_t *record, bool pinned);
void
detail_tab_pin(search_tool_t *tool, detail_tab_t *tab);
void
detail_tab_close(search_tool_t *tool, detail_tab_t *tab);
void
detail_tab_next(search_tool_t *tool);
void
detail_tab_prev(search_tool_t *tool);
void
details_clear_stale(search_tool_t *tool);
void
draw_details_panel(search_tool_t *tool, float panel_h);

typedef struct tool_cache_s tool_cache_t;
struct tool_cache_s {
  record_t *records;
  uint32_t  record_cap;

  uint32_t build_cursor;
  uint32_t build_total;

  string_pool_t strings;
  uworld_t     *world;

  slot_list_t     live_slots;
  pending_queue_t pending_slots;
};

bool
build_in_progress(search_tool_t *tool);

bool
slot_list_init(arena_t *arena, slot_list_t *list, uint32_t cap);
void
slot_list_reset(slot_list_t *list);

bool
pending_queue_init(arena_t *arena, pending_queue_t *list, uint32_t cap);
bool
push_back_pending(pending_queue_t *queue, uint32_t slot);
bool
pop_front_pending(pending_queue_t *queue, uint32_t *out_slot);
bool
push_front_pending(pending_queue_t *queue, uint32_t slot);
void
clear_pending(pending_queue_t *queue);

void
add_live(search_tool_t *tool, record_t *record);
void
remove_live(search_tool_t *tool, record_t *record);

void
add_match(search_tool_t *tool, record_t *record);
void
remove_match(search_tool_t *tool, record_t *record);
void
clear_match(search_tool_t *tool);
void
rebuild_matches(search_tool_t *tool);

void
add_visible(search_tool_t *tool, record_t *record);
void
remove_visible(search_tool_t *tool, record_t *record);
void
clear_visible(search_tool_t *tool);
void
rebuild_visible(search_tool_t *tool);

bool
cache_name(search_tool_t *tool, record_t *record);
bool
cache_outer_name(search_tool_t *tool, record_t *record);

bool
cache_names(search_tool_t *tool, record_t *record);
void
release_names(search_tool_t *tool, record_t *record);

bool
remove_slot(search_tool_t *tool, uint32_t slot);
bool
cache_slot(search_tool_t *tool, uint32_t slot);

void
on_uobject_created(uobject_t *obj, int32_t idx, void *user);
void
on_uobject_deleted(uobject_t *obj, int32_t idx, void *user);

bool
build_cache_tick(search_tool_t *tool, uint64_t start_us);
void
build_cache_all(search_tool_t *tool);

bool
process_pending_slots_tick(search_tool_t *tool, uint64_t start_us);

typedef struct tool_search_s tool_search_t;
struct tool_search_s {
  query_t query;

  slot_list_t matches;
  slot_list_t visible;

  pending_queue_t pending_search_types;
  pending_queue_t pending_search_instances;
  pending_queue_t pending_select;

  uint32_t search_cursor;
  uint32_t search_total;
  uint32_t gen;

  bool match_outer;
  bool match_type_instances;
  bool match_type_children;
  bool match_props;

  bool     has_byte_code;
  bool     ignore_case;
  bool     exact_match;
  uint32_t enabled_kind_mask;
};

bool
uobject_item_matches_record(fuobject_item_t *item, record_t *record);
bool
record_matches_query(search_tool_t *tool, record_t *record);
bool
record_passes_visible_filter(search_tool_t *tool, record_t *record);
void
update_visible(search_tool_t *tool, record_t *record);
bool
update_match(search_tool_t *tool, record_t *record);
bool
add_to_search(search_tool_t *tool, record_t *record);

bool
search_matching_tick(search_tool_t *tool, uint64_t start_us);
bool
select_visible_tick(search_tool_t *tool, uint64_t start_us);
void
start_search(search_tool_t *tool, str_t query);

typedef struct tool_ui_s tool_ui_t;
struct tool_ui_s {
  char input_query[FNAME_ENTRY_NAME_SIZE];
  bool closed;

  ui_nk_splitter_state_t results_split;

  uint32_t page_index;
};

typedef struct tool_details_s tool_details_t;
struct tool_details_s {
  detail_tab_t tabs[TOOL_MAX_DETAILS_TABS];
  int          count;

  detail_tab_t *free;
  detail_tab_t *first;
  detail_tab_t *last;
  detail_tab_t *selected_tab;
  detail_tab_t *preview_tab;
};

typedef struct tool_cfg_s tool_cfg_t;
struct tool_cfg_s {
  mod_cfg_handle_t max_results_h;
  mod_cfg_handle_t work_budget_h;
  mod_cfg_handle_t open_window_h;
};

struct search_tool_s {
  arena_t           *perm;
  tool_cache_t       cache;
  tool_search_t      search;
  tool_ui_t          ui;
  tool_details_t     details;
  tool_cfg_t         cfg;
  mod_handle_t       handle;
  struct nk_context *ctx;
  bool               inited;
};

bool
work_budget_exhausted(search_tool_t *tool, uint64_t start_us);

#endif /* BUILTIN_UOBJECT_SEARCH_INTERNAL_H */
