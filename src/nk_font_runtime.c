#include "nk_font_runtime.h"

#include "file.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

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

  *source = (struct nk_runtime_font_source){ttf.data, ttf.len, style};
  return true;
}

bool
nk_runtime_font_source_system_mapped(struct nk_runtime_font_source *source,
                                     const char                    *name,
                                     enum nk_runtime_font_style     style)
{
  char path[MAX_PATH];
  int  len = GetWindowsDirectoryA(path, COUNTOF(path));
  if (!len || len + 7 + (int)strlen(name) >= COUNTOF(path)) {
    return false;
  }

  snprintf(path + len, COUNTOF(path) - len, "\\Fonts\\%s", name);

  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }

  void         *view = NULL;
  LARGE_INTEGER size = {0};

  if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 0x7FFFFFFF) {
    /* copy-on-write: the font code may write to the data */
    HANDLE mapping = CreateFileMappingA(file, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    if (mapping) {
      view = MapViewOfFile(mapping, FILE_MAP_COPY, 0, 0, 0);
      CloseHandle(mapping);
    }
  }

  CloseHandle(file);

  if (!view) {
    return false;
  }

  *source = (struct nk_runtime_font_source){view, (nk_size)size.QuadPart, style};
  return true;
}

bool
nk_runtime_fonts_init(struct nk_runtime_fonts             *fonts,
                      arena_t                             *perm,
                      const struct nk_runtime_font_source *sources,
                      int                                  source_count,
                      float                                size,
                      int                                  width,
                      int                                  height,
                      int                                  max_glyphs)
{
  NK_ASSERT(source_count > 0 && source_count <= NK_RUNTIME_FONT_CAPACITY);
  NK_ASSERT(sources[0].style == NK_RUNTIME_FONT_REGULAR);

  nk_size bytes  = nk_glyph_atlas_memory_size(width, height, max_glyphs);
  void   *memory = arena_push_aligned(perm, bytes, 16);

  if (!memory || !nk_glyph_atlas_init(&fonts->atlas, memory, bytes, width, height, max_glyphs)) {
    return false;
  }

  fonts->face_count = 0;

  for (int i = 0; i < source_count; ++i) {
    struct nk_runtime_font_face *face = &fonts->faces[fonts->face_count];

    face->ttf      = sources[i].ttf;
    face->ttf_size = sources[i].ttf_size;
    face->style    = sources[i].style;

    if (nk_font_init(&face->font, &fonts->atlas, face->ttf, face->ttf_size, 0, size)) {
      fonts->face_count += 1;
    } else if (i == 0) {
      return false;
    }
  }
  return true;
}

struct nk_font *
nk_runtime_font(struct nk_runtime_fonts *fonts, enum nk_runtime_font_style style)
{
  for (int i = 0; i < fonts->face_count; ++i) {
    if (fonts->faces[i].style == style) {
      return &fonts->faces[i].font;
    }
  }
  return NULL;
}
