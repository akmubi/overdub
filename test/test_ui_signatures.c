#include "signatures.h"

/* TestUI has no game process to scan, so engine entry points stay unavailable */
#define UE_FUNC_NORMAL(NAME, RET, ...) \
  NAME##_fn_t NAME = NULL;
#define UE_FUNC_HOOKED(NAME, RET, ...) \
  NAME##_fn_t NAME        = NULL;      \
  NAME##_fn_t NAME##_real = NULL;

#include "unreal_funcs.inc"
