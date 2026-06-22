#include "file.h"

#include "scratch.h"
#include "arena.h"
#include "str.h"
#include "path.h"

#include <windows.h>

bool
file_exists(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  DWORD attr   = GetFileAttributesW(path16.data);
  bool  exists = (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);

  scratch_end(tmp);
  return exists;
}

uint64_t
file_size(str_t path)
{
  if (str_is_empty(path)) {
    return 0;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);
  uint64_t    size   = 0;

  WIN32_FILE_ATTRIBUTE_DATA data;
  if (GetFileAttributesExW(path16.data, GetFileExInfoStandard, &data)) {
    size = ((uint64_t)data.nFileSizeHigh << 32) | (uint64_t)data.nFileSizeLow;
  }

  scratch_end(tmp);
  return size;
}

str_t
file_read_all(str_t path, arena_t *arena)
{
  str_t result = {0};
  if (str_is_empty(path) || !arena) {
    return result;
  }

  tmp_arena_t tmp    = scratch_begin(arena);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  HANDLE h = CreateFileW(path16.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER size_li;
    if (GetFileSizeEx(h, &size_li) && size_li.QuadPart > 0) {
      uint64_t size = size_li.QuadPart;
      uint8_t *data = ARENA_PUSH_ARRAY_ZERO(arena, uint8_t, size);
      if (data) {
        uint64_t total_read = 0;
        while (total_read < size) {
          DWORD to_read   = (DWORD)MIN_VAL((uint64_t)UINT32_MAX, (size - total_read));
          DWORD just_read = 0;

          if (!ReadFile(h, data + total_read, to_read, &just_read, NULL)) {
            break;
          }

          if (just_read == 0) {
            break;
          }

          total_read += just_read;
        }

        if (total_read != size) {
          arena_pop(arena, size);
        } else {
          result = str_make(data, total_read);
        }
      }
    }
    CloseHandle(h);
  }

  scratch_end(tmp);
  return result;
}

bool
file_write(str_t s, str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);
  bool        result = false;

  HANDLE h = CreateFileW(path16.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    if (str_is_empty(s)) {
      // empty string -> empty file
      result = true;
    } else {
      bool     success       = true;
      uint64_t total_written = 0;
      while (total_written < s.len) {
        DWORD to_write     = (DWORD)MIN_VAL((uint64_t)UINT32_MAX, (s.len - total_written));
        DWORD just_written = 0;

        if (!WriteFile(h, s.data + total_written, to_write, &just_written, NULL)) {
          success = false;
          break;
        }

        if (just_written == 0) {
          success = false;
          break;
        }

        total_written += (uint64_t)just_written;
      }
      result = success && (total_written == s.len);
    }
    CloseHandle(h);
  }

  scratch_end(tmp);
  return result;
}

bool
file_write_lines(str_list_t lines, str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);
  bool        result = false;

  HANDLE h = CreateFileW(path16.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h != INVALID_HANDLE_VALUE) {
    bool     success       = true;
    uint64_t total_written = 0;
    uint64_t total_size    = lines.total_bytes + (lines.count > 1 ? lines.count - 1 : 0);

    for (str_node_t *node = lines.first; node && success; node = node->next) {
      str_t chunks[] = {
        node->str,
        (node != lines.last) ? STR_LIT("\n") : STR_NULL,
      };

      for (uint64_t chunk_idx = 0; chunk_idx < 2; ++chunk_idx) {
        str_t chunk = chunks[chunk_idx];
        if (str_is_empty(chunk)) {
          continue;
        }

        uint64_t chunk_written = 0;
        while (chunk_written < chunk.len) {
          DWORD to_write     = (DWORD)MIN_VAL((uint64_t)UINT32_MAX, (chunk.len - chunk_written));
          DWORD just_written = 0;

          if (!WriteFile(h, chunk.data + chunk_written, to_write, &just_written, NULL)) {
            success = false;
            break;
          }

          if (just_written == 0) {
            success = false;
            break;
          }

          chunk_written += (uint64_t)just_written;
          total_written += (uint64_t)just_written;
        }
      }
    }

    result = success && (total_written == total_size);
    CloseHandle(h);
  }

  scratch_end(tmp);
  return result;
}

bool
file_copy(str_t dst, str_t src)
{
  if (str_is_empty(dst) || str_is_empty(src)) {
    return false;
  }

  tmp_arena_t tmp   = scratch_begin(NULL);
  str16_t     src16 = str16_from_str(tmp.arena, src);
  str16_t     dst16 = str16_from_str(tmp.arena, dst);

  bool result = (CopyFileW(src16.data, dst16.data, FALSE) != 0);

  scratch_end(tmp);
  return result;
}

bool
file_delete(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  bool result = (DeleteFileW(path16.data) != 0);

  scratch_end(tmp);
  return result;
}

bool
dir_exists(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  DWORD attr   = GetFileAttributesW(path16.data);
  bool  exists = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);

  scratch_end(tmp);
  return exists;
}

bool
dir_create(str_t path)
{
  if (dir_exists(path)) {
    return true;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  bool result = CreateDirectoryW(path16.data, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;

  scratch_end(tmp);
  return result;
}

bool
dir_create_recursive(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  if (dir_exists(path)) {
    return true;
  }

  /* walk the path and create each segment */
  for (uint64_t i = 0; i < path.len; ++i) {
    uint8_t c = path.data[i];
    if (c == '/' || c == '\\') {
      str_t segment = str_slice(path, 0, i);
      if (!str_is_empty(segment) && !dir_exists(segment)) {
        if (!dir_create(segment)) {
          return false;
        }
      }
    }
  }

  return dir_create(path); 
}

static inline bool
is_current_or_parent_dir(const wchar_t *name)
{
  if (!name || name[0] == L'\0') {
    return false;
  }

  /* '.' and '..' */
  return (name[0] == L'.' && (name[1] == L'\0' || (name[1] == L'.' && name[2] == L'\0')));
}

static dir_entry_list_t
dir_list_internal(str_t path, str_t pattern, arena_t *arena)
{
  tmp_arena_t      tmp    = scratch_begin(arena);
  dir_entry_list_t result = {0};

  path = path_replace_slashes_inplace(str_push_copy(tmp.arena, path), '\\');

  /* build search pattern: path\* or path\pattern */
  str_t search_pattern;
  if (str_is_empty(path)) {
    if (str_is_empty(pattern)) {
      search_pattern = STR_LIT("*");
    } else {
      search_pattern = pattern;
    }
  } else {
    if (str_is_empty(pattern)) {
      search_pattern = str_push_fmt(tmp.arena, "%.*s\\*", STR_ARG(path));
    } else {
      search_pattern = str_push_fmt(tmp.arena, "%.*s\\%.*s", STR_ARG(path), STR_ARG(pattern));
    }
  }

  str16_t pattern16 = str16_from_str(tmp.arena, search_pattern);

  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(pattern16.data, &fd);
  if (h != INVALID_HANDLE_VALUE) {
    /* first pass: count entries */
    int count = 0;
    do {
      if (is_current_or_parent_dir(fd.cFileName)) {
        continue;
      }
      count += 1;
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    if (count > 0) {
      /* second pass: fill entries */
      result.items = ARENA_PUSH_ARRAY_ZERO(arena, dir_entry_t, count);
      result.count = 0;

      if (result.items) {
        h = FindFirstFileW(pattern16.data, &fd);
        if (h != INVALID_HANDLE_VALUE) {
          do {
            if (is_current_or_parent_dir(fd.cFileName)) {
              continue;
            }

            if (result.count >= count) {
              break;
            }

            dir_entry_t *entry  = &result.items[result.count];
            str16_t      name16 = str16_from_wstr(fd.cFileName);

            entry->name = str_from_str16(arena, name16);
            entry->path = path_join(arena, path, entry->name);
            entry->kind = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DIR_ENTRY_DIR : DIR_ENTRY_FILE;
            entry->size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;

            result.count += 1;
          } while (FindNextFileW(h, &fd));
          FindClose(h);
        }
      }
    }
  }

  scratch_end(tmp);
  return result;
}

dir_entry_list_t
dir_list(str_t path, arena_t *arena)
{
  return dir_list_internal(path, (str_t){0}, arena);
}

dir_entry_list_t
dir_list_filter(str_t path, str_t pattern, arena_t *arena)
{
  return dir_list_internal(path, pattern, arena);
}

bool
dir_delete(str_t path)
{
  if (str_is_empty(path)) {
    return false;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  str16_t     path16 = str16_from_str(tmp.arena, path);

  bool result = (RemoveDirectoryW(path16.data) != 0);

  scratch_end(tmp);
  return result;
}

bool
dir_delete_recursive(str_t path)
{
  tmp_arena_t      tmp     = scratch_begin(NULL);
  dir_entry_list_t entries = dir_list(path, tmp.arena);
  for (int i = 0; i < entries.count; ++i) {
    dir_entry_t *e = &entries.items[i];
    if (e->kind == DIR_ENTRY_DIR) {
      dir_delete_recursive(e->path);
    } else {
      file_delete(e->path);
    }
  }

  scratch_end(tmp);
  return dir_delete(path);
}
