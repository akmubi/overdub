#ifndef STRING_POOL_H
#define STRING_POOL_H

#include "arena.h"
#include "str.h"
#include "types.h"

typedef struct string_pool_alloc_s string_pool_alloc_t;
struct string_pool_alloc_s {
  string_pool_alloc_t *next; // used only while the block is on free list
  uint32_t             cap;  // payload capacity in bytes, including trailing '\0' if used as string
  uint32_t             len;  // payload length in bytes, excluding trailing '\0'
  uint8_t              data[];
};

typedef struct string_pool_chunk_s string_pool_chunk_t;
struct string_pool_chunk_s {
  string_pool_chunk_t *next;
  uint32_t             used;
  uint32_t             cap;
  uint8_t              data[];
};

typedef struct string_pool_s string_pool_t;
struct string_pool_s {
  arena_t *arena;

  string_pool_chunk_t *first_chunk;
  string_pool_chunk_t *last_chunk;

  string_pool_alloc_t *free_list;

  uint32_t min_chunk_payload;
  uint64_t total_chunk_payload;
  uint64_t live_bytes;
};

string_pool_t
string_pool_create(arena_t *arena, uint32_t min_chunk_payload);
void
string_pool_reset(string_pool_t *pool);

void *
string_pool_alloc_bytes(string_pool_t *pool, uint32_t size);
void
string_pool_free_bytes(string_pool_t *pool, void *ptr);

str_t
string_pool_push(string_pool_t *pool, str_t src);
str_t
string_pool_push_cstr(string_pool_t *pool, const char *cstr);

void
string_pool_release(string_pool_t *pool, str_t s);
void
string_pool_replace(string_pool_t *pool, str_t *dst, str_t src);
str_t
string_pool_realloc(string_pool_t *pool, str_t original, uint32_t new_len);

#endif /* STRING_POOL_H */
