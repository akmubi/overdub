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
  enum nk_runtime_font_style style;
};

struct nk_runtime_font_face {
  struct nk_font             font; // in the atlas: must not move
  void                      *ttf;
  nk_size                    ttf_size;
  enum nk_runtime_font_style style;
};

struct nk_runtime_fonts {
  struct nk_glyph_atlas       atlas;
  struct nk_runtime_font_face faces[NK_RUNTIME_FONT_CAPACITY];
  int                         face_count;
};

bool
nk_runtime_font_source_system(struct nk_runtime_font_source *source,
                              arena_t                       *arena,
                              const char                    *name,
                              enum nk_runtime_font_style     style);

/* maps the file instead of reading it, for large fonts */
bool
nk_runtime_font_source_system_mapped(struct nk_runtime_font_source *source,
                                     const char                    *name,
                                     enum nk_runtime_font_style     style);

/* sources[0] is required, the others are optional */
bool
nk_runtime_fonts_init(struct nk_runtime_fonts             *fonts,
                      arena_t                             *permanent,
                      const struct nk_runtime_font_source *sources,
                      int                                  source_count,
                      float                                size,
                      int                                  width,
                      int                                  height,
                      int                                  max_glyphs);

struct nk_font *
nk_runtime_font(struct nk_runtime_fonts *fonts, enum nk_runtime_font_style style);

#endif /* NK_FONT_RUNTIME_H */
