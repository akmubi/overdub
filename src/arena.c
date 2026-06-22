#include "arena.h"
#include "types.h"

#include <windows.h>

static uint64_t
mem_page_size(void)
{
  static uint64_t page_size = 0;
  if (page_size == 0) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    page_size = info.dwPageSize;
  }
  return page_size;
}

static void *
mem_reserve(uint64_t size)
{
  return VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
}

static void
mem_release(void *ptr, uint64_t size)
{
  UNUSED_VAR(size);
  VirtualFree(ptr, 0, MEM_RELEASE);
}

static bool
mem_commit(void *ptr, uint64_t size)
{
  uint64_t page_size    = mem_page_size();
  uint64_t snapped_size = size;

  snapped_size += page_size - 1;
  snapped_size -= snapped_size % page_size;
  return VirtualAlloc(ptr, snapped_size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

static void
mem_decommit(void *ptr, uint64_t size)
{
#if COMPILER_MSVC
  #pragma warning(suppress: 6250)
#endif
  VirtualFree(ptr, size, MEM_DECOMMIT);
}

arena_t
arena_new_static(void *buf, uint64_t size)
{
  arena_t result = {0};
  if (buf && size > 0) {
    result = (arena_t){
      .kind         = ARENA_KIND_STATIC,
      .backing      = buf,
      .used         = 0,
      .reserved     = size,
      .committed    = size,
      .commit_size  = size,
      .reserve_size = size,
    };
  }
  return result;
}

arena_t
arena_new_dynamic(uint64_t reserve_size, uint64_t commit_size)
{
  arena_t result = {0};
  if (reserve_size > 0 && commit_size > 0) {
    uint64_t page_size = mem_page_size();
    commit_size   = ALIGN_UP(commit_size, page_size);
    reserve_size  = ALIGN_UP(reserve_size, page_size);

    void *ptr = mem_reserve(reserve_size);
    if (ptr) {
      result = (arena_t) {
        .kind         = ARENA_KIND_DYNAMIC,
        .backing      = ptr,
        .used         = 0,
        .reserved     = reserve_size,
        .committed    = 0,
        .commit_size  = commit_size,
        .reserve_size = reserve_size,
      };
    }
  }
  return result;
}

uint64_t
arena_get_used(arena_t *arena)
{
  if (arena) {
    return arena->used;
  }
  return 0;
}

uint64_t
arena_get_reserved(arena_t *arena)
{
  if (arena) {
    return arena->reserved;
  }
  return 0;
}

uint64_t
arena_get_committed(arena_t *arena)
{
  if (arena) {
    return arena->committed;
  }
  return 0;
}

void
arena_set_used(arena_t *arena, uint64_t new_used)
{
  if (arena) {
    arena->used = MIN_VAL(new_used, arena->used);
  }
}

void
arena_reset(arena_t *a)
{
  if (a) {
    a->used = 0;
  }
}

void
arena_destroy(arena_t *a)
{
  if (a && a->kind == ARENA_KIND_DYNAMIC && a->backing && a->reserve_size > 0) {
    if (a->committed > 0) {
      mem_decommit(a->backing, a->committed);
    }
    mem_release(a->backing, a->reserve_size);
    mem_zero(a, sizeof(*a));
  }
}

static inline uint64_t
align_pad(uintptr_t p, uint64_t a)
{
  uint64_t r = (uint64_t)(p % a);
  return r ? (a - r) : 0;
}

void *
arena_push_aligned(arena_t *a, uint64_t size, uint64_t alignment)
{
  ASSERT(a != NULL);

  void *result = NULL;

  if (alignment == 0) {
    alignment = 1;
  }

  uintptr_t base_ptr = (uintptr_t)a->backing + (uintptr_t)a->used;
  uint64_t  pad      = align_pad(base_ptr, alignment);
  MASSERT(pad <= UINT64_MAX - size, "pad: %010llX, size: %010llX", pad, size);
  if (pad > UINT64_MAX - size) {
    return NULL;
  }

  uint64_t advance = pad + size;
  MASSERT(a->used <= UINT64_MAX - advance, "a->used: %010llX, advance: %010llX", a->used, advance);
  if (a->used > UINT64_MAX - advance) {
    return NULL;
  }

  uint64_t need = a->used + advance;
  if (size > 0 && need <= a->reserved) {
    switch (a->kind) {
      case ARENA_KIND_STATIC: {
        result  = (uint8_t *)a->backing + a->used + pad;
        a->used = need;
        break;
      }

      case ARENA_KIND_DYNAMIC: {
        if (need > a->committed) {
          // round grow_to up to a multiple of commit_size
          uint64_t grow_to = need;
          uint64_t rem     = (a->commit_size ? (grow_to % a->commit_size) : 0);
          if (rem) {
            uint64_t add = a->commit_size - rem;
            MASSERT(grow_to <= UINT64_MAX - add, "grow_to: %010llX, add: %010llX", grow_to, add); // should never happen
            grow_to += add;
          }

          if (grow_to > a->reserved) {
            grow_to = a->reserved;
          }

          uint64_t delta = grow_to - a->committed;
          if (delta) {
            if (mem_commit((uint8_t *)a->backing + a->committed, delta)) {
              a->committed += delta;
              result        = (uint8_t *)a->backing + a->used + pad;
              a->used       = need;
            }
          }
        } else {
          result  = (uint8_t *)a->backing + a->used + pad;
          a->used = need;
        }
        break;
      }

      default:
        MASSERT(0, "invalid arena kind: %d", a->kind);
    }
  }

  return result;
}

void *
arena_push_zero_aligned(arena_t *a, uint64_t size, uint64_t alignment)
{
  void *result = arena_push_aligned(a, size, alignment);
  if (result) {
    mem_zero(result, size);
  }
  return result;
}

void
arena_pop_to(arena_t *a, uint64_t new_used)
{
  ASSERT(a != NULL);
  a->used = MIN_VAL(new_used, a->used);

  if (a->kind == ARENA_KIND_DYNAMIC) {
    // keep committed = ceil(used, commit_size)
    uint64_t want_committed = ALIGN_UP(a->used, a->commit_size);
    if (want_committed < a->committed) {
      uint64_t decommit_from  = want_committed;
      uint64_t decommit_bytes = a->committed - want_committed;
      mem_decommit((uint8_t *)a->backing + decommit_from, decommit_bytes);
      a->committed = want_committed;
    }
  }
}

void
arena_pop(arena_t *a, uint64_t size)
{
  if (size > a->used) {
    size = a->used;
  }
  arena_pop_to(a, a->used - size);
}
