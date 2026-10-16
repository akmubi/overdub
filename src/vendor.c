#include "types.h"

#if COMPILER_MSVC
#  pragma warning(push, 0)
#elif COMPILER_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#define STB_SPRINTF_IMPLEMENTATION
#include "stb/stb_sprintf.h"

#include "profiler.h"

#include "minhook/src/buffer.c"
#include "minhook/src/hde/hde64.c"
#include "minhook/src/hook.c"
#include "minhook/src/trampoline.c"

#define NK_PROFILE_SCOPE_BEGIN(NAME, SCOPE) PROF_SCOPE_BEGIN(NAME, SCOPE)
#define NK_PROFILE_SCOPE_END(SCOPE)         PROF_SCOPE_END(SCOPE)
#define NK_API                              extern MOD_API
#include "nuklear/src/nuklear.c"

#if COMPILER_MSVC
#  pragma warning(pop)
#elif COMPILER_GCC
#  pragma GCC diagnostic pop
#endif
