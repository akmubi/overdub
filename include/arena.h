#ifndef ARENA_H
#define ARENA_H

#include "types.h"

#define ARENA_PUSH_ARRAY(ARENA, T, N)      (T *)arena_push_aligned((ARENA), (N) * sizeof(T), ALIGNOF(T))
#define ARENA_PUSH_ARRAY_ZERO(ARENA, T, N) (T *)arena_push_zero_aligned((ARENA), (N) * sizeof(T), ALIGNOF(T))
#define ARENA_PUSH(ARENA, T)               ARENA_PUSH_ARRAY(ARENA, T, 1)
#define ARENA_PUSH_ZERO(ARENA, T)          ARENA_PUSH_ARRAY_ZERO(ARENA, T, 1)

typedef int arena_kind_t;
enum {
  ARENA_KIND_STATIC  = 0,
  ARENA_KIND_DYNAMIC = 1,
};

typedef struct arena_s arena_t;
struct arena_s {
  arena_kind_t  kind;
  void         *backing;
  uint64_t      used;
  uint64_t      reserved;
  uint64_t      committed;
  uint64_t      commit_size;
  uint64_t      reserve_size;
};

arena_t
arena_new_static(void *buf, uint64_t size);
arena_t
arena_new_dynamic(uint64_t reserve_size, uint64_t commit_size);

uint64_t
arena_get_used(arena_t *arena);
uint64_t
arena_get_reserved(arena_t *arena);
uint64_t
arena_get_committed(arena_t *arena);

void
arena_set_used(arena_t *arena, uint64_t new_used);
void
arena_reset(arena_t *arena);
void
arena_destroy(arena_t *arena);

void *
arena_push_aligned(arena_t *a, uint64_t size, uint64_t alignment);
void *
arena_push_zero_aligned(arena_t *a, uint64_t size, uint64_t alignment);

void
arena_pop_to(arena_t *a, uint64_t new_used);
void
arena_pop(arena_t *a, uint64_t size);

typedef struct tmp_arena_s tmp_arena_t;
struct tmp_arena_s {
  arena_t *arena;
  uint64_t pos;
};

static inline tmp_arena_t
tmp_arena_begin(arena_t *a)
{
  return (tmp_arena_t){
    .arena = a,
    .pos   = arena_get_used(a),
  };
}

static inline void
tmp_arena_end(tmp_arena_t tmp)
{
  arena_pop_to(tmp.arena, tmp.pos);
}

#endif /* ARENA_H */
