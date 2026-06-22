#ifndef VERSION_H
#define VERSION_H

#include "types.h"

#define MAKE_VERSION(MAJOR, MINOR, PATCH) \
  { \
    .major = MAJOR, \
    .minor = MINOR, \
    .patch = PATCH, \
  }

#define VERSION_ARG(V) (V).major, (V).minor, (V).patch

typedef struct version_s version_t;
struct version_s {
  uint16_t major;
  uint16_t minor;
  uint32_t patch;
};

static inline int
version_compare(version_t a, version_t b)
{
  if (a.major != b.major) return (a.major > b.major) ? 1 : -1;
  if (a.minor != b.minor) return (a.minor > b.minor) ? 1 : -1;
  if (a.patch != b.patch) return (a.patch > b.patch) ? 1 : -1;
  return 0;
}

#endif /* VERSION_H */
