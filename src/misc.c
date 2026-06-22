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
