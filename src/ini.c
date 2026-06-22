#include "ini.h"

#include "scratch.h"

bool
ini_is_line_section(str_t line)
{
  return (line.len >= 2 && line.data[0] == '[' && line.data[line.len - 1] == ']');
}

bool
ini_parse_section_header(str_t line, str_t *out_name, str_t *out_arg)
{
  *out_name = (str_t){0};
  *out_arg  = (str_t){0};

  str_t    body    = str_trim_space(str_slice(line, 1, line.len - 1));
  uint64_t dot_idx = 0;
  if (str_find(body, STR_LIT("."), 0, &dot_idx)) {
    *out_name = str_trim_space(str_slice(body, 0, dot_idx));
    *out_arg  = str_trim_space(str_slice(body, dot_idx + 1, body.len));
  } else {
    *out_name = body;
  }

  return !str_is_empty(*out_name);
}

static void
ini_section_list_push(ini_section_list_t *list, ini_section_t *section)
{
  QUEUE_PUSH(list->first, list->last, section);
  list->count += 1;
}

ini_section_list_t
ini_parse_sections(arena_t *arena, str_array_t lines)
{
  ini_section_list_t result  = {0};
  ini_section_t     *section = NULL;
  for (uint64_t i = 0; i < lines.count; ++i) {
    str_t line = lines.items[i];

    if (ini_is_line_section(line)) {
      str_t name = {0};
      str_t arg  = {0};
      if (ini_parse_section_header(line, &name, &arg)) {
        section = ARENA_PUSH_ZERO(arena, ini_section_t);
        if (section) {
          section->name = str_push_copy(arena, name);
          section->arg  = str_push_copy(arena, arg);

          ini_section_list_push(&result, section);
        }
      } else {
        section = NULL;
      }
      continue;
    }

    if (section) {
      str_list_push(arena, &section->lines, str_push_copy(arena, line));
    }
  }

  return result;
}

bool
ini_parse_kv(str_t line, str_t *out_key, str_t *out_value)
{
  if (!out_key || !out_value) {
    return false;
  }

  if (str_is_empty(line)) {
    return false;
  }

  *out_key   = (str_t){0};
  *out_value = (str_t){0};

  uint64_t eq_idx = 0;
  if (!str_find(line, STR_LIT("="), 0, &eq_idx)) {
    return false;
  }

  *out_key   = str_trim_space(str_slice(line, 0, eq_idx));
  *out_value = str_trim_space(str_slice(line, eq_idx + 1, line.len));

  return !str_is_empty(*out_key);
}

str_array_t
ini_split_lines(arena_t *arena, str_t text)
{
  if (!arena || str_is_empty(text)) {
    return (str_array_t){0};
  }

  tmp_arena_t tmp       = scratch_begin(arena);
  str_array_t out_array = {0};
  {
    str_list_t  parts     = {0};
    str_list_t  raw_lines = str_split_lines(tmp.arena, text);
    for (str_node_t *node = raw_lines.first; node; node = node->next) {
      str_t line = node->str;

      line = str_trim_comment(line, STR_LIT(";"), 0);
      line = str_trim_space(line);

      str_list_push(tmp.arena, &parts, line);
    }
    out_array = str_array_copy_from_list(arena, parts);
  }
  scratch_end(tmp);

  return out_array;
}
