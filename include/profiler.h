#ifndef PROFILER_H
#define PROFILER_H

#include "types.h"

#define PROF_MAX_ZONES (256)

#define PROF_SCOPE_BEGIN(NAME, SCOPEVAR)                                          \
  static uint32_t CAT(SCOPEVAR, _zone_id) = UINT32_MAX;                           \
  if (CAT(SCOPEVAR, _zone_id) == UINT32_MAX) {                                    \
    CAT(SCOPEVAR, _zone_id) = profiler_register_zone((NAME), __FILE__, __LINE__); \
  }                                                                               \
  prof_scope_t SCOPEVAR = profiler_scope_begin(CAT(SCOPEVAR, _zone_id))
#define PROF_SCOPE_END(SCOPEVAR) profiler_scope_end(&(SCOPEVAR))

#define PROF_FUNC_BEGIN() PROF_SCOPE_BEGIN(__func__, prof_func_scope)
#define PROF_FUNC_END()   PROF_SCOPE_END(prof_func_scope)

typedef struct prof_zone_s prof_zone_t;
struct prof_zone_s {
  const char *name;
  const char *file;
  uint32_t    line;

  uint64_t call_count;
  uint64_t total_us;
  uint64_t min_us;
  uint64_t max_us;
};

typedef struct prof_scope_s prof_scope_t;
struct prof_scope_s {
  uint32_t zone_id;
  uint64_t start_us;
  bool     active;
};

void
profiler_reset(void);

uint32_t
profiler_register_zone(const char *name, const char *file, uint32_t line);

prof_scope_t
profiler_scope_begin(uint32_t zone_id);

void
profiler_scope_end(prof_scope_t *scope);

void
profiler_log_report(void);

#endif /* PROFILER_H */
