#ifndef VENDOR_NUKLEAR_H
#define VENDOR_NUKLEAR_H

#include "types.h"

#if COMPILER_MSVC
#  pragma warning(push, 0)
#elif COMPILER_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#ifndef NK_API
#  define NK_API MOD_API
#endif

#include "nuklear/include/nuklear.h"

#if COMPILER_MSVC
#  pragma warning(pop)
#elif COMPILER_GCC
#  pragma GCC diagnostic pop
#endif

#endif /* VENDOR_NUKLEAR_H */
