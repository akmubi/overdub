#ifndef INI_H
#define INI_H

#include "str.h"
#include "arena.h"

typedef struct ini_section_s ini_section_t;
struct ini_section_s {
  ini_section_t *next;
  str_t          name;
  str_t          arg;
  str_list_t     lines;
};

typedef struct ini_section_list_s ini_section_list_t;
struct ini_section_list_s {
  ini_section_t *first;
  ini_section_t *last;
  int            count;
};

bool
ini_is_line_section(str_t line);
bool
ini_parse_section_header(str_t line, str_t *out_name, str_t *out_arg);

ini_section_list_t
ini_parse_sections(arena_t *arena, str_array_t lines);

bool
ini_parse_kv(str_t line, str_t *out_key, str_t *out_value);
str_array_t
ini_split_lines(arena_t *arena, str_t text);

#endif /* INI_H */
