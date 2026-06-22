#ifndef PATH_H
#define PATH_H

#include "str.h"
#include "arena.h"

str_t
path_base(str_t path);
str_t
path_dir(str_t path);
str_t
path_get_ext(str_t path);
str_t
path_trim_ext(str_t path);
str_t
path_replace_slashes_inplace(str_t path, char ch);

bool
path_equal(str_t a, str_t b);
bool
path_has_prefix(str_t path, str_t prefix);
bool
path_make_relative(str_t path, str_t base, str_t *out);
str_t
path_push_relative(arena_t *arena, str_t path, str_t base);

bool
path_is_abs(str_t path);

str_list_t
path_push_parts(arena_t *arena, str_t path);

str_t
path_module_dir(arena_t *perm);

str_t
path_join(arena_t *arena, str_t a, str_t b);

#endif /* PATH_H */
