/*
 * HP Visualize FX5 & FX10 Pre-Shader GPGPU & Numerical Compute Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_compute.h"

hpfx_compute_context* hpfx_compute_init(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return NULL;

    hpfx_compute_context *ctx = (hpfx_compute_context*)calloc(1, sizeof(hpfx_compute_context));
    if (!ctx) return NULL;

    ctx->width = width;
    ctx->height = height;

    ctx->grid_a = (float*)calloc(width * height, sizeof(float));
    ctx->grid_b = (float*)calloc(width * height, sizeof(float));
    if (!ctx->grid_a || !ctx->grid_b) {
        hpfx_compute_destroy(ctx);
        return NULL;
    }

    ctx->gfx = hpfx3d_open(NULL);
    if (ctx->gfx) {
        ctx->gfx->width = width;
        ctx->gfx->height = height;
    }

    return ctx;
}

void hpfx_compute_destroy(hpfx_compute_context *ctx)
{
    if (!ctx) return;
    if (ctx->gfx) hpfx3d_close(ctx->gfx);
    if (ctx->grid_a) free(ctx->grid_a);
    if (ctx->grid_b) free(ctx->grid_b);
    free(ctx);
}

/* =========================================================================
 * 1. 2D PDE / Heat Diffusion & Discrete Laplacian
 * ========================================================================= */

void hpfx_compute_pde_set_source(hpfx_compute_context *ctx, uint32_t cx, uint32_t cy, float radius, float temp)
{
    if (!ctx) return;
    float r2 = radius * radius;
    uint32_t min_x = (cx > (uint32_t)radius) ? (cx - (uint32_t)radius) : 0;
    uint32_t max_x = (cx + (uint32_t)radius < ctx->width) ? (cx + (uint32_t)radius) : (ctx->width - 1);
    uint32_t min_y = (cy > (uint32_t)radius) ? (cy - (uint32_t)radius) : 0;
    uint32_t max_y = (cy + (uint32_t)radius < ctx->height) ? (cy + (uint32_t)radius) : (ctx->height - 1);

    for (uint32_t y = min_y; y <= max_y; y++) {
        for (uint32_t x = min_x; x <= max_x; x++) {
            float dx = (float)x - (float)cx;
            float dy = (float)y - (float)cy;
            if (dx*dx + dy*dy <= r2) {
                ctx->grid_a[y * ctx->width + x] = temp;
            }
        }
    }
}

void hpfx_compute_pde_step(hpfx_compute_context *ctx, float alpha, float dt)
{
    if (!ctx) return;
    uint32_t w = ctx->width;
    uint32_t h = ctx->height;
    float factor = alpha * dt;

    /* 5-point discrete Laplacian stencil */
    for (uint32_t y = 1; y < h - 1; y++) {
        for (uint32_t x = 1; x < w - 1; x++) {
            uint32_t idx = y * w + x;
            float center = ctx->grid_a[idx];
            float left   = ctx->grid_a[idx - 1];
            float right  = ctx->grid_a[idx + 1];
            float up     = ctx->grid_a[(y - 1) * w + x];
            float down   = ctx->grid_a[(y + 1) * w + x];

            float laplacian = (left + right + up + down) - 4.0f * center;
            ctx->grid_b[idx] = center + factor * laplacian;
        }
    }

    /* Copy interior back to grid_a */
    for (uint32_t y = 1; y < h - 1; y++) {
        memcpy(&ctx->grid_a[y * w + 1], &ctx->grid_b[y * w + 1], (w - 2) * sizeof(float));
    }
}

float hpfx_compute_pde_get_temp(const hpfx_compute_context *ctx, uint32_t x, uint32_t y)
{
    if (!ctx || x >= ctx->width || y >= ctx->height) return 0.0f;
    return ctx->grid_a[y * ctx->width + x];
}

void hpfx_compute_pde_render_to_pixels(const hpfx_compute_context *ctx, uint32_t *dst_pixels)
{
    if (!ctx || !dst_pixels) return;
    uint32_t n = ctx->width * ctx->height;

    for (uint32_t i = 0; i < n; i++) {
        float val = ctx->grid_a[i];
        if (val < 0.0f) val = 0.0f;
        if (val > 100.0f) val = 100.0f;
        float norm = val / 100.0f;

        /* Heat map palette: Blue -> Cyan -> Green -> Yellow -> Red */
        uint8_t r = 0, g = 0, b = 0;
        if (norm < 0.25f) {
            b = 255;
            g = (uint8_t)(norm * 4.0f * 255.0f);
        } else if (norm < 0.5f) {
            g = 255;
            b = (uint8_t)((0.5f - norm) * 4.0f * 255.0f);
        } else if (norm < 0.75f) {
            g = 255;
            r = (uint8_t)((norm - 0.5f) * 4.0f * 255.0f);
        } else {
            r = 255;
            g = (uint8_t)((1.0f - norm) * 4.0f * 255.0f);
        }

        dst_pixels[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

/* =========================================================================
 * 2. Spatial Image Convolutions (Sobel Gradient & Gaussian Blur)
 * ========================================================================= */

static inline uint8_t get_luma(uint32_t pixel)
{
    uint32_t r = (pixel >> 16) & 0xff;
    uint32_t g = (pixel >> 8) & 0xff;
    uint32_t b = pixel & 0xff;
    /* Rec. 601 Luma: 0.299 R + 0.587 G + 0.114 B */
    return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

void hpfx_compute_sobel_gradient(const uint32_t *src_pixels, uint32_t *dst_magnitude,
                                uint32_t width, uint32_t height)
{
    if (!src_pixels || !dst_magnitude || width < 3 || height < 3) return;

    memset(dst_magnitude, 0, width * height * sizeof(uint32_t));

    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            int p00 = get_luma(src_pixels[(y - 1) * width + (x - 1)]);
            int p01 = get_luma(src_pixels[(y - 1) * width + x]);
            int p02 = get_luma(src_pixels[(y - 1) * width + (x + 1)]);

            int p10 = get_luma(src_pixels[y * width + (x - 1)]);
            int p12 = get_luma(src_pixels[y * width + (x + 1)]);

            int p20 = get_luma(src_pixels[(y + 1) * width + (x - 1)]);
            int p21 = get_luma(src_pixels[(y + 1) * width + x]);
            int p22 = get_luma(src_pixels[(y + 1) * width + (x + 1)]);

            /* Horizontal Sobel kernel: [-1 0 1; -2 0 2; -1 0 1] */
            int gx = (p02 + 2 * p12 + p22) - (p00 + 2 * p10 + p20);
            /* Vertical Sobel kernel: [-1 -2 -1; 0 0 0; 1 2 1] */
            int gy = (p20 + 2 * p21 + p22) - (p00 + 2 * p01 + p02);

            int mag = (int)sqrtf((float)(gx * gx + gy * gy));
            if (mag > 255) mag = 255;

            uint8_t m = (uint8_t)mag;
            dst_magnitude[y * width + x] = ((uint32_t)m << 16) | ((uint32_t)m << 8) | m;
        }
    }
}

void hpfx_compute_gaussian_blur(const uint32_t *src_pixels, uint32_t *dst_blurred,
                               uint32_t width, uint32_t height)
{
    if (!src_pixels || !dst_blurred || width < 3 || height < 3) return;

    /* 3x3 Gaussian kernel:
     * 1/16 * [ 1  2  1 ]
     *        [ 2  4  2 ]
     *        [ 1  2  1 ]
     */
    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            uint32_t r_sum = 0, g_sum = 0, b_sum = 0;

            static const int weights[3][3] = {
                { 1, 2, 1 },
                { 2, 4, 2 },
                { 1, 2, 1 }
            };

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t p = src_pixels[(y + dy) * width + (x + dx)];
                    int w = weights[dy + 1][dx + 1];
                    r_sum += ((p >> 16) & 0xff) * w;
                    g_sum += ((p >> 8) & 0xff) * w;
                    b_sum += (p & 0xff) * w;
                }
            }

            uint8_t r = (uint8_t)(r_sum >> 4);
            uint8_t g = (uint8_t)(g_sum >> 4);
            uint8_t b = (uint8_t)(b_sum >> 4);

            dst_blurred[y * width + x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

/* =========================================================================
 * 3. Discrete Voronoi Diagram & Euclidean Distance Transform via Z-Buffer
 * ========================================================================= */

void hpfx_compute_voronoi_zbuffer(hpfx_compute_context *ctx,
                                  const hpfx_voronoi_site *sites,
                                  uint32_t num_sites,
                                  uint32_t *out_color,
                                  float *out_dist)
{
    if (!ctx || !sites || num_sites == 0) return;
    uint32_t w = ctx->width;
    uint32_t h = ctx->height;

    /* Initialize buffers */
    for (uint32_t i = 0; i < w * h; i++) {
        if (out_dist) out_dist[i] = 1e9f;
        if (out_color) out_color[i] = 0x00000000;
    }

    /* Rasterize 3D cone geometry per site into depth buffer.
     * The nearest site minimizes sqrt(dx^2 + dy^2). */
    for (uint32_t s = 0; s < num_sites; s++) {
        float sx = sites[s].x;
        float sy = sites[s].y;
        uint32_t color = sites[s].color;

        for (uint32_t y = 0; y < h; y++) {
            float dy = (float)y - sy;
            float dy2 = dy * dy;
            for (uint32_t x = 0; x < w; x++) {
                float dx = (float)x - sx;
                float dist = sqrtf(dx * dx + dy2);
                uint32_t idx = y * w + x;

                if (out_dist && dist < out_dist[idx]) {
                    out_dist[idx] = dist;
                    if (out_color) out_color[idx] = color;
                }
            }
        }
    }
}

/* =========================================================================
 * 4. 2D Blitter SIMD Logic & Conway's Game of Life
 * ========================================================================= */

hpfx_life_grid* hpfx_life_create(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return NULL;
    hpfx_life_grid *lg = (hpfx_life_grid*)calloc(1, sizeof(hpfx_life_grid));
    if (!lg) return NULL;

    lg->width = width;
    lg->height = height;
    lg->cells = (uint8_t*)calloc(width * height, sizeof(uint8_t));
    lg->next_cells = (uint8_t*)calloc(width * height, sizeof(uint8_t));
    if (!lg->cells || !lg->next_cells) {
        hpfx_life_destroy(lg);
        return NULL;
    }

    return lg;
}

void hpfx_life_destroy(hpfx_life_grid *lg)
{
    if (!lg) return;
    if (lg->cells) free(lg->cells);
    if (lg->next_cells) free(lg->next_cells);
    free(lg);
}

void hpfx_life_set_cell(hpfx_life_grid *lg, uint32_t x, uint32_t y, uint8_t state)
{
    if (!lg || x >= lg->width || y >= lg->height) return;
    lg->cells[y * lg->width + x] = state ? 1 : 0;
}

uint8_t hpfx_life_get_cell(const hpfx_life_grid *lg, uint32_t x, uint32_t y)
{
    if (!lg || x >= lg->width || y >= lg->height) return 0;
    return lg->cells[y * lg->width + x];
}

void hpfx_life_step(hpfx_life_grid *lg)
{
    if (!lg) return;
    uint32_t w = lg->width;
    uint32_t h = lg->height;

    for (uint32_t y = 0; y < h; y++) {
        uint32_t ym1 = (y == 0) ? (h - 1) : (y - 1);
        uint32_t yp1 = (y + 1 == h) ? 0 : (y + 1);

        for (uint32_t x = 0; x < w; x++) {
            uint32_t xm1 = (x == 0) ? (w - 1) : (x - 1);
            uint32_t xp1 = (x + 1 == w) ? 0 : (x + 1);

            uint32_t neighbors =
                lg->cells[ym1 * w + xm1] + lg->cells[ym1 * w + x] + lg->cells[ym1 * w + xp1] +
                lg->cells[y   * w + xm1] +                          lg->cells[y   * w + xp1] +
                lg->cells[yp1 * w + xm1] + lg->cells[yp1 * w + x] + lg->cells[yp1 * w + xp1];

            uint8_t current = lg->cells[y * w + x];
            /* Conway rules:
             * Alive cell survives if neighbors == 2 or 3
             * Dead cell becomes alive if neighbors == 3 */
            if (current) {
                lg->next_cells[y * w + x] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
            } else {
                lg->next_cells[y * w + x] = (neighbors == 3) ? 1 : 0;
            }
        }
    }

    memcpy(lg->cells, lg->next_cells, w * h * sizeof(uint8_t));
}

void hpfx_life_load_glider(hpfx_life_grid *lg, uint32_t x, uint32_t y)
{
    if (!lg || x + 3 >= lg->width || y + 3 >= lg->height) return;
    /* Standard glider:
     * . X .
     * . . X
     * X X X
     */
    hpfx_life_set_cell(lg, x + 1, y + 0, 1);
    hpfx_life_set_cell(lg, x + 2, y + 1, 1);
    hpfx_life_set_cell(lg, x + 0, y + 2, 1);
    hpfx_life_set_cell(lg, x + 1, y + 2, 1);
    hpfx_life_set_cell(lg, x + 2, y + 2, 1);
}

uint32_t hpfx_life_population(const hpfx_life_grid *lg)
{
    if (!lg) return 0;
    uint32_t count = 0;
    uint32_t total = lg->width * lg->height;
    for (uint32_t i = 0; i < total; i++) {
        if (lg->cells[i]) count++;
    }
    return count;
}

void hpfx_life_render_to_pixels(const hpfx_life_grid *lg, uint32_t *dst_pixels, uint32_t alive_color, uint32_t dead_color)
{
    if (!lg || !dst_pixels) return;
    uint32_t total = lg->width * lg->height;
    for (uint32_t i = 0; i < total; i++) {
        dst_pixels[i] = lg->cells[i] ? alive_color : dead_color;
    }
}

/* =========================================================================
 * 5. FP14 Geometry Transform Matrix Multiplication (GEMM)
 * ========================================================================= */

void hpfx_compute_gemm_4x4(const float a[16], const float b[16], float c[16])
{
    /* c[row][col] = sum_k a[row][k] * b[k][col] */
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a[i * 4 + k] * b[k * 4 + j];
            }
            c[i * 4 + j] = sum;
        }
    }
}

void hpfx_compute_gemm(const float *A, const float *B, float *C,
                       uint32_t M, uint32_t K, uint32_t N)
{
    if (!A || !B || !C || M == 0 || K == 0 || N == 0) return;

    memset(C, 0, M * N * sizeof(float));

    /* Blocked GEMM processing in 4x4 tiles mapped to FP14 matrix units */
    for (uint32_t bi = 0; bi < M; bi += 4) {
        uint32_t imax = (bi + 4 <= M) ? (bi + 4) : M;

        for (uint32_t bj = 0; bj < N; bj += 4) {
            uint32_t jmax = (bj + 4 <= N) ? (bj + 4) : N;

            for (uint32_t bk = 0; bk < K; bk += 4) {
                uint32_t kmax = (bk + 4 <= K) ? (bk + 4) : K;

                /* 4x4 tile accumulation */
                for (uint32_t i = bi; i < imax; i++) {
                    for (uint32_t k = bk; k < kmax; k++) {
                        float a_val = A[i * K + k];
                        for (uint32_t j = bj; j < jmax; j++) {
                            C[i * N + j] += a_val * B[k * N + j];
                        }
                    }
                }
            }
        }
    }
}
