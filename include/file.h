#ifndef FILE_H
#define FILE_H

#include "types.h"
#include "str.h"

typedef uint8_t dir_entry_kind_t;
enum {
  DIR_ENTRY_FILE,
  DIR_ENTRY_DIR,
};

typedef struct dir_entry_s dir_entry_t;
struct dir_entry_s {
  str_t            name; // filename only, no path
  str_t            path; // full path
  dir_entry_kind_t kind;
  uint64_t         size; // file size in bytes (0 for dirs)
};

typedef struct dir_entry_list_s dir_entry_list_t;
struct dir_entry_list_s {
  dir_entry_t *items;
  int          count;
};

bool
file_exists(str_t path);
uint64_t
file_size(str_t path);

str_t
file_read_all(str_t path, arena_t *arena);
bool
file_write(str_t s, str_t path);
bool
file_write_lines(str_list_t lines, str_t path);
bool
file_copy(str_t dst, str_t src);
bool
file_delete(str_t path);

bool
dir_exists(str_t path);

/* create a directory (does nothing if it already exists) */
bool
dir_create(str_t path);

/* create a directory and all parent directories */
bool
dir_create_recursive(str_t path);

/* list the contents of a directory (excludes "." and "..") */
dir_entry_list_t
dir_list(str_t path, arena_t *arena);

/* list contents matching a wildcard pattern (e.g. "*.dll") */
dir_entry_list_t
dir_list_filter(str_t path, str_t pattern, arena_t *arena);

/* delete a directory (must be empty) */
bool
dir_delete(str_t path);

/* delete a directory and all its contents recursively */
bool
dir_delete_recursive(str_t path);

#endif /* FILE_H */
