#ifndef BUILTIN_COMMON_H
#define BUILTIN_COMMON_H

#include "str.h"
#include "types.h"
#include "ui_nuklear.h"
#include "unreal.h"

typedef struct query_s query_t;
struct query_s {
  uint16_t ansi_len;
  uint16_t wide_len;
  uint8_t  ansi_name[FNAME_ENTRY_NAME_SIZE];
  uint16_t wide_name[FNAME_ENTRY_NAME_SIZE];
};

static inline bool
query_is_empty(query_t *query)
{
  return (query->ansi_len == 0 && query->wide_len == 0);
}

bool
query_matches_fname_entry(fname_entry_t *entry, query_t *query, bool ignore_case, bool exact_match);
bool
query_matches_fname(fname_t name, query_t *query, bool ignore_case, bool exact_match);
void
query_set(query_t *query, str_t text);

typedef uint8_t uobject_kind_t;
enum {
  UOBJECT_KIND_UNKNOWN = 0,

  /* type / reflection */
  UOBJECT_KIND_CDO,
  UOBJECT_KIND_PACKAGE,
  UOBJECT_KIND_CLASS,
  UOBJECT_KIND_STRUCT,
  UOBJECT_KIND_ENUM,
  UOBJECT_KIND_FUNC,

  /* runtime world graph */
  UOBJECT_KIND_WORLD,
  UOBJECT_KIND_LEVEL,
  UOBJECT_KIND_ACTOR,
  UOBJECT_KIND_ACTOR_COMPONENT,

  /* gameplay/runtime singletons */
  UOBJECT_KIND_GAME_INSTANCE,
  UOBJECT_KIND_LOCAL_PLAYER,
  UOBJECT_KIND_PLAYER_CONTROLLER,
  UOBJECT_KIND_PAWN,
  UOBJECT_KIND_HUD,

  /* UI */
  UOBJECT_KIND_WIDGET,

  /* asset-ish objects */
  UOBJECT_KIND_BLUEPRINT,
  UOBJECT_KIND_DATA_ASSET,
  UOBJECT_KIND_DATA_TABLE,

  UOBJECT_KIND_MAX,
};
STATIC_ASSERT(UOBJECT_KIND_MAX <= UINT8_MAX, "uobject kind does not fit into uint8_t");

uobject_kind_t
uobject_kind(uobject_t *obj);
struct nk_color
uobject_kind_color(uobject_kind_t kind);
str_t
uobject_kind_to_str(uobject_kind_t kind);
bool
uobject_kind_checkbox(struct nk_context *ctx, uint32_t *enabled_kind_mask, uobject_kind_t kind);

#endif /* BUILTIN_COMMON_H */
