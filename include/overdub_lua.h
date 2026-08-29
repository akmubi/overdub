#ifndef OVERDUB_LUA_H
#define OVERDUB_LUA_H

#if defined(_WIN32) && !defined(OVERDUB_BUILD_DLL) && !defined(LUA_BUILD_AS_DLL)
#  define LUA_BUILD_AS_DLL
#  define OVERDUB_LUA_UNDEF_BUILD_AS_DLL
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"
#include "lua/src/lualib.h"

#if defined(__cplusplus)
}
#endif

#if defined(OVERDUB_LUA_UNDEF_BUILD_AS_DLL)
#  undef OVERDUB_LUA_UNDEF_BUILD_AS_DLL
#  undef LUA_BUILD_AS_DLL
#endif

#if defined(_WIN32)
#  if defined(__cplusplus)
#    define OVERDUB_LUA_MODULE extern "C" __declspec(dllexport)
#  else
#    define OVERDUB_LUA_MODULE __declspec(dllexport)
#  endif
#else
#  if defined(__cplusplus)
#    define OVERDUB_LUA_MODULE extern "C"
#  else
#    define OVERDUB_LUA_MODULE
#  endif
#endif

#endif /* OVERDUB_LUA_H */
