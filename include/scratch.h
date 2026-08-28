#ifndef SCRATCH_H
#define SCRATCH_H

#include "types.h"
#include "arena.h"

MOD_EXTERN_C_BEGIN

#define SCRATCH_POOL_SIZE (2)

typedef struct scratch_s scratch_t;
struct scratch_s {
  bool    inited;
  arena_t arenas[SCRATCH_POOL_SIZE];
};

MOD_API arena_t *
scratch_get(arena_t *conflict);

MOD_API tmp_arena_t
scratch_begin(arena_t *conflict);
MOD_API void
scratch_end(tmp_arena_t tmp);

MOD_API void
scratch_reset(void);
MOD_API void
scratch_destroy(void);

MOD_API bool
scratch_has_conflict(arena_t *conflict);

MOD_EXTERN_C_END

#endif /* SCRATCH_H */
