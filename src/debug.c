#include "debug.h"
#include "arena.h"
#include "log.h"
#include "path.h"
#include "scratch.h"
#include "str.h"
#include "types.h"

#include <windows.h>
#include <dbghelp.h>

#define STACK_TRACE_MAX_FRAMES (64)

void
debug_stacktrace_push_lines(str_list_t *lines, arena_t *arena, uint32_t start_idx, uint32_t end_idx)
{
  static bool initialized = false;

  HANDLE process = GetCurrentProcess();
  if (!initialized) {
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

    if (!SymInitialize(process, NULL, TRUE)) {
      str_list_push_fmt(arena, lines, "SymInitialize failed (error: %lu)", GetLastError());
      return;
    }
    initialized = true;
  }

  void  *frames[STACK_TRACE_MAX_FRAMES] = {0};
  USHORT frame_count                    = CaptureStackBackTrace(2, STACK_TRACE_MAX_FRAMES, frames, NULL);

  if (end_idx >= frame_count) {
    end_idx = frame_count;
  }

  for (USHORT i = start_idx; i < end_idx; ++i) {
    DWORD64 addr = (DWORD64)(uintptr_t)frames[end_idx - i - 1]; // in reverse

    char            sym_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {0};
    SYMBOL_INFO    *sym                                         = (SYMBOL_INFO *)sym_buf;
    DWORD64         sym_disp                                    = 0;
    DWORD           line_disp                                   = 0;
    IMAGEHLP_LINE64 line                                        = {0};

    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen   = MAX_SYM_NAME;
    line.SizeOfStruct = sizeof(line);

    if (addr != 0) {
      --addr;
    }

    if (SymFromAddr(process, addr, &sym_disp, sym)) {
      if (SymGetLineFromAddr64(process, addr, &line_disp, &line)) {
        str_t name = path_base(str_from_cstr(line.FileName));
        int   num  = line.LineNumber;

        str_list_push_fmt(arena, lines, "%012llX %s (%.*s:%d)", (uint64_t)addr, sym->Name, STR_ARG(name), num);
      } else {
        str_list_push_fmt(arena, lines, "%012llX %s", (uint64_t)addr, sym->Name);
      }
    } else {
      str_list_push_fmt(arena, lines, "%012llX <unknown>", (uint64_t)addr);
    }
  }
}

void
my_assert_msg(const char *expr, const char *file, int line, const char *fmt, ...)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_list_t lines = {0};

    str_list_push_fmt(tmp.arena, &lines, "Expression: %s", expr);
    str_list_push_fmt(tmp.arena, &lines, "File: %s", file);
    str_list_push_fmt(tmp.arena, &lines, "Line: %d", line);

    if (fmt && fmt[0] != '\0') {
      va_list args;
      va_start(args, fmt);
      str_list_push(tmp.arena, &lines, str_push_vfmt(tmp.arena, fmt, args));
      va_end(args);
    }

    str_list_push(tmp.arena, &lines, STR_LIT("Stack trace:"));
    debug_stacktrace_push_lines(&lines, tmp.arena, 0, 10);

    str_t result = str_list_join(tmp.arena, lines, STR_NULL, STR_LIT("\n"), STR_NULL);

    LOG_ERROR("%.*s", STR_ARG(result));
    MessageBoxA(NULL, (const char *)result.data, "Assertion failed", MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND | MB_TOPMOST);
  }
  scratch_end(tmp);
}

void
my_assert(const char *expr, const char *file, int line)
{
  tmp_arena_t tmp = scratch_begin(NULL);
  {
    str_list_t lines = {0};

    str_list_push_fmt(tmp.arena, &lines, "Expression: %s", expr);
    str_list_push_fmt(tmp.arena, &lines, "File: %s", file);
    str_list_push_fmt(tmp.arena, &lines, "Line: %d", line);

    str_list_push(tmp.arena, &lines, STR_LIT("Stack trace:"));
    debug_stacktrace_push_lines(&lines, tmp.arena, 0, 10);

    str_t result = str_list_join(tmp.arena, lines, STR_NULL, STR_LIT("\n"), STR_NULL);

    LOG_ERROR("%.*s", STR_ARG(result));
    MessageBoxA(NULL, (const char *)result.data, "Assertion failed", MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND | MB_TOPMOST);
  }
  scratch_end(tmp);
}
