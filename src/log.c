#include "log.h"

#include "arena.h"
#include "globals.h"
#include "path.h"
#include "scratch.h"
#include "str.h"
#include "ui_console.h"

#include <windows.h>

typedef uint32_t log_color_t;
enum {
  COLOR_RESET  = 0,
  COLOR_BLACK  = 1,
  COLOR_RED    = 2,
  COLOR_GREEN  = 3,
  COLOR_YELLOW = 4,
  COLOR_BLUE   = 5,
  COLOR_PURPLE = 6,
  COLOR_CYAN   = 7,
  COLOR_WHITE  = 8,
};

typedef struct logger_s logger_t;
struct logger_s {
  str_t            file_path;
  log_level_t      level;
  log_sink_flags_t default_sinks;
  HANDLE           file;
  SRWLOCK          rwlock;
  arena_t          perm;
  tmp_arena_t      tmp_msg;
};

static logger_t g_logger = {
  .level         = LOG_LEVEL_INFO,
  .default_sinks = LOG_SINK_DEFAULT,
  .file          = INVALID_HANDLE_VALUE,
  .rwlock        = SRWLOCK_INIT,
};

static WORD g_console_attr_default = 0;

static inline const char *
log_level_to_str(log_level_t level)
{
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return "DBG";
  case LOG_LEVEL_INFO:
    return "INF";
  case LOG_LEVEL_WARN:
    return "WRN";
  case LOG_LEVEL_ERROR:
    return "ERR";
  }
  return "UNK";
}

static inline log_color_t
log_level_color(log_level_t level)
{
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return COLOR_PURPLE;
  case LOG_LEVEL_INFO:
    return COLOR_WHITE;
  case LOG_LEVEL_WARN:
    return COLOR_YELLOW;
  case LOG_LEVEL_ERROR:
    return COLOR_RED;
  }
  return COLOR_WHITE;
}

static WORD
color_to_console_attr(log_color_t color)
{
  WORD attr = 0;
  switch (color) {
  case COLOR_BLACK:
    attr = 0;
    break;
  case COLOR_RED:
    attr = FOREGROUND_RED | FOREGROUND_INTENSITY;
    break;
  case COLOR_GREEN:
    attr = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    break;
  case COLOR_YELLOW:
    attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    break;
  case COLOR_BLUE:
    attr = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    break;
  case COLOR_PURPLE:
    attr = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    break;
  case COLOR_CYAN:
    attr = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    break;
  case COLOR_WHITE:
    attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    break;
  default:
    attr = g_console_attr_default;
    break;
  }
  return attr;
}

static void
console_set_color(log_color_t color)
{
  HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (color == COLOR_RESET) {
    SetConsoleTextAttribute(h_out, g_console_attr_default);
  } else {
    SetConsoleTextAttribute(h_out, color_to_console_attr(color));
  }
}

static void
console_new_private(const wchar_t *title)
{
  FreeConsole();
  MASSERT(AllocConsole(), "failed to allocate console");

  if (title) {
    SetConsoleTitleW(title);
  }

  // disable QuickEdit to avoid stalls
  HANDLE h_in = GetStdHandle(STD_INPUT_HANDLE);
  if (h_in && h_in != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(h_in, &mode)) {
      mode &= ~ENABLE_QUICK_EDIT_MODE;
      mode |= ENABLE_EXTENDED_FLAGS;
      SetConsoleMode(h_in, mode);
    }
  }

  HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);

  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(h_out, &info)) {
    g_console_attr_default = info.wAttributes;
  } else {
    g_console_attr_default = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
  }
}

static void
write_two_digits(char *dst, int v)
{
  dst[0] = (char)('0' + (v / 10));
  dst[1] = (char)('0' + (v % 10));
}

static void
write_three_digits(char *dst, int v)
{
  dst[0] = (char)('0' + (v / 100));
  dst[1] = (char)('0' + ((v % 100) / 10));
  dst[2] = (char)('0' + (v % 10));
}

// returns length (always 23), format: "YYYY-MM-DD HH:MM:SS.MMM"
static void
format_time_stamp(char buf[32])
{
  SYSTEMTIME st;
  GetLocalTime(&st);

  int year = (int)st.wYear;

  buf[0] = (char)('0' + (year / 1000) % 10);
  buf[1] = (char)('0' + (year / 100) % 10);
  buf[2] = (char)('0' + (year / 10) % 10);
  buf[3] = (char)('0' + (year % 10));
  buf[4] = '-';
  write_two_digits(buf + 5, (int)st.wMonth);
  buf[7] = '-';
  write_two_digits(buf + 8, (int)st.wDay);
  buf[10] = ' ';
  write_two_digits(buf + 11, (int)st.wHour);
  buf[13] = ':';
  write_two_digits(buf + 14, (int)st.wMinute);
  buf[16] = ':';
  write_two_digits(buf + 17, (int)st.wSecond);
  buf[19] = '.';
  write_three_digits(buf + 20, (int)st.wMilliseconds);
  buf[23] = '\0';
}

static void
write_console(str_t msg)
{
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (!h || h == INVALID_HANDLE_VALUE || str_is_empty(msg)) {
    return;
  }

  DWORD written = 0;
  WriteConsoleA(h, msg.data, (DWORD)msg.len, &written, NULL);
}

static void
write_file(str_t msg)
{
  if (g_logger.file == INVALID_HANDLE_VALUE || str_is_empty(msg)) {
    return;
  }

  DWORD written = 0;
  WriteFile(g_logger.file, msg.data, (DWORD)msg.len, &written, NULL);
}

static HANDLE
open_log_file(str_t file_path)
{
  if (str_is_empty(file_path)) {
    return false;
  }

  tmp_arena_t tmp         = scratch_begin(NULL);
  HANDLE      result      = INVALID_HANDLE_VALUE;
  str16_t     file_path16 = str16_from_str(tmp.arena, file_path);

  result = CreateFileW(file_path16.data,
                       GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

  scratch_end(tmp);
  return result;
}

void
log_init(str_t file_path, log_level_t level, bool open_console)
{
  if (open_console) {
    console_new_private(L"Log window");
  }

  g_logger.file  = INVALID_HANDLE_VALUE;
  g_logger.level = level;
  g_logger.perm  = arena_new_dynamic(64 * KB, 16 * KB);

  if (path_is_abs(file_path)) {
    /* absolute path */
    g_logger.file_path = str_push_copy(&g_logger.perm, file_path);
  } else {
    /* relative path */
    g_logger.file_path = path_join(&g_logger.perm, globals.game_dir, file_path);
  }

  g_logger.tmp_msg = tmp_arena_begin(&g_logger.perm);
}

void
log_set_level(log_level_t level)
{
  g_logger.level = level;
}

void
log_set_default_sinks(log_sink_flags_t sinks)
{
  g_logger.default_sinks = sinks;
}

void
log_emit(log_level_t level, log_sink_flags_t sinks, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  log_emitv(level, sinks, fmt, ap);
  va_end(ap);
}

void
log_emitv(log_level_t level, log_sink_flags_t sinks, const char *fmt, va_list ap)
{
  if (level < g_logger.level) {
    return;
  }

  if (sinks == LOG_SINK_NONE) {
    sinks = g_logger.default_sinks;
  }

  if (sinks & LOG_SINK_UI) {
#if !defined BUILD_TEST
    va_list ap_ui;
    va_copy(ap_ui, ap);
    ui_console_logv(&globals.ui_manager.console, ui_console_level_from_log(level), fmt, ap_ui);
    va_end(ap_ui);
#endif
  }

  if ((sinks & (LOG_SINK_FILE | LOG_SINK_WINCON)) == 0) {
    return;
  }

  char ts[32];
  format_time_stamp(ts);
  str_t ts_str = str_from_cstr_with_cap(ts, sizeof(ts));

  AcquireSRWLockExclusive(&g_logger.rwlock);
  {
    tmp_arena_end(g_logger.tmp_msg);

    va_list ap_msg;
    va_copy(ap_msg, ap);
    str_t msg = str_push_vfmt(g_logger.tmp_msg.arena, fmt, ap_msg);
    va_end(ap_msg);

    str_t       level_name  = str_push_fmt(g_logger.tmp_msg.arena, " [%s] ", log_level_to_str(level));
    log_color_t level_color = log_level_color(level);

    str_list_t msg_lines = str_split_lines(g_logger.tmp_msg.arena, msg);

    if (sinks & LOG_SINK_WINCON) {
      console_set_color(level_color);
      for (str_node_t *node = msg_lines.first; node; node = node->next) {
        write_console(ts_str);
        write_console(level_name);
        write_console(node->str);
        write_console(STR_LIT("\n"));
      }
      console_set_color(COLOR_RESET);
    }

    if (sinks & LOG_SINK_FILE) {
      if (g_logger.file == INVALID_HANDLE_VALUE) {
        g_logger.file = open_log_file(g_logger.file_path);
      }

      if (g_logger.file != INVALID_HANDLE_VALUE) {
        for (str_node_t *node = msg_lines.first; node; node = node->next) {
          write_file(ts_str);
          write_file(level_name);
          write_file(node->str);
          write_file(STR_LIT("\r\n"));
        }
      }
    }
  }
  ReleaseSRWLockExclusive(&g_logger.rwlock);
}

void
log_flush(void)
{
  AcquireSRWLockExclusive(&g_logger.rwlock);

  if (g_logger.file != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(g_logger.file);
  }

  ReleaseSRWLockExclusive(&g_logger.rwlock);
}

void
log_deinit(void)
{
  AcquireSRWLockExclusive(&g_logger.rwlock);

  if (g_logger.file != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(g_logger.file);
    CloseHandle(g_logger.file);
    g_logger.file = INVALID_HANDLE_VALUE;
  }

  arena_destroy(&g_logger.perm);
  FreeConsole();

  ReleaseSRWLockExclusive(&g_logger.rwlock);
}
