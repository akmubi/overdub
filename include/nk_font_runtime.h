#ifndef NK_FONT_RUNTIME_H
#define NK_FONT_RUNTIME_H

#include "arena.h"
#include "vendor_nuklear.h"

#define NK_RUNTIME_FONT_CAPACITY 16

enum nk_runtime_font_style {
  NK_RUNTIME_FONT_REGULAR,
  NK_RUNTIME_FONT_BOLD,
  NK_RUNTIME_FONT_ITALIC,
  NK_RUNTIME_FONT_BOLD_ITALIC,
  NK_RUNTIME_FONT_FALLBACK,
};

struct nk_runtime_font_source {
  void                      *ttf;
  nk_size                    ttf_size;
  const nk_rune             *range;
  enum nk_runtime_font_style style;
};

struct nk_runtime_font_face {
  struct nk_font            *font;
  void                      *ttf;
  nk_size                    ttf_size;
  enum nk_runtime_font_style style;
};

struct nk_runtime_fonts {
  struct nk_font_atlas        atlas;
  struct nk_runtime_font_face faces[NK_RUNTIME_FONT_CAPACITY];
  int                         face_count;
  const void                 *pixels;
  int                         width;
  int                         height;
};

bool
nk_runtime_font_source_system(struct nk_runtime_font_source *source,
                              arena_t                       *arena,
                              const char                    *name,
                              enum nk_runtime_font_style     style);

/* sources[0] is the required regular face.
 * Remaining style and fallback faces are optional. */
bool
nk_runtime_fonts_bake(struct nk_runtime_fonts             *fonts,
                      arena_t                             *permanent,
                      arena_t                             *temporary,
                      const struct nk_runtime_font_source *sources,
                      int                                  source_count,
                      float                                size);

struct nk_font *
nk_runtime_font(struct nk_runtime_fonts *fonts, enum nk_runtime_font_style style);

bool
nk_runtime_font_rasterize(const void           *ttf,
                          float                 font_height,
                          nk_glyph_id           id,
                          void                 *alpha,
                          int                   stride,
                          struct nk_font_glyph *glyph);

#endif /* NK_FONT_RUNTIME_H */
