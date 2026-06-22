#include "profiler.h"
#include "log.h"

static prof_zone_t g_prof_zones[PROF_MAX_ZONES];
static uint32_t    g_prof_zone_count = 0;

void
profiler_reset(void)
{
  for (uint32_t i = 0; i < g_prof_zone_count; ++i) {
    prof_zone_t *z = &g_prof_zones[i];

    z->call_count = 0;
    z->total_us   = 0;
    z->min_us     = UINT64_MAX;
    z->max_us     = 0;
  }
}

uint32_t
profiler_register_zone(const char *name, const char *file, uint32_t line)
{
  ASSERT(name != NULL);
  ASSERT(file != NULL);

  uint32_t id = g_prof_zone_count++;
  ASSERT(id < PROF_MAX_ZONES);

  g_prof_zones[id] = (prof_zone_t){
    .name   = name,
    .file   = file,
    .line   = line,
    .min_us = UINT64_MAX,
  };
  return id;
}

prof_scope_t
profiler_scope_begin(uint32_t zone_id)
{
  ASSERT(zone_id < g_prof_zone_count);

  return (prof_scope_t){
    .zone_id  = zone_id,
    .start_us = time_now_us(),
    .active   = true,
  };
}

void
profiler_scope_end(prof_scope_t *scope)
{
  ASSERT(scope != NULL);

  if (!scope->active) {
    return;
  }

  uint64_t elapsed_us = time_now_us() - scope->start_us;

  prof_zone_t *z = &g_prof_zones[scope->zone_id];

  z->call_count += 1;
  z->total_us   += elapsed_us;
  z->min_us      = MIN_VAL(z->min_us, elapsed_us);
  z->max_us      = MAX_VAL(z->max_us, elapsed_us);

  scope->active = false;
}

void
profiler_log_report(void)
{
  LOG_INFO("---- profiler report ----");

  for (uint32_t i = 0; i < g_prof_zone_count; ++i) {
    prof_zone_t *z = &g_prof_zones[i];

    if (z->call_count == 0) {
      continue;
    }

    uint64_t avg_us = z->total_us / z->call_count;

    LOG_INFO("%-40s calls=%llu total=%llu us avg=%llu us min=%llu us max=%llu us", z->name, z->call_count, z->total_us, avg_us, z->min_us, z->max_us);
  }
}
