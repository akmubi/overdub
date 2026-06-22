#ifndef SIGNATURES_H
#define SIGNATURES_H

#include "types.h"
#include "unreal.h"

/* decleare normal and hooked unreal function signatures */

#define DECL_FUNC_NORMAL(DECL, NAME, RET, ...) \
  typedef RET(DECL *NAME##_fn_t)(__VA_ARGS__); \
  extern NAME##_fn_t NAME;

#define DECL_FUNC_HOOKED(DECL, NAME, RET, ...) \
  typedef RET(DECL *NAME##_fn_t)(__VA_ARGS__); \
  extern NAME##_fn_t NAME;                     \
  extern NAME##_fn_t NAME##_real;              \
  extern RET DECL    NAME##_hook(__VA_ARGS__); // defined elsewhere

#define UE_FUNC_NORMAL(NAME, RET, ...) DECL_FUNC_NORMAL(__fastcall, NAME, RET, __VA_ARGS__)
#define UE_FUNC_HOOKED(NAME, RET, ...) DECL_FUNC_HOOKED(__fastcall, NAME, RET, __VA_ARGS__)

#include "unreal_funcs.inc"

bool
scan_user_module_signatures(void);

#endif /* SIGNATURES_H */
