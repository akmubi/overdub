#include "log.h"

#include "arena.h"
#include "globals.h"
#include "path.h"
#include "scratch.h"
#include "str.h"
#include "ui_console.h"
#include "unreal.h"

#include <process.h>
#include <stdio.h>
#include <windows.h>

enum {
  LOG_QUEUE_UI,
  LOG_QUEUE_FILE,
  LOG_QUEUE_COUNT,
};

typedef struct log_atomic_pair_s log_atomic_pair_t;
ALIGNED_TYPE(struct, 16) log_atomic_pair_s
{
  volatile LONG64 values[2];
};

typedef struct log_queue_slot_s log_queue_slot_t;
struct log_queue_slot_s {
  volatile LONG64  sequence;
  uint64_t         timestamp;
  uint64_t         frame;
  uint32_t         len;
  log_level_t      level;
  log_sink_flags_t sinks;
  HANDLE           completion;
};

typedef struct log_queue_s log_queue_t;
ALIGNED_TYPE(struct, 64) log_queue_s
{
  volatile LONG64   read_position;
  volatile LONG64   dropped;
  uint8_t          *entries;
  log_queue_slot_t *slots;
};

typedef struct log_reservation_s log_reservation_t;
struct log_reservation_s {
  uint64_t positions[LOG_QUEUE_COUNT];
  bool     reserved[LOG_QUEUE_COUNT];
};

typedef struct logger_s logger_t;
struct logger_s {
  arena_t           perm;
  str_t             file_path;
  log_queue_t       queues[LOG_QUEUE_COUNT];
  log_atomic_pair_t write_positions;
  log_atomic_pair_t frame_clock; // second and its first global frame
  volatile LONG64   current_frame;
  volatile LONG     level;
  volatile LONG     default_sinks;
  volatile LONG     accepting;
  volatile LONG     active_calls;
  volatile LONG     stopping;
  volatile LONG     file_error;
  HANDLE            writer;
  HANDLE            wake;
  unsigned int      writer_id;
  HANDLE            file;
  uint8_t          *file_buffer;
  uint64_t          file_buffer_len;
  bool              console_opened;
};

static logger_t g_logger = {
  .level         = LOG_LEVEL_INFO,
  .default_sinks = LOG_SINK_DEFAULT,
  .file          = INVALID_HANDLE_VALUE,
};

STATIC_ASSERT(CONFIG_LOG_QUEUE_CAP > 0 && (CONFIG_LOG_QUEUE_CAP & (CONFIG_LOG_QUEUE_CAP - 1)) == 0, "log queue capacity must be a power of two");
STATIC_ASSERT(CONFIG_LOG_MAX_MESSAGE_LEN == 1024, "log entries must hold 1 KB of text");
STATIC_ASSERT(ALIGNOF(log_queue_t)       == 64,   "log queues must be cache-line aligned");
STATIC_ASSERT(ALIGNOF(log_atomic_pair_t) == 16,   "atomic pairs must be 16-byte aligned");

static uint64_t
log_atomic_read(volatile LONG64 *value)
{
  return (uint64_t)InterlockedCompareExchange64(value, 0, 0);
}

static bool
log_pair_compare_exchange(log_atomic_pair_t *pair, LONG64 high, LONG64 low, LONG64 expected[2])
{
#if defined(_MSC_VER)
  return InterlockedCompareExchange128(pair->values, high, low, expected) != 0;
#else
  typedef unsigned __int128 log_pair_value_t;

  log_pair_value_t expected_value = ((log_pair_value_t)(uint64_t)expected[1] << 64) | (uint64_t)expected[0];
  log_pair_value_t desired_value  = ((log_pair_value_t)(uint64_t)high << 64) | (uint64_t)low;
  log_pair_value_t actual_value   = __sync_val_compare_and_swap((volatile log_pair_value_t *)pair->values, expected_value, desired_value);

  expected[0] = (LONG64)(uint64_t)actual_value;
  expected[1] = (LONG64)(uint64_t)(actual_value >> 64);
  return actual_value == expected_value;
#endif
}

static void
log_pair_read(log_atomic_pair_t *pair, LONG64 values[2])
{
  values[0] = 0;
  values[1] = 0;
  log_pair_compare_exchange(pair, 0, 0, values);
}

static bool
log_begin(void)
{
  // keep the queue storage alive until this call finishes
  InterlockedIncrement(&g_logger.active_calls);
  bool accepting = InterlockedCompareExchange(&g_logger.accepting, 0, 0) != 0;

  if (!accepting) {
    InterlockedDecrement(&g_logger.active_calls);
  }

  return accepting;
}

static void
log_end(void)
{
  InterlockedDecrement(&g_logger.active_calls);
}

static uint64_t
log_timestamp(void)
{
  FILETIME now;
  GetSystemTimeAsFileTime(&now);
  return ((uint64_t)now.dwHighDateTime << 32) | now.dwLowDateTime;
}

static bool
log_frame_clock_update(log_atomic_pair_t *clock, LONG64 previous[2], uint64_t second, uint64_t frame_counter, uint64_t *frame)
{
  uint64_t first_frame = (uint64_t)previous[1];

  if ((uint64_t)previous[0] == second && frame_counter >= first_frame) {
    *frame = frame_counter - first_frame;
    return true;
  }

  if (log_pair_compare_exchange(clock, (LONG64)frame_counter, (LONG64)second, previous)) {
    *frame = 0;
    return true;
  }

  return false;
}

static void
log_capture_time(log_message_t *message)
{
  ALIGNED_VAR(LONG64 clock[2], 16);
  log_pair_read(&g_logger.frame_clock, clock);
  bool captured = false;

  while (!captured) {
    message->timestamp = log_timestamp();

    uint64_t second = message->timestamp / 10000000;
    uint64_t frame  = log_atomic_read(&g_logger.current_frame);
    captured = log_frame_clock_update(&g_logger.frame_clock, clock, second, frame, &message->frame);
  }
}

static int
log_queue_index(uint64_t position)
{
  return (int)(position & (CONFIG_LOG_QUEUE_CAP - 1));
}

static uint8_t *
log_queue_entry(log_queue_t *queue, uint64_t position)
{
  return queue->entries + (uint64_t)log_queue_index(position) * CONFIG_LOG_MAX_MESSAGE_LEN;
}

static log_reservation_t
log_reserve(log_sink_flags_t sinks, bool report_drops)
{
  ALIGNED_VAR(LONG64 positions[LOG_QUEUE_COUNT], 16);
  log_pair_read(&g_logger.write_positions, positions);

  log_reservation_t result = {0};

  bool requested[LOG_QUEUE_COUNT] = {
    (sinks & LOG_SINK_UI) != 0,
    (sinks & (LOG_SINK_FILE | LOG_SINK_WINCON)) != 0,
  };

  bool claimed = false;
  while (!claimed) {
    LONG64 next[LOG_QUEUE_COUNT];

    for (int i = 0; i < LOG_QUEUE_COUNT; ++i) {
      uint64_t          position = (uint64_t)positions[i];
      log_queue_slot_t *slot     = &g_logger.queues[i].slots[log_queue_index(position)];

      result.positions[i] = position;
      result.reserved[i]  = requested[i] && log_atomic_read(&slot->sequence) == position;
      next[i]             = (LONG64)(position + result.reserved[i]);
    }

    /* reserve both positions together so the sinks agree on message order */
    claimed = log_pair_compare_exchange(&g_logger.write_positions, next[LOG_QUEUE_FILE], next[LOG_QUEUE_UI], positions);
  }

  if (report_drops) {
    for (int i = 0; i < LOG_QUEUE_COUNT; ++i) {
      if (requested[i] && !result.reserved[i]) {
        InterlockedIncrement64(&g_logger.queues[i].dropped);
      }
    }
  }

  return result;
}

static void
log_queue_publish(log_queue_t *queue, uint64_t position, const log_message_t *message, log_sink_flags_t sinks, HANDLE completion)
{
  int               index = log_queue_index(position);
  log_queue_slot_t *slot  = &queue->slots[index];

  slot->timestamp  = message->timestamp;
  slot->frame      = message->frame;
  slot->level      = message->level;
  slot->sinks      = sinks;
  slot->len        = (uint32_t)message->text.len;
  slot->completion = completion;

  if (slot->len > 0) {
    mem_copy(log_queue_entry(queue, position), message->text.data, slot->len);
  }

  /* publish only after the metadata and text are complete */
  InterlockedExchange64(&slot->sequence, (LONG64)(position + 1));
}

static bool
log_queue_peek(log_queue_t *queue, uint64_t position, log_message_t *message)
{
  int               index = log_queue_index(position);
  log_queue_slot_t *slot  = &queue->slots[index];

  if (log_atomic_read(&slot->sequence) != position + 1) {
    return false;
  }

  *message = (log_message_t){
    .level     = slot->level,
    .timestamp = slot->timestamp,
    .frame     = slot->frame,
    .text      = {.data = log_queue_entry(queue, position), .len = slot->len},
  };
  return true;
}

static void
log_queue_release(log_queue_t *queue, uint64_t position)
{
  log_queue_slot_t *slot = &queue->slots[log_queue_index(position)];
  InterlockedExchange64(&slot->sequence,       (LONG64)(position + CONFIG_LOG_QUEUE_CAP));
  InterlockedExchange64(&queue->read_position, (LONG64)(position + 1));
}

static void
log_submit(log_level_t level, log_sink_flags_t sinks, str_t text)
{
  log_message_t message = {
    .level = level,
    .text  = {
      .data = text.data,
      .len  = MIN_VAL(text.len, CONFIG_LOG_MAX_MESSAGE_LEN),
    },
  };
  log_capture_time(&message);
  log_reservation_t reservation = log_reserve(sinks, true);

  for (int i = 0; i < LOG_QUEUE_COUNT; ++i) {
    if (reservation.reserved[i]) {
      log_queue_publish(&g_logger.queues[i], reservation.positions[i], &message, sinks, NULL);
    }
  }

  if (reservation.reserved[LOG_QUEUE_FILE] && reservation.positions[LOG_QUEUE_FILE] == log_atomic_read(&g_logger.queues[LOG_QUEUE_FILE].read_position)) {
    SetEvent(g_logger.wake);
  }
}

void
log_ui_enqueue(log_level_t level, str_t text)
{
  if (log_begin()) {
    log_submit(level, LOG_SINK_UI, text);
    log_end();
  }
}

void
log_ui_on_frame_begin(uint64_t frame_counter)
{
  if (!unreal_is_in_game_thread()) {
    return;
  }

  if (log_begin()) {
    InterlockedExchange64(&g_logger.current_frame, (LONG64)frame_counter);
    log_message_t message = {0};
    log_capture_time(&message);
    log_end();
  }
}

static void
log_ui_drain(ui_console_t *console)
{
  log_queue_t *queue = &g_logger.queues[LOG_QUEUE_UI];
  ALIGNED_VAR(LONG64 positions[LOG_QUEUE_COUNT], 16);
  log_pair_read(&g_logger.write_positions, positions);

  uint64_t read_position = log_atomic_read(&queue->read_position);
  uint64_t count         = MIN_VAL((uint64_t)positions[LOG_QUEUE_UI] - read_position, CONFIG_LOG_UI_DRAIN_LIMIT);
  bool     ready         = true;

  for (uint64_t i = 0; i < count && ready; ++i) {
    log_message_t message;
    ready = log_queue_peek(queue, read_position, &message);

    if (ready) {
      ui_console_append_log(console, &message);
      log_queue_release(queue, read_position);
      read_position += 1;
    }
  }

  uint64_t dropped = (uint64_t)InterlockedExchange64(&queue->dropped, 0);
  if (dropped > 0) {
    char text[128];
    snprintf(text, sizeof(text), "UI console queue overflow: dropped %llu new messages", (unsigned long long)dropped);

    log_message_t message = {
      .level = LOG_LEVEL_WARN,
      .text  = str_from_cstr(text),
    };
    log_capture_time(&message);

    ui_console_append_log(console, &message);
  }
}

bool
log_ui_flush(ui_console_t *console)
{
  if (!unreal_is_in_game_thread() || !console || !console->inited) {
    return false;
  }

  bool drained = false;

  if (log_begin()) {
    log_ui_drain(console);
    drained = true;
    log_end();
  }

  return drained;
}

static WORD g_console_attr_default = 0;

static inline const char *
log_level_to_str(log_level_t level)
{
  switch (level) {
    case LOG_LEVEL_DEBUG: return "DBG";
    case LOG_LEVEL_INFO:  return "INF";
    case LOG_LEVEL_WARN:  return "WRN";
    case LOG_LEVEL_ERROR: return "ERR";
  }
  return "UNK";
}

static WORD
log_level_console_color(log_level_t level)
{
  switch (level) {
    case LOG_LEVEL_DEBUG: return FOREGROUND_RED | FOREGROUND_BLUE  | FOREGROUND_INTENSITY;
    case LOG_LEVEL_INFO:  return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    case LOG_LEVEL_WARN:  return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case LOG_LEVEL_ERROR: return FOREGROUND_RED | FOREGROUND_INTENSITY;
    default:              return g_console_attr_default;
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
log_file_error(DWORD error)
{
  DWORD previous = (DWORD)InterlockedExchange(&g_logger.file_error, (LONG)error);

  if (previous != error) {
    char text[128];
    snprintf(text, sizeof(text), "Log file error: Windows error %lu", error);
    OutputDebugStringA(text);
    log_ui_enqueue(LOG_LEVEL_ERROR, str_from_cstr(text));
  }
}

static void
log_file_write_buffer(void)
{
  uint64_t offset  = 0;
  bool     written = g_logger.file != INVALID_HANDLE_VALUE;

  while (written && offset < g_logger.file_buffer_len) {
    DWORD count = 0;
    written = WriteFile(g_logger.file, g_logger.file_buffer + offset, (DWORD)(g_logger.file_buffer_len - offset), &count, NULL) != 0;

    if (!written) {
      log_file_error(GetLastError());
    } else if (count == 0) {
      log_file_error(ERROR_WRITE_FAULT);
      written = false;
    } else {
      offset += count;
    }
  }

  g_logger.file_buffer_len = 0;
}

static void
log_file_flush(void)
{
  log_file_write_buffer();

  if (g_logger.file != INVALID_HANDLE_VALUE && !FlushFileBuffers(g_logger.file)) {
    log_file_error(GetLastError());
  }
}

static void
log_file_append(const log_message_t *message, log_sink_flags_t sinks)
{
  FILETIME utc = {
    .dwLowDateTime  = (DWORD)message->timestamp,
    .dwHighDateTime = (DWORD)(message->timestamp >> 32),
  };

  FILETIME   local;
  SYSTEMTIME time = {0};
  FileTimeToLocalFileTime(&utc, &local);
  FileTimeToSystemTime(&local, &time);

  char prefix[64];
  int  prefix_len = snprintf(prefix, sizeof(prefix), "%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] ",
                             time.wYear, time.wMonth, time.wDay,
                             time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                             log_level_to_str(message->level));

  uint64_t cursor     = 0;
  HANDLE   console    = GetStdHandle(STD_OUTPUT_HANDLE);
  bool     to_console = (sinks & LOG_SINK_WINCON) && console && console != INVALID_HANDLE_VALUE;
  bool     to_file    = (sinks & LOG_SINK_FILE) && g_logger.file != INVALID_HANDLE_VALUE;

  if (to_console) {
    SetConsoleTextAttribute(console, log_level_console_color(message->level));
  }

  do {
    uint64_t start = cursor;
    while (cursor < message->text.len && message->text.data[cursor] != '\r' && message->text.data[cursor] != '\n') {
      cursor += 1;
    }

    char     line[CONFIG_LOG_MAX_MESSAGE_LEN + 64];
    uint64_t line_len = (uint64_t)prefix_len + cursor - start;
    mem_copy(line, prefix, (uint64_t)prefix_len);

    if (cursor > start) {
      mem_copy(line + prefix_len, message->text.data + start, cursor - start);
    }

    line[line_len++] = '\r';
    line[line_len++] = '\n';

    if (to_file) {
      if (g_logger.file_buffer_len + line_len > CONFIG_LOG_FILE_BUFFER_SIZE) {
        log_file_write_buffer();
      }

      mem_copy(g_logger.file_buffer + g_logger.file_buffer_len, line, line_len);
      g_logger.file_buffer_len += line_len;
    }

    if (to_console) {
      DWORD written;
      WriteConsoleA(console, line, (DWORD)line_len, &written, NULL);
    }

    if (cursor < message->text.len) {
      uint8_t newline = message->text.data[cursor];
      cursor += 1;

      if (cursor < message->text.len) {
        uint8_t next   = message->text.data[cursor];
        bool    paired = (newline == '\r' && next == '\n') || (newline == '\n' && next == '\r');

        if (paired) {
          cursor += 1;
        }
      }
    }
  } while (cursor < message->text.len);

  if (to_console) {
    SetConsoleTextAttribute(console, g_console_attr_default);
  }
}

static void
log_file_report_drops(void)
{
  uint64_t dropped = (uint64_t)InterlockedExchange64(&g_logger.queues[LOG_QUEUE_FILE].dropped, 0);

  if (dropped > 0) {
    char text[128];
    snprintf(text, sizeof(text), "File log queue overflow: dropped %llu new messages", (unsigned long long)dropped);

    log_message_t overflow = {
      .level = LOG_LEVEL_WARN,
      .text  = str_from_cstr(text),
    };
    log_capture_time(&overflow);

    log_file_append(&overflow, LOG_SINK_FILE | LOG_SINK_WINCON);
    log_ui_enqueue(LOG_LEVEL_WARN, overflow.text);
  }
}

static unsigned __stdcall
log_file_writer(void *user)
{
  (void)user;

  DWORD       open_error = ERROR_SUCCESS;
  tmp_arena_t tmp        = scratch_begin(NULL);
  {
    str16_t path = str16_from_str(tmp.arena, g_logger.file_path);
    g_logger.file = CreateFileW(path.data, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (g_logger.file == INVALID_HANDLE_VALUE) {
      open_error = GetLastError();
    }
  }
  scratch_end(tmp);

  if (g_logger.file == INVALID_HANDLE_VALUE) {
    log_file_error(open_error);
  }

  log_queue_t *queue   = &g_logger.queues[LOG_QUEUE_FILE];
  bool         stopped = false;

  while (!stopped) {
    uint64_t      position = log_atomic_read(&queue->read_position);
    log_message_t message  = {0};
    bool          ready    = log_queue_peek(queue, position, &message);
    uint64_t      count    = 0;

    while (ready && count < CONFIG_LOG_QUEUE_CAP) {
      log_queue_slot_t *slot = &queue->slots[log_queue_index(position)];

      if (slot->completion) {
        log_file_report_drops();
        log_file_flush();
        SetEvent(slot->completion);
      } else {
        log_file_append(&message, slot->sinks);
      }

      log_queue_release(queue, position);
      position += 1;
      count    += 1;
      ready     = log_queue_peek(queue, position, &message);
    }

    log_file_report_drops();
    log_file_write_buffer();
    stopped = InterlockedCompareExchange(&g_logger.stopping, 0, 0) != 0 && !ready;

    if (!stopped && !ready) {
      WaitForSingleObject(g_logger.wake, 50);
    }
  }

  log_file_flush();

  if (g_logger.file != INVALID_HANDLE_VALUE) {
    CloseHandle(g_logger.file);
    g_logger.file = INVALID_HANDLE_VALUE;
  }

  scratch_destroy();
  return 0;
}

void
log_init(str_t file_path, log_level_t level, bool open_console)
{
  /* initialize before starting producers, never wait for the writer in DllMain */
  if (InterlockedCompareExchange(&g_logger.accepting, 0, 0)) {
    return;
  }

  uint64_t queue_bytes = LOG_QUEUE_COUNT * CONFIG_LOG_QUEUE_CAP * (CONFIG_LOG_MAX_MESSAGE_LEN + sizeof(log_queue_slot_t));

  g_logger.perm = arena_new_dynamic(queue_bytes + CONFIG_LOG_FILE_BUFFER_SIZE + 64 * KB, 64 * KB);

  bool allocated = g_logger.perm.backing != NULL;
  for (int i = 0; i < LOG_QUEUE_COUNT && allocated; ++i) {
    log_queue_t *queue = &g_logger.queues[i];

    queue->entries = arena_push_aligned(&g_logger.perm, CONFIG_LOG_QUEUE_CAP * CONFIG_LOG_MAX_MESSAGE_LEN, 64);
    queue->slots   = ARENA_PUSH_ARRAY_ZERO(&g_logger.perm, log_queue_slot_t, CONFIG_LOG_QUEUE_CAP);

    allocated = queue->entries && queue->slots;

    if (allocated) {
      for (uint64_t slot = 0; slot < CONFIG_LOG_QUEUE_CAP; ++slot) {
        queue->slots[slot].sequence = (LONG64)slot;
      }
    }

    InterlockedExchange64(&queue->read_position, 0);
    InterlockedExchange64(&queue->dropped, 0);
  }

  g_logger.file_buffer = ARENA_PUSH_ARRAY(&g_logger.perm, uint8_t, CONFIG_LOG_FILE_BUFFER_SIZE);

  if (path_is_abs(file_path)) {
    g_logger.file_path = str_push_copy(&g_logger.perm, file_path);
  } else {
    g_logger.file_path = path_join(&g_logger.perm, globals.game_dir, file_path);
  }

  allocated = allocated && g_logger.file_buffer && !str_is_empty(g_logger.file_path);
  g_logger.wake = CreateEventA(NULL, FALSE, FALSE, NULL);

  if (!allocated || !g_logger.wake) {
    if (g_logger.wake) {
      CloseHandle(g_logger.wake);
      g_logger.wake = NULL;
    }

    arena_destroy(&g_logger.perm);
    OutputDebugStringA("Failed to initialize logger");
    return;
  }

  g_logger.write_positions = (log_atomic_pair_t){0};
  g_logger.frame_clock     = (log_atomic_pair_t){0};

  InterlockedExchange64(&g_logger.current_frame, 0);
  InterlockedExchange(&g_logger.stopping, 0);
  InterlockedExchange(&g_logger.file_error, 0);
  InterlockedExchange(&g_logger.level, (LONG)level);

  g_logger.file_buffer_len = 0;
  g_logger.file            = INVALID_HANDLE_VALUE;
  g_logger.console_opened  = open_console;

  if (open_console) {
    console_new_private(L"Log window");
  }

  InterlockedExchange(&g_logger.accepting, 1);
  g_logger.writer = (HANDLE)_beginthreadex(NULL, 0, log_file_writer, NULL, 0, &g_logger.writer_id);

  if (!g_logger.writer) {
    InterlockedExchange(&g_logger.accepting, 0);
    CloseHandle(g_logger.wake);
    g_logger.wake = NULL;
    arena_destroy(&g_logger.perm);
    OutputDebugStringA("Failed to start log writer");

    if (g_logger.console_opened) {
      FreeConsole();
      g_logger.console_opened = false;
    }
  }
}

void
log_set_level(log_level_t level)
{
  InterlockedExchange(&g_logger.level, (LONG)level);
}

void
log_set_default_sinks(log_sink_flags_t sinks)
{
  InterlockedExchange(&g_logger.default_sinks, (LONG)sinks);
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
  if (level < (log_level_t)InterlockedCompareExchange(&g_logger.level, 0, 0) || !fmt) {
    return;
  }

  if (sinks == LOG_SINK_NONE) {
    sinks = (log_sink_flags_t)InterlockedCompareExchange(&g_logger.default_sinks, 0, 0);
  }

  if ((sinks & LOG_SINK_DEFAULT) == 0) {
    return;
  }

  if (log_begin()) {
    char  text[CONFIG_LOG_MAX_MESSAGE_LEN + 1];
    int   len     = vsnprintf(text, sizeof(text), fmt, ap);
    str_t message = STR_LIT("[log formatting failed]");

    if (len >= 0) {
      message = (str_t){.data = (uint8_t *)text, .len = MIN_VAL((uint64_t)len, CONFIG_LOG_MAX_MESSAGE_LEN)};
    }

    log_submit(level, sinks, message);
    log_end();
  }
}

static void
log_wait_for_file(void)
{
  HANDLE completion = CreateEventA(NULL, FALSE, FALSE, NULL);

  if (!completion) {
    OutputDebugStringA("Failed to create log flush event");
    return;
  }

  log_reservation_t reservation = log_reserve(LOG_SINK_FILE, false);

  while (!reservation.reserved[LOG_QUEUE_FILE]) {
    SetEvent(g_logger.wake);
    Sleep(1);
    reservation = log_reserve(LOG_SINK_FILE, false);
  }

  log_message_t message = {0};
  log_queue_publish(&g_logger.queues[LOG_QUEUE_FILE], reservation.positions[LOG_QUEUE_FILE], &message, 0, completion);
  SetEvent(g_logger.wake);
  WaitForSingleObject(completion, INFINITE);
  CloseHandle(completion);
}

void
log_flush(void)
{
  if (log_begin()) {
    if (thread_current_id() != g_logger.writer_id) {
      log_wait_for_file();
    }
    log_end();
  }
}

void
log_deinit(void)
{
  /* stop and join the writer outside DllMain */
  if (!InterlockedExchange(&g_logger.accepting, 0)) {
    return;
  }

  while (InterlockedCompareExchange(&g_logger.active_calls, 0, 0) != 0) {
    Sleep(1);
  }

  InterlockedExchange(&g_logger.stopping, 1);
  SetEvent(g_logger.wake);
  WaitForSingleObject(g_logger.writer, INFINITE);
  CloseHandle(g_logger.writer);
  CloseHandle(g_logger.wake);

  g_logger.writer    = NULL;
  g_logger.wake      = NULL;
  g_logger.writer_id = 0;

  arena_destroy(&g_logger.perm);

  if (g_logger.console_opened) {
    FreeConsole();
    g_logger.console_opened = false;
  }
}
