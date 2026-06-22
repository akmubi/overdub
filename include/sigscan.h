#ifndef SIGSCAN_H
#define SIGSCAN_H

#include "types.h"

typedef uint8_t sig_kind_t;
enum {
  SIG_DIRECT,      // use match address + add
  SIG_RIPREL32_AT, // read disp32 at match+op_off, target = (op+4)+disp32
};

typedef int sigscan_err_t;
enum {
  SIG_ERR_OK                  = 0,
  SIG_ERR_INVALID_PATTERN     = -1,
  SIG_ERR_TARGET_NOT_READABLE = -2,
  SIG_ERR_NOT_FOUND           = -3,
};

static inline const char *
sig_err_to_str(sigscan_err_t err)
{
  switch (err) {
  case SIG_ERR_OK:
    return "ok";
  case SIG_ERR_INVALID_PATTERN:
    return "invalid pattern";
  case SIG_ERR_TARGET_NOT_READABLE:
    return "target is not readable";
  case SIG_ERR_NOT_FOUND:
    return "target not found";
  }
  return "unknown";
}

typedef struct sigscan_entry_s sigscan_entry_t;
struct sigscan_entry_s {
  const char *name;          // for debugging/logging
  const char *pattern;       // signature pattern, e.g. "40 55 56 ..."
  void      **pp;            // write back pointer, e.g. &ProcessEvent
  sig_kind_t  kind;          // entry kind
  uint8_t     op_off;        // offset of the operand inside the matched bytes
  uint8_t     deref;         // 0..2 times *(ptr)
  void       *addr;          // resulting address
  uintptr_t   first_rvas[8]; // first RVAs to try
};

/* using macros for templates because they're constant */

#define SIG_ENTRY_DIRECT(PTR, PATTERN, ...) \
  {                                         \
    .name       = #PTR,                     \
    .pattern    = (PATTERN),                \
    .pp         = (void **)&PTR,            \
    .kind       = SIG_DIRECT,               \
    .op_off     = 0,                        \
    .deref      = 0,                        \
    .addr       = NULL,                     \
    .first_rvas = {__VA_ARGS__},            \
  }

#define SIG_ENTRY_MOV64_PTR(PTR, PATTERN, ...) \
  {                                            \
    .name       = #PTR,                        \
    .pattern    = (PATTERN),                   \
    .pp         = (void **)&PTR,               \
    .kind       = SIG_RIPREL32_AT,             \
    .op_off     = 3,                           \
    .deref      = 1,                           \
    .addr       = NULL,                        \
    .first_rvas = {__VA_ARGS__},               \
  }

#define SIG_ENTRY_MOV64_ADDR(PTR, PATTERN, ...) \
  {                                             \
    .name       = #PTR,                         \
    .pattern    = (PATTERN),                    \
    .pp         = (void **)&PTR,                \
    .kind       = SIG_RIPREL32_AT,              \
    .op_off     = 3,                            \
    .deref      = 0,                            \
    .addr       = NULL,                         \
    .first_rvas = {__VA_ARGS__},                \
  }

#define SIG_ENTRY_MOV32_PTR(PTR, PATTERN, ...) \
  {                                            \
    .name       = #PTR,                        \
    .pattern    = (PATTERN),                   \
    .pp         = (void **)&PTR,               \
    .kind       = SIG_RIPREL32_AT,             \
    .op_off     = 2,                           \
    .deref      = 1,                           \
    .addr       = NULL,                        \
    .first_rvas = {__VA_ARGS__},               \
  }

#define SIG_ENTRY_LEA_ADDR(PTR, PATTERN, ...) \
  {                                           \
    .name       = #PTR,                       \
    .pattern    = (PATTERN),                  \
    .pp         = (void **)&PTR,              \
    .kind       = SIG_RIPREL32_AT,            \
    .op_off     = 3,                          \
    .deref      = 0,                          \
    .addr       = NULL,                       \
    .first_rvas = {__VA_ARGS__},              \
  }

#define SIG_ENTRY_CALL_ARG(PTR, PATTERN, ...) \
  {                                           \
    .name       = #PTR,                       \
    .pattern    = (PATTERN),                  \
    .pp         = (void **)&PTR,              \
    .kind       = SIG_RIPREL32_AT,            \
    .op_off     = 1,                          \
    .deref      = 0,                          \
    .addr       = NULL,                       \
    .first_rvas = {__VA_ARGS__},              \
  }

/* executable section walker */
typedef struct sigscan_exec_span_s sigscan_exec_span_t;
struct sigscan_exec_span_s {
  char     name[8];
  uint8_t *base;
  uint64_t size;
};

int
sigscan_build_exec_spans(sigscan_exec_span_t *spans, int max_spans, void *module_base);

sigscan_err_t
sigscan_scan_entry(sigscan_exec_span_t *spans, int num_spans, void *module_base, sigscan_entry_t *entry);

#endif /* SIGSCAN_H */
