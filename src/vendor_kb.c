#include "types.h"

#if COMPILER_MSVC
#  pragma warning(push, 0)
#elif COMPILER_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "kb/kb_text_shape.h"

#if COMPILER_MSVC
#  pragma warning(pop)
#elif COMPILER_GCC
#  pragma GCC diagnostic pop
#endif
