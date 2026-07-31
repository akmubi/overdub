#include "nk_font_runtime.h"

#include "file.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static const nk_rune nk_font_ranges[] = {
  0x00020, 0x0007E,
  0x000A0, 0x000FF,
  0x00100, 0x0017F,
  0x00400, 0x0052F,
  0x02000, 0x0206F,
  0x020A0, 0x020CF,
  0x02100, 0x0214F,
  0x02190, 0x021FF,
  0x02200, 0x022FF,
  0x02500, 0x0257F,
  0x02580, 0x0259F,
  0x025A0, 0x025FF,
  0x0EB52, 0x0EB52,
  0x0F452, 0x0F452,
  0x0FFFD, 0x0FFFD,
  0xF018F, 0xF018F,
  0xF03CC, 0xF03CC,
  0,
};

static void *
nk_arena_alloc(nk_handle userdata, void *old, nk_size size)
{
  (void)old;
  return arena_push_aligned(userdata.ptr, size, 16);
}

static void
nk_arena_free(nk_handle userdata, void *memory)
{
  (void)userdata;
  (void)memory;
}

bool
nk_runtime_font_source_system(struct nk_runtime_font_source *source,
                              arena_t                       *arena,
                              const char                    *name,
                              enum nk_runtime_font_style     style)
{
  char path[MAX_PATH];
  int  len = GetWindowsDirectoryA(path, COUNTOF(path));
  if (!len || len + 7 + (int)strlen(name) >= COUNTOF(path)) {
    return false;
  }

  len += snprintf(path + len, COUNTOF(path) - len, "\\Fonts\\%s", name);
  str_t ttf = file_read_all(str_make(path, len), arena);
  if (!ttf.len) {
    return false;
  }

  *source = (struct nk_runtime_font_source){ttf.data, ttf.len, NULL, style};
  return true;
}

static struct nk_font *
nk_runtime_font_add(struct nk_font_atlas *atlas, const struct nk_runtime_font_source *source, float size)
{
  struct nk_font_config config = nk_font_config(-size);
  {
    config.range                   = source->range ? source->range : nk_font_ranges;
    config.ttf_blob                = source->ttf;
    config.ttf_size                = source->ttf_size;
    config.ttf_data_owned_by_atlas = 1;
  }
  return nk_font_atlas_add(atlas, &config);
}

bool
nk_runtime_fonts_bake(struct nk_runtime_fonts             *fonts,
                      arena_t                             *perm,
                      arena_t                             *tmp,
                      const struct nk_runtime_font_source *sources,
                      int                                  source_count,
                      float                                size)
{
  NK_ASSERT(source_count > 0 && source_count <= NK_RUNTIME_FONT_CAPACITY);
  NK_ASSERT(sources[0].style == NK_RUNTIME_FONT_REGULAR);

  struct nk_allocator perm_allocator = {
    .userdata.ptr = perm,
    .alloc        = nk_arena_alloc,
    .free         = nk_arena_free,
  };

  struct nk_allocator tmp_allocator = {
    .userdata.ptr = tmp,
    .alloc        = nk_arena_alloc,
    .free         = nk_arena_free,
  };

  nk_font_atlas_init_custom(&fonts->atlas, &perm_allocator, &tmp_allocator);
  nk_font_atlas_begin(&fonts->atlas);

  fonts->face_count = source_count;
  for (int i = 0; i < source_count; ++i) {
    fonts->faces[i].font     = nk_runtime_font_add(&fonts->atlas, &sources[i], size);
    fonts->faces[i].ttf      = sources[i].ttf;
    fonts->faces[i].ttf_size = sources[i].ttf_size;
    fonts->faces[i].style    = sources[i].style;
  }

  fonts->pixels = nk_font_atlas_bake(&fonts->atlas, &fonts->width, &fonts->height, NK_FONT_ATLAS_RGBA32);
  return fonts->pixels != NULL;
}

struct nk_font *
nk_runtime_font(struct nk_runtime_fonts *fonts, enum nk_runtime_font_style style)
{
  for (int i = 0; i < fonts->face_count; ++i) {
    if (fonts->faces[i].style == style) {
      return fonts->faces[i].font;
    }
  }
  return NULL;
}
