#include "nuklear_d3d11.h"
#include "profiler.h"

#include <d3d11.h>
#include <xinput.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "nuklear_d3d11_pixel_shader.h"
#include "nuklear_d3d11_vertex_shader.h"

struct nk_d3d11_vertex {
  float   position[2];
  float   uv[2];
  nk_byte col[4];
};

#define NK_D3D11_TEXT_CACHE_SIZE (8 * 1024 * 1024)

static struct {
  struct nk_context      ctx;
  struct nk_buffer       cmds;
  void                  *text_memory;
  struct nk_glyph_atlas *atlas;

  struct nk_draw_null_texture tex_null;
  unsigned int                max_vertex_buffer;
  unsigned int                max_index_buffer;

  D3D11_VIEWPORT            viewport;
  ID3D11Device             *device;
  ID3D11RasterizerState    *rasterizer_state;
  ID3D11VertexShader       *vertex_shader;
  ID3D11InputLayout        *input_layout;
  ID3D11Buffer             *const_buffer;
  ID3D11PixelShader        *pixel_shader;
  ID3D11BlendState         *blend_state;
  ID3D11Buffer             *index_buffer;
  ID3D11Buffer             *vertex_buffer;
  ID3D11ShaderResourceView *font_texture_view;
  ID3D11Texture2D          *font_texture;
  ID3D11SamplerState       *sampler_state;

  /* XInput, loaded at the first poll */
  HMODULE xinput;
  DWORD(WINAPI *xinput_get_state)(DWORD, XINPUT_STATE *);
  nk_bool  xinput_tried;
  nk_bool  pad_connected[XUSER_MAX_COUNT];
  ULONGLONG pad_retry; /* look for new pads once a second */
  nk_bool  pad_down[NK_KEY_NAV_WINDOW_MENU - NK_KEY_NAV_UP + 1];
} d3d11;

static void
nk_d3d11_upload_atlas(ID3D11DeviceContext *context)
{
  int       x, y, w, h;
  D3D11_BOX box;

  if (!d3d11.atlas || !d3d11.font_texture || !nk_glyph_atlas_dirty(d3d11.atlas, &x, &y, &w, &h)) {
    return;
  }

  box.left   = (UINT)x;
  box.top    = (UINT)y;
  box.front  = 0;
  box.right  = (UINT)(x + w);
  box.bottom = (UINT)(y + h);
  box.back   = 1;
  ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)d3d11.font_texture, 0, &box,
                                        d3d11.atlas->pixels + ((size_t)y * (size_t)d3d11.atlas->width + (size_t)x) * 4,
                                        (UINT)d3d11.atlas->width * 4, 0);
  nk_glyph_atlas_clean(d3d11.atlas);
}

NK_API void
nk_d3d11_render(ID3D11DeviceContext *context, enum nk_anti_aliasing AA)
{
  const float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const UINT  stride          = sizeof(struct nk_d3d11_vertex);
  const UINT  offset          = 0;
#ifdef NK_UINT_DRAW_INDEX
  DXGI_FORMAT index_buffer_format = DXGI_FORMAT_R32_UINT;
#else
  DXGI_FORMAT index_buffer_format = DXGI_FORMAT_R16_UINT;
#endif

  ID3D11DeviceContext_IASetInputLayout(context, d3d11.input_layout);
  ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &d3d11.vertex_buffer, &stride, &offset);
  ID3D11DeviceContext_IASetIndexBuffer(context, d3d11.index_buffer, index_buffer_format, 0);
  ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  ID3D11DeviceContext_VSSetShader(context, d3d11.vertex_shader, NULL, 0);
  ID3D11DeviceContext_VSSetConstantBuffers(context, 0, 1, &d3d11.const_buffer);

  ID3D11DeviceContext_PSSetShader(context, d3d11.pixel_shader, NULL, 0);
  ID3D11DeviceContext_PSSetSamplers(context, 0, 1, &d3d11.sampler_state);

  ID3D11DeviceContext_OMSetBlendState(context, d3d11.blend_state, blend_factor, 0xffffffff);
  ID3D11DeviceContext_RSSetState(context, d3d11.rasterizer_state);
  ID3D11DeviceContext_RSSetViewports(context, 1, &d3d11.viewport);

  /* Convert from command queue into draw list and draw to screen */
  { /* load draw vertices & elements directly into vertex + element buffer */
    D3D11_MAPPED_SUBRESOURCE      vertices;
    D3D11_MAPPED_SUBRESOURCE      indices;
    const struct nk_draw_command *cmd;
    UINT                          offset = 0;
    HRESULT                       hr;

    hr =
      ID3D11DeviceContext_Map(context, (ID3D11Resource *)d3d11.vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vertices);

    if (FAILED(hr)) {
      /* device removed or reset: drop the frame */
      nk_clear(&d3d11.ctx);
      nk_buffer_clear(&d3d11.cmds);
      return;
    }

    hr =
      ID3D11DeviceContext_Map(context, (ID3D11Resource *)d3d11.index_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &indices);

    if (FAILED(hr)) {
      ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)d3d11.vertex_buffer, 0);
      nk_clear(&d3d11.ctx);
      nk_buffer_clear(&d3d11.cmds);
      return;
    }

    { /* fill converting configuration */
      struct nk_convert_config                              config;
      NK_STORAGE const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_d3d11_vertex, position)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_d3d11_vertex, uv)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_d3d11_vertex, col)},
        {NK_VERTEX_LAYOUT_END}};

      memset(&config, 0, sizeof(config));
      config.vertex_layout        = vertex_layout;
      config.vertex_size          = sizeof(struct nk_d3d11_vertex);
      config.vertex_alignment     = NK_ALIGNOF(struct nk_d3d11_vertex);
      config.global_alpha         = 1.0f;
      config.shape_AA             = AA;
      config.line_AA              = AA;
      config.circle_segment_count = 22;
      config.curve_segment_count  = 22;
      config.arc_segment_count    = 22;
      config.tex_null             = d3d11.tex_null;

      { /* setup buffers to load vertices and elements */
        struct nk_buffer vbuf, ibuf;

        nk_buffer_init_fixed(&vbuf, vertices.pData, (size_t)d3d11.max_vertex_buffer);
        nk_buffer_init_fixed(&ibuf, indices.pData, (size_t)d3d11.max_index_buffer);
        PROF_SCOPE_BEGIN("nk_convert", convert);
        nk_convert(&d3d11.ctx, &d3d11.cmds, &vbuf, &ibuf, &config);
        PROF_SCOPE_END(convert);
      }
    }

    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)d3d11.vertex_buffer, 0);
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)d3d11.index_buffer, 0);
    nk_d3d11_upload_atlas(context);

    /* iterate over and execute each draw command */
    PROF_SCOPE_BEGIN("d3d11.draw", draw);
    nk_draw_foreach(cmd, &d3d11.ctx, &d3d11.cmds)
    {
      D3D11_RECT                scissor;
      ID3D11ShaderResourceView *texture_view = (ID3D11ShaderResourceView *)cmd->texture.ptr;
      if (!cmd->elem_count) {
        continue;
      }

      scissor.left   = (LONG)(cmd->clip_rect.x);
      scissor.right  = (LONG)(cmd->clip_rect.x + cmd->clip_rect.w);
      scissor.top    = (LONG)(cmd->clip_rect.y);
      scissor.bottom = (LONG)(cmd->clip_rect.y + cmd->clip_rect.h);

      ID3D11DeviceContext_PSSetShaderResources(context, 0, 1, &texture_view);
      ID3D11DeviceContext_RSSetScissorRects(context, 1, &scissor);
      ID3D11DeviceContext_DrawIndexed(context, (UINT)cmd->elem_count, offset, 0);
      offset += cmd->elem_count;
    }
    PROF_SCOPE_END(draw);
    nk_clear(&d3d11.ctx);
    nk_buffer_clear(&d3d11.cmds);
  }
}

static void
nk_d3d11_get_projection_matrix(int width, int height, float *result)
{
  const float L = 0.0f;
  const float R = (float)width;
  const float T = 0.0f;
  const float B = (float)height;
  float       matrix[4][4] = {
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.5f, 0.0f},
    {0.0f, 0.0f, 0.5f, 1.0f},
  };

  matrix[0][0] = 2.0f / (R - L);
  matrix[1][1] = 2.0f / (T - B);
  matrix[3][0] = (R + L) / (L - R);
  matrix[3][1] = (T + B) / (B - T);
  memcpy(result, matrix, sizeof(matrix));
}

NK_API void
nk_d3d11_resize(ID3D11DeviceContext *context, int width, int height)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  if (SUCCEEDED(ID3D11DeviceContext_Map(
        context, (ID3D11Resource *)d3d11.const_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
    nk_d3d11_get_projection_matrix(width, height, (float *)mapped.pData);
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)d3d11.const_buffer, 0);

    d3d11.viewport.Width  = (float)width;
    d3d11.viewport.Height = (float)height;
  }
}

NK_API int
nk_d3d11_handle_event(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  static int insert_toggle = 0;

  switch (msg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
      int down = !((lparam >> 31) & 1);
      int ctrl = GetKeyState(VK_CONTROL) & (1 << 15);

      switch (wparam) {
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
          nk_input_key(&d3d11.ctx, NK_KEY_SHIFT, down);
          return 1;

        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
          nk_input_key(&d3d11.ctx, NK_KEY_CTRL, down);

          if (!down) {
            int k;

            for (k = NK_KEY_NAV_SCROLL_UP; k <= NK_KEY_NAV_SCROLL_RIGHT; ++k) {
              nk_input_key(&d3d11.ctx, (enum nk_keys)k, 0);
            }
          }
          return 1;

        case VK_DELETE:
          nk_input_key(&d3d11.ctx, NK_KEY_DEL, down);
          return 1;

        case VK_RETURN:
        case VK_SEPARATOR:
          nk_input_key(&d3d11.ctx, NK_KEY_ENTER, down);
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_ACTIVATE, down);
          return 1;

        case VK_APPS:
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_MENU, down);
          return 1;

        case VK_F6: {
          int shift = GetKeyState(VK_SHIFT) & (1 << 15);

          nk_input_key(&d3d11.ctx, NK_KEY_NAV_WINDOW_NEXT, down && !shift);
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_WINDOW_PREV, down && shift);
          return 1;
        }

        case VK_TAB: {
          /* not with Ctrl/Alt: Ctrl+Tab stays free */
          int shift = GetKeyState(VK_SHIFT) & (1 << 15);
          int plain = !ctrl && !(GetKeyState(VK_MENU) & (1 << 15));

          nk_input_key(&d3d11.ctx, NK_KEY_TAB, down);
          nk_input_key(&d3d11.ctx, NK_KEY_FOCUS_NEXT, down && plain && !shift);
          nk_input_key(&d3d11.ctx, NK_KEY_FOCUS_PREV, down && plain && shift);
          return 1;
        }

        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT: {
          static const enum nk_keys text[4] = {NK_KEY_UP, NK_KEY_DOWN, NK_KEY_LEFT, NK_KEY_RIGHT};
          int dir                           = wparam == VK_UP ? 0 : wparam == VK_DOWN ? 1 : wparam == VK_LEFT ? 2 : 3;
          int alt                           = GetKeyState(VK_MENU) & (1 << 15);
          int shift                         = GetKeyState(VK_SHIFT) & (1 << 15);
          int plain                         = !ctrl && !alt;

          nk_input_key(&d3d11.ctx, (ctrl && dir >= 2) ? (dir == 2 ? NK_KEY_TEXT_WORD_LEFT : NK_KEY_TEXT_WORD_RIGHT) : text[dir], down && !alt);
          nk_input_key(&d3d11.ctx, (enum nk_keys)(NK_KEY_NAV_UP + dir), down && (plain || (alt && shift) || (ctrl && shift && !alt)));
          nk_input_key(&d3d11.ctx, (enum nk_keys)(NK_KEY_NAV_SCROLL_UP + dir), down && ctrl && !alt && !shift);
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_WINDOW_MOVE, down && ctrl && shift && !alt);
          nk_input_key(&d3d11.ctx, (enum nk_keys)(NK_KEY_NAV_WINDOW_UP + dir), down && alt && !shift);
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_WINDOW_SIZE, down && alt && shift);
          return 1;
        }

        case VK_F2:
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_KEYBOARD, down);
          return 1;

        case 'W':
          /* Alt+W: window menu */
          if ((GetKeyState(VK_MENU) & (1 << 15)) || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_NAV_WINDOW_MENU, down);

            if (down) {
              return 1;
            }
          }
          break;

        case 'M':
          /* Alt+M: maximize */
          if ((GetKeyState(VK_MENU) & (1 << 15)) || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_NAV_WINDOW_MAXIMIZE, down);

            if (down) {
              return 1;
            }
          }
          break;

        case VK_BACK:
          nk_input_key(&d3d11.ctx, NK_KEY_BACKSPACE, down);
          return 1;

        case VK_HOME:
          nk_input_key(&d3d11.ctx, NK_KEY_TEXT_START, down);
          nk_input_key(&d3d11.ctx, NK_KEY_SCROLL_START, down);
          return 1;

        case VK_END:
          nk_input_key(&d3d11.ctx, NK_KEY_TEXT_END, down);
          nk_input_key(&d3d11.ctx, NK_KEY_SCROLL_END, down);
          return 1;

        case VK_NEXT:
          nk_input_key(&d3d11.ctx, NK_KEY_SCROLL_DOWN, down);
          return 1;

        case VK_PRIOR:
          nk_input_key(&d3d11.ctx, NK_KEY_SCROLL_UP, down);
          return 1;

        case VK_ESCAPE:
          nk_input_key(&d3d11.ctx, NK_KEY_TEXT_RESET_MODE, down);
          nk_input_key(&d3d11.ctx, NK_KEY_NAV_CANCEL, down);
          return 1;

        case VK_INSERT:
          if (down && !((lparam >> 30) & 1)) {
            insert_toggle = !insert_toggle;
          }

          nk_input_key(&d3d11.ctx, insert_toggle ? NK_KEY_TEXT_REPLACE_MODE : NK_KEY_TEXT_INSERT_MODE, down);
          return 1;

        case 'A':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_TEXT_SELECT_ALL, down);
            return 1;
          }
          break;

        case 'B':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_TEXT_LINE_START, down);
            return 1;
          }
          break;

        case 'E':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_TEXT_LINE_END, down);
            return 1;
          }
          break;

        case 'C':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_COPY, down);
            return 1;
          }
          break;

        case 'V':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_PASTE, down);
            return 1;
          }
          break;

        case 'X':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_CUT, down);
            return 1;
          }
          break;

        case 'Z':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_TEXT_UNDO, down);
            return 1;
          }
          break;

        case 'R':
          if (ctrl || !down) {
            nk_input_key(&d3d11.ctx, NK_KEY_TEXT_REDO, down);
            return 1;
          }
          break;
      }
      return 0;
    }

    case WM_CHAR:
      if (wparam >= 32) {
        nk_input_unicode(&d3d11.ctx, (nk_rune)wparam);
        return 1;
      }
      break;

    case WM_LBUTTONDOWN:
      nk_input_button(&d3d11.ctx, NK_BUTTON_LEFT, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return 1;

    case WM_LBUTTONUP:
      nk_input_button(&d3d11.ctx, NK_BUTTON_DOUBLE, (short)LOWORD(lparam), (short)HIWORD(lparam), 0);
      nk_input_button(&d3d11.ctx, NK_BUTTON_LEFT, (short)LOWORD(lparam), (short)HIWORD(lparam), 0);
      ReleaseCapture();
      return 1;

    case WM_RBUTTONDOWN:
      nk_input_button(&d3d11.ctx, NK_BUTTON_RIGHT, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return 1;

    case WM_RBUTTONUP:
      nk_input_button(&d3d11.ctx, NK_BUTTON_RIGHT, (short)LOWORD(lparam), (short)HIWORD(lparam), 0);
      ReleaseCapture();
      return 1;

    case WM_MBUTTONDOWN:
      nk_input_button(&d3d11.ctx, NK_BUTTON_MIDDLE, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return 1;

    case WM_MBUTTONUP:
      nk_input_button(&d3d11.ctx, NK_BUTTON_MIDDLE, (short)LOWORD(lparam), (short)HIWORD(lparam), 0);
      ReleaseCapture();
      return 1;

    case WM_XBUTTONDOWN:
      switch (GET_XBUTTON_WPARAM(wparam)) {
        case XBUTTON1:
          nk_input_button(&d3d11.ctx, NK_BUTTON_X1, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
          break;

        case XBUTTON2:
          nk_input_button(&d3d11.ctx, NK_BUTTON_X2, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
          break;
      }

      SetCapture(wnd);
      return 1;

    case WM_XBUTTONUP:
      switch (GET_XBUTTON_WPARAM(wparam)) {
        case XBUTTON1:
          nk_input_button(&d3d11.ctx, NK_BUTTON_X1, (short)LOWORD(lparam), (short)HIWORD(lparam), 0);
          break;

        case XBUTTON2:
          nk_input_button(&d3d11.ctx, NK_BUTTON_X2, (short)LOWORD(lparam), (short)HIWORD(lparam), 0);
          break;
      }

      ReleaseCapture();
      return 1;

    case WM_MOUSEWHEEL:
      nk_input_scroll(&d3d11.ctx, nk_vec2(0, (float)(short)HIWORD(wparam) / WHEEL_DELTA));
      return 1;

    case WM_MOUSEMOVE:
      nk_input_motion(&d3d11.ctx, (short)LOWORD(lparam), (short)HIWORD(lparam));
      return 1;

    case WM_LBUTTONDBLCLK:
      /* with CS_DBLCLKS the second press arrives only as this message */
      nk_input_button(&d3d11.ctx, NK_BUTTON_DOUBLE, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
      nk_input_button(&d3d11.ctx, NK_BUTTON_LEFT, (short)LOWORD(lparam), (short)HIWORD(lparam), 1);
      SetCapture(wnd);
      return 1;
  }

  return 0;
}

static struct nk_context *
nk_d3d11_init_internal(ID3D11Device *device,
                       int           width,
                       int           height,
                       unsigned int  max_vertex_buffer,
                       unsigned int  max_index_buffer,
                       void         *memory,
                       nk_size       memory_size,
                       nk_size       text_cache_size)
{
  HRESULT hr;

  d3d11.max_vertex_buffer = max_vertex_buffer;
  d3d11.max_index_buffer  = max_index_buffer;
  d3d11.device            = device;
  ID3D11Device_AddRef(device);

  if (memory) {
    if (!nk_init_fixed(&d3d11.ctx, memory, memory_size, 0) || !nk_text_memory_init_fixed(&d3d11.ctx, text_cache_size)) {
      return NULL;
    }
  } else {
    nk_init_default(&d3d11.ctx, 0);
    d3d11.text_memory = malloc(NK_D3D11_TEXT_CACHE_SIZE);

    if (!d3d11.text_memory || !nk_text_memory_init(&d3d11.ctx, d3d11.text_memory, NK_D3D11_TEXT_CACHE_SIZE)) {
      return NULL;
    }
  }

  nk_buffer_init_default(&d3d11.cmds);

  { /* rasterizer state */
    D3D11_RASTERIZER_DESC desc;

    memset(&desc, 0, sizeof(desc));
    desc.FillMode              = D3D11_FILL_SOLID;
    desc.CullMode              = D3D11_CULL_NONE;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias             = 0;
    desc.DepthBiasClamp        = 0;
    desc.SlopeScaledDepthBias  = 0.0f;
    desc.DepthClipEnable       = TRUE;
    desc.ScissorEnable         = TRUE;
    desc.MultisampleEnable     = FALSE;
    desc.AntialiasedLineEnable = FALSE;
    hr                         = ID3D11Device_CreateRasterizerState(device, &desc, &d3d11.rasterizer_state);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* vertex shader */
  {
    hr = ID3D11Device_CreateVertexShader(
      device, nk_d3d11_vertex_shader, sizeof(nk_d3d11_vertex_shader), NULL, &d3d11.vertex_shader);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* input layout */
  {
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION",
       0,
       DXGI_FORMAT_R32G32B32A32_FLOAT,
       0,
       offsetof(struct nk_d3d11_vertex, position),
       D3D11_INPUT_PER_VERTEX_DATA,
       0},
      {"TEXCOORD",
       0,
       DXGI_FORMAT_R32G32_FLOAT,
       0,
       offsetof(struct nk_d3d11_vertex, uv),
       D3D11_INPUT_PER_VERTEX_DATA,
       0},
      {"COLOR",
       0,
       DXGI_FORMAT_R8G8B8A8_UNORM,
       0,
       offsetof(struct nk_d3d11_vertex, col),
       D3D11_INPUT_PER_VERTEX_DATA,
       0},
    };

    hr = ID3D11Device_CreateInputLayout(
      device, layout, ARRAYSIZE(layout), nk_d3d11_vertex_shader, sizeof(nk_d3d11_vertex_shader), &d3d11.input_layout);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* constant buffer */
  {
    float             matrix[4 * 4];
    D3D11_BUFFER_DESC desc;

    memset(&desc, 0, sizeof(desc));
    desc.ByteWidth      = sizeof(matrix);
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags      = 0;

    {
      D3D11_SUBRESOURCE_DATA data;

      data.pSysMem          = matrix;
      data.SysMemPitch      = 0;
      data.SysMemSlicePitch = 0;

      nk_d3d11_get_projection_matrix(width, height, matrix);
      hr = ID3D11Device_CreateBuffer(device, &desc, &data, &d3d11.const_buffer);
      NK_ASSERT(SUCCEEDED(hr));
    }
  }

  /* pixel shader */
  {
    hr = ID3D11Device_CreatePixelShader(
      device, nk_d3d11_pixel_shader, sizeof(nk_d3d11_pixel_shader), NULL, &d3d11.pixel_shader);
    NK_ASSERT(SUCCEEDED(hr));
  }

  { /* blend state */
    D3D11_BLEND_DESC desc;

    memset(&desc, 0, sizeof(desc));
    desc.AlphaToCoverageEnable                 = FALSE;
    desc.RenderTarget[0].BlendEnable           = TRUE;
    desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr                                         = ID3D11Device_CreateBlendState(device, &desc, &d3d11.blend_state);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* vertex buffer */
  {
    D3D11_BUFFER_DESC desc;

    memset(&desc, 0, sizeof(desc));
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth      = max_vertex_buffer;
    desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags      = 0;
    hr                  = ID3D11Device_CreateBuffer(device, &desc, NULL, &d3d11.vertex_buffer);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* index buffer */
  {
    D3D11_BUFFER_DESC desc;

    memset(&desc, 0, sizeof(desc));
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth      = max_index_buffer;
    desc.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr                  = ID3D11Device_CreateBuffer(device, &desc, NULL, &d3d11.index_buffer);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* sampler state */
  {
    D3D11_SAMPLER_DESC desc;

    memset(&desc, 0, sizeof(desc));
    desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MipLODBias     = 0.0f;
    desc.MaxAnisotropy  = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MinLOD         = 0.0f;
    desc.MaxLOD         = D3D11_FLOAT32_MAX;
    hr                  = ID3D11Device_CreateSamplerState(device, &desc, &d3d11.sampler_state);
    NK_ASSERT(SUCCEEDED(hr));
  }

  /* viewport */
  {
    d3d11.viewport.TopLeftX = 0.0f;
    d3d11.viewport.TopLeftY = 0.0f;
    d3d11.viewport.Width    = (float)width;
    d3d11.viewport.Height   = (float)height;
    d3d11.viewport.MinDepth = 0.0f;
    d3d11.viewport.MaxDepth = 1.0f;
  }
  return &d3d11.ctx;
}

NK_API struct nk_context *
nk_d3d11_init(
  ID3D11Device *device, int width, int height, unsigned int max_vertex_buffer, unsigned int max_index_buffer)
{
  return nk_d3d11_init_internal(device, width, height, max_vertex_buffer, max_index_buffer, NULL, 0, 0);
}

NK_API struct nk_context *
nk_d3d11_init_fixed(ID3D11Device *device,
                    int           width,
                    int           height,
                    unsigned int  max_vertex_buffer,
                    unsigned int  max_index_buffer,
                    void         *memory,
                    nk_size       memory_size,
                    nk_size       text_cache_size)
{
  return nk_d3d11_init_internal(device, width, height, max_vertex_buffer, max_index_buffer, memory, memory_size, text_cache_size);
}

NK_API nk_bool
nk_d3d11_setup_atlas(struct nk_glyph_atlas *atlas)
{
  D3D11_TEXTURE2D_DESC            desc;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv;
  HRESULT                         hr;

  memset(&desc, 0, sizeof(desc));
  desc.Width              = (UINT)atlas->width;
  desc.Height             = (UINT)atlas->height;
  desc.MipLevels          = 1;
  desc.ArraySize          = 1;
  desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count   = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage              = D3D11_USAGE_DEFAULT;
  desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags     = 0;

  hr = ID3D11Device_CreateTexture2D(d3d11.device, &desc, NULL, &d3d11.font_texture);

  if (FAILED(hr)) {
    return nk_false;
  }

  memset(&srv, 0, sizeof(srv));
  srv.Format                    = desc.Format;
  srv.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
  srv.Texture2D.MipLevels       = 1;
  srv.Texture2D.MostDetailedMip = 0;
  hr                            = ID3D11Device_CreateShaderResourceView(d3d11.device, (ID3D11Resource *)d3d11.font_texture, &srv, &d3d11.font_texture_view);

  if (FAILED(hr)) {
    ID3D11Texture2D_Release(d3d11.font_texture);
    d3d11.font_texture = NULL;
    return nk_false;
  }

  d3d11.atlas = atlas;
  nk_glyph_atlas_set_texture(atlas, nk_handle_ptr(d3d11.font_texture_view));
  d3d11.tex_null = atlas->null;
  return nk_true;
}

#define NK_D3D11_STICK_THRESHOLD 16000

NK_API nk_bool
nk_d3d11_poll_gamepad(void)
{
  nk_bool   down[NK_KEY_NAV_WINDOW_MENU - NK_KEY_NAV_UP + 1];
  nk_bool   connected = nk_false;
  ULONGLONG now       = GetTickCount64();
  nk_bool   retry     = now >= d3d11.pad_retry;
  DWORD     i;

  if (!d3d11.xinput_tried) {
    d3d11.xinput_tried = nk_true;
    d3d11.xinput       = LoadLibraryA("xinput1_4.dll");

    if (!d3d11.xinput) {
      d3d11.xinput = LoadLibraryA("xinput9_1_0.dll");
    }

    if (d3d11.xinput) {
      d3d11.xinput_get_state = (DWORD(WINAPI *)(DWORD, XINPUT_STATE *))(void (*)(void))GetProcAddress(d3d11.xinput, "XInputGetState");
    }
  }

  if (!d3d11.xinput_get_state) {
    return nk_false;
  }

  if (retry) {
    d3d11.pad_retry = now + 1000;
  }

  memset(down, 0, sizeof(down));

  for (i = 0; i < XUSER_MAX_COUNT; ++i) {
    XINPUT_STATE state;
    WORD         b;

    if (!d3d11.pad_connected[i] && !retry) {
      continue;
    }

    d3d11.pad_connected[i] = d3d11.xinput_get_state(i, &state) == ERROR_SUCCESS;

    if (!d3d11.pad_connected[i]) {
      continue;
    }

    connected                                      = nk_true;
    b                                              = state.Gamepad.wButtons;
    down[NK_KEY_NAV_UP - NK_KEY_NAV_UP]           |= (b & XINPUT_GAMEPAD_DPAD_UP) || state.Gamepad.sThumbLY > NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_DOWN - NK_KEY_NAV_UP]         |= (b & XINPUT_GAMEPAD_DPAD_DOWN) || state.Gamepad.sThumbLY < -NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_LEFT - NK_KEY_NAV_UP]         |= (b & XINPUT_GAMEPAD_DPAD_LEFT) || state.Gamepad.sThumbLX < -NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_RIGHT - NK_KEY_NAV_UP]        |= (b & XINPUT_GAMEPAD_DPAD_RIGHT) || state.Gamepad.sThumbLX > NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_ACTIVATE - NK_KEY_NAV_UP]     |= (b & XINPUT_GAMEPAD_A) != 0;
    down[NK_KEY_NAV_CANCEL - NK_KEY_NAV_UP]       |= (b & XINPUT_GAMEPAD_B) != 0;
    down[NK_KEY_NAV_MENU - NK_KEY_NAV_UP]         |= (b & XINPUT_GAMEPAD_X) != 0;
    down[NK_KEY_NAV_WINDOW_PREV - NK_KEY_NAV_UP]  |= (b & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    down[NK_KEY_NAV_WINDOW_NEXT - NK_KEY_NAV_UP]  |= (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
    down[NK_KEY_NAV_WORD_PREV - NK_KEY_NAV_UP]    |= state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    down[NK_KEY_NAV_WORD_NEXT - NK_KEY_NAV_UP]    |= state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    down[NK_KEY_NAV_SCROLL_UP - NK_KEY_NAV_UP]    |= state.Gamepad.sThumbRY > NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_SCROLL_DOWN - NK_KEY_NAV_UP]  |= state.Gamepad.sThumbRY < -NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_SCROLL_LEFT - NK_KEY_NAV_UP]  |= state.Gamepad.sThumbRX < -NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_SCROLL_RIGHT - NK_KEY_NAV_UP] |= state.Gamepad.sThumbRX > NK_D3D11_STICK_THRESHOLD;
    down[NK_KEY_NAV_KEYBOARD - NK_KEY_NAV_UP]     |= (b & XINPUT_GAMEPAD_Y) != 0;
    /* Menu: window menu; View and the stick clicks stay free */
    down[NK_KEY_NAV_WINDOW_MENU - NK_KEY_NAV_UP] |= (b & XINPUT_GAMEPAD_START) != 0;
  }

  for (i = 0; i < NK_LEN(down); ++i) {
    if (down[i] != d3d11.pad_down[i]) {
      d3d11.pad_down[i] = down[i];
      nk_input_key(&d3d11.ctx, (enum nk_keys)(NK_KEY_NAV_UP + (int)i), down[i]);
    }
  }
  return connected;
}

NK_API
void
nk_d3d11_shutdown(void)
{
  nk_buffer_free(&d3d11.cmds);
  nk_free(&d3d11.ctx);
  free(d3d11.text_memory);
  d3d11.text_memory = NULL;

  ID3D11SamplerState_Release(d3d11.sampler_state);

  if (d3d11.font_texture_view) {
    ID3D11ShaderResourceView_Release(d3d11.font_texture_view);
  }

  if (d3d11.font_texture) {
    ID3D11Texture2D_Release(d3d11.font_texture);
  }

  ID3D11Buffer_Release(d3d11.vertex_buffer);
  ID3D11Buffer_Release(d3d11.index_buffer);
  ID3D11BlendState_Release(d3d11.blend_state);
  ID3D11PixelShader_Release(d3d11.pixel_shader);
  ID3D11Buffer_Release(d3d11.const_buffer);
  ID3D11VertexShader_Release(d3d11.vertex_shader);
  ID3D11InputLayout_Release(d3d11.input_layout);
  ID3D11RasterizerState_Release(d3d11.rasterizer_state);
  ID3D11Device_Release(d3d11.device);

  if (d3d11.xinput) {
    FreeLibrary(d3d11.xinput);
  }

  d3d11.xinput           = NULL;
  d3d11.xinput_get_state = NULL;
  d3d11.xinput_tried     = nk_false;
}
