#ifndef NK_PREBAKED_FONT_H
#define NK_PREBAKED_FONT_H

#include "types.h"
#include "vendor_nuklear.h"

typedef struct nk_baked_font_desc_s nk_baked_font_desc_t;
struct nk_baked_font_desc_s {
  uint32_t tex_w;
  uint32_t tex_h;

  uint32_t white_x;
  uint32_t white_y;

  float height;
  float ascent;
  float descent;

  uint32_t glyph_count;
  uint32_t fallback_glyph_idx;
};

extern const uint8_t  g_nk_font_atlas_rgba[];
extern const uint32_t g_nk_font_atlas_rgba_size;

extern const nk_baked_font_desc_t g_nk_font_body_desc;
extern const nk_baked_font_desc_t g_nk_font_title_desc;

extern struct nk_font_glyph g_nk_font_body_glyphs[];
extern struct nk_font_glyph g_nk_font_title_glyphs[];

#endif /* NK_PREBAKED_FONT_H */
