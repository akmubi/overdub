#include <d3d11.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define WINDOW_WIDTH  1600
#define WINDOW_HEIGHT 800
#define USER_TEXTURES 6

#define MAX_VERTEX_BUFFER (1 * MB)
#define MAX_INDEX_BUFFER  (256 * KB)

#include "arena.h"
#include "builtin.h"
#include "config.h"
#include "globals.h"
#include "input.h"
#include "log.h"
#include "mod_manager.h"
#include "path.h"
#include "scratch.h"
#include "str.h"
#include "types.h"
#include "ui_manager.h"
#include "ui_nuklear.h"

#include "vendor/nuklear/d3d11/nuklear_d3d11.h"
#include "vendor_stb.h"

extern int
test_ui_overview(struct nk_context *ctx, unsigned int viewport_width, unsigned int viewport_height);
extern int
test_ui_style_configurator(struct nk_context *ctx, struct nk_color color_table[NK_COLOR_COUNT], unsigned int viewport_width, unsigned int viewport_height);
extern void
test_dump_prebaked_font_data(void);

static IDXGISwapChain         *swap_chain;
static ID3D11Device           *device;
static ID3D11DeviceContext    *context;
static ID3D11RenderTargetView *rt_view;

static void
set_swap_chain_size(int width, int height)
{
  if (rt_view) {
    ID3D11RenderTargetView_Release(rt_view);
  }

  ID3D11DeviceContext_OMSetRenderTargets(context, 0, NULL, NULL);

  HRESULT hr = IDXGISwapChain_ResizeBuffers(swap_chain, 0, width, height, DXGI_FORMAT_UNKNOWN, 0);
  if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR) {
    MessageBoxW(NULL, L"DXGI device is removed or reset!", L"Error", 0);
    exit(0);
  }
  ASSERT(SUCCEEDED(hr));

  ID3D11Texture2D              *back_buffer = NULL;
  D3D11_RENDER_TARGET_VIEW_DESC desc        = {
    .Format        = DXGI_FORMAT_R8G8B8A8_UNORM,
    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
  };

  hr = IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, (void **)&back_buffer);
  ASSERT(SUCCEEDED(hr));

  hr = ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)back_buffer, &desc, &rt_view);
  ASSERT(SUCCEEDED(hr));

  ID3D11Texture2D_Release(back_buffer);
}

static struct nk_colorf bg       = {.r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f};
static int              g_width  = WINDOW_WIDTH;
static int              g_height = WINDOW_HEIGHT;

static input_mod_flags_t
ui_input_mods_from_win32(void)
{
  input_mod_flags_t mods = 0;

  if (GetKeyState(VK_SHIFT) & 0x8000) {
    mods |= INPUT_MOD_SHIFT;
  }

  if (GetKeyState(VK_CONTROL) & 0x8000) {
    mods |= INPUT_MOD_CTRL;
  }

  if (GetKeyState(VK_MENU) & 0x8000) {
    mods |= INPUT_MOD_ALT;
  }

  return mods;
}

static input_key_kind_t
ui_input_key_from_vk(WPARAM vk, LPARAM lparam)
{
  switch (vk) {
  case 'A':
    return INPUT_KEY_A;
  case 'B':
    return INPUT_KEY_B;
  case 'C':
    return INPUT_KEY_C;
  case 'D':
    return INPUT_KEY_D;
  case 'E':
    return INPUT_KEY_E;
  case 'F':
    return INPUT_KEY_F;
  case 'G':
    return INPUT_KEY_G;
  case 'H':
    return INPUT_KEY_H;
  case 'I':
    return INPUT_KEY_I;
  case 'J':
    return INPUT_KEY_J;
  case 'K':
    return INPUT_KEY_K;
  case 'L':
    return INPUT_KEY_L;
  case 'M':
    return INPUT_KEY_M;
  case 'N':
    return INPUT_KEY_N;
  case 'O':
    return INPUT_KEY_O;
  case 'P':
    return INPUT_KEY_P;
  case 'Q':
    return INPUT_KEY_Q;
  case 'R':
    return INPUT_KEY_R;
  case 'S':
    return INPUT_KEY_S;
  case 'T':
    return INPUT_KEY_T;
  case 'U':
    return INPUT_KEY_U;
  case 'V':
    return INPUT_KEY_V;
  case 'W':
    return INPUT_KEY_W;
  case 'X':
    return INPUT_KEY_X;
  case 'Y':
    return INPUT_KEY_Y;
  case 'Z':
    return INPUT_KEY_Z;

  case '0':
    return INPUT_KEY_0;
  case '1':
    return INPUT_KEY_1;
  case '2':
    return INPUT_KEY_2;
  case '3':
    return INPUT_KEY_3;
  case '4':
    return INPUT_KEY_4;
  case '5':
    return INPUT_KEY_5;
  case '6':
    return INPUT_KEY_6;
  case '7':
    return INPUT_KEY_7;
  case '8':
    return INPUT_KEY_8;
  case '9':
    return INPUT_KEY_9;

  case VK_F1:
    return INPUT_KEY_F1;
  case VK_F2:
    return INPUT_KEY_F2;
  case VK_F3:
    return INPUT_KEY_F3;
  case VK_F4:
    return INPUT_KEY_F4;
  case VK_F5:
    return INPUT_KEY_F5;
  case VK_F6:
    return INPUT_KEY_F6;
  case VK_F7:
    return INPUT_KEY_F7;
  case VK_F8:
    return INPUT_KEY_F8;
  case VK_F9:
    return INPUT_KEY_F9;
  case VK_F10:
    return INPUT_KEY_F10;
  case VK_F11:
    return INPUT_KEY_F11;
  case VK_F12:
    return INPUT_KEY_F12;

  case VK_LEFT:
    return INPUT_KEY_LEFT;
  case VK_RIGHT:
    return INPUT_KEY_RIGHT;
  case VK_UP:
    return INPUT_KEY_UP;
  case VK_DOWN:
    return INPUT_KEY_DOWN;
  case VK_HOME:
    return INPUT_KEY_HOME;
  case VK_END:
    return INPUT_KEY_END;
  case VK_PRIOR:
    return INPUT_KEY_PAGE_UP;
  case VK_NEXT:
    return INPUT_KEY_PAGE_DOWN;
  case VK_RETURN:
    return INPUT_KEY_ENTER;
  case VK_TAB:
    return INPUT_KEY_TAB;
  case VK_BACK:
    return INPUT_KEY_BACKSPACE;
  case VK_ESCAPE:
    return INPUT_KEY_ESCAPE;
  case VK_SPACE:
    return INPUT_KEY_SPACE;
  case VK_INSERT:
    return INPUT_KEY_INSERT;
  case VK_DELETE:
    return INPUT_KEY_DELETE;

  case VK_OEM_COMMA:
    return INPUT_KEY_COMMA;
  case VK_OEM_PERIOD:
    return INPUT_KEY_PERIOD;
  case VK_OEM_2:
    return INPUT_KEY_SLASH;
  case VK_OEM_1:
    return INPUT_KEY_SEMICOLON;
  case VK_OEM_7:
    return INPUT_KEY_APOSTROPHE;
  case VK_OEM_4:
    return INPUT_KEY_LEFT_BRACKET;
  case VK_OEM_6:
    return INPUT_KEY_RIGHT_BRACKET;
  case VK_OEM_5:
    return INPUT_KEY_BACKSLASH;
  case VK_OEM_MINUS:
    return INPUT_KEY_HYPHEN;
  case VK_OEM_PLUS:
    return INPUT_KEY_EQUALS;
  case VK_OEM_3:
    return INPUT_KEY_BACKTICK;

  case VK_LSHIFT:
    return INPUT_KEY_LEFT_SHIFT;
  case VK_RSHIFT:
    return INPUT_KEY_RIGHT_SHIFT;
  case VK_LCONTROL:
    return INPUT_KEY_LEFT_CTRL;
  case VK_RCONTROL:
    return INPUT_KEY_RIGHT_CTRL;
  case VK_LMENU:
    return INPUT_KEY_LEFT_ALT;
  case VK_RMENU:
    return INPUT_KEY_RIGHT_ALT;

  case VK_SHIFT: {
    UINT sc  = (UINT)((lparam >> 16) & 0xFF);
    UINT lvk = MapVirtualKeyW(sc, MAPVK_VSC_TO_VK_EX);
    return (lvk == VK_RSHIFT) ? INPUT_KEY_RIGHT_SHIFT : INPUT_KEY_LEFT_SHIFT;
  }

  case VK_CONTROL:
    return ((lparam & (1u << 24)) != 0) ? INPUT_KEY_RIGHT_CTRL : INPUT_KEY_LEFT_CTRL;

  case VK_MENU:
    return ((lparam & (1u << 24)) != 0) ? INPUT_KEY_RIGHT_ALT : INPUT_KEY_LEFT_ALT;
  }

  return INPUT_KEY_NONE;
}

static bool
ui_input_event_from_win32(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam, input_event_t *out)
{
  if (!out) {
    return false;
  }

  mem_zero(out, sizeof(*out));
  out->modifiers = ui_input_mods_from_win32();

  POINT pt = {0};
  GetCursorPos(&pt);
  out->screen_x = (uint32_t)pt.x;
  out->screen_y = (uint32_t)pt.y;

  POINT client_pt = pt;
  ScreenToClient(wnd, &client_pt);
  out->client_x = (uint32_t)client_pt.x;
  out->client_y = (uint32_t)client_pt.y;

  switch (msg) {
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    out->kind      = INPUT_EVENT_KEY_DOWN;
    out->key       = ui_input_key_from_vk(wparam, lparam);
    out->is_repeat = (((uint32_t)lparam >> 30) & 1) != 0;
    return out->key != INPUT_KEY_NONE;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    out->kind      = INPUT_EVENT_KEY_UP;
    out->key       = ui_input_key_from_vk(wparam, lparam);
    out->is_repeat = false;
    return out->key != INPUT_KEY_NONE;

  case WM_CHAR:
    out->kind      = INPUT_EVENT_KEY_CHAR;
    out->character = (uint32_t)wparam;
    return true;

  case WM_LBUTTONDOWN:
    out->kind = INPUT_EVENT_MOUSE_DOWN;
    out->key  = INPUT_KEY_MOUSE_LEFT;
    return true;

  case WM_LBUTTONUP:
    out->kind = INPUT_EVENT_MOUSE_UP;
    out->key  = INPUT_KEY_MOUSE_LEFT;
    return true;

  case WM_LBUTTONDBLCLK:
    out->kind = INPUT_EVENT_MOUSE_DBLCLICK;
    out->key  = INPUT_KEY_MOUSE_LEFT;
    return true;

  case WM_RBUTTONDBLCLK:
    out->kind = INPUT_EVENT_MOUSE_DBLCLICK;
    out->key  = INPUT_KEY_MOUSE_RIGHT;
    return true;

  case WM_RBUTTONDOWN:
    out->kind = INPUT_EVENT_MOUSE_DOWN;
    out->key  = INPUT_KEY_MOUSE_RIGHT;
    return true;

  case WM_RBUTTONUP:
    out->kind = INPUT_EVENT_MOUSE_UP;
    out->key  = INPUT_KEY_MOUSE_RIGHT;
    return true;

  case WM_MBUTTONDOWN:
    out->kind = INPUT_EVENT_MOUSE_DOWN;
    out->key  = INPUT_KEY_MOUSE_MIDDLE;
    return true;

  case WM_MBUTTONUP:
    out->kind = INPUT_EVENT_MOUSE_UP;
    out->key  = INPUT_KEY_MOUSE_MIDDLE;
    return true;

  case WM_XBUTTONDOWN:
    out->kind = INPUT_EVENT_MOUSE_DOWN;
    out->key  = (GET_XBUTTON_WPARAM(wparam) == XBUTTON2) ? INPUT_KEY_MOUSE_THUMB_2 : INPUT_KEY_MOUSE_THUMB_1;
    return true;

  case WM_XBUTTONUP:
    out->kind = INPUT_EVENT_MOUSE_UP;
    out->key  = (GET_XBUTTON_WPARAM(wparam) == XBUTTON2) ? INPUT_KEY_MOUSE_THUMB_2 : INPUT_KEY_MOUSE_THUMB_1;
    return true;

  case WM_MOUSEMOVE:
    out->kind    = INPUT_EVENT_MOUSE_MOVE;
    out->key     = INPUT_KEY_MOUSE_X;
    out->delta_x = 0.0f;
    out->delta_y = 0.0f;
    return true;

  case WM_MOUSEWHEEL: {
    short delta      = (short)HIWORD(wparam);
    out->kind        = INPUT_EVENT_MOUSE_WHEEL;
    out->wheel_delta = (float)delta / (float)WHEEL_DELTA;
    out->key         = (delta > 0) ? INPUT_KEY_MOUSE_WHEEL_UP : INPUT_KEY_MOUSE_WHEEL_DOWN;
    return true;
  }

  case WM_MOUSELEAVE: {
    out->kind          = INPUT_EVENT_APP_ACTIVATION;
    out->app_activated = false;
    return true;
  }
  }

  return false;
}

static bool
on_input_event(input_event_t *ev)
{
  globals.keys_down[INPUT_KEY_MOUSE_WHEEL_UP]   = false;
  globals.keys_down[INPUT_KEY_MOUSE_WHEEL_DOWN] = false;

  switch (ev->kind) {
  case INPUT_EVENT_KEY_DOWN:
  case INPUT_EVENT_MOUSE_DOWN:
  case INPUT_EVENT_MOUSE_DBLCLICK:
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

#if 0
  if (ev->kind != INPUT_EVENT_MOUSE_MOVE) {
    str_t kind_str = input_event_kind_to_str(ev->kind);
    if (ev->kind == INPUT_EVENT_KEY_CHAR) {
      LOG_WARN("%-10s: kind: %.*s, key: %u", consumer, STR_ARG(kind_str), ev->character);
    } else {
      LOG_WARN("%-10s: kind: %.*s, key: %.*s", consumer, STR_ARG(kind_str), STR_ARG(input_key_to_str(ev->key)));
    }

    bg.r = (rand() % 256) / 255.0f;
    bg.g = (rand() % 256) / 255.0f;
    bg.b = (rand() % 256) / 255.0f;
  }
#else
  (void)consumer;
#endif
  return consumed;
}

static LRESULT CALLBACK
WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;

  case WM_SIZE:
    if (swap_chain) {
      g_width  = LOWORD(lparam);
      g_height = HIWORD(lparam);
      set_swap_chain_size(g_width, g_height);
      nk_d3d11_resize(context, g_width, g_height);
    }
    break;
  case WM_MOUSEMOVE: {
    TRACKMOUSEEVENT tme = {
      .cbSize    = sizeof(tme),
      .dwFlags   = TME_LEAVE,
      .hwndTrack = wnd,
    };
    TrackMouseEvent(&tme);
    break;
  }
  }

  input_event_t ev = {0};
  if (ui_input_event_from_win32(wnd, msg, wparam, lparam, &ev)) {
    if (on_input_event(&ev)) {
      // if (ev.kind != INPUT_EVENT_MOUSE_MOVE) {
      //   LOG_INFO("KIND: %.*s, KEY: %.*s", STR_ARG(input_event_kind_to_str(ev.kind)),
      //   STR_ARG(input_key_to_str(ev.key)));
      // }
      return 0;
    }
  }

  return DefWindowProcW(wnd, msg, wparam, lparam);
}

static void
ui_seed_test_manager(mod_manager_t *manager)
{
  str_t difficulties[] = {
    STR_CLIT("easy"),
    STR_CLIT("normal"),
    STR_CLIT("insane"),
    STR_CLIT("hard"),
    STR_CLIT("rhythm_master"),
  };

  str_t camera_modes[] = {
    STR_CLIT("tight"),
    STR_CLIT("default"),
    STR_CLIT("wide"),
  };

  struct {
    mod_info_t           info;
    mod_dll_info_t       dll;
    mod_asset_info_t     asset;
    mod_blueprint_info_t blueprints[10];
    int                  blueprint_count;
    mod_option_info_t    options[10];
    int                  option_count;
  } entries[] = {
    {
      .info =
        {
          .id          = STR_CLIT("boss-rush"),
          .name        = STR_CLIT("Boss RUSH"),
          .author      = STR_CLIT("akmubi"),
          .description = STR_CLIT("Adds a mode where you can fight all the bosses in the game one after another. Replaces Rhythm Tower"),
          .kind        = MOD_KIND_MOD,
          .version     = MAKE_VERSION(1, 0, 0),
        },
      .dll =
        {
          .path = STR_CLIT("main.dll"),
        },
      .asset =
        {
          .pak_path  = STR_CLIT("assets/BossRUSH.pak"),
          .utoc_path = STR_CLIT("assets/BossRUSH.utoc"),
          .ucas_path = STR_CLIT("assets/BossRUSH.ucas"),
        },
      .blueprint_count = 2,
      .blueprints =
        {
          {
            .id                       = STR_CLIT("boss-rush-main"),
            .mod_actor_class_path     = STR_CLIT("/Game/BossRUSH/BossRUSHActorMain.BossRUSHActorMain_C"),
            .attach_to                = MOD_SPAWN_CONTEXT_CUSTOM,
            .custom_attach_class_path = STR_CLIT("/Game/Hibiki/HbkPlayerController.HbkPlayerController_C"),
            .auto_spawn               = true,
          },
          {
            .id                    = STR_CLIT("boss-rush-debug-ui"),
            .mod_actor_class_path  = STR_CLIT("/Game/BossRUSH/BossRUSHActorDebugUI.BossRUSHActorDebugUI_C"),
            .attach_to             = MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER,
            .auto_spawn            = false,
            .default_spawn_keybind = keybind_parse(STR_LIT("dpad_left+dpad_right"), KEYBIND_NULL),
          },
        },
      .option_count = 4,
      .options =
        {
          {
            .type                    = MOD_OPTION_BOOL,
            .id                      = STR_CLIT("include_korsica"),
            .label                   = STR_CLIT("Include Korsica"),
            .description             = STR_CLIT("Include Korsica boss fight (which is weird)"),
            .val.boolean.default_val = true,
          },
          {
            .type                    = MOD_OPTION_BOOL,
            .id                      = STR_CLIT("enable_timer"),
            .label                   = STR_CLIT("Enable timer"),
            .description             = STR_CLIT("Enable in-game timer (from Rhythm Tower)"),
            .val.boolean.default_val = false,
          },
          {
            .type        = MOD_OPTION_ENUM,
            .id          = STR_CLIT("difficulty"),
            .label       = STR_CLIT("Difficulty"),
            .description = STR_CLIT("Difficulty of the boss fights"),
            .val.enumeration =
              {
                .values =
                  {
                    .items = difficulties,
                    .count = COUNTOF(difficulties),
                  },
                .default_val = STR_CLIT("normal"),
              },
          },
          {
            .type                    = MOD_OPTION_BOOL,
            .id                      = STR_CLIT("enable_score"),
            .label                   = STR_CLIT("Enable score"),
            .description             = STR_CLIT("Enable default scoring system from Rhythm Tower"),
            .val.boolean.default_val = false,
          },
        },
    },
    {
      .info =
        {
          .id          = STR_CLIT("no-tutorials"),
          .name        = STR_CLIT("No tutorials"),
          .author      = STR_CLIT("akmubi"),
          .description = STR_CLIT("Disable tutorial popups and some sequences."),
          .kind        = MOD_KIND_MOD,
          .version     = MAKE_VERSION(1, 2, 0),
        },
      .dll =
        {
          .path = STR_CLIT("main.dll"),
        },
      .option_count = 5,
      .options =
        {
          {
            .type                    = MOD_OPTION_BOOL,
            .id                      = STR_CLIT("enabled"),
            .label                   = STR_CLIT("Enabled"),
            .description             = STR_CLIT("Enable camera override."),
            .val.boolean.default_val = true,
          },
          {
            .type        = MOD_OPTION_INT,
            .id          = STR_CLIT("fov"),
            .label       = STR_CLIT("Camera FOV"),
            .description = STR_CLIT("Vertical field of view."),
            .val.integer = {.default_val = 82, .min_val = 60, .max_val = 130, .step = 1},
          },
          {
            .type         = MOD_OPTION_FLOAT,
            .id           = STR_CLIT("sensitivity"),
            .label        = STR_CLIT("Sensitivity"),
            .description  = STR_CLIT("Camera sensitivity multiplier."),
            .val.floating = {.default_val = 1.15f, .min_val = 0.10f, .max_val = 3.0f, .step = 0.05f},
          },
          {
            .type        = MOD_OPTION_ENUM,
            .id          = STR_CLIT("mode"),
            .label       = STR_CLIT("Mode"),
            .description = STR_CLIT("Camera framing mode."),
            .val.enumeration =
              {
                .values      = {.items = camera_modes, .count = COUNTOF(camera_modes)},
                .default_val = STR_CLIT("default"),
              },
          },
          {
            .type                  = MOD_OPTION_COLOR,
            .id                    = STR_CLIT("debug_color"),
            .label                 = STR_CLIT("Debug color"),
            .description           = STR_CLIT("Debug draw color."),
            .val.color.default_val = {255, 64, 64, 255},
          },
        },
    },
#if 0
    {
      .info =
        {
          .id          = STR_CLIT("hd-texture-pack"),
          .name        = STR_CLIT("HD Texture Pack"),
          .author      = STR_CLIT("art-team"),
          .description = STR_CLIT("Assets-only pack used to exercise mount state and ordering."),
          .kind        = MOD_KIND_MOD,
          .version     = MAKE_VERSION(2, 0, 1),
        },
      .asset =
        {
          .pak_path  = STR_CLIT("assets/HDTexturePack.pak"),
          .utoc_path = STR_CLIT("assets/HDTexturePack.utoc"),
          .ucas_path = STR_CLIT("assets/HDTexturePack.ucas"),
        },
    },
    {
      .info =
        {
          .id          = STR_CLIT("combo-meter-plus"),
          .name        = STR_CLIT("Combo Meter Plus"),
          .author      = STR_CLIT("akmubi"),
          .description = STR_CLIT("UI/gameplay mod with code, assets and a few editor-friendly options."),
          .kind        = MOD_KIND_MOD,
          .version     = MAKE_VERSION(0, 9, 4),
        },
      .dll =
        {
          .path = STR_CLIT("combo_meter.dll"),
        },
      .asset =
        {
          .pak_path  = STR_CLIT("assets/ComboMeterPlus.pak"),
          .utoc_path = STR_CLIT("assets/ComboMeterPlus.utoc"),
          .ucas_path = STR_CLIT("assets/ComboMeterPlus.ucas"),
        },
      .option_count = 4,
      .options =
        {
          {
            .type                = MOD_OPTION_BOOL,
            .id                  = STR_CLIT("show_hit_sparks"),
            .label               = STR_CLIT("Show hit sparks"),
            .description         = STR_CLIT("Enable extra UI hit spark effects."),
            .boolean.default_val = true,
          },
          {
            .type        = MOD_OPTION_INT,
            .id          = STR_CLIT("meter_scale"),
            .label       = STR_CLIT("Meter scale"),
            .description = STR_CLIT("UI scale percent."),
            .integer     = {.default_val = 100, .min_val = 50, .max_val = 200, .step = 5},
          },
          {
            .type        = MOD_OPTION_ENUM,
            .id          = STR_CLIT("meter_style"),
            .label       = STR_CLIT("Meter style"),
            .description = STR_CLIT("Visual style of the combo meter."),
            .enumeration =
              {
                .values      = {.items = meter_styles, .count = COUNTOF(meter_styles)},
                .default_val = STR_CLIT("arcade"),
              },
          },
          {
            .type               = MOD_OPTION_STRING,
            .id                 = STR_CLIT("profile_name"),
            .label              = STR_CLIT("Profile"),
            .description        = STR_CLIT("Preset name."),
            .string.default_val = STR_CLIT("Default"),
          },
        },
    },
    {
      .info =
        {
          .id          = STR_CLIT("object-browser"),
          .name        = STR_CLIT("Object Browser"),
          .author      = STR_CLIT("akmubi"),
          .description = STR_CLIT("Inspection-oriented tool with code-backed UI only.\n"
                                  "\n"
                                  "Test.\n"),
          .kind        = MOD_KIND_TOOL,
          .version     = MAKE_VERSION(0, 2, 1),
        },
      .dll =
        {
          .path = STR_CLIT("object_browser.dll"),
        },
    },
    {
      .info =
        {
          .id          = STR_CLIT("training-room-plus"),
          .name        = STR_CLIT("Training Room Plus"),
          .author      = STR_CLIT("someone"),
          .description = STR_CLIT("Blueprint-heavy gameplay mod with spawnable helpers."),
          .kind        = MOD_KIND_MOD,
          .version     = MAKE_VERSION(0, 3, 0),
        },
      .asset =
        {
          .pak_path  = STR_CLIT("assets/TrainingRoomPlus.pak"),
          .utoc_path = STR_CLIT("assets/TrainingRoomPlus.utoc"),
          .ucas_path = STR_CLIT("assets/TrainingRoomPlus.ucas"),
        },
      .blueprint_count = 2,
      .blueprints =
        {
          {
            .id                   = STR_CLIT("training-room-main"),
            .mod_actor_class_path = STR_CLIT("/Game/TRP/BP_TrainingRoomMain.BP_TrainingRoomMain_C"),
            .attach_to            = MOD_SPAWN_CONTEXT_WORLD,
            .auto_spawn           = true,
          },
          {
            .id                    = STR_CLIT("training-room-helper"),
            .mod_actor_class_path  = STR_CLIT("/Game/TRP/BP_TrainingRoomHelper.BP_TrainingRoomHelper_C"),
            .attach_to             = MOD_SPAWN_CONTEXT_PLAYER_CONTROLLER,
            .auto_spawn            = false,
            .default_spawn_keybind = keybind_parse(STR_LIT("f7"), KEYBIND_NULL),
          },
        },
    },
#endif
  };

  for (int i = 0; i < COUNTOF(entries); ++i) {
    mod_manifest_t manifest = {
      .mod_dir         = STR_NULL,
      .manifest_path   = STR_NULL,
      .config_path     = STR_NULL,
      .info            = entries[i].info,
      .dll             = entries[i].dll,
      .asset           = entries[i].asset,
      .blueprints      = entries[i].blueprints,
      .blueprint_count = entries[i].blueprint_count,
      .options         = entries[i].options,
      .option_count    = entries[i].option_count,
    };

    mod_handle_t h = mod_register(manager, &manifest);
    ASSERT(h != MOD_HANDLE_INVALID);
  }
}

typedef enum {
  UOBJECT_KIND_UNKNOWN = 0,
  UOBJECT_KIND_CDO,
  UOBJECT_KIND_CLASS,
  UOBJECT_KIND_STRUCT,
  UOBJECT_KIND_ENUM,
  UOBJECT_KIND_FUNC,
  UOBJECT_KIND_PACKAGE,
  UOBJECT_KIND_MAX,
} uobject_kind_t;

static struct nk_color
uobject_kind_color(uobject_kind_t kind)
{
  switch (kind) {
  case UOBJECT_KIND_CDO:
    return nk_rgb(181, 192, 201);
  case UOBJECT_KIND_CLASS:
    return nk_rgb(0, 191, 255);
  case UOBJECT_KIND_STRUCT:
    return nk_rgb(255, 215, 0);
  case UOBJECT_KIND_ENUM:
    return nk_rgb(76, 175, 80);
  case UOBJECT_KIND_FUNC:
    return nk_rgb(255, 79, 0);
  case UOBJECT_KIND_PACKAGE:
    return nk_rgb(255, 64, 129);
  case UOBJECT_KIND_UNKNOWN:
  default:
    return nk_rgb(222, 226, 230);
  }
}

static str_t
uobject_kind_to_str(uobject_kind_t kind)
{
  switch (kind) {
  case UOBJECT_KIND_CDO:
    return STR_LIT("CDO");
  case UOBJECT_KIND_CLASS:
    return STR_LIT("Class");
  case UOBJECT_KIND_STRUCT:
    return STR_LIT("Struct");
  case UOBJECT_KIND_ENUM:
    return STR_LIT("Enum");
  case UOBJECT_KIND_FUNC:
    return STR_LIT("Function");
  case UOBJECT_KIND_PACKAGE:
    return STR_LIT("Package");
  case UOBJECT_KIND_UNKNOWN:
    return STR_LIT("Instance/Other");
  default:
    return STR_LIT("-");
  }
}

static void
ui_tabs(struct nk_context *ctx, const char *group_name, str_t tab_names[], int tab_count, int *selected_idx, int *closed_idx)
{
  ASSERT(ctx != NULL);
  ASSERT(group_name != NULL && group_name[0] != '\0');

  if (!tab_names || tab_count == 0) {
    return;
  }

  if (closed_idx) {
    *closed_idx = -1;
  }

  nk_style_push_vec2(ctx, &ctx->style.scrollh.padding, nk_vec2(0.0f, 0.0f));
  nk_style_push_vec2(ctx, &ctx->style.window.group_padding, nk_vec2(0.0f, 0.0f));
  nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0.0f, 0.0f));
  nk_style_push_float(ctx, &ctx->style.button.rounding, 0.0f);
  nk_style_push_style_item(ctx, &ctx->style.button.normal, ctx->style.property.normal);
  nk_style_push_style_item(ctx, &ctx->style.button.hover, ctx->style.property.hover);
  nk_style_push_style_item(ctx, &ctx->style.button.active, ctx->style.property.active);
  nk_style_push_color(ctx, &ctx->style.button.border_color, ctx->style.property.border_color);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    float *widths = ARENA_PUSH_ARRAY(tmp.arena, float, tab_count);
    ASSERT(widths != NULL);

    float tab_h       = 18.0f;
    float text_pad    = 4.0f;
    float group_pad_y = ctx->style.window.group_padding.y;
    float group_pad_x = ctx->style.window.group_padding.x;
    float scroll_h    = ctx->style.window.scrollbar_size.y;

    float button_pad = ctx->style.button.padding.x + ctx->style.button.border;
    float x_width    = ui_text_width(ctx, STR_LIT("X")) + 2.0f * button_pad;

    float total_width = 0.0f;
    for (int i = 0; i < tab_count; ++i) {
      widths[i]    = ui_text_width(ctx, tab_names[i]) + 2.0f * (text_pad + button_pad);
      total_width += widths[i] + x_width;
    }

    float max_content_width   = nk_window_get_content_region_size(ctx).x;
    float total_content_width = total_width + 2.0f * group_pad_x;

    bool has_scrollbar = (total_content_width > max_content_width);

    float    total_h     = group_pad_y + tab_h + group_pad_y + (has_scrollbar)*scroll_h;
    nk_flags group_flags = (has_scrollbar) ? NK_WINDOW_NO_SCROLLBAR_V : NK_WINDOW_NO_SCROLLBAR;

    nk_layout_row_dynamic(ctx, total_h, 1);

    if (nk_group_begin(ctx, group_name, group_flags)) {
      nk_layout_row_begin(ctx, NK_STATIC, tab_h, tab_count * 2);
      {
        for (int i = 0; i < tab_count; ++i) {
          nk_layout_row_push(ctx, widths[i]);
          bool is_selected = (selected_idx) ? (*selected_idx == i) : false;

          if (is_selected) {
            nk_style_push_style_item(ctx, &ctx->style.button.normal, UI_ITEM(UI_C_ACCENT));
            nk_style_push_style_item(ctx, &ctx->style.button.hover, UI_ITEM(UI_C_ACCENT_HOVER));
            nk_style_push_style_item(ctx, &ctx->style.button.active, UI_ITEM(UI_C_ACCENT_ACTIVE));
            nk_style_push_color(ctx, &ctx->style.button.border_color, UI_C_BORDER_FOCUS);
          }

          if (ui_button_str(ctx, tab_names[i])) {
            if (selected_idx) {
              *selected_idx = i;
            }
          }

          nk_layout_row_push(ctx, x_width);
          if (nk_button_symbol(ctx, NK_SYMBOL_X)) {
            if (closed_idx) {
              *closed_idx = i;
            }
          }

          if (is_selected) {
            nk_style_pop_color(ctx);
            nk_style_pop_style_item(ctx);
            nk_style_pop_style_item(ctx);
            nk_style_pop_style_item(ctx);
          }
        }
      }
      nk_layout_row_end(ctx);
      nk_group_end(ctx);
    }
  }
  scratch_end(tmp);

  nk_style_pop_color(ctx);
  nk_style_pop_style_item(ctx);
  nk_style_pop_style_item(ctx);
  nk_style_pop_style_item(ctx);
  nk_style_pop_float(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
  nk_style_pop_vec2(ctx);
}

static void
draw_test_window(struct nk_context *ctx)
{
  int width  = NK_MAX(0, g_width - (g_width / 10));
  int height = NK_MAX(0, g_height - (g_height / 10));

  nk_flags       flags  = NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE;
  struct nk_rect bounds = nk_rect((g_width - width) * 0.5f, (g_height - height) * 0.5f, width, height);
  const char    *name   = "Test Window";

  if (nk_begin(ctx, name, bounds, flags)) {
    // static arena_t     arenas[64];
    static uint64_t    cap  = 8;
    static str_array_t tabs = {0};

    if (!tabs.items) {
      // needs to be initialized and allocated from tool's arena
      tabs.items = ARENA_PUSH_ARRAY_ZERO(&globals.perm, str_t, cap);
      tabs.count = 0;
    }

    static int current_tab = 0;

    struct nk_input *in = NULL;
    if (!(ctx->current->layout->flags & NK_WINDOW_ROM) && !(ctx->current->layout->flags & NK_WINDOW_NO_INPUT) && ui_nk_current_panel_accepts_input(ctx)) {
      in = &ctx->input;
    }

    if (in) {
      if (tabs.count > 0 && nk_input_is_key_pressed(in, NK_KEY_TAB)) {
        if (in->keyboard.keys[NK_KEY_SHIFT].down) {
          current_tab = (current_tab == 0) ? (int)tabs.count - 1 : current_tab - 1;
        } else {
          current_tab = (current_tab + 1) % tabs.count;
        }
      }

      str_t text = str_from_cstr_with_cap(in->keyboard.text, in->keyboard.text_len);
      if (str_equal(text, STR_LIT("+"), 0)) {
        // when new tab is created, are new dedicated arena should be created

        if (tabs.count < cap) {
          tabs.items[tabs.count]  = str_push_fmt(&globals.perm, "Tab-%llu", tabs.count + 1);
          tabs.count             += 1;
        }
      }

      if (str_equal(text, STR_LIT("-"), 0)) {
        // when the tab is closed the respective arena should be destroyed

        if ((uint64_t)current_tab < tabs.count) {
          for (uint64_t i = current_tab; i < tabs.count - 1; ++i) {
            tabs.items[i] = tabs.items[i + 1];
          }
          tabs.count -= 1;
        }
      }
    }

    int closed_tab = -1;
    ui_tabs(ctx, "test.tabs", tabs.items, tabs.count, &current_tab, &closed_tab);

    if (closed_tab >= 0 && (uint64_t)closed_tab < tabs.count) {
      for (uint64_t i = closed_tab; i < tabs.count - 1; ++i) {
        tabs.items[i] = tabs.items[i + 1];
      }
      tabs.count -= 1;
    }

    if (tabs.count > 0 && (uint64_t)current_tab >= tabs.count) {
      current_tab = tabs.count - 1;
    }
  }
  nk_end(ctx);
}

int
main(void)
{
  globals.perm     = arena_new_dynamic(CONFIG_PERM_ARENA_SIZE, 64 * KB);
  globals.game_dir = path_module_dir(&globals.perm);

  log_init(STR_LIT(CONFIG_LOG_FILE_NAME), CONFIG_LOG_LEVEL, false);

  WNDCLASSW wc = {
    .style         = CS_DBLCLKS,
    .lpfnWndProc   = WindowProc,
    .hInstance     = GetModuleHandleW(0),
    .hIcon         = LoadIcon(NULL, IDI_APPLICATION),
    .hCursor       = LoadCursor(NULL, IDC_ARROW),
    .lpszClassName = L"NuklearWindowClass",
  };
  RegisterClassW(&wc);

  RECT  rect    = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
  DWORD style   = WS_OVERLAPPEDWINDOW;
  DWORD exstyle = WS_EX_APPWINDOW;

  AdjustWindowRectEx(&rect, style, FALSE, exstyle);
  HWND wnd = CreateWindowExW(exstyle,
                             wc.lpszClassName,
                             L"Nuklear Direct3D 11 Demo",
                             style | WS_VISIBLE,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rect.right - rect.left,
                             rect.bottom - rect.top,
                             NULL,
                             NULL,
                             wc.hInstance,
                             NULL);

  D3D_FEATURE_LEVEL    feature_level   = {0};
  DXGI_SWAP_CHAIN_DESC swap_chain_desc = {
    .BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM,
    .BufferDesc.RefreshRate.Numerator   = 180,
    .BufferDesc.RefreshRate.Denominator = 1,
    .SampleDesc.Count                   = 1,
    .SampleDesc.Quality                 = 0,
    .BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount                        = 1,
    .OutputWindow                       = wnd,
    .Windowed                           = TRUE,
    .SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD,
    .Flags                              = 0,
  };

  if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, &feature_level, &context))) {
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, &feature_level, &context);
    ASSERT(SUCCEEDED(hr));
  }
  set_swap_chain_size(WINDOW_WIDTH, WINDOW_HEIGHT);

  struct nk_context *ctx = nk_d3d11_init(device, WINDOW_WIDTH, WINDOW_HEIGHT, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER);
  ASSERT(ctx != NULL);

  struct nk_font *font_body  = NULL;
  struct nk_font *font_title = NULL;
  nk_d3d11_setup_fonts(&font_body, &font_title);

  nk_style_set_font(ctx, &font_body->handle);

  mod_manager_init(&globals.mod_manager, globals.game_dir);
  mod_manager_startup_load_cfg(&globals.mod_manager);
  {
    register_builtin_uobject_search(&globals.mod_manager);
    register_builtin_ufunction_tracer(&globals.mod_manager);
    register_builtin_tweaks(&globals.mod_manager);

    ui_seed_test_manager(&globals.mod_manager);
  }
  mod_manager_startup_load_mods(&globals.mod_manager);

  ui_manager_preinit(&globals.ui_manager, &globals.mod_manager, &globals.perm);
  ui_manager_init(&globals.ui_manager, ctx, font_body, font_title, g_width, g_height);

  // ui_mod_manager_open(&globals.ui_manager.main);
  // ui_console_open(&globals.ui_console);

  globals.ui_manager.main.selected = (globals.mod_manager.mod_order.want[0]);

  tmp_arena_t tmp = scratch_begin(NULL);
  {
    LOG_DEBUG("overlay: '%.*s'", STR_ARG(keybind_to_str(globals.mod_manager.cfg.overlay_toggle, tmp.arena)));
    LOG_DEBUG("console: '%.*s'", STR_ARG(keybind_to_str(globals.ui_manager.console.cfg.toggle_bind, tmp.arena)));
  }
  scratch_end(tmp);

  mod_manager_start_dlls(&globals.mod_manager);

  double frame_time_prev = (double)time_now_us();
  int    running         = 1;
  while (running) {
    double frame_time_now = (double)time_now_us();
    double delta          = (frame_time_now - frame_time_prev) / 1000000.0;

    MSG msg;
    nk_input_begin(ctx);
    globals.ui_manager.input_begun = true;
    globals.ui_manager.input_frame = globals.frame_counter;

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        running = 0;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    nk_input_end(ctx);
    globals.ui_manager.input_begun = false;

    if (keybind_is_pressed(keybind_parse(STR_LIT("Ctrl+C"), KEYBIND_NULL))) {
      LOG_DEBUG("delta: %g\n\n\nTEST\nTEST", delta);
    }

    if (delta > 0.00001f) {
      float fps = 1.0f / delta;
      if (globals.fps <= 0.0f) {
        globals.fps = fps;
      } else {
        globals.fps += (fps - globals.fps) * 0.10f;
      }
    }

    mod_manager_dispatch_tick(&globals.mod_manager, delta);

    ui_manager_on_frame_end(&globals.ui_manager, g_width, g_height);

    test_ui_overview(ctx, g_width, g_height);

    ID3D11DeviceContext_ClearRenderTargetView(context, rt_view, &bg.r);
    ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rt_view, NULL);
    nk_d3d11_render(context, NK_ANTI_ALIASING_ON);
    HRESULT hr = IDXGISwapChain_Present(swap_chain, 1, 0);
    if (hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_REMOVED) {
      MessageBoxW(NULL, L"D3D11 device is lost or removed!", L"Error", 0);
      break;
    } else if (hr == DXGI_STATUS_OCCLUDED) {
      Sleep(10);
    }
    ASSERT(SUCCEEDED(hr));

    mem_copy(globals.prev_keys_down, globals.keys_down, sizeof(globals.prev_keys_down));

    scratch_reset();
    globals.frame_counter += 1;

    frame_time_prev = frame_time_now;
  }

  ID3D11DeviceContext_ClearState(context);
  nk_d3d11_shutdown();
  ID3D11RenderTargetView_Release(rt_view);
  ID3D11DeviceContext_Release(context);
  ID3D11Device_Release(device);
  IDXGISwapChain_Release(swap_chain);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  return 0;
}
