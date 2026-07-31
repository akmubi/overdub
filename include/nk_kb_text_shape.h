#ifndef NK_KB_TEXT_SHAPE_H
#define NK_KB_TEXT_SHAPE_H

#include "vendor_kb.h"
#include "vendor_nuklear.h"

/**
 * Context, fonts and the font array are caller-owned.
 * The context is dedicated to this backend.
 * Each font's UserData points to its nk_user_font.
 * Fonts are ordered from lowest to highest fallback priority; request.font is promoted to the top.
 */
struct nk_kb_text_backend {
  kbts_shape_context *context;
  kbts_font         **fonts;
  struct nk_buffer   *temporary;
  int                 num_fonts;
};

NK_API struct nk_text_shape *
nk_kb_text_shape_build(nk_handle userdata, struct nk_buffer *cache, const struct nk_text_request *request);

#endif /* NK_KB_TEXT_SHAPE_H */
