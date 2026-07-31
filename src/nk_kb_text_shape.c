#include "nk_kb_text_shape.h"

static kbts_u32
nk_kb_tag(nk_uint tag)
{
  return KBTS_FOURCC((tag >> 24) & 0xff, (tag >> 16) & 0xff, (tag >> 8) & 0xff, tag & 0xff);
}

static kbts_direction
nk_kb_direction(enum nk_text_direction direction)
{
  switch (direction) {
    case NK_TEXT_DIRECTION_LTR: return KBTS_DIRECTION_LTR;
    case NK_TEXT_DIRECTION_RTL: return KBTS_DIRECTION_RTL;
    default:                    return KBTS_DIRECTION_DONT_KNOW;
  }
}

static enum nk_text_direction
nk_kb_run_direction(kbts_direction direction)
{
  return direction == KBTS_DIRECTION_RTL ? NK_TEXT_DIRECTION_RTL : NK_TEXT_DIRECTION_LTR;
}

static nk_flags
nk_kb_boundary_flags(kbts_break_flags flags)
{
  nk_flags result = 0;
  if (flags & KBTS_BREAK_FLAG_GRAPHEME)  result |= NK_TEXT_BOUNDARY_GRAPHEME;
  if (flags & KBTS_BREAK_FLAG_WORD)      result |= NK_TEXT_BOUNDARY_WORD;
  if (flags & KBTS_BREAK_FLAG_LINE_SOFT) result |= NK_TEXT_BOUNDARY_LINE_SOFT;
  if (flags & KBTS_BREAK_FLAG_LINE_HARD) result |= NK_TEXT_BOUNDARY_LINE_HARD | NK_TEXT_BOUNDARY_PARAGRAPH;
  return result;
}

static void
nk_kb_select_fonts(struct nk_kb_text_backend *backend, const struct nk_user_font *preferred_font)
{
  int i;
  while (kbts_ShapePopFont(backend->context)) {
  }

  for (i = 0; i < backend->num_fonts; ++i) {
    if (backend->fonts[i]->UserData != preferred_font) {
      kbts_ShapePushFont(backend->context, backend->fonts[i]);
    }
  }

  for (i = 0; i < backend->num_fonts; ++i) {
    if (backend->fonts[i]->UserData == preferred_font) {
      kbts_ShapePushFont(backend->context, backend->fonts[i]);
    }
  }
}

static float
nk_kb_font_scale(kbts_font *font, float height)
{
  kbts_font_info2_1 info;
  NK_MEMSET(&info, 0, sizeof(info));
  info.Base.Size = sizeof(info);
  kbts_GetFontInfo2(font, &info.Base);
  return height / info.UnitsPerEm;
}

static float
nk_kb_text_baseline(struct nk_kb_text_backend *backend, const struct nk_user_font *font, float height)
{
  for (int i = 0; i < backend->num_fonts; ++i) {
    if (backend->fonts[i]->UserData == font) {
      kbts_font_info2_1 info;
      NK_MEMSET(&info, 0, sizeof(info));
      info.Base.Size = sizeof(info);
      kbts_GetFontInfo2(backend->fonts[i], &info.Base);
      return info.Ascent * height / info.UnitsPerEm;
    }
  }
  return height;
}

static int
nk_kb_font_covers(kbts_font *font, const char *source, int source_len)
{
  kbts_font_coverage_test test;
  const char             *end = source + source_len;

  kbts_FontCoverageTestBegin(&test, font);
  while (source < end) {
    kbts_decode decoded = kbts_DecodeUtf8(source, end - source);
    kbts_FontCoverageTestCodepoint(&test, decoded.Codepoint);
    source += decoded.SourceCharactersConsumed;
  }
  return kbts_FontCoverageTestEnd(&test);
}

static kbts_font *
nk_kb_grapheme_font(struct nk_kb_text_backend *backend, const struct nk_user_font *preferred_font, const char *source, int source_len)
{
  int i;
  for (i = 0; i < backend->num_fonts; ++i) {
    if (backend->fonts[i]->UserData == preferred_font && nk_kb_font_covers(backend->fonts[i], source, source_len)) {
      return backend->fonts[i];
    }
  }

  for (i = backend->num_fonts - 1; i >= 0; --i) {
    if (backend->fonts[i]->UserData != preferred_font && nk_kb_font_covers(backend->fonts[i], source, source_len)) {
      return backend->fonts[i];
    }
  }

  for (i = 0; i < backend->num_fonts; ++i) {
    if (backend->fonts[i]->UserData == preferred_font) {
      return backend->fonts[i];
    }
  }
  return backend->fonts[backend->num_fonts - 1];
}

static void
nk_kb_grapheme_properties(const char *source, int source_len, kbts_direction *direction, kbts_script *script)
{
  kbts_GuessTextPropertiesUtf8(source, source_len, direction, script);
}

static int
nk_kb_cluster_idx(const struct nk_text_cluster *clusters, int num_clusters, int source_offset)
{
  int first = 0;
  int count = num_clusters;
  while (count > 0) {
    int step = count / 2;
    int idx  = first + step;
    if (clusters[idx].source_end <= source_offset) {
      first  = idx + 1;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  return first;
}

static void
nk_kb_reverse_runs(struct nk_text_run *runs, int begin, int end)
{
  while (begin < --end) {
    struct nk_text_run swap = runs[begin];
    runs[begin++] = runs[end];
    runs[end] = swap;
  }
}

static void
nk_kb_reorder_shape(struct nk_text_shape *shape, enum nk_text_direction direction)
{
  unsigned int paragraph_level = direction == NK_TEXT_DIRECTION_RTL;
  unsigned int max_level       = paragraph_level;
  unsigned int min_odd_level   = ~0u;
  int          i;

  if (direction == NK_TEXT_DIRECTION_INVALID) {
    for (i = 0; i < shape->num_runs; ++i) {
      if (shape->runs[i].bidi_level != ~0u) {
        paragraph_level = shape->runs[i].direction == NK_TEXT_DIRECTION_RTL;
        break;
      }
    }
  }

  for (i = 0; i < shape->num_runs; ++i) {
    if (shape->runs[i].bidi_level != ~0u) {
      if (shape->runs[i].direction == NK_TEXT_DIRECTION_RTL) {
        shape->runs[i].bidi_level = 1;
      } else {
        shape->runs[i].bidi_level = paragraph_level ? 2 : 0;
      }
    }
  }

  for (i = 0; i < shape->num_runs; ++i) {
    if (shape->runs[i].bidi_level == ~0u) {
      unsigned int left  = paragraph_level;
      unsigned int right = paragraph_level;
      int          j;

      for (j = i; j > 0; --j) {
        if (shape->runs[j - 1].bidi_level != ~0u) {
          left = shape->runs[j - 1].bidi_level;
          break;
        }
      }
      for (j = i + 1; j < shape->num_runs; ++j) {
        if (shape->runs[j].bidi_level != ~0u) {
          right = shape->runs[j].bidi_level;
          break;
        }
      }
      shape->runs[i].bidi_level = (left & 1) == (right & 1) ? left : paragraph_level;
    }

    max_level = NK_MAX(max_level, shape->runs[i].bidi_level);
    if (shape->runs[i].bidi_level & 1) {
      min_odd_level = NK_MIN(min_odd_level, shape->runs[i].bidi_level);
    }
  }

  for (unsigned int level = max_level; level >= min_odd_level; --level) {
    for (i = 0; i < shape->num_runs;) {
      int begin;
      while (i < shape->num_runs && shape->runs[i].bidi_level < level) {
        i += 1;
      }
      begin = i;
      while (i < shape->num_runs && shape->runs[i].bidi_level >= level) {
        i += 1;
      }
      if (i - begin > 1) {
        nk_kb_reverse_runs(shape->runs, begin, i);
      }
    }
  }
}

NK_API struct nk_text_shape *
nk_kb_text_shape_build(nk_handle userdata, struct nk_buffer *cache, const struct nk_text_request *request)
{
  struct nk_kb_text_backend *backend = userdata.ptr;
  struct nk_text_shape      *shape;
  kbts_break_flags          *break_flags;
  kbts_direction             paragraph_direction = nk_kb_direction(request->direction);
  struct nk_text_chunk_list  run_list;
  struct nk_text_chunk_list  glyph_list;
  int                        i;

  nk_buffer_clear(backend->temporary);
  shape = nk_buffer_alloc(cache, NK_BUFFER_FRONT, sizeof(*shape), NK_ALIGNOF(struct nk_text_shape));
  if (!shape) {
    return NULL;
  }
  NK_MEMSET(shape, 0, sizeof(*shape));
  shape->baseline = nk_kb_text_baseline(backend, request->font, request->font_height);

  shape->source = nk_buffer_alloc(cache, NK_BUFFER_FRONT, request->source_len + 1, NK_ALIGNOF(char));
  if (!shape->source) {
    return NULL;
  }
  NK_MEMCPY(shape->source, request->source, request->source_len);
  shape->source[request->source_len] = '\0';
  shape->source_len                  = request->source_len;

  if (!request->source_len) {
    return shape;
  }

  break_flags = nk_buffer_alloc(backend->temporary, NK_BUFFER_FRONT, sizeof(*break_flags) * (request->source_len + 1), NK_ALIGNOF(kbts_break_flags));
  if (!break_flags) {
    return NULL;
  }
  kbts_BreakEntireStringUtf8(paragraph_direction, KBTS_JAPANESE_LINE_BREAK_STYLE_NORMAL, KBTS_BREAK_CONFIG_FLAG_NONE,
                            request->source, request->source_len, NULL, 0, NULL, break_flags, request->source_len + 1, NULL);

  for (i = 1; i <= request->source_len; ++i) {
    if (nk_kb_boundary_flags(break_flags[i])) {
      shape->num_boundaries += 1;
    }
  }

  if (shape->num_boundaries) {
    int boundary_idx = 0;

    shape->boundaries = nk_buffer_alloc(cache, NK_BUFFER_FRONT, sizeof(*shape->boundaries) * shape->num_boundaries, NK_ALIGNOF(struct nk_text_boundary));
    if (!shape->boundaries) {
      return NULL;
    }

    for (i = 1; i <= request->source_len; ++i) {
      nk_flags flags = nk_kb_boundary_flags(break_flags[i]);
      if (flags) {
        shape->boundaries[boundary_idx].source_offset = i;
        shape->boundaries[boundary_idx].flags         = flags;
        boundary_idx                                 += 1;
      }
    }
  }

  shape->num_clusters = 1;
  for (i = 1; i < request->source_len; ++i) {
    if (break_flags[i] & KBTS_BREAK_FLAG_GRAPHEME) {
      shape->num_clusters += 1;
    }
  }

  shape->clusters = nk_buffer_alloc(cache, NK_BUFFER_FRONT, sizeof(*shape->clusters) * shape->num_clusters, NK_ALIGNOF(struct nk_text_cluster));
  if (!shape->clusters) {
    return NULL;
  }

  NK_MEMSET(shape->clusters, 0, sizeof(*shape->clusters) * shape->num_clusters);
  {
    int cluster_idx  = 0;
    int source_begin = 0;

    for (i = 1; i <= request->source_len; ++i) {
      if (i == request->source_len || (break_flags[i] & KBTS_BREAK_FLAG_GRAPHEME)) {
        shape->clusters[cluster_idx].source_begin = source_begin;
        shape->clusters[cluster_idx].source_end   = i;

        source_begin   = i;
        cluster_idx += 1;
      }
    }
  }
  nk_kb_select_fonts(backend, request->font);
  nk_text_chunk_list_init(&run_list,   sizeof(struct nk_text_run));
  nk_text_chunk_list_init(&glyph_list, sizeof(struct nk_text_glyph));

  for (int segment_cluster = 0; segment_cluster < shape->num_clusters;) {
    int         segment_begin       = shape->clusters[segment_cluster].source_begin;
    int         segment_cluster_end = segment_cluster + 1;
    kbts_font      *segment_font = nk_kb_grapheme_font(backend, request->font, request->source + segment_begin, shape->clusters[segment_cluster].source_end - segment_begin);
    kbts_direction  segment_direction;
    kbts_script     segment_script;
    nk_kb_grapheme_properties(request->source + segment_begin, shape->clusters[segment_cluster].source_end - segment_begin, &segment_direction, &segment_script);

    while (segment_cluster_end < shape->num_clusters) {
      struct nk_text_cluster *cluster    = &shape->clusters[segment_cluster_end];
      kbts_font              *font       = nk_kb_grapheme_font(backend, request->font, request->source + cluster->source_begin, cluster->source_end - cluster->source_begin);
      kbts_direction          direction;
      kbts_script             script;
      kbts_break_flags        run_breaks = break_flags[cluster->source_begin] & KBTS_BREAK_FLAG_DIRECTION;
      nk_kb_grapheme_properties(request->source + cluster->source_begin, cluster->source_end - cluster->source_begin, &direction, &script);
      if (font != segment_font || (!request->script_tag && (script != segment_script || direction != segment_direction || run_breaks))) {
        break;
      }
      segment_cluster_end += 1;
    }

    int segment_end = shape->clusters[segment_cluster_end - 1].source_end;

    kbts_ShapeBegin(backend->context, paragraph_direction, KBTS_LANGUAGE_DONT_KNOW);
    for (i = 0; i < request->num_features; ++i) {
      kbts_ShapePushFeature(backend->context, nk_kb_tag(request->features[i].tag), request->features[i].value);
    }

    if (request->script_tag) {
      kbts_script    script    = kbts_ScriptTagToScript(nk_kb_tag(request->script_tag));
      kbts_direction direction = paragraph_direction;
      if (direction == KBTS_DIRECTION_DONT_KNOW) {
        direction = kbts_ScriptDirection(script);
      }

      kbts_ShapeBeginManualRuns(backend->context);
      kbts_ShapeNextManualRun(backend->context, direction, script);
    }

    kbts_ShapeUtf8WithUserId(backend->context, request->source + segment_begin, segment_end - segment_begin, segment_begin, KBTS_USER_ID_GENERATION_MODE_SOURCE_INDEX);
    if (request->script_tag) {
      kbts_ShapeEndManualRuns(backend->context);
    }

    for (i = request->num_features; i > 0; --i) {
      kbts_ShapePopFeature(backend->context, nk_kb_tag(request->features[i - 1].tag));
    }

    kbts_ShapeEnd(backend->context);
    if (kbts_ShapeError(backend->context)) {
      return NULL;
    }

    kbts_run kb_run;
    while (kbts_ShapeRun(backend->context, &kb_run)) {
      struct nk_text_run run;
      kbts_glyph        *kb_glyph;
      float              scale;
      int                run_glyph_begin = shape->num_glyphs;

      scale = kbts_GlyphIteratorIsValid(&kb_run.Glyphs) ? nk_kb_font_scale(kb_run.Font, request->font_height) : 0.0f;

      NK_MEMSET(&run, 0, sizeof(run));
      run.font         = kb_run.Font->UserData;
      run.source_begin = request->source_len;
      run.glyph_begin  = run_glyph_begin;
      run.direction    = nk_kb_run_direction(kb_run.Direction);
      run.bidi_level   = !request->script_tag && segment_direction == KBTS_DIRECTION_DONT_KNOW ? ~0u : run.direction == NK_TEXT_DIRECTION_RTL;

      while (kbts_GlyphIteratorNext(&kb_run.Glyphs, &kb_glyph)) {
        kbts_shape_codepoint codepoint;
        struct nk_text_glyph glyph;
        int                  cluster_idx;

        kbts_ShapeGetShapeCodepoint(backend->context, kb_glyph->UserIdOrCodepointIndex, &codepoint);
        cluster_idx = nk_kb_cluster_idx(shape->clusters, shape->num_clusters, codepoint.UserId);

        glyph.id          =  kb_glyph->Id;
        glyph.cluster_idx =  cluster_idx;
        glyph.offset_x    =  kb_glyph->OffsetX  * scale;
        glyph.offset_y    = -kb_glyph->OffsetY  * scale;
        glyph.advance_x   =  kb_glyph->AdvanceX * scale;
        glyph.advance_y   = -kb_glyph->AdvanceY * scale;

        if (!nk_text_chunk_list_push(&glyph_list, backend->temporary, &glyph)) {
          return NULL;
        }

        if (!shape->clusters[cluster_idx].glyph_count) {
          shape->clusters[cluster_idx].glyph_begin = shape->num_glyphs;
        }

        shape->clusters[cluster_idx].glyph_count += 1;
        if (shape->clusters[cluster_idx].source_begin < run.source_begin) {
          run.source_begin = shape->clusters[cluster_idx].source_begin;
        }

        if (shape->clusters[cluster_idx].source_end > run.source_end) {
          run.source_end = shape->clusters[cluster_idx].source_end;
        }

        shape->advance.x  += glyph.advance_x;
        shape->advance.y  += glyph.advance_y;
        shape->num_glyphs += 1;
      }

      run.glyph_count = shape->num_glyphs - run_glyph_begin;
      if (run.glyph_count) {
        if (!nk_text_chunk_list_push(&run_list, backend->temporary, &run)) {
          return NULL;
        }
        shape->num_runs += 1;
      }
    }

    if (kbts_ShapeError(backend->context)) {
      return NULL;
    }
    segment_cluster = segment_cluster_end;
  }

  if (shape->num_runs) {
    shape->runs = nk_text_chunk_list_flatten(&run_list, cache, NK_ALIGNOF(struct nk_text_run));
    if (!shape->runs) {
      return NULL;
    }
  }

  if (shape->num_glyphs) {
    shape->glyphs = nk_text_chunk_list_flatten(&glyph_list, cache, NK_ALIGNOF(struct nk_text_glyph));
    if (!shape->glyphs) {
      return NULL;
    }
  }

  nk_kb_reorder_shape(shape, request->direction);
  return shape;
}
