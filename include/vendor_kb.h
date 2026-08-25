#ifndef VENDOR_KB_H
#define VENDOR_KB_H

#include "types.h"

#if COMPILER_MSVC
#  pragma warning(push, 0)
#elif COMPILER_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "kb/kb_text_shape.h"

#if COMPILER_MSVC
#  pragma warning(pop)
#elif COMPILER_GCC
#  pragma GCC diagnostic pop
#endif

#endif /* VENDOR_KB_H */
