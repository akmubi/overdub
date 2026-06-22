#include "arena.h"
#include "builtin.h"
#include "debug.h"
#include "file.h"
#include "globals.h"
#include "input.h"
#include "log.h"
#include "mod_manager.h"
#include "nk_d3d12.h"
#include "path.h"
#include "scratch.h"
#include "signatures.h"
#include "str.h"
#include "types.h"
#include "ui_manager.h"
#include "unreal.h"

#include "vendor_minhook.h"

#include <windows.h>

typedef HCURSOR(WINAPI *pfn_set_cursor_t)(HCURSOR);
typedef int(WINAPI *pfn_show_cursor_t)(BOOL);

static pfn_set_cursor_t  g_set_cursor_real  = NULL;
static pfn_show_cursor_t g_show_cursor_real = NULL;
static HCURSOR           g_arrow_cursor     = NULL;

static volatile LONG g_cursor_restore_requested = 0;
static volatile LONG g_ui_cursor_forced_visible = 0;

static HCURSOR WINAPI
set_cursor_hook(HCURSOR cursor)
{
  if (InterlockedCompareExchange(&g_ui_cursor_forced_visible, 0, 0)) {
    if (!g_arrow_cursor) {
      g_arrow_cursor = LoadCursorA(NULL, IDC_ARROW);
    }
    return g_set_cursor_real(g_arrow_cursor);
  }
  return g_set_cursor_real(cursor);
}

static int WINAPI
show_cursor_hook(BOOL show)
{
  int result = g_show_cursor_real(show);

  if (!show && InterlockedCompareExchange(&g_ui_cursor_forced_visible, 0, 0)) {
    InterlockedExchange(&g_cursor_restore_requested, 1);
  }

  return result;
}

static void
ui_force_os_cursor_visible_tick(void)
{
  if (!InterlockedCompareExchange(&g_ui_cursor_forced_visible, 0, 0)) {
    return;
  }

  if (!g_show_cursor_real || !g_set_cursor_real) {
    return;
  }

  if (!g_arrow_cursor) {
    g_arrow_cursor = LoadCursorA(NULL, IDC_ARROW);
  }

  for (int i = 0; i < 16; ++i) {
    CURSORINFO ci = {
      .cbSize = sizeof(ci),
    };

    if (GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) {
      break;
    }

    g_show_cursor_real(TRUE);
  }

  g_set_cursor_real(g_arrow_cursor);
  InterlockedExchange(&g_cursor_restore_requested, 0);
}

static void
on_engine_init(void)
{
  LOG_INFO("Engine initialized");

  globals.game_thread_id = GetCurrentThreadId();
  globals.hwnd           = (uint64_t)FindWindowW(L"UnrealWindow", NULL);

  LOG_DEBUG("on_engine_init: Thread ID: %lu", GetCurrentThreadId());

  unreal_common_collect(&globals.unreal);

  mod_manager_start_dlls(&globals.mod_manager);
  mod_manager_start_blueprints(&globals.mod_manager);
}

static void
on_engine_tick_pre(float delta)
{
  if (!globals.engine_inited) {
    on_engine_init();
    globals.engine_inited = true;
  }

  if (delta > 0.00001f) {
    float fps = 1.0f / delta;
    if (globals.fps <= 0.0f) {
      globals.fps = fps;
    } else {
      globals.fps += (fps - globals.fps) * 0.10f;
    }
  }

  ui_manager_on_frame_begin(&globals.ui_manager, globals.frame_counter);

  if (ui_manager_should_show_cursor(&globals.ui_manager)) {
    InterlockedExchange(&g_ui_cursor_forced_visible, 1);
    InterlockedExchange(&g_cursor_restore_requested, 1);
  } else {
    InterlockedExchange(&g_ui_cursor_forced_visible, 0);
  }

  if (globals.engine_inited) {
    mod_manager_dispatch_tick(&globals.mod_manager, delta);
  }
}

static void
on_engine_tick_post(void)
{
  mem_copy(globals.prev_keys_down, globals.keys_down, sizeof(globals.prev_keys_down));

  if (globals.ui_manager.inited && globals.ui_manager.ctx) {
    unsigned int vw = 0;
    unsigned int vh = 0;
    nk_d3d12_get_viewport_size(&vw, &vh);
    ui_manager_on_frame_end(&globals.ui_manager, vw, vh);
    nk_d3d12_commit(globals.ui_manager.ctx);
  }
  ui_force_os_cursor_visible_tick();

  scratch_reset();

  globals.frame_counter += 1;
}

static bool
on_input_event(input_event_t *ev)
{
  if (!globals.input_inited) {
    input_key_map_init();
    globals.input_inited = true;

    LOG_INFO("Input key map initialized");
  }

  globals.keys_down[INPUT_KEY_MOUSE_WHEEL_UP]   = false;
  globals.keys_down[INPUT_KEY_MOUSE_WHEEL_DOWN] = false;

  switch (ev->kind) {
  case INPUT_EVENT_KEY_DOWN:
  case INPUT_EVENT_MOUSE_DOWN:
    globals.keys_down[ev->key] = true;
    break;
  case INPUT_EVENT_KEY_UP:
  case INPUT_EVENT_MOUSE_UP:
    globals.keys_down[ev->key] = false;
    break;
  case INPUT_EVENT_MOUSE_WHEEL:
    /* MOUSE_WHEEL_UP or MOUSE_WHEEL_DOWN */
    globals.keys_down[ev->key] = true;
    break;
  case INPUT_EVENT_APP_ACTIVATION:
    if (!ev->app_activated) {
      /* out of focus, all held keys should be released */
      mem_zero(globals.keys_down, sizeof(globals.keys_down));
    }
    break;
  }

  bool        consumed = false;
  const char *consumer = "Engine";

  if (ui_manager_on_input_event_pre(&globals.ui_manager, globals.frame_counter, ev)) {
    consumed = true;
    consumer = "UI (pre)";
  }

  if (!consumed && globals.engine_inited && mod_manager_dispatch_input(&globals.mod_manager, ev)) {
    consumed = true;
    consumer = "Mods";
  }

  if (!consumed && ui_manager_on_input_event_post(&globals.ui_manager, ev)) {
    consumed = true;
    consumer = "UI (post)";
  }

  (void)consumer;

#if 0
  if (ev->kind != INPUT_EVENT_MOUSE_MOVE) {
    str_t kind_str = input_event_kind_to_str(ev->kind);
    if (ev->kind == INPUT_EVENT_KEY_CHAR) {
      LOG_WARN("%-10s: kind: %.*s, key: %u", consumer, STR_ARG(kind_str), ev->character);
    } else {
      LOG_WARN("%-10s: kind: %.*s, key: %.*s", consumer, STR_ARG(kind_str), STR_ARG(input_key_to_str(ev->key)));
    }
  }
#endif
  return consumed;
}

static DWORD WINAPI
dxgi_hook_thread(LPVOID param)
{
  (void)param;
  nk_d3d12_install_hooks();
  return 0;
}

static void
loader_init(void)
{
  uint64_t start = time_now_us();

  globals.perm     = arena_new_dynamic(CONFIG_PERM_ARENA_SIZE, 64 * KB);
  globals.game_dir = path_module_dir(&globals.perm);

  MASSERT(globals.perm.backing != NULL, "failed to create global permanent arena");

  ASSERT(!str_is_empty(globals.game_dir));
  MASSERT(dir_exists(globals.game_dir), "invalid game directory: '%.*s'", STR_ARG(globals.game_dir));

  log_init(STR_LIT(CONFIG_LOG_FILE_NAME), CONFIG_LOG_LEVEL, false);
  nk_d3d12_preinit(&globals.perm);
  ui_manager_preinit(&globals.ui_manager, &globals.mod_manager, &globals.perm);
  scan_user_module_signatures();

  globals.listeners = ARENA_PUSH_ARRAY_ZERO(&globals.perm, uobject_listener_t, CONFIG_UOBJECT_ARRAY_MAX_LISTENERS);
  ASSERT(globals.listeners != NULL);

  mod_manager_init(&globals.mod_manager, globals.game_dir);
  mod_manager_startup_load_cfg(&globals.mod_manager);
  {
    register_builtin_uobject_search(&globals.mod_manager);
    register_builtin_ufunction_tracer(&globals.mod_manager);
    register_builtin_tweaks(&globals.mod_manager);
  }
  mod_manager_startup_load_mods(&globals.mod_manager);

  LOG_DEBUG("perm arena: committed/reserved: %llu/%llu", globals.perm.committed, globals.perm.reserved);

  /* cursor hook */
  HMODULE user32 = GetModuleHandleA("user32.dll");
  ASSERT(user32 != NULL);

  pfn_set_cursor_t set_cursor_addr = (pfn_set_cursor_t)(void *)GetProcAddress(user32, "SetCursor");
  MH_CreateHook((LPVOID)set_cursor_addr, (LPVOID)set_cursor_hook, (LPVOID *)&g_set_cursor_real);
  MH_EnableHook((LPVOID)set_cursor_addr);

  pfn_show_cursor_t show_cursor_addr = (pfn_show_cursor_t)(void *)GetProcAddress(user32, "ShowCursor");
  MH_CreateHook((LPVOID)show_cursor_addr, (LPVOID)show_cursor_hook, (LPVOID *)&g_show_cursor_real);
  MH_EnableHook((LPVOID)show_cursor_addr);

  LOG_DEBUG("init time (us): %llu", time_now_us() - start);

  HANDLE thread = CreateThread(NULL, 0, dxgi_hook_thread, NULL, 0, NULL);
  if (thread) {
    CloseHandle(thread);
  }
}

BOOL WINAPI
DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
  (void)inst;
  (void)reserved;

  if (reason == DLL_PROCESS_ATTACH) {
    ASSERT(MH_Initialize() == MH_OK);
    loader_init();
  }
  return TRUE;
}

/* ===================================================== HOOKS ====================================================== */

bool __fastcall
pak_file_mount_hook(void *self, const wchar_t *pak_filename, int pak_order, const wchar_t *path, bool load_index)
{
  LOG_DEBUG("===> PAK MOUNT (BEGIN)");
  if (!globals.pak_file) {
    globals.pak_file = self;
  }

  tmp_arena_t tmp    = scratch_begin(NULL);
  bool        result = false;
  {
    str_t pak_filename_str = str_from_str16(tmp.arena, str16_from_wstr(pak_filename));
    str_t path_str         = str_from_str16(tmp.arena, str16_from_wstr(path));

    LOG_DEBUG("Pak filename: %.*s", STR_ARG(pak_filename_str));
    LOG_DEBUG("Pak order:    %u", pak_order);
    LOG_DEBUG("Path:         %.*s", STR_ARG(path_str));
    LOG_DEBUG("Load index:   %d", load_index);

    result = pak_file_mount_real(self, pak_filename, pak_order, path, load_index);
    LOG_DEBUG("OK:           %s", result ? "true" : "false");
    LOG_DEBUG("===> PAK MOUNT (END)");
  }
  scratch_end(tmp);

  LOG_DEBUG("pak_file_mount_hook: Thread ID: %lu", GetCurrentThreadId());

  static bool mod_manager_assets_mounted = false;
  if (!mod_manager_assets_mounted) {
    /* NOTE: set before mounting to prevent recursive re-entry */
    mod_manager_assets_mounted = true;
    mod_manager_mount_assets(&globals.mod_manager);
  }

  return result;
}

fio_status_t *__fastcall
io_dispatcher_mount_hook(void *self, fio_status_t *st, fio_env_t *env, fguid_t *guid, faes_key_t *key)
{
  LOG_DEBUG("===> IOSTORE MOUNT (BEGIN)");
  if (!globals.io_dispatcher) {
    globals.io_dispatcher = self;
  }

  fio_status_t *ret = io_dispatcher_mount_real(self, st, env, guid, key);
  tmp_arena_t   tmp = scratch_begin(NULL);
  {
    str_t path_str = str_from_str16(tmp.arena, str16_make(env->path.data, env->path.len));

    LOG_DEBUG("Path:   %.*s", STR_ARG(path_str));
    LOG_DEBUG("Order:  %u", env->order);
    LOG_DEBUG("GUID:   %u.%u.%u.%u", guid->a, guid->b, guid->c, guid->d);
    LOG_DEBUG("Status: %.*s", STR_ARG(unreal_fio_status_to_str(*ret, tmp.arena)));
    LOG_DEBUG("===> IOSTORE MOUNT (END)");
  }
  scratch_end(tmp);
  return ret;
}

void __fastcall
game_engine_tick_hook(void *self, float delta, bool idle_mode)
{
  on_engine_tick_pre(delta);
  game_engine_tick_real(self, delta, idle_mode);
  on_engine_tick_post();
}

void __fastcall
process_event_hook(uobject_t *self, ufunc_t *func, void *params)
{
  if (globals.game_thread_id != GetCurrentThreadId() || !globals.engine_inited) {
    process_event_real(self, func, params);
    return;
  }

  bool consumed = mod_manager_dispatch_process_event_pre(&globals.mod_manager, self, func, params);
  if (!consumed) {
    process_event_real(self, func, params);
  }
  mod_manager_dispatch_process_event_post(&globals.mod_manager, self, func, params, consumed);
}

void __fastcall
ufunction_invoke_hook(ufunc_t *func, uobject_t *obj, fframe_t *stack, void *result)
{
  if (globals.game_thread_id != GetCurrentThreadId() || !globals.engine_inited) {
    ufunction_invoke_real(func, obj, stack, result);
    return;
  }

  bool consumed = mod_manager_dispatch_ufunction_invoke_pre(&globals.mod_manager, func, obj, stack, result);
  if (!consumed) {
    ufunction_invoke_real(func, obj, stack, result);
  }
  mod_manager_dispatch_ufunction_invoke_post(&globals.mod_manager, func, obj, stack, result, consumed);
}

bool __fastcall
process_mouse_move_event_hook(void *self, fpointer_event_t mouse_event, bool is_synthetic)
{
  input_event_t input_event = input_event_from_pointer_event(INPUT_EVENT_MOUSE_MOVE, &mouse_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_mouse_move_event_real(self, mouse_event, is_synthetic);
}

bool __fastcall
process_mouse_button_down_event_hook(void *self, void *platform_win, fpointer_event_t mouse_event)
{
  input_event_t input_event = input_event_from_pointer_event(INPUT_EVENT_MOUSE_DOWN, &mouse_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_mouse_button_down_event_real(self, platform_win, mouse_event);
}

bool __fastcall
process_mouse_button_up_event_hook(void *self, fpointer_event_t mouse_event)
{
  input_event_t input_event = input_event_from_pointer_event(INPUT_EVENT_MOUSE_UP, &mouse_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_mouse_button_up_event_real(self, mouse_event);
}

bool __fastcall
process_mouse_button_double_click_event_hook(void *self, void *platform_win, fpointer_event_t mouse_event)
{
  input_event_t input_event = input_event_from_pointer_event(INPUT_EVENT_MOUSE_DBLCLICK, &mouse_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_mouse_button_double_click_event_real(self, platform_win, mouse_event);
}

bool __fastcall
process_mouse_wheel_or_gesture_event_hook(void *self, fpointer_event_t wheel_event, fpointer_event_t *gesture_event)
{
  (void)gesture_event;

  input_event_t input_event = input_event_from_pointer_event(INPUT_EVENT_MOUSE_WHEEL, &wheel_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_mouse_wheel_or_gesture_event_real(self, wheel_event, gesture_event);
}

bool __fastcall
process_key_char_event_hook(void *self, fcharacter_event_t character_event)
{
  input_event_t input_event = input_event_from_character_event(&character_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_key_char_event_real(self, character_event);
}

bool __fastcall
process_key_down_event_hook(void *self, fkey_event_t key_event)
{
  input_event_t input_event = input_event_from_key_event(INPUT_EVENT_KEY_DOWN, &key_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_key_down_event_real(self, key_event);
}

bool __fastcall
process_key_up_event_hook(void *self, fkey_event_t key_event)
{
  input_event_t input_event = input_event_from_key_event(INPUT_EVENT_KEY_UP, &key_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_key_up_event_real(self, key_event);
}

bool __fastcall
process_analog_input_event_hook(void *self, fanalog_input_event_t analog_input_event)
{
  input_event_t input_event = input_event_from_analog_event(&analog_input_event);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_analog_input_event_real(self, analog_input_event);
}

bool __fastcall
process_app_activation_event_hook(void *self, bool app_activated)
{
  input_event_t input_event = input_event_from_app_activation_event(app_activated);
  if (on_input_event(&input_event)) {
    return true;
  }
  return process_app_activation_event_real(self, app_activated);
}

void __fastcall
d3d12_viewport_init_hook(void *self)
{
  d3d12_viewport_init_real(self);

  uint8_t *adapter = *(uint8_t **)((uint8_t *)self + 0x18);
  if (!adapter) {
    return;
  }

  uint8_t *device = *(uint8_t **)(adapter + 0x828);
  if (!device) {
    return;
  }

  uint8_t *inner = *(uint8_t **)(device + 0x38);
  if (!inner) {
    return;
  }

  ID3D12CommandQueue *queue = *(ID3D12CommandQueue **)(inner + 0x28);
  if (queue) {
    nk_d3d12_set_command_queue(queue);
  }
}