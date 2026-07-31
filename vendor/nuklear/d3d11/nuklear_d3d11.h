/*
 * Nuklear - 1.32.0 - public domain
 * no warrenty implied; use at your own risk.
 * authored from 2015-2016 by Micha Mettke
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_D3D11_H_
#define NK_D3D11_H_

#include <windows.h>

#include "nk_font_runtime.h"
#include "vendor_nuklear.h"

typedef struct ID3D11Device        ID3D11Device;
typedef struct ID3D11DeviceContext ID3D11DeviceContext;

NK_API struct nk_context *
nk_d3d11_init(ID3D11Device *device, int width, int height, unsigned int max_vertex_buffer, unsigned int max_index_buffer);
NK_API struct nk_context *
nk_d3d11_init_fixed(ID3D11Device *device,
                    int           width,
                    int           height,
                    unsigned int  max_vertex_buffer,
                    unsigned int  max_index_buffer,
                    void         *memory,
                    nk_size       memory_size,
                    nk_size       text_cache_size);
NK_API int
nk_d3d11_handle_event(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
NK_API void
nk_d3d11_render(ID3D11DeviceContext *context, enum nk_anti_aliasing);
NK_API void
nk_d3d11_resize(ID3D11DeviceContext *context, int width, int height);
NK_API void
nk_d3d11_shutdown(void);

NK_API void
nk_d3d11_setup_fonts(struct nk_runtime_fonts *fonts);

#endif /* NK_D3D11_H_ */
