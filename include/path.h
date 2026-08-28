#ifndef PATH_H
#define PATH_H

#include "str.h"
#include "arena.h"

MOD_EXTERN_C_BEGIN

MOD_API str_t
path_base(str_t path);
MOD_API str_t
path_dir(str_t path);
MOD_API str_t
path_get_ext(str_t path);
MOD_API str_t
path_trim_ext(str_t path);
MOD_API str_t
path_replace_slashes_inplace(str_t path, char ch);

MOD_API bool
path_equal(str_t a, str_t b);
MOD_API bool
path_has_prefix(str_t path, str_t prefix);
MOD_API bool
path_make_relative(str_t path, str_t base, str_t *out);
MOD_API str_t
path_push_relative(arena_t *arena, str_t path, str_t base);

MOD_API bool
path_is_abs(str_t path);

MOD_API str_list_t
path_push_parts(arena_t *arena, str_t path);

MOD_API str_t
path_module_dir(arena_t *perm);

MOD_API str_t
path_join(arena_t *arena, str_t a, str_t b);

MOD_EXTERN_C_END

#endif /* PATH_H */
