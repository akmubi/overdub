#ifndef SCRATCH_H
#define SCRATCH_H

#include "types.h"
#include "arena.h"

#define SCRATCH_POOL_SIZE (2)

typedef struct scratch_s scratch_t;
struct scratch_s {
  bool    inited;
  arena_t arenas[SCRATCH_POOL_SIZE];
};

arena_t *
scratch_get(arena_t *conflict);

tmp_arena_t
scratch_begin(arena_t *conflict);
void
scratch_end(tmp_arena_t tmp);

void
scratch_reset(void);
void
scratch_destroy(void);

bool
scratch_has_conflict(arena_t *conflict);

#endif /* SCRATCH_H */
