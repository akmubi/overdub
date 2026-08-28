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
#define NK_API                              MOD_API
#include "nuklear/src/nuklear.c"

#include "nk_font_runtime.h"

#include "scratch.h"

static void *
nk_runtime_tmp_alloc(nk_handle userdata, void *old, nk_size size)
{
  (void)old;
  return arena_push_aligned(userdata.ptr, size, 16);
}

static void
nk_runtime_tmp_free(nk_handle userdata, void *memory)
{
  (void)userdata;
  (void)memory;
}

bool
nk_runtime_font_rasterize(const void           *ttf,
                          float                 font_height,
                          nk_glyph_id           id,
                          void                 *alpha,
                          int                   stride,
                          struct nk_font_glyph *glyph)
{
  bool        result = false;
  tmp_arena_t tmp    = scratch_begin(NULL);
  {
    float scale;
    int   x0;
    int   y0;
    int   x1;
    int   y1;
    int   advance;
    int   left_bearing;

    struct nk_allocator allocator = {
      .userdata.ptr = tmp.arena,
      .alloc        = nk_runtime_tmp_alloc,
      .free         = nk_runtime_tmp_free,
    };

    stbtt_fontinfo info = {
      .userdata = &allocator,
    };

    if (stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
      scale = stbtt_ScaleForMappingEmToPixels(&info, font_height);
      stbtt_GetGlyphBitmapBox(&info, id, scale, scale, &x0, &y0, &x1, &y1);
      stbtt_GetGlyphHMetrics(&info, id, &advance, &left_bearing);

      if (x1 - x0 <= stride && y1 - y0 <= stride) {
        if (alpha && x1 > x0 && y1 > y0) {
          stbtt_MakeGlyphBitmap(&info, alpha, x1 - x0, y1 - y0, stride, scale, scale, id);
        }

        (void)left_bearing;

        mem_zero(glyph, sizeof(*glyph));
        glyph->id       = id;
        glyph->xadvance = advance * scale;
        glyph->x0       = x0;
        glyph->y0       = y0;
        glyph->x1       = x1;
        glyph->y1       = y1;
        glyph->w        = x1 - x0;
        glyph->h        = y1 - y0;
        result          = true;
      }
    }
  }
  tmp_arena_end(tmp);

  return result;
}

#if defined BUILD_TEST_UI
#  include "nuklear/d3d11/nuklear_d3d11.c"
#endif

#if COMPILER_MSVC
#  pragma warning(pop)
#elif COMPILER_GCC
#  pragma GCC diagnostic pop
#endif
