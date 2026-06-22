#include "scratch.h"
#include "vendor_nuklear.h"

#include <stdio.h>
#include <string.h>

#include "fonts/nk_font_cascadia.h"

static void
nk_dump_u8_array(FILE *f, const char *name, const uint8_t *data, uint64_t size)
{
  fprintf(f, "const uint8_t %s[] = {\n", name);

  for (uint64_t i = 0; i < size; ++i) {
    if ((i % 12) == 0) {
      fprintf(f, "  ");
    }

    fprintf(f, "0x%02X", data[i]);

    if (i + 1 < size) {
      fprintf(f, ", ");
    }

    if ((i % 12) == 11) {
      fprintf(f, "\n");
    }
  }

  if ((size % 12) != 0) {
    fprintf(f, "\n");
  }

  fprintf(f, "};\n");
  fprintf(f, "const uint32_t %s_size = %llu;\n\n", name, (unsigned long long)size);
}

static int
nk_glyph_cmp_codepoint(const void *a, const void *b)
{
  const struct nk_font_glyph *ga = a;
  const struct nk_font_glyph *gb = b;

  if (ga->codepoint < gb->codepoint) {
    return -1;
  }

  if (ga->codepoint > gb->codepoint) {
    return 1;
  }

  return 0;
}

static uint32_t
nk_find_fallback_idx(struct nk_font_glyph *glyphs, uint32_t count, nk_rune fallback_codepoint)
{
  for (uint32_t i = 0; i < count; ++i) {
    if (glyphs[i].codepoint == fallback_codepoint) {
      return i;
    }
  }

  return 0;
}

static void
nk_dump_glyph_array(FILE                       *f,
                    const char                 *name,
                    const struct nk_font_glyph *src_glyphs,
                    uint32_t                    glyph_count,
                    nk_rune                     fallback_codepoint,
                    uint32_t                   *out_fallback_idx)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    struct nk_font_glyph *glyphs = ARENA_PUSH_ARRAY(tmp.arena, struct nk_font_glyph, glyph_count);
    ASSERT(glyphs != NULL);

    memcpy(glyphs, src_glyphs, sizeof(*glyphs) * glyph_count);

    qsort(glyphs, glyph_count, sizeof(*glyphs), nk_glyph_cmp_codepoint);

    *out_fallback_idx = nk_find_fallback_idx(glyphs, glyph_count, fallback_codepoint);

    fprintf(f, "struct nk_font_glyph %s[] = {\n", name);

    for (uint32_t i = 0; i < glyph_count; ++i) {
      const struct nk_font_glyph *g = &glyphs[i];

      fprintf(f,
              "  { .codepoint = 0x%08X, .xadvance = %.9ff, "
              ".x0 = %.9ff, .y0 = %.9ff, .x1 = %.9ff, .y1 = %.9ff, "
              ".w = %.9ff, .h = %.9ff, "
              ".u0 = %.9ff, .v0 = %.9ff, .u1 = %.9ff, .v1 = %.9ff },\n",
              (unsigned)g->codepoint,
              g->xadvance,
              g->x0,
              g->y0,
              g->x1,
              g->y1,
              g->w,
              g->h,
              g->u0,
              g->v0,
              g->u1,
              g->v1);
    }

    fprintf(f, "};\n\n");
  }
  scratch_end(tmp);
}

static void
nk_dump_font_desc(FILE           *f,
                  const char     *name,
                  struct nk_font *font,
                  int             atlas_w,
                  int             atlas_h,
                  int             white_x,
                  int             white_y,
                  uint32_t        fallback_glyph_idx)
{
  fprintf(f,
          "const nk_baked_font_desc_t %s = {\n"
          "  .tex_w              = %d,\n"
          "  .tex_h              = %d,\n"
          "  .white_x            = %d,\n"
          "  .white_y            = %d,\n"
          "  .height             = %.9ff,\n"
          "  .ascent             = %.9ff,\n"
          "  .descent            = %.9ff,\n"
          "  .glyph_count        = %u,\n"
          "  .fallback_glyph_idx = %u,\n"
          "};\n\n",
          name,
          atlas_w,
          atlas_h,
          white_x,
          white_y,
          font->info.height,
          font->info.ascent,
          font->info.descent,
          (unsigned)font->info.glyph_count,
          fallback_glyph_idx);
}

void
test_dump_prebaked_font_data(void)
{
  struct nk_font_atlas atlas = {0};
  struct nk_font      *font_body;
  struct nk_font      *font_title;

  nk_font_atlas_init_default(&atlas);
  nk_font_atlas_begin(&atlas);

  void   *font_data      = (void *)g_nk_font_cascadia;
  nk_size font_data_size = sizeof(g_nk_font_cascadia);

  {
    struct nk_font_config cfg = nk_font_config(CONFIG_NK_FONT_BODY_SIZE);
    cfg.range                 = g_nk_font_cascadia_glyph_ranges;
    font_body                 = nk_font_atlas_add_from_memory(&atlas, font_data, font_data_size, cfg.size, &cfg);
  }

  {
    struct nk_font_config cfg = nk_font_config(CONFIG_NK_FONT_TITLE_SIZE);
    cfg.range                 = g_nk_font_cascadia_glyph_ranges;
    font_title                = nk_font_atlas_add_from_memory(&atlas, font_data, font_data_size, cfg.size, &cfg);
  }

  int atlas_w = 0;
  int atlas_h = 0;

  const void *rgba_data = nk_font_atlas_bake(&atlas, &atlas_w, &atlas_h, NK_FONT_ATLAS_RGBA32);
  MASSERT(rgba_data != NULL, "failed to bake font atlas");

  FILE *f = fopen("nk_prebaked_font.c", "wb");
  if (f) {
    uint32_t body_fallback_idx  = 0;
    uint32_t title_fallback_idx = 0;

    uint64_t rgba_size = (uint64_t)atlas_w * (uint64_t)atlas_h * 4;

    fprintf(f, "#include \"vendor_nuklear.h\"\n");
    fprintf(f, "#include \"nk_prebaked_font.h\"\n\n");

    nk_dump_u8_array(f, "g_nk_font_atlas_rgba", rgba_data, rgba_size);

    nk_dump_glyph_array(f,
                        "g_nk_font_body_glyphs",
                        font_body->glyphs,
                        (uint32_t)font_body->info.glyph_count,
                        font_body->fallback_codepoint,
                        &body_fallback_idx);

    nk_dump_glyph_array(f,
                        "g_nk_font_title_glyphs",
                        font_title->glyphs,
                        (uint32_t)font_title->info.glyph_count,
                        font_title->fallback_codepoint,
                        &title_fallback_idx);

    nk_dump_font_desc(
      f, "g_nk_font_body_desc", font_body, atlas_w, atlas_h, atlas.custom.x, atlas.custom.y, body_fallback_idx);

    nk_dump_font_desc(
      f, "g_nk_font_title_desc", font_title, atlas_w, atlas_h, atlas.custom.x, atlas.custom.y, title_fallback_idx);

    fclose(f);
  }
}
