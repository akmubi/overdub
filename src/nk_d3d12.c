#include "nk_d3d12.h"
#include "globals.h"
#include "log.h"

#include "vendor_minhook.h"

#define INITGUID
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <initguid.h>

#define DXGI_SWAPCHAIN_PRESENT_VTIDX       8
#define DXGI_SWAPCHAIN_RESIZEBUFFERS_VTIDX 13

typedef HRESULT(STDMETHODCALLTYPE *pfn_present_t)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *pfn_resize_buffers_t)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);

static HRESULT STDMETHODCALLTYPE
present_hook(IDXGISwapChain *self, UINT sync_interval, UINT flags);
static HRESULT STDMETHODCALLTYPE
resize_buffers_hook(IDXGISwapChain *self, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags);

static pfn_present_t        g_present_real        = NULL;
static pfn_resize_buffers_t g_resize_buffers_real = NULL;

/* pre-compiled shader blobs */
#include "nk_d3d12_ps.h"
#include "nk_d3d12_vs.h"

/* font */
#include "nk_prebaked_font.h"

typedef struct nk_d3d12_frame_s nk_d3d12_frame_t;
struct nk_d3d12_frame_s {
  ID3D12CommandAllocator     *cmd_alloc;
  ID3D12Resource             *back_buffer; // ref from swap chain
  D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
  uint64_t                    fence_value;
};

/* global state */
static struct {
  void *cmd_buf;
  void *nk_mem;

  /* D3D12 objects */
  ID3D12Device              *device;
  ID3D12CommandQueue        *cmd_queue;
  IDXGISwapChain3           *swap_chain;
  ID3D12GraphicsCommandList *cmd_list;
  ID3D12RootSignature       *root_sig;
  ID3D12PipelineState       *pso;
  ID3D12Fence               *fence;
  HANDLE                     fence_event;
  uint64_t                   fence_counter;

  /* descriptor heaps */
  ID3D12DescriptorHeap *rtv_heap; // render target views
  ID3D12DescriptorHeap *srv_heap; // shader resource views (font atlas)
  UINT                  rtv_increment;

  /* per-frame resources */
  nk_d3d12_frame_t frames[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];
  UINT             num_back_buffers;

  /* upload buffers (vertex + index) */
  ID3D12Resource *vb_upload[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];
  ID3D12Resource *ib_upload[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];
  void           *vb_mapped[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];
  void           *ib_mapped[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];

  /* constant buffer (projection matrix) - one per back buffer */
  ID3D12Resource *cb_upload[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];
  void           *cb_mapped[CONFIG_NK_D3D12_MAX_BACK_BUFFERS];

  /* font atlas texture */
  ID3D12Resource *font_texture;

  /* nuklear */
  struct nk_context nk_ctx;
  struct nk_buffer  nk_cmds; // command buffer for nk_convert
  struct nk_font    font_body_obj;
  struct nk_font    font_title_obj;
  struct nk_font   *font_body;
  struct nk_font   *font_title;

  /* double-buffered snapshots */
  nk_d3d12_snapshot_t snap[2];
  volatile LONG       snap_read_idx;  // render thread reads this
  int                 snap_write_idx; // game thread's private write slot

  struct nk_draw_null_texture null_tex;

  unsigned int width;
  unsigned int height;
  bool         inited;
} g_nk = {0};

static void
nk_d3d12_wait_for_fence(uint64_t value)
{
  if (ID3D12Fence_GetCompletedValue(g_nk.fence) < value) {
    ID3D12Fence_SetEventOnCompletion(g_nk.fence, value, g_nk.fence_event);
    WaitForSingleObject(g_nk.fence_event, INFINITE);
  }
}

static void
nk_d3d12_wait_idle(void)
{
  g_nk.fence_counter += 1;
  ID3D12CommandQueue_Signal(g_nk.cmd_queue, g_nk.fence, g_nk.fence_counter);
  nk_d3d12_wait_for_fence(g_nk.fence_counter);
}

static HRESULT
nk_d3d12_create_upload_buffer(ID3D12Resource **out, void **mapped, uint64_t size)
{
  D3D12_HEAP_PROPERTIES heap_props = {
    .Type = D3D12_HEAP_TYPE_UPLOAD,
  };

  D3D12_RESOURCE_DESC desc = {
    .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Width            = size,
    .Height           = 1,
    .DepthOrArraySize = 1,
    .MipLevels        = 1,
    .SampleDesc.Count = 1,
    .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
  };

  HRESULT hr = ID3D12Device_CreateCommittedResource(g_nk.device,
                                                    &heap_props,
                                                    D3D12_HEAP_FLAG_NONE,
                                                    &desc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    NULL,
                                                    &IID_ID3D12Resource,
                                                    (void **)out);

  if (SUCCEEDED(hr) && mapped) {
    D3D12_RANGE range = {0};
    hr                = ID3D12Resource_Map(*out, 0, &range, mapped);
  }

  return hr;
}

static bool
nk_d3d12_create_root_sig(void)
{
  /*
   * root signature layout:
   *   [0] CBV              - inline root CBV for the projection matrix (register b0)
   *   [1] descriptor table - 1 SRV for font atlas (register t0)
   *   static sampler       - bilinear wrap (register s0)
   */

  D3D12_DESCRIPTOR_RANGE srv_range = {
    .RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
    .NumDescriptors     = 1,
    .BaseShaderRegister = 0, // t0
  };

  D3D12_ROOT_PARAMETER params[2] = {
    /* param 0: inline CBV (constant buffer view, no descriptor needed) */
    [0] =
      {
        .ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV,
        .Descriptor.ShaderRegister = 0, // b0
        .ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX,
      },
    /* param 1: descriptor table with 1 SRV */
    [1] =
      {
        .ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable.NumDescriptorRanges = 1,
        .DescriptorTable.pDescriptorRanges   = &srv_range,
        .ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL,
      },
  };

  D3D12_STATIC_SAMPLER_DESC sampler = {
    .Filter           = D3D12_FILTER_MIN_MAG_MIP_POINT, // D3D12_FILTER_MIN_MAG_MIP_LINEAR,
    .AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .MaxLOD           = D3D12_FLOAT32_MAX,
    .ShaderRegister   = 0, // s0
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
  };

  D3D12_ROOT_SIGNATURE_DESC root_desc = {
    .NumParameters     = COUNTOF(params),
    .pParameters       = params,
    .NumStaticSamplers = 1,
    .pStaticSamplers   = &sampler,
    .Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                         D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                         D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                         D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS,
  };

  ID3DBlob *sig_blob   = NULL;
  ID3DBlob *error_blob = NULL;
  HRESULT   hr         = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
  if (FAILED(hr)) {
    if (error_blob) {
      ID3D10Blob_Release(error_blob);
    }
    return false;
  }

  hr = ID3D12Device_CreateRootSignature(g_nk.device,
                                        0,
                                        ID3D10Blob_GetBufferPointer(sig_blob),
                                        ID3D10Blob_GetBufferSize(sig_blob),
                                        &IID_ID3D12RootSignature,
                                        (void **)&g_nk.root_sig);

  ID3D10Blob_Release(sig_blob);
  if (error_blob) {
    ID3D10Blob_Release(error_blob);
  }
  return SUCCEEDED(hr);
}

static bool
nk_d3d12_create_pso(DXGI_FORMAT rtv_format)
{
  D3D12_INPUT_ELEMENT_DESC input_layout[] = {
    {
      .SemanticName      = "POSITION",
      .Format            = DXGI_FORMAT_R32G32_FLOAT,
      .AlignedByteOffset = offsetof(nk_d3d12_vertex_t, pos),
      .InputSlotClass    = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    },
    {
      .SemanticName      = "TEXCOORD",
      .Format            = DXGI_FORMAT_R32G32_FLOAT,
      .AlignedByteOffset = offsetof(nk_d3d12_vertex_t, uv),
      .InputSlotClass    = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    },
    {
      .SemanticName      = "COLOR",
      .Format            = DXGI_FORMAT_R8G8B8A8_UNORM,
      .AlignedByteOffset = offsetof(nk_d3d12_vertex_t, col),
      .InputSlotClass    = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
    },
  };

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
    .pRootSignature = g_nk.root_sig,
    .VS =
      {
        .pShaderBytecode = nk_d3d12_vs_data,
        .BytecodeLength  = sizeof(nk_d3d12_vs_data),
      },
    .PS =
      {
        .pShaderBytecode = nk_d3d12_ps_data,
        .BytecodeLength  = sizeof(nk_d3d12_ps_data),
      },
    .InputLayout =
      {
        .pInputElementDescs = input_layout,
        .NumElements        = COUNTOF(input_layout),
      },
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets      = 1,
    .RTVFormats            = {rtv_format},
    .SampleDesc.Count      = 1,
    .SampleMask            = UINT_MAX,
    .RasterizerState =
      {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_NONE, // UI: no culling
      },
    .DepthStencilState =
      {
        .DepthEnable = FALSE,
      },
    .BlendState.RenderTarget[0] =
      {
        .BlendEnable           = TRUE,
        .SrcBlend              = D3D12_BLEND_SRC_ALPHA,
        .DestBlend             = D3D12_BLEND_INV_SRC_ALPHA,
        .BlendOp               = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha         = D3D12_BLEND_ONE,
        .DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA,
        .BlendOpAlpha          = D3D12_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
      },
  };

  HRESULT hr =
    ID3D12Device_CreateGraphicsPipelineState(g_nk.device, &pso_desc, &IID_ID3D12PipelineState, (void **)&g_nk.pso);
  return SUCCEEDED(hr);
}

static const struct nk_font_glyph *
nk_d3d12_find_glyph(const struct nk_font *font, nk_rune codepoint)
{
  uint32_t lo = 0;
  uint32_t hi = font->info.glyph_count;

  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    nk_rune  cp  = font->glyphs[mid].codepoint;

    if (cp < codepoint) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  if (lo < font->info.glyph_count && font->glyphs[lo].codepoint == codepoint) {
    return &font->glyphs[lo];
  }

  return font->fallback;
}

static float
nk_d3d12_font_width(nk_handle handle, float height, const char *text, int len)
{
  struct nk_font *font = handle.ptr;
  if (!font || !text || len <= 0) {
    return 0.0f;
  }

  float scale = height / font->info.height;
  float width = 0.0f;

  int off = 0;
  while (off < len) {
    nk_rune cp        = 0;
    int     glyph_len = nk_utf_decode(text + off, &cp, len - off);
    if (glyph_len <= 0 || cp == NK_UTF_INVALID) {
      break;
    }

    const struct nk_font_glyph *g = nk_d3d12_find_glyph(font, cp);

    width += g->xadvance * scale;
    off   += glyph_len;
  }

  return width;
}

static void
nk_d3d12_font_query(
  nk_handle handle, float height, struct nk_user_font_glyph *out, nk_rune codepoint, nk_rune next_codepoint)
{
  (void)next_codepoint;

  struct nk_font *font = handle.ptr;
  if (!font || !out) {
    return;
  }

  const struct nk_font_glyph *g     = nk_d3d12_find_glyph(font, codepoint);
  float                       scale = height / font->info.height;

  out->width    = (g->x1 - g->x0) * scale;
  out->height   = (g->y1 - g->y0) * scale;
  out->offset   = nk_vec2(g->x0 * scale, g->y0 * scale);
  out->xadvance = g->xadvance * scale;
  out->uv[0]    = nk_vec2(g->u0, g->v0);
  out->uv[1]    = nk_vec2(g->u1, g->v1);
}

static void
nk_d3d12_setup_prebaked_font(struct nk_font             *font,
                             const nk_baked_font_desc_t *desc,
                             struct nk_font_glyph       *glyphs,
                             nk_handle                   texture)
{
  mem_zero(font, sizeof(*font));

  font->handle.userdata.ptr = font;
  font->handle.height       = desc->height;
  font->handle.width        = nk_d3d12_font_width;
  font->handle.query        = nk_d3d12_font_query;
  font->handle.texture      = texture;

  font->info.height      = desc->height;
  font->info.ascent      = desc->ascent;
  font->info.descent     = desc->descent;
  font->info.glyph_count = desc->glyph_count;

  font->glyphs   = glyphs;
  font->fallback = &glyphs[desc->fallback_glyph_idx];
  font->texture  = texture;
}

static bool
nk_d3d12_upload_prebaked_font_atlas(void)
{
  int atlas_w = (int)g_nk_font_body_desc.tex_w;
  int atlas_h = (int)g_nk_font_body_desc.tex_h;

  const uint8_t *rgba_data = g_nk_font_atlas_rgba;
  uint64_t       row_pitch = (uint64_t)atlas_w * 4;

  ASSERT(g_nk_font_atlas_rgba_size == (uint32_t)(atlas_w * atlas_h * 4));

  D3D12_HEAP_PROPERTIES default_heap = {
    .Type = D3D12_HEAP_TYPE_DEFAULT,
  };

  D3D12_RESOURCE_DESC tex_desc = {
    .Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    .Width            = (uint64_t)atlas_w,
    .Height           = (UINT)atlas_h,
    .DepthOrArraySize = 1,
    .MipLevels        = 1,
    .Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
    .SampleDesc.Count = 1,
    .Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN,
  };

  HRESULT hr = ID3D12Device_CreateCommittedResource(g_nk.device,
                                                    &default_heap,
                                                    D3D12_HEAP_FLAG_NONE,
                                                    &tex_desc,
                                                    D3D12_RESOURCE_STATE_COPY_DEST,
                                                    NULL,
                                                    &IID_ID3D12Resource,
                                                    (void **)&g_nk.font_texture);
  if (FAILED(hr)) {
    return false;
  }

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint  = {0};
  UINT                               num_rows   = 0;
  UINT64                             row_bytes  = 0;
  UINT64                             total_size = 0;

  ID3D12Device_GetCopyableFootprints(g_nk.device, &tex_desc, 0, 1, 0, &footprint, &num_rows, &row_bytes, &total_size);

  ID3D12Resource *upload_buf = NULL;
  void           *upload_ptr = NULL;

  hr = nk_d3d12_create_upload_buffer(&upload_buf, &upload_ptr, total_size);
  if (FAILED(hr)) {
    LOG_ERROR("failed to create upload buffer: 0x%08lX", hr);

    if (g_nk.font_texture) {
      ID3D12Resource_Release(g_nk.font_texture);
      g_nk.font_texture = NULL;
    }

    return false;
  }

  uint64_t dst_pitch = footprint.Footprint.RowPitch;
  for (UINT y = 0; y < num_rows; ++y) {
    mem_copy((uint8_t *)upload_ptr + footprint.Offset + (uint64_t)y * dst_pitch,
             (void *)(rgba_data + (uint64_t)y * row_pitch),
             (uint64_t)row_bytes);
  }

  D3D12_TEXTURE_COPY_LOCATION dst = {
    .pResource        = g_nk.font_texture,
    .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
    .SubresourceIndex = 0,
  };

  D3D12_TEXTURE_COPY_LOCATION src = {
    .pResource       = upload_buf,
    .Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
    .PlacedFootprint = footprint,
  };

  nk_d3d12_frame_t *upload_frame = &g_nk.frames[0];

  ID3D12CommandAllocator_Reset(upload_frame->cmd_alloc);
  ID3D12GraphicsCommandList_Reset(g_nk.cmd_list, upload_frame->cmd_alloc, NULL);

  ID3D12GraphicsCommandList_CopyTextureRegion(g_nk.cmd_list, &dst, 0, 0, 0, &src, NULL);

  D3D12_RESOURCE_BARRIER barrier = {
    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    .Transition =
      {
        .pResource   = g_nk.font_texture,
        .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
        .StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      },
  };

  ID3D12GraphicsCommandList_ResourceBarrier(g_nk.cmd_list, 1, &barrier);
  ID3D12GraphicsCommandList_Close(g_nk.cmd_list);

  ID3D12CommandList *lists[] = {
    (ID3D12CommandList *)g_nk.cmd_list,
  };

  ID3D12CommandQueue_ExecuteCommandLists(g_nk.cmd_queue, 1, lists);

  g_nk.fence_counter += 1;
  ID3D12CommandQueue_Signal(g_nk.cmd_queue, g_nk.fence, g_nk.fence_counter);
  nk_d3d12_wait_for_fence(g_nk.fence_counter);

  ID3D12Resource_Unmap(upload_buf, 0, NULL);
  ID3D12Resource_Release(upload_buf);

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
    .Format                  = DXGI_FORMAT_R8G8B8A8_UNORM,
    .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D.MipLevels     = 1,
  };

  D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu;
  g_nk.srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g_nk.srv_heap, &srv_cpu);
  ID3D12Device_CreateShaderResourceView(g_nk.device, g_nk.font_texture, &srv_desc, srv_cpu);

  g_nk.null_tex.texture = nk_handle_ptr(g_nk.font_texture);
  g_nk.null_tex.uv      = nk_vec2(((float)g_nk_font_body_desc.white_x + 0.5f) / (float)atlas_w,
                                  ((float)g_nk_font_body_desc.white_y + 0.5f) / (float)atlas_h);

  return true;
}

static bool
nk_d3d12_create_rtvs(void)
{
  DXGI_SWAP_CHAIN_DESC1 sc_desc;
  IDXGISwapChain3_GetDesc1(g_nk.swap_chain, &sc_desc);
  g_nk.num_back_buffers = CLAMP_TOP(sc_desc.BufferCount, CONFIG_NK_D3D12_MAX_BACK_BUFFERS);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle;
  g_nk.rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(g_nk.rtv_heap, &rtv_handle);

  for (UINT i = 0; i < g_nk.num_back_buffers; ++i) {
    HRESULT hr =
      IDXGISwapChain3_GetBuffer(g_nk.swap_chain, i, &IID_ID3D12Resource, (void **)&g_nk.frames[i].back_buffer);

    if (FAILED(hr)) {
      return false;
    }

    ID3D12Device_CreateRenderTargetView(g_nk.device, g_nk.frames[i].back_buffer, NULL, rtv_handle);
    g_nk.frames[i].rtv_handle  = rtv_handle;
    rtv_handle.ptr            += g_nk.rtv_increment;
  }
  return true;
}

static void
nk_d3d12_release_rtvs(void)
{
  for (UINT i = 0; i < g_nk.num_back_buffers; ++i) {
    if (g_nk.frames[i].back_buffer) {
      ID3D12Resource_Release(g_nk.frames[i].back_buffer);
      g_nk.frames[i].back_buffer = NULL;
    }
  }
}

void
nk_d3d12_preinit(arena_t *perm)
{
  g_nk.cmd_buf = arena_push_aligned(perm, CONFIG_NK_CMD_BUF_SIZE, 16);
  MASSERT(g_nk.cmd_buf != NULL,
          "nuklear command buffer: failed to allocate %llu bytes (committed/reserved: %llu/%llu)",
          CONFIG_NK_CMD_BUF_SIZE,
          perm->committed,
          perm->reserved);

  g_nk.nk_mem = arena_push_aligned(perm, CONFIG_NK_INTERNAL_STATE_SIZE, 16);
  MASSERT(g_nk.nk_mem != NULL,
          "nuklear internal state: failed to allocate %llu bytes (committed/reserved: %llu/%llu)",
          CONFIG_NK_INTERNAL_STATE_SIZE,
          perm->committed,
          perm->reserved);

  for (int i = 0; i < 2; ++i) {
    nk_d3d12_snapshot_t *s = &g_nk.snap[i];

    s->vertices = ARENA_PUSH_ARRAY(perm, nk_d3d12_vertex_t, CONFIG_NK_D3D12_MAX_VERTICES);
    MASSERT(s->vertices != NULL,
            "vertices[%d]: failed to allocate %llu bytes (committed/reserved: %llu/%llu)",
            i,
            CONFIG_NK_D3D12_MAX_VERTICES * sizeof(nk_d3d12_vertex_t),
            perm->committed,
            perm->reserved);

    s->indices = ARENA_PUSH_ARRAY(perm, nk_draw_index, CONFIG_NK_D3D12_MAX_INDICES);
    MASSERT(s->indices != NULL,
            "indices[%d]: failed to allocate %llu bytes (committed/reserved: %llu/%llu)",
            i,
            CONFIG_NK_D3D12_MAX_INDICES * sizeof(nk_draw_index),
            perm->committed,
            perm->reserved);

    s->commands = ARENA_PUSH_ARRAY(perm, nk_d3d12_draw_cmd_t, CONFIG_NK_D3D12_MAX_DRAW_CMDS);
    MASSERT(s->commands != NULL,
            "commands[%d]: failed to allocate %llu bytes (committed/reserved: %llu/%llu)",
            i,
            CONFIG_NK_D3D12_MAX_DRAW_CMDS * sizeof(nk_d3d12_draw_cmd_t),
            perm->committed,
            perm->reserved);

    s->vertex_count  = 0;
    s->index_count   = 0;
    s->command_count = 0;
    s->viewport_w    = 0;
    s->viewport_h    = 0;
  }
}

static bool
nk_d3d12_init(IDXGISwapChain *swap)
{
  if (g_nk.inited) {
    return true;
  }

  if (!g_nk.cmd_queue) {
    return false;
  }

  HRESULT hr;

  hr = IDXGISwapChain_GetDevice(swap, &IID_ID3D12Device, (void **)&g_nk.device);
  if (FAILED(hr)) {
    LOG_ERROR("%d: IDXGISwapChain_GetDevice: 0x%08lX", __LINE__, hr);
    return false;
  }

  hr = IDXGISwapChain_QueryInterface(swap, &IID_IDXGISwapChain3, (void **)&g_nk.swap_chain);
  if (FAILED(hr)) {
    LOG_ERROR("%d: IDXGISwapChain_QueryInterface: 0x%08lX", __LINE__, hr);
    return false;
  }

  DXGI_SWAP_CHAIN_DESC desc;
  IDXGISwapChain_GetDesc(swap, &desc);
  g_nk.width  = desc.BufferDesc.Width;
  g_nk.height = desc.BufferDesc.Height;

  /* descriptor heaps */
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {
    .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    .NumDescriptors = CONFIG_NK_D3D12_MAX_BACK_BUFFERS,
  };

  hr =
    ID3D12Device_CreateDescriptorHeap(g_nk.device, &rtv_heap_desc, &IID_ID3D12DescriptorHeap, (void **)&g_nk.rtv_heap);

  if (FAILED(hr)) {
    LOG_ERROR("%d: ID3D12Device_CreateDescriptorHeap: 0x%08lX", __LINE__, hr);
    return false;
  }

  g_nk.rtv_increment = ID3D12Device_GetDescriptorHandleIncrementSize(g_nk.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {
    .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
    .NumDescriptors = 1, // just the font atlas
    .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
  };

  hr =
    ID3D12Device_CreateDescriptorHeap(g_nk.device, &srv_heap_desc, &IID_ID3D12DescriptorHeap, (void **)&g_nk.srv_heap);

  if (FAILED(hr)) {
    LOG_ERROR("%d: ID3D12Device_CreateDescriptorHeap: 0x%08lX", __LINE__, hr);
    return false;
  }

  /* fence */
  hr = ID3D12Device_CreateFence(g_nk.device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&g_nk.fence);

  if (FAILED(hr)) {
    LOG_ERROR("%d: ID3D12Device_CreateFence: 0x%08lX", __LINE__, hr);
    return false;
  }

  g_nk.fence_event   = CreateEventA(NULL, FALSE, FALSE, NULL);
  g_nk.fence_counter = 0;

  /* per-frame command allocators */
  DXGI_SWAP_CHAIN_DESC1 sc_desc;
  IDXGISwapChain3_GetDesc1(g_nk.swap_chain, &sc_desc);
  g_nk.num_back_buffers = CLAMP_TOP(sc_desc.BufferCount, CONFIG_NK_D3D12_MAX_BACK_BUFFERS);
  for (UINT i = 0; i < g_nk.num_back_buffers; ++i) {
    hr = ID3D12Device_CreateCommandAllocator(
      g_nk.device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&g_nk.frames[i].cmd_alloc);

    if (FAILED(hr)) {
      LOG_ERROR("%d: ID3D12Device_CreateCommandAllocator[%d]: 0x%08lX", __LINE__, i, hr);
      return false;
    }
  }

  /* command list (re-used across frames by resetting) */
  hr = ID3D12Device_CreateCommandList(g_nk.device,
                                      0,
                                      D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      g_nk.frames[0].cmd_alloc,
                                      NULL,
                                      &IID_ID3D12GraphicsCommandList,
                                      (void **)&g_nk.cmd_list);

  if (FAILED(hr)) {
    LOG_ERROR("%d: ID3D12Device_CreateCommandList: 0x%08lX", __LINE__, hr);
    return false;
  }
  ID3D12GraphicsCommandList_Close(g_nk.cmd_list); // start in closed state

  /* RTVs */
  if (!nk_d3d12_create_rtvs()) {
    LOG_ERROR("%d: nk_d3d12_create_rtvs", __LINE__);
    return false;
  }

  /* upload buffers (VB + IB + CB per back buffer) */
  uint64_t vb_size = CONFIG_NK_D3D12_MAX_VERTICES * sizeof(nk_d3d12_vertex_t);
  uint64_t ib_size = CONFIG_NK_D3D12_MAX_INDICES * sizeof(nk_draw_index);
  uint64_t cb_size = ALIGN_UP(sizeof(float) * 16, 256); // D3D12 CBV alignment = 256

  for (UINT i = 0; i < g_nk.num_back_buffers; ++i) {
    hr = nk_d3d12_create_upload_buffer(&g_nk.vb_upload[i], &g_nk.vb_mapped[i], vb_size);
    if (FAILED(hr)) {
      LOG_ERROR("%d: nk_d3d12_create_upload_buffer[v%d]: 0x%08lX", __LINE__, i, hr);
      return false;
    }

    hr = nk_d3d12_create_upload_buffer(&g_nk.ib_upload[i], &g_nk.ib_mapped[i], ib_size);
    if (FAILED(hr)) {
      LOG_ERROR("%d: nk_d3d12_create_upload_buffer[i%d]: 0x%08lX", __LINE__, i, hr);
      return false;
    }

    hr = nk_d3d12_create_upload_buffer(&g_nk.cb_upload[i], &g_nk.cb_mapped[i], cb_size);
    if (FAILED(hr)) {
      LOG_ERROR("%d: nk_d3d12_create_upload_buffer[c%d]: 0x%08lX", __LINE__, i, hr);
      return false;
    }
  }

  /* root signature + PSO */
  if (!nk_d3d12_create_root_sig()) {
    LOG_ERROR("%d: nk_d3d12_create_root_sig failed", __LINE__);
    return false;
  }

  if (!nk_d3d12_create_pso(sc_desc.Format)) {
    LOG_ERROR("%d: nk_d3d12_create_pso failed", __LINE__);
    return false;
  }

  if (!nk_d3d12_upload_prebaked_font_atlas()) {
    LOG_ERROR("%d: nk_d3d12_upload_prebaked_font_atlas", __LINE__);
    return false;
  }

  nk_handle font_tex = nk_handle_ptr(g_nk.font_texture);

  nk_d3d12_setup_prebaked_font(&g_nk.font_body_obj, &g_nk_font_body_desc, g_nk_font_body_glyphs, font_tex);
  nk_d3d12_setup_prebaked_font(&g_nk.font_title_obj, &g_nk_font_title_desc, g_nk_font_title_glyphs, font_tex);

  g_nk.font_body  = &g_nk.font_body_obj;
  g_nk.font_title = &g_nk.font_title_obj;

  /* Nuklear context */

  ASSERT(g_nk.cmd_buf != NULL);
  ASSERT(g_nk.nk_mem != NULL);

  nk_buffer_init_fixed(&g_nk.nk_cmds, g_nk.cmd_buf, CONFIG_NK_CMD_BUF_SIZE);
  if (!nk_init_fixed(&g_nk.nk_ctx, g_nk.nk_mem, CONFIG_NK_INTERNAL_STATE_SIZE, &g_nk.font_body->handle)) {
    LOG_ERROR("%d: nk_init_fixed", __LINE__);
    return false;
  }

  ui_manager_init(&globals.ui_manager, &g_nk.nk_ctx, g_nk.font_body, g_nk.font_title, g_nk.width, g_nk.height);

  /* snapshots */
  g_nk.snap_read_idx  = -1; // no snapshot available yet
  g_nk.snap_write_idx = 0;

  g_nk.inited = true;
  return true;
}

void
nk_d3d12_shutdown(void)
{
  if (!g_nk.inited) {
    return;
  }

  nk_d3d12_wait_idle();

  nk_free(&g_nk.nk_ctx);

  for (UINT i = 0; i < g_nk.num_back_buffers; ++i) {
    if (g_nk.vb_upload[i]) {
      ID3D12Resource_Release(g_nk.vb_upload[i]);
    }

    if (g_nk.ib_upload[i]) {
      ID3D12Resource_Release(g_nk.ib_upload[i]);
    }

    if (g_nk.cb_upload[i]) {
      ID3D12Resource_Release(g_nk.cb_upload[i]);
    }

    if (g_nk.frames[i].cmd_alloc) {
      ID3D12CommandAllocator_Release(g_nk.frames[i].cmd_alloc);
    }

    if (g_nk.frames[i].back_buffer) {
      ID3D12Resource_Release(g_nk.frames[i].back_buffer);
    }
  }

  if (g_nk.font_texture) {
    ID3D12Resource_Release(g_nk.font_texture);
  }

  if (g_nk.cmd_list) {
    ID3D12GraphicsCommandList_Release(g_nk.cmd_list);
  }

  if (g_nk.pso) {
    ID3D12PipelineState_Release(g_nk.pso);
  }

  if (g_nk.root_sig) {
    ID3D12RootSignature_Release(g_nk.root_sig);
  }

  if (g_nk.fence) {
    ID3D12Fence_Release(g_nk.fence);
  }

  if (g_nk.srv_heap) {
    ID3D12DescriptorHeap_Release(g_nk.srv_heap);
  }

  if (g_nk.rtv_heap) {
    ID3D12DescriptorHeap_Release(g_nk.rtv_heap);
  }

  if (g_nk.fence_event) {
    CloseHandle(g_nk.fence_event);
  }

  mem_zero(&g_nk, sizeof(g_nk));
}

void
nk_d3d12_commit(struct nk_context *ctx)
{
  nk_d3d12_snapshot_t *snap = &g_nk.snap[g_nk.snap_write_idx];
  snap->vertex_count        = 0;
  snap->index_count         = 0;
  snap->command_count       = 0;
  snap->viewport_w          = g_nk.width;
  snap->viewport_h          = g_nk.height;

  /* configure nk_convert to output into our vertex layout */
  static const struct nk_draw_vertex_layout_element vertex_layout[] = {
    {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(nk_d3d12_vertex_t, pos)},
    {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(nk_d3d12_vertex_t, uv)},
    {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(nk_d3d12_vertex_t, col)},
    {NK_VERTEX_LAYOUT_END},
  };

  if (!snap->vertices || !snap->indices || !snap->commands) {
    return;
  }

  struct nk_convert_config cfg = {
    .vertex_layout        = vertex_layout,
    .vertex_size          = sizeof(nk_d3d12_vertex_t),
    .vertex_alignment     = ALIGNOF(nk_d3d12_vertex_t),
    .shape_AA             = NK_ANTI_ALIASING_ON,
    .line_AA              = NK_ANTI_ALIASING_ON,
    .circle_segment_count = 22,
    .curve_segment_count  = 22,
    .arc_segment_count    = 22,
    .global_alpha         = 1.0f,
    .tex_null             = g_nk.null_tex,
  };

  /* temporary nk_buffers pointing into our snapshot arrays */
  struct nk_buffer vbuf;
  struct nk_buffer ibuf;
  nk_buffer_init_fixed(&vbuf, snap->vertices, CONFIG_NK_D3D12_MAX_VERTICES * sizeof(nk_d3d12_vertex_t));
  nk_buffer_init_fixed(&ibuf, snap->indices, CONFIG_NK_D3D12_MAX_INDICES * sizeof(nk_draw_index));

  /* reset the command buffer for re-use */
  nk_buffer_clear(&g_nk.nk_cmds);

  nk_flags convert_result = nk_convert(ctx, &g_nk.nk_cmds, &vbuf, &ibuf, &cfg);
  if (convert_result != NK_CONVERT_SUCCESS) {
    LOG_WARN("%llu: nk_convert failed: 0x%08X", globals.frame_counter, convert_result);
    nk_clear(ctx);
    return;
  }

  snap->vertex_count = (uint32_t)(vbuf.allocated / sizeof(nk_d3d12_vertex_t));
  snap->index_count  = (uint32_t)(ibuf.allocated / sizeof(nk_draw_index));

  /* walk the draw commands and flatten them into a snapshot */
  uint32_t cmd_idx    = 0;
  uint32_t idx_offset = 0;

  const struct nk_draw_command *cmd;
  nk_draw_foreach(cmd, ctx, &g_nk.nk_cmds)
  {
    if (!cmd->elem_count) {
      continue;
    }

    if (cmd_idx >= CONFIG_NK_D3D12_MAX_DRAW_CMDS) {
      break;
    }

    nk_d3d12_draw_cmd_t *dc = &snap->commands[cmd_idx++];
    dc->elem_count          = cmd->elem_count;
    dc->idx_offset          = idx_offset;
    dc->vtx_offset          = 0;
    dc->clip_x              = cmd->clip_rect.x;
    dc->clip_y              = cmd->clip_rect.y;
    dc->clip_w              = cmd->clip_rect.w;
    dc->clip_h              = cmd->clip_rect.h;
    dc->has_texture         = true;

    idx_offset += cmd->elem_count;
  }

  snap->command_count = cmd_idx;

  nk_clear(ctx);

  /* publish: atomically make this snapshot the one the render thread reads.
   * the render thread reads snap_read_idx; we swap our write slot in. */
  int prev_write = g_nk.snap_write_idx;
  InterlockedExchange(&g_nk.snap_read_idx, (LONG)prev_write);

  /* next frame we write to the OTHER slot */
  g_nk.snap_write_idx = 1 - prev_write;
}

void
nk_d3d12_render(void)
{
  if (!g_nk.inited) {
    return;
  }

  /* read the latest snapshot index atomically */
  LONG read_idx = InterlockedCompareExchange(&g_nk.snap_read_idx, -1, -1);
  if (read_idx < 0 || read_idx > 1) {
    return; // no snapshot committed yet
  }

  nk_d3d12_snapshot_t *snap = &g_nk.snap[read_idx];
  if (snap->command_count == 0 || snap->vertex_count == 0) {
    return;
  }

  if (!snap->vertices || !snap->indices || !snap->commands) {
    return;
  }

  UINT back_idx = IDXGISwapChain3_GetCurrentBackBufferIndex(g_nk.swap_chain);
  if (back_idx >= g_nk.num_back_buffers) {
    return;
  }

  nk_d3d12_frame_t *frame = &g_nk.frames[back_idx];

  /* wait until the GPU is done with this frame slot's previous work */
  nk_d3d12_wait_for_fence(frame->fence_value);

  /* upload vertex + index data into the persistently-mapped upload buffers */
  mem_copy(g_nk.vb_mapped[back_idx], snap->vertices, snap->vertex_count * sizeof(nk_d3d12_vertex_t));
  mem_copy(g_nk.ib_mapped[back_idx], snap->indices, snap->index_count * sizeof(nk_draw_index));

  /* upload projection matrix (orthographic, top-left origin) */
  float L = 0.0f;
  float R = (float)snap->viewport_w;
  float T = 0.0f;
  float B = (float)snap->viewport_h;

  /* column-major 4x4 orthographic projection */
  float proj[16] = {
    2.0f / (R - L),
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    2.0f / (T - B),
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.5f,
    0.0f,
    (R + L) / (L - R),
    (T + B) / (B - T),
    0.5f,
    1.0f,
  };
  mem_copy(g_nk.cb_mapped[back_idx], proj, sizeof(proj));

  /* record commands */
  ID3D12CommandAllocator_Reset(frame->cmd_alloc);
  ID3D12GraphicsCommandList_Reset(g_nk.cmd_list, frame->cmd_alloc, g_nk.pso);

  /* transition back buffer: PRESENT -> RENDER_TARGET */
  D3D12_RESOURCE_BARRIER barrier_rt = {
    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    .Transition =
      {
        .pResource   = frame->back_buffer,
        .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
        .StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      },
  };
  ID3D12GraphicsCommandList_ResourceBarrier(g_nk.cmd_list, 1, &barrier_rt);

  /* set render target (we DON'T clear - we're drawing over the game) */
  ID3D12GraphicsCommandList_OMSetRenderTargets(g_nk.cmd_list, 1, &frame->rtv_handle, FALSE, NULL);

  /* set root signature + descriptor heap */
  ID3D12GraphicsCommandList_SetGraphicsRootSignature(g_nk.cmd_list, g_nk.root_sig);

  ID3D12DescriptorHeap *heaps[] = {
    g_nk.srv_heap,
  };
  ID3D12GraphicsCommandList_SetDescriptorHeaps(g_nk.cmd_list, 1, heaps);

  /* root param 0: CBV (projection matrix) */
  D3D12_GPU_VIRTUAL_ADDRESS cb_gpu_addr = ID3D12Resource_GetGPUVirtualAddress(g_nk.cb_upload[back_idx]);
  ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(g_nk.cmd_list, 0, cb_gpu_addr);

  /* root param 1: SRV table (font atlas) */
  D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu;
  g_nk.srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(g_nk.srv_heap, &srv_gpu);
  ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(g_nk.cmd_list, 1, srv_gpu);

  /* set viewport + scissor to full screen (individual draw cmds set scissor) */
  D3D12_VIEWPORT vp = {
    .Width    = (float)snap->viewport_w,
    .Height   = (float)snap->viewport_h,
    .MaxDepth = 1.0f,
  };
  ID3D12GraphicsCommandList_RSSetViewports(g_nk.cmd_list, 1, &vp);

  /* bind vertex + index buffers */
  D3D12_VERTEX_BUFFER_VIEW vbv = {
    .BufferLocation = ID3D12Resource_GetGPUVirtualAddress(g_nk.vb_upload[back_idx]),
    .SizeInBytes    = snap->vertex_count * sizeof(nk_d3d12_vertex_t),
    .StrideInBytes  = sizeof(nk_d3d12_vertex_t),
  };

  D3D12_INDEX_BUFFER_VIEW ibv = {
    .BufferLocation = ID3D12Resource_GetGPUVirtualAddress(g_nk.ib_upload[back_idx]),
    .SizeInBytes    = snap->index_count * sizeof(nk_draw_index),
    .Format         = DXGI_FORMAT_R32_UINT,
  };

  ID3D12GraphicsCommandList_IASetVertexBuffers(g_nk.cmd_list, 0, 1, &vbv);
  ID3D12GraphicsCommandList_IASetIndexBuffer(g_nk.cmd_list, &ibv);
  ID3D12GraphicsCommandList_IASetPrimitiveTopology(g_nk.cmd_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  /* issue draw commands */
  for (uint32_t i = 0; i < snap->command_count; ++i) {
    nk_d3d12_draw_cmd_t *dc = &snap->commands[i];

    /* scissor rect from nuklear's clip rect */
    LONG sx = (LONG)dc->clip_x;
    LONG sy = (LONG)dc->clip_y;
    LONG sw = (LONG)(dc->clip_x + dc->clip_w);
    LONG sh = (LONG)(dc->clip_y + dc->clip_h);

    /* clamp to viewport */
    sx = CLAMP_BOTTOM(sx, 0);
    sy = CLAMP_BOTTOM(sy, 0);
    sw = CLAMP_TOP(sw, (LONG)snap->viewport_w);
    sh = CLAMP_TOP(sh, (LONG)snap->viewport_h);

    if (sw <= sx || sh <= sy) {
      continue;
    }

    D3D12_RECT scissor = {sx, sy, sw, sh};
    ID3D12GraphicsCommandList_RSSetScissorRects(g_nk.cmd_list, 1, &scissor);
    ID3D12GraphicsCommandList_DrawIndexedInstanced(g_nk.cmd_list, dc->elem_count, 1, dc->idx_offset, dc->vtx_offset, 0);
  }

  /* transition back buffer: RENDER_TARGET -> PRESENT */
  D3D12_RESOURCE_BARRIER barrier_present = {
    .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
    .Transition =
      {
        .pResource   = frame->back_buffer,
        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
        .StateAfter  = D3D12_RESOURCE_STATE_PRESENT,
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      },
  };
  ID3D12GraphicsCommandList_ResourceBarrier(g_nk.cmd_list, 1, &barrier_present);
  ID3D12GraphicsCommandList_Close(g_nk.cmd_list);

  ID3D12CommandList *lists[] = {
    (ID3D12CommandList *)g_nk.cmd_list,
  };
  ID3D12CommandQueue_ExecuteCommandLists(g_nk.cmd_queue, 1, lists);

  /* signal fence so we know when this frame is done */
  g_nk.fence_counter += 1;
  frame->fence_value  = g_nk.fence_counter;
  ID3D12CommandQueue_Signal(g_nk.cmd_queue, g_nk.fence, g_nk.fence_counter);
}

void
nk_d3d12_resize_before(void)
{
  if (!g_nk.inited) {
    return;
  }

  /* must release all back buffer references before ResizeBuffers */
  nk_d3d12_wait_idle();
  nk_d3d12_release_rtvs();
}

void
nk_d3d12_resize_after(unsigned int w, unsigned int h)
{
  if (!g_nk.inited) {
    return;
  }

  g_nk.width  = w;
  g_nk.height = h;

  nk_d3d12_create_rtvs();
}

void
nk_d3d12_get_viewport_size(unsigned int *w, unsigned int *h)
{
  if (w) {
    *w = g_nk.width;
  }

  if (h) {
    *h = g_nk.height;
  }
}

void
nk_d3d12_set_command_queue(ID3D12CommandQueue *queue)
{
  if (queue && !g_nk.cmd_queue) {
    g_nk.cmd_queue = queue;
  }
}

bool
nk_d3d12_install_hooks(void)
{
  void *present_addr = NULL;
  void *resize_addr  = NULL;

  /* dummy window */
  WNDCLASSEXW wc = {
    .cbSize        = sizeof(wc),
    .lpfnWndProc   = DefWindowProcW,
    .lpszClassName = L"nk_dummy_wc",
    .hInstance     = GetModuleHandleW(NULL),
  };
  RegisterClassExW(&wc);

  HWND dummy_hwnd = CreateWindowExW(0, L"nk_dummy_wc", L"", WS_OVERLAPPED, 0, 0, 4, 4, NULL, NULL, wc.hInstance, NULL);

  /* D3D11 device + swap chain */
  DXGI_SWAP_CHAIN_DESC sc_desc = {
    .BufferDesc =
      {
        .Width  = 4,
        .Height = 4,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      },
    .SampleDesc.Count = 1,
    .BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount      = 1,
    .OutputWindow     = dummy_hwnd,
    .Windowed         = TRUE,
    .SwapEffect       = DXGI_SWAP_EFFECT_DISCARD,
  };

  D3D_FEATURE_LEVEL    feat_level = D3D_FEATURE_LEVEL_11_0;
  IDXGISwapChain      *dummy_swap = NULL;
  ID3D11Device        *dummy_dev  = NULL;
  ID3D11DeviceContext *dummy_ctx  = NULL;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL,
                                             D3D_DRIVER_TYPE_HARDWARE,
                                             NULL,
                                             0,
                                             &feat_level,
                                             1,
                                             D3D11_SDK_VERSION,
                                             &sc_desc,
                                             &dummy_swap,
                                             &dummy_dev,
                                             NULL,
                                             &dummy_ctx);

  if (FAILED(hr)) {
    LOG_ERROR("D3D11CreateDeviceAndSwapChain failed: 0x%08lX", hr);
    DestroyWindow(dummy_hwnd);
    UnregisterClassW(L"nk_dummy_wc", wc.hInstance);
    return false;
  }

  /* read vtable */
  void **sc_vtable = *(void ***)dummy_swap;
  present_addr     = sc_vtable[DXGI_SWAPCHAIN_PRESENT_VTIDX];
  resize_addr      = sc_vtable[DXGI_SWAPCHAIN_RESIZEBUFFERS_VTIDX];

  /* cleanup */
  ID3D11DeviceContext_Release(dummy_ctx);
  ID3D11Device_Release(dummy_dev);
  IDXGISwapChain_Release(dummy_swap);
  DestroyWindow(dummy_hwnd);
  UnregisterClassW(L"nk_dummy_wc", wc.hInstance);

  if (!present_addr) {
    LOG_ERROR("failed to obtain DXGI vtable addresses");
    return false;
  }

  MH_STATUS mh = MH_CreateHook(present_addr, (LPVOID)present_hook, (LPVOID *)&g_present_real);
  if (mh != MH_OK) {
    LOG_ERROR("failed to hook Present: %s", MH_StatusToString(mh));
    return false;
  }
  MH_EnableHook(present_addr);

  if (resize_addr) {
    mh = MH_CreateHook(resize_addr, (LPVOID)resize_buffers_hook, (LPVOID *)&g_resize_buffers_real);
    if (mh != MH_OK) {
      LOG_WARN("failed to hook ResizeBuffers: %s", MH_StatusToString(mh));
    } else {
      MH_EnableHook(resize_addr);
    }
  }

  return true;
}

HRESULT STDMETHODCALLTYPE
present_hook(IDXGISwapChain *self, UINT sync_interval, UINT flags)
{
  if (!g_nk.inited) {
    nk_d3d12_init(self);
  } else {
    /* Render UI BEFORE the real Present. At this point the back
     * buffer is in PRESENT state (the game finished rendering). Our
     * renderer transitions it to RENDER_TARGET, draws, transitions
     * back to PRESENT, then the real Present flips it */
    if (globals.ui_manager.inited) {
      nk_d3d12_render();
    }
  }

  return g_present_real(self, sync_interval, flags);
}

HRESULT STDMETHODCALLTYPE
resize_buffers_hook(IDXGISwapChain *self, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags)
{
  /* release back buffer references BEFORE the resize */
  nk_d3d12_resize_before();

  HRESULT hr = g_resize_buffers_real(self, count, w, h, fmt, flags);
  if (SUCCEEDED(hr)) {
    DXGI_SWAP_CHAIN_DESC desc;
    IDXGISwapChain_GetDesc(self, &desc);

    nk_d3d12_resize_after(desc.BufferDesc.Width, desc.BufferDesc.Height);
  }
  return hr;
}
