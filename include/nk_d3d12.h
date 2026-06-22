#ifndef NK_D3D12_H
#define NK_D3D12_H

#include "arena.h"

#include "vendor_nuklear.h"

typedef struct nk_d3d12_draw_cmd_s nk_d3d12_draw_cmd_t;
struct nk_d3d12_draw_cmd_s {
  uint32_t elem_count; // number of indices
  uint32_t idx_offset; // offset into the index buffer (in indices)
  int32_t  vtx_offset; // offset into the vertex buffer (in vertices)
  float    clip_x;
  float    clip_y;
  float    clip_w;
  float    clip_h;
  bool     has_texture;
};

PACKED_STRUCT(nk_d3d12_vertex_s, {
  float   pos[2];
  float   uv[2];
  uint8_t col[4]; // RGBA
});
typedef struct nk_d3d12_vertex_s nk_d3d12_vertex_t;

// a snapshot = one frame's worth of draw data, produced by the game thread
typedef struct nk_d3d12_snapshot_s nk_d3d12_snapshot_t;
struct nk_d3d12_snapshot_s {
  nk_d3d12_vertex_t   *vertices;
  nk_draw_index       *indices;
  nk_d3d12_draw_cmd_t *commands;
  uint32_t             vertex_count;
  uint32_t             index_count;
  uint32_t             command_count;
  uint32_t             viewport_w;
  uint32_t             viewport_h;
};

void
nk_d3d12_preinit(arena_t *perm);
void
nk_d3d12_shutdown(void);

void
nk_d3d12_commit(struct nk_context *ctx);
void
nk_d3d12_render(void);
void
nk_d3d12_resize_before(void);
void
nk_d3d12_resize_after(unsigned int w, unsigned int h);
void
nk_d3d12_get_viewport_size(unsigned int *w, unsigned int *h);

typedef struct ID3D12CommandQueue ID3D12CommandQueue;

void
nk_d3d12_set_command_queue(ID3D12CommandQueue *queue);
bool
nk_d3d12_install_hooks(void);

#endif /* NK_D3D12_H */
