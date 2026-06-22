#if defined(_MSC_VER)
#  pragma warning(push, 0) /* disable all warnings */
#endif

#define STB_SPRINTF_IMPLEMENTATION
#include "stb/stb_sprintf.h"

#include "minhook/src/buffer.c"
#include "minhook/src/hde/hde64.c"
#include "minhook/src/hook.c"
#include "minhook/src/trampoline.c"

#include "nuklear/src/nuklear.c"

#if defined BUILD_TEST_UI
#  include "nuklear/d3d11/nuklear_d3d11.c"
#endif

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
