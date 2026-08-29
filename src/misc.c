#include "types.h"

#include <math.h>
#include <windows.h>

uint64_t
time_now_us(void)
{
  static LARGE_INTEGER freq = {0};
  LARGE_INTEGER        now;

  if (freq.QuadPart == 0) {
    QueryPerformanceFrequency(&freq);
  }

  QueryPerformanceCounter(&now);
  return (uint64_t)((now.QuadPart * 1000000ull) / freq.QuadPart);
}

uint32_t
thread_current_id(void)
{
  return (uint32_t)GetCurrentThreadId();
}

int32_t
atomic_i32_increment(volatile int32_t *value)
{
  return (int32_t)InterlockedIncrement((volatile LONG *)value);
}

int32_t
atomic_i32_compare_exchange(volatile int32_t *value, int32_t exchange, int32_t comparand)
{
  return (int32_t)InterlockedCompareExchange((volatile LONG *)value, (LONG)exchange, (LONG)comparand);
}

bool
float_equal(float a, float b, float min_v, float step)
{
  if (step <= 0.0f) {
    return fabsf(a - b) <= 0.000001f;
  }

  int ai = (int)floorf(((a - min_v) / step) + 0.5f);
  int bi = (int)floorf(((b - min_v) / step) + 0.5f);

  return ai == bi;
}
