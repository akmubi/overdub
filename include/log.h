#ifndef LOG_H
#define LOG_H

#include "str.h"
#include "types.h"

#define LOG_ERROR(...) log_emit(LOG_LEVEL_ERROR, LOG_SINK_DEFAULT, __VA_ARGS__)
#define LOG_WARN(...)  log_emit(LOG_LEVEL_WARN, LOG_SINK_DEFAULT, __VA_ARGS__)
#define LOG_INFO(...)  log_emit(LOG_LEVEL_INFO, LOG_SINK_DEFAULT, __VA_ARGS__)
#define LOG_DEBUG(...) log_emit(LOG_LEVEL_DEBUG, LOG_SINK_DEFAULT, __VA_ARGS__)

#define CONSOLE_ERROR(...) log_emit(LOG_LEVEL_ERROR, LOG_SINK_UI_ONLY, __VA_ARGS__)
#define CONSOLE_WARN(...)  log_emit(LOG_LEVEL_WARN, LOG_SINK_UI_ONLY, __VA_ARGS__)
#define CONSOLE_INFO(...)  log_emit(LOG_LEVEL_INFO, LOG_SINK_UI_ONLY, __VA_ARGS__)
#define CONSOLE_DEBUG(...) log_emit(LOG_LEVEL_DEBUG, LOG_SINK_UI_ONLY, __VA_ARGS__)

#define LOG_EMIT_LIMITED(N, LEVEL, SINK, ...)    \
  do {                                     \
    static int UNIQUE_NAME(n_logged_) = 0; \
    if (UNIQUE_NAME(n_logged_) < (N)) {    \
      log_emit(LEVEL, SINK, __VA_ARGS__);  \
      UNIQUE_NAME(n_logged_) += 1;         \
    }                                      \
  } while (0)

#define LOG_ERROR_LIMITED(N, ...) LOG_EMIT_LIMITED(N, LOG_LEVEL_ERROR, LOG_SINK_DEFAULT, __VA_ARGS__)
#define LOG_WARN_LIMITED(N, ...)  LOG_EMIT_LIMITED(N, LOG_LEVEL_WARN, LOG_SINK_DEFAULT, __VA_ARGS__)
#define LOG_INFO_LIMITED(N, ...)  LOG_EMIT_LIMITED(N, LOG_LEVEL_INFO, LOG_SINK_DEFAULT, __VA_ARGS__)
#define LOG_DEBUG_LIMITED(N, ...) LOG_EMIT_LIMITED(N, LOG_LEVEL_DEBUG, LOG_SINK_DEFAULT, __VA_ARGS__)

#define CONSOLE_ERROR_LIMITED(N, ...) LOG_EMIT_LIMITED(N, LOG_LEVEL_ERROR, LOG_SINK_UI_ONLY, __VA_ARGS__)
#define CONSOLE_WARN_LIMITED(N, ...)  LOG_EMIT_LIMITED(N, LOG_LEVEL_WARN, LOG_SINK_UI_ONLY, __VA_ARGS__)
#define CONSOLE_INFO_LIMITED(N, ...)  LOG_EMIT_LIMITED(N, LOG_LEVEL_INFO, LOG_SINK_UI_ONLY, __VA_ARGS__)
#define CONSOLE_DEBUG_LIMITED(N, ...) LOG_EMIT_LIMITED(N, LOG_LEVEL_DEBUG, LOG_SINK_UI_ONLY, __VA_ARGS__)

typedef unsigned int log_level_t;
enum {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO  = 1,
  LOG_LEVEL_WARN  = 2,
  LOG_LEVEL_ERROR = 3,
};

typedef uint32_t log_sink_flags_t;
enum {
  LOG_SINK_NONE   = 0,
  LOG_SINK_FILE   = (1 << 0),
  LOG_SINK_WINCON = (1 << 1),
  LOG_SINK_UI     = (1 << 2),

  LOG_SINK_DEFAULT = LOG_SINK_FILE | LOG_SINK_WINCON | LOG_SINK_UI,
  LOG_SINK_UI_ONLY = LOG_SINK_UI,
};

void
log_init(str_t file_path, log_level_t level, bool open_console);
void
log_set_level(log_level_t level);
void
log_set_default_sinks(log_sink_flags_t sinks);

void
log_emit(log_level_t level, log_sink_flags_t sinks, const char *fmt, ...) ATTR_FORMAT(3, 4);
void
log_emitv(log_level_t level, log_sink_flags_t sinks, const char *fmt, va_list ap);

void
log_flush(void);
void
log_deinit(void);

#endif /* LOG_H */
