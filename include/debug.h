#ifndef DEBUG_H
#define DEBUG_H

#include "arena.h"
#include "str.h"

void
debug_stacktrace_push_lines(str_list_t *lines, arena_t *arena, uint32_t start_idx, uint32_t end_idx);

#endif /* DEBUG_H */
