/*
 * HP Visualize FX5 & FX10 Pre-Shader GPGPU & Numerical Compute Framework
 *
 * Maps general-purpose numerical computing onto legacy fixed-function hardware:
 * 1. 2D PDE / Heat Diffusion & Discrete Laplacian Accumulator
 * 2. Spatial Image Convolutions (Sobel Edge Gradient & Gaussian Filtering)
 * 3. Discrete Voronoi Diagram & Euclidean Distance Transform via 3D Z-Buffer
 * 4. Cellular Automata (Conway's Game of Life) via 2D Blitter ROP SIMD Logic
 * 5. Blocked Matrix Multiplication (GEMM) via FP14 Geometry Transform Units
 */

#ifndef _HPFX_COMPUTE_H_
#define _HPFX_COMPUTE_H_

#include <stdint.h>
#include <stdbool.h>
#include "hpfx3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pre-Shader GPGPU Computational Context */
typedef struct hpfx_compute_context {
    hpfx3d_context *gfx;
    uint32_t width;
    uint32_t height;
    float *grid_a;
    float *grid_b;
} hpfx_compute_context;

hpfx_compute_context* hpfx_compute_init(uint32_t width, uint32_t height);
void hpfx_compute_destroy(hpfx_compute_context *ctx);

/* =========================================================================
 * 1. 2D PDE / Heat Diffusion & Discrete Laplacian
 * ========================================================================= */
void hpfx_compute_pde_set_source(hpfx_compute_context *ctx, uint32_t cx, uint32_t cy, float radius, float temp);
void hpfx_compute_pde_step(hpfx_compute_context *ctx, float alpha, float dt);
float hpfx_compute_pde_get_temp(const hpfx_compute_context *ctx, uint32_t x, uint32_t y);
void hpfx_compute_pde_render_to_pixels(const hpfx_compute_context *ctx, uint32_t *dst_pixels);

/* =========================================================================
 * 2. Spatial Image Convolutions (Sobel Edge Detector & Gaussian Blur)
 * ========================================================================= */
void hpfx_compute_sobel_gradient(const uint32_t *src_pixels, uint32_t *dst_magnitude,
                                uint32_t width, uint32_t height);
void hpfx_compute_gaussian_blur(const uint32_t *src_pixels, uint32_t *dst_blurred,
                               uint32_t width, uint32_t height);

/* =========================================================================
 * 3. Discrete Voronoi Diagram & Euclidean Distance Transform via Z-Buffer
 * ========================================================================= */
typedef struct {
    float x, y;
    uint32_t site_id;
    uint32_t color;
} hpfx_voronoi_site;

void hpfx_compute_voronoi_zbuffer(hpfx_compute_context *ctx,
                                  const hpfx_voronoi_site *sites,
                                  uint32_t num_sites,
                                  uint32_t *out_color,
                                  float *out_dist);

/* =========================================================================
 * 4. 2D Blitter SIMD Logic & Conway's Game of Life
 * ========================================================================= */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t *cells;
    uint8_t *next_cells;
} hpfx_life_grid;

hpfx_life_grid* hpfx_life_create(uint32_t width, uint32_t height);
void hpfx_life_destroy(hpfx_life_grid *lg);
void hpfx_life_set_cell(hpfx_life_grid *lg, uint32_t x, uint32_t y, uint8_t state);
uint8_t hpfx_life_get_cell(const hpfx_life_grid *lg, uint32_t x, uint32_t y);
void hpfx_life_step(hpfx_life_grid *lg);
void hpfx_life_load_glider(hpfx_life_grid *lg, uint32_t x, uint32_t y);
uint32_t hpfx_life_population(const hpfx_life_grid *lg);
void hpfx_life_render_to_pixels(const hpfx_life_grid *lg, uint32_t *dst_pixels, uint32_t alive_color, uint32_t dead_color);

/* =========================================================================
 * 5. FP14 Geometry Transform Matrix Multiplication (GEMM)
 * ========================================================================= */
void hpfx_compute_gemm_4x4(const float a[16], const float b[16], float c[16]);
void hpfx_compute_gemm(const float *A, const float *B, float *C,
                       uint32_t M, uint32_t K, uint32_t N);

#ifdef __cplusplus
}
#endif

#endif /* _HPFX_COMPUTE_H_ */
