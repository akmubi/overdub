#include "scratch.h"

/* per-thread scratch */
static THREAD_LOCAL scratch_t g_scratch = {0};

static void
scratch_init(void)
{
  if (!g_scratch.inited) {
    for (int i = 0; i < SCRATCH_POOL_SIZE; ++i) {
      g_scratch.arenas[i] = arena_new_dynamic(CONFIG_SCRATCH_RESERVE_SIZE, CONFIG_SCRATCH_COMMIT_SIZE);
    }
    g_scratch.inited = true;
  }
}

void
scratch_reset(void)
{
  for (int i = 0; i < SCRATCH_POOL_SIZE; ++i) {
    arena_reset(&g_scratch.arenas[i]);
  }
}

void
scratch_destroy(void)
{
  if (g_scratch.inited) {
    for (int i = 0; i < SCRATCH_POOL_SIZE; ++i) {
      arena_destroy(&g_scratch.arenas[i]);
      g_scratch.arenas[i] = (arena_t) {0};
    }
    g_scratch.inited = false;
  }
}

arena_t *
scratch_get(arena_t *conflict)
{
  scratch_init();

  for (int i = 0; i < SCRATCH_POOL_SIZE; ++i) {
    if (&g_scratch.arenas[i] != conflict) {
      return &g_scratch.arenas[i];
    }
  }
  ASSERT(0);
  return &g_scratch.arenas[0];
}

tmp_arena_t
scratch_begin(arena_t *conflict)
{
  return tmp_arena_begin(scratch_get(conflict));
}

void
scratch_end(tmp_arena_t tmp)
{
  tmp_arena_end(tmp);
}

bool
scratch_has_conflict(arena_t *conflict)
{
  for (int i = 0; i < SCRATCH_POOL_SIZE; ++i) {
    if (&g_scratch.arenas[i] == conflict) {
      return true;
    }
  }
  return false;
}