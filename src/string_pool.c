#include "string_pool.h"

/*
 * Freed allocations reuse the same header and sit on free_list.
 * Header is already big enough, so payload can stay tiny, but keeping
 * a small floor reduces pathological fragmentation from lots of 1-2 byte strings.
 */
#define MIN_PAYLOAD_SIZE 8

static inline uint32_t
string_pool_alloc_overhead(void)
{
  return (uint32_t)offsetof(string_pool_alloc_t, data);
}

static inline uint32_t
string_pool_chunk_overhead(void)
{
  return (uint32_t)offsetof(string_pool_chunk_t, data);
}

static inline uint32_t
string_pool_total_size_from_payload(uint32_t payload_size)
{
  return string_pool_alloc_overhead() + payload_size;
}

static inline string_pool_alloc_t *
string_pool_ptr_to_alloc(void *ptr)
{
  if (!ptr) {
    return NULL;
  }
  return (string_pool_alloc_t *)((uint8_t *)ptr - string_pool_alloc_overhead());
}

static string_pool_chunk_t *
string_pool_push_chunk(string_pool_t *pool, uint32_t min_payload_size)
{
  uint32_t payload_size = MAX_VAL(pool->min_chunk_payload, min_payload_size);
  uint32_t total_size   = string_pool_chunk_overhead() + payload_size;

  string_pool_chunk_t *chunk = arena_push_zero_aligned(pool->arena, total_size, ALIGNOF(string_pool_chunk_t));
  if (!chunk) {
    return NULL;
  }

  chunk->next = NULL;
  chunk->used = 0;
  chunk->cap  = payload_size;

  if (!pool->first_chunk) {
    pool->first_chunk = chunk;
    pool->last_chunk  = chunk;
  } else {
    pool->last_chunk->next = chunk;
    pool->last_chunk       = chunk;
  }

  pool->total_chunk_payload += payload_size;
  return chunk;
}

static string_pool_alloc_t *
string_pool_try_alloc_from_free_list(string_pool_t *pool, uint32_t payload_size)
{
  string_pool_alloc_t *prev = NULL;
  string_pool_alloc_t *node = pool->free_list;

  while (node) {
    if (node->cap >= payload_size) {
      if (prev) {
        prev->next = node->next;
      } else {
        pool->free_list = node->next;
      }

      node->next = NULL;
      node->len  = 0;
      return node;
    }

    prev = node;
    node = node->next;
  }

  return NULL;
}

static string_pool_alloc_t *
string_pool_try_alloc_from_chunk(string_pool_t *pool, uint32_t payload_size)
{
  uint32_t total_size = ALIGN_UP(string_pool_total_size_from_payload(payload_size), ALIGNOF(string_pool_alloc_t));

  string_pool_chunk_t *chunk = pool->last_chunk;
  if (!chunk || chunk->used + total_size > chunk->cap) {
    chunk = string_pool_push_chunk(pool, total_size);
    if (!chunk) {
      return NULL;
    }
  }

  ASSERT(chunk->used + total_size <= chunk->cap);

  string_pool_alloc_t *alloc  = (string_pool_alloc_t *)(chunk->data + chunk->used);
  chunk->used                += total_size;

  alloc->next = NULL;
  alloc->cap  = payload_size;
  alloc->len  = 0;
  return alloc;
}

string_pool_t
string_pool_create(arena_t *arena, uint32_t min_chunk_payload)
{
  return (string_pool_t){
    .arena             = arena,
    .min_chunk_payload = (min_chunk_payload > 0) ? min_chunk_payload : (4 * KB),
  };
}

void
string_pool_reset(string_pool_t *pool)
{
  if (!pool) {
    return;
  }

  arena_reset(pool->arena);

  pool->first_chunk         = NULL;
  pool->last_chunk          = NULL;
  pool->free_list           = NULL;
  pool->total_chunk_payload = 0;
  pool->live_bytes          = 0;
}

void *
string_pool_alloc_bytes(string_pool_t *pool, uint32_t size)
{
  ASSERT(pool != NULL);

  if (size == 0) {
    return NULL;
  }

  size = MAX_VAL(size, MIN_PAYLOAD_SIZE);

  string_pool_alloc_t *alloc = string_pool_try_alloc_from_free_list(pool, size);
  if (!alloc) {
    alloc = string_pool_try_alloc_from_chunk(pool, size);
    if (!alloc) {
      return NULL;
    }
  }

  alloc->len        = size;
  pool->live_bytes += alloc->cap;
  return alloc->data;
}

void
string_pool_free_bytes(string_pool_t *pool, void *ptr)
{
  if (!pool || !ptr) {
    return;
  }

  string_pool_alloc_t *alloc = string_pool_ptr_to_alloc(ptr);

  if (pool->live_bytes >= alloc->cap) {
    pool->live_bytes -= alloc->cap;
  } else {
    pool->live_bytes = 0;
  }

  alloc->len      = 0;
  alloc->next     = pool->free_list;
  pool->free_list = alloc;
}

str_t
string_pool_push(string_pool_t *pool, str_t src)
{
  if (!pool || str_is_empty(src)) {
    return STR_NULL;
  }

  uint32_t need = MAX_VAL((uint32_t)src.len + 1, MIN_PAYLOAD_SIZE); // keep trailing '\0' for convenience

  string_pool_alloc_t *alloc = string_pool_try_alloc_from_free_list(pool, need);
  if (!alloc) {
    alloc = string_pool_try_alloc_from_chunk(pool, need);
    if (!alloc) {
      return STR_NULL;
    }
  }

  mem_copy(alloc->data, src.data, src.len);
  alloc->data[src.len] = '\0';
  alloc->len           = (uint32_t)src.len;

  pool->live_bytes += alloc->cap;

  return str_make(alloc->data, src.len);
}

str_t
string_pool_push_cstr(string_pool_t *pool, const char *cstr)
{
  if (!cstr) {
    return STR_NULL;
  }
  return string_pool_push(pool, str_from_cstr(cstr));
}

void
string_pool_release(string_pool_t *pool, str_t s)
{
  if (!pool || !s.data) {
    return;
  }

  string_pool_alloc_t *alloc = string_pool_ptr_to_alloc(s.data);

  if (pool->live_bytes >= alloc->cap) {
    pool->live_bytes -= alloc->cap;
  } else {
    pool->live_bytes = 0;
  }

  alloc->len      = 0;
  alloc->next     = pool->free_list;
  pool->free_list = alloc;
}

void
string_pool_replace(string_pool_t *pool, str_t *dst, str_t src)
{
  ASSERT(dst != NULL);

  if (dst->data) {
    string_pool_release(pool, *dst);
  }
  *dst = string_pool_push(pool, src);
}

str_t
string_pool_realloc(string_pool_t *pool, str_t original, uint32_t new_len)
{
  ASSERT(pool != NULL);

  uint32_t need = MAX_VAL(new_len + 1, MIN_PAYLOAD_SIZE);

  if (original.data) {
    string_pool_alloc_t *old = string_pool_ptr_to_alloc(original.data);
    if (old && old->cap >= need) {
      old->len           = new_len;
      old->data[new_len] = '\0';
      return str_make(original.data, new_len);
    }
  }

  string_pool_alloc_t *alloc = string_pool_try_alloc_from_free_list(pool, need);
  if (!alloc) {
    alloc = string_pool_try_alloc_from_chunk(pool, need);
    if (!alloc) {
      return STR_NULL;
    }
  }

  alloc->len            = new_len;
  alloc->data[new_len]  = '\0';
  pool->live_bytes     += alloc->cap;

  if (original.data) {
    string_pool_release(pool, original);
  }

  return str_make(alloc->data, new_len);
}
