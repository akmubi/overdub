#ifndef GLOBALS_H
#define GLOBALS_H

#include "arena.h"
#include "mod_manager.h"
#include "sigscan.h"
#include "ui_console.h"
#include "ui_keybind_capture.h"
#include "ui_manager.h"
#include "ui_mod_manager.h"
#include "unreal.h"

typedef struct globals_s globals_t;
struct globals_s {
  arena_t  perm;
  bool     engine_inited;
  bool     input_inited;
  uint32_t game_thread_id;
  str_t    game_dir;

  sigscan_exec_span_t user_spans[16];
  int                 num_user_spans;
  void               *user_module_base;

  void *io_dispatcher;
  void *pak_file;

  fuobject_array_t   *uobjects;
  fname_pool_t       *name_pool;
  uworld_t          **gworld_ptr; // points to current world
  fnative_func_ptr_t *natives;

  unreal_common_t     unreal;
  uobject_listener_t *listeners;

  /* window */
  uint64_t hwnd;

  /* input */
  bool prev_keys_down[INPUT_KEY_MAX];
  bool keys_down[INPUT_KEY_MAX];

  /* mod manager */
  mod_manager_t mod_manager;

  ui_manager_t ui_manager;

  float    fps;
  uint64_t frame_counter;
};

extern globals_t globals;

#endif /* GLOBALS_H */
