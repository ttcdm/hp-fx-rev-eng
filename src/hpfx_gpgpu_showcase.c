/*
 * HP Visualize FX Pre-Shader GPGPU Scientific Simulation Showcase
 *
 * Demonstrates high-performance numerical computing mapped to the graphics pipeline:
 * - Quadrant 1: 2D PDE Heat Diffusion & Thermal Isothermal Mapping
 * - Quadrant 2: Discrete Voronoi Tessellation & Euclidean Distance Field (via 3D Z-buffer)
 * - Quadrant 3: Spatial Image Convolution (Sobel Edge Detector & Neon Flow)
 * - Quadrant 4: Conway's Game of Life SIMD Cellular Automata Universe
 * - Composited into a unified 1024x1024 multi-panel scientific dashboard poster
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_compute.h"

#define PANEL_SIZE 512
#define TOTAL_WIDTH (PANEL_SIZE * 2)
#define TOTAL_HEIGHT (PANEL_SIZE * 2)

static void render_pde_panel(uint32_t *out_pixels)
{
    hpfx_compute_context *ctx = hpfx_compute_init(PANEL_SIZE, PANEL_SIZE);
    if (!ctx) return;

    /* Central multi-core heat source with cooling fins */
    hpfx_compute_pde_set_source(ctx, PANEL_SIZE / 2, PANEL_SIZE / 2, 35.0f, 100.0f);
    hpfx_compute_pde_set_source(ctx, PANEL_SIZE / 2 - 90, PANEL_SIZE / 2 - 60, 20.0f, 85.0f);
    hpfx_compute_pde_set_source(ctx, PANEL_SIZE / 2 + 90, PANEL_SIZE / 2 + 60, 20.0f, 85.0f);

    /* Run 120 diffusion steps */
    for (int i = 0; i < 120; i++) {
        hpfx_compute_pde_step(ctx, 0.22f, 0.5f);
    }

    hpfx_compute_pde_render_to_pixels(ctx, out_pixels);

    /* Overlay isothermal contour rings */
    for (int y = 0; y < PANEL_SIZE; y++) {
        for (int x = 0; x < PANEL_SIZE; x++) {
            float temp = hpfx_compute_pde_get_temp(ctx, x, y);
            int itemp = (int)temp;
            if (itemp > 5 && (itemp % 10 == 0 || itemp % 10 == 1)) {
                out_pixels[y * PANEL_SIZE + x] = 0x00ffffff; /* White contour line */
            }
        }
    }

    hpfx_compute_destroy(ctx);
}

static void render_voronoi_panel(uint32_t *out_pixels)
{
    hpfx_compute_context *ctx = hpfx_compute_init(PANEL_SIZE, PANEL_SIZE);
    if (!ctx) return;

    /* 14 Golden Spiral seed sites */
    const int NUM_SITES = 14;
    hpfx_voronoi_site sites[14];
    uint32_t palette[14] = {
        0x00e74c3c, 0x003498db, 0x002ecc71, 0x00f1c40f,
        0x009b59b6, 0x001abc9c, 0x00e67e22, 0x0034495e,
        0x00d35400, 0x0027ae60, 0x002980b9, 0x008e44ad,
        0x0016a085, 0x00c0392b
    };

    float cx = PANEL_SIZE * 0.5f;
    float cy = PANEL_SIZE * 0.5f;
    for (int i = 0; i < NUM_SITES; i++) {
        float angle = (float)i * 2.39996323f; /* Golden angle rad */
        float r = 18.0f * sqrtf((float)i + 1.0f) * (PANEL_SIZE / 256.0f);
        sites[i].x = cx + r * cosf(angle);
        sites[i].y = cy + r * sinf(angle);
        sites[i].site_id = i + 1;
        sites[i].color = palette[i % 14];
    }

    uint32_t *vor_color = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));
    float *vor_dist = (float*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(float));

    hpfx_compute_voronoi_zbuffer(ctx, sites, NUM_SITES, vor_color, vor_dist);

    /* Shade cell interiors with distance gradient and boundary borders */
    for (int y = 0; y < PANEL_SIZE; y++) {
        for (int x = 0; x < PANEL_SIZE; x++) {
            int idx = y * PANEL_SIZE + x;
            uint32_t c = vor_color[idx];
            float d = vor_dist[idx];

            /* Concentric distance isocontours */
            float ripple = cosf(d * 0.25f);
            float shade = 0.7f + 0.3f * ripple;
            if (shade < 0.2f) shade = 0.2f;

            uint32_t r = (uint32_t)(((c >> 16) & 0xff) * shade);
            uint32_t g = (uint32_t)(((c >> 8) & 0xff) * shade);
            uint32_t b = (uint32_t)((c & 0xff) * shade);

            /* Mark site seeds with glowing white point */
            if (d < 3.5f) {
                out_pixels[idx] = 0x00ffffff;
            } else {
                out_pixels[idx] = (r << 16) | (g << 8) | b;
            }
        }
    }

    free(vor_color);
    free(vor_dist);
    hpfx_compute_destroy(ctx);
}

static void render_sobel_panel(uint32_t *out_pixels)
{
    /* Generate high-contrast geometric mandala source pattern */
    uint32_t *src = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));
    uint32_t *dst = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));

    float cx = PANEL_SIZE * 0.5f;
    float cy = PANEL_SIZE * 0.5f;

    for (int y = 0; y < PANEL_SIZE; y++) {
        for (int x = 0; x < PANEL_SIZE; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float r = sqrtf(dx * dx + dy * dy);
            float theta = atan2f(dy, dx);

            /* Concentric rings + 8-fold radial spokes */
            int ring = ((int)(r / 16.0f)) % 2;
            int spoke = ((int)((theta + 3.14159f) / (3.14159f / 4.0f))) % 2;

            uint8_t val = (ring ^ spoke) ? 230 : 25;
            src[y * PANEL_SIZE + x] = ((uint32_t)val << 16) | ((uint32_t)val << 8) | val;
        }
    }

    /* Apply 3x3 Sobel Edge Operator */
    hpfx_compute_sobel_gradient(src, dst, PANEL_SIZE, PANEL_SIZE);

    /* Tint edges with electric cyan glow */
    for (int i = 0; i < PANEL_SIZE * PANEL_SIZE; i++) {
        uint32_t mag = dst[i] & 0xff;
        uint32_t r = mag / 3;
        uint32_t g = (uint32_t)(mag * 0.85f);
        uint32_t b = mag;
        out_pixels[i] = (r << 16) | (g << 8) | b;
    }

    free(src);
    free(dst);
}

static void render_life_panel(uint32_t *out_pixels)
{
    hpfx_life_grid *lg = hpfx_life_create(PANEL_SIZE, PANEL_SIZE);
    if (!lg) return;

    /* Seed multiple glider fleets, pulsars, and random blocks */
    for (int gy = 40; gy < PANEL_SIZE - 60; gy += 80) {
        for (int gx = 40; gx < PANEL_SIZE - 60; gx += 80) {
            hpfx_life_load_glider(lg, gx, gy);
            /* Add blinking oscillators */
            hpfx_life_set_cell(lg, gx + 20, gy + 20, 1);
            hpfx_life_set_cell(lg, gx + 21, gy + 20, 1);
            hpfx_life_set_cell(lg, gx + 22, gy + 20, 1);
        }
    }

    /* Add central chaotic R-pentomino */
    int mid = PANEL_SIZE / 2;
    hpfx_life_set_cell(lg, mid + 1, mid + 0, 1);
    hpfx_life_set_cell(lg, mid + 2, mid + 0, 1);
    hpfx_life_set_cell(lg, mid + 0, mid + 1, 1);
    hpfx_life_set_cell(lg, mid + 1, mid + 1, 1);
    hpfx_life_set_cell(lg, mid + 1, mid + 2, 1);

    /* Evolve for 64 generations */
    for (int gen = 0; gen < 64; gen++) {
        hpfx_life_step(lg);
    }

    /* Render alive cells as neon emerald on dark slate */
    hpfx_life_render_to_pixels(lg, out_pixels, 0x0000ff88, 0x000a1017);

    hpfx_life_destroy(lg);
}

static void save_composite_ppm(const char *filename,
                               const uint32_t *p00, const uint32_t *p01,
                               const uint32_t *p10, const uint32_t *p11)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", TOTAL_WIDTH, TOTAL_HEIGHT);

    for (int y = 0; y < TOTAL_HEIGHT; y++) {
        int py = y % PANEL_SIZE;
        bool bottom = (y >= PANEL_SIZE);

        for (int x = 0; x < TOTAL_WIDTH; x++) {
            int px = x % PANEL_SIZE;
            bool right = (x >= PANEL_SIZE);

            /* Panel border lines (2-pixel border) */
            if (x == PANEL_SIZE - 1 || x == PANEL_SIZE || y == PANEL_SIZE - 1 || y == PANEL_SIZE) {
                fputc(255, f); fputc(255, f); fputc(255, f);
                continue;
            }

            uint32_t pixel;
            if (!bottom && !right)      pixel = p00[py * PANEL_SIZE + px]; /* Top-Left: PDE */
            else if (!bottom && right)  pixel = p01[py * PANEL_SIZE + px]; /* Top-Right: Voronoi */
            else if (bottom && !right)  pixel = p10[py * PANEL_SIZE + px]; /* Bottom-Left: Sobel */
            else                        pixel = p11[py * PANEL_SIZE + px]; /* Bottom-Right: Life */

            fputc((pixel >> 16) & 0xff, f);
            fputc((pixel >> 8) & 0xff, f);
            fputc(pixel & 0xff, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *out_ppm = "hpfx_render_gpgpu_poster.ppm";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out_ppm = argv[++i];
    }

    printf("===================================================================\n");
    printf(" HP Visualize FX Pre-Shader GPGPU Scientific Simulation Showcase\n");
    printf(" Target: HP Visualize FX5 / FX10 Architecture Simulation\n");
    printf("===================================================================\n");
    printf("Generating 1024x1024 Quad-Panel Poster:\n");
    printf("  [1/4] Panel 1: 2D PDE Heat Diffusion with Isothermal Contours...\n");
    uint32_t *p00 = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));
    render_pde_panel(p00);

    printf("  [2/4] Panel 2: Discrete Voronoi & Euclidean Distance Transform...\n");
    uint32_t *p01 = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));
    render_voronoi_panel(p01);

    printf("  [3/4] Panel 3: Spatial Sobel Edge Convolutions & Gradient Flow...\n");
    uint32_t *p10 = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));
    render_sobel_panel(p10);

    printf("  [4/4] Panel 4: Conway's Game of Life Cellular Automata Simulation...\n");
    uint32_t *p11 = (uint32_t*)malloc(PANEL_SIZE * PANEL_SIZE * sizeof(uint32_t));
    render_life_panel(p11);

    printf("[Export] Writing 4-panel poster composite to: %s\n", out_ppm);
    save_composite_ppm(out_ppm, p00, p01, p10, p11);

    free(p00); free(p01); free(p10); free(p11);
    printf("GPGPU Scientific Simulation Showcase finished successfully.\n");
    return 0;
}
