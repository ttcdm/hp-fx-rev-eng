/*
 * HP Visualize FX Hardware Z-Buffer 3D Computational Geometry Demo
 *
 * Implements the Hoff et al. (SIGGRAPH 1999) hardware algorithm:
 * "Fast Computation of Generalized Voronoi Diagrams Using Graphics Hardware"
 *
 * Fully computed natively on the GPU:
 * - The CPU constructs 3D right circular polygonal cones per site
 * - The HP Visualize FX Summit/Lego pipeline transforms and rasterizes the cones
 * - The hardware 24-bit Z-Buffer (HPFX_DEPTH_LESS) natively computes the minimum
 *   Euclidean distance transform and Voronoi cell boundaries!
 * - Zero CPU distance calculation, zero CPU neighbor search.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx3d.h"

#define PI 3.14159265358979323846f
#define NUM_SITES 16
#define CONE_FACETS 64

typedef struct {
    float x, y;
    uint32_t color;
} voronoi_site_t;

/* 16 distinct workstation palette colors */
static const uint32_t s_palette[NUM_SITES] = {
    0x00e74c3c, 0x002ecc71, 0x003498db, 0x00f1c40f,
    0x009b59b6, 0x001abc9c, 0x00e67e22, 0x0034495e,
    0x00e84393, 0x0000cec9, 0x006c5ce7, 0x00fdcb6e,
    0x00d63031, 0x000984e3, 0x0000b894, 0x00fd79a8
};

static void draw_site_cone(hpfx3d_context *ctx, float sx, float sy, float radius, float height, uint32_t color)
{
    float dth = 2.0f * PI / (float)CONE_FACETS;

    /* Apex of cone sits closest to camera along -Z (z = -1.0f) */
    hpfx3d_vec3 apex = { sx, sy, -1.0f };

    /* Base of cone expands radially at farther depth (z = -1.0f - height) */
    for (int i = 0; i < CONE_FACETS; i++) {
        float th0 = (float)i * dth;
        float th1 = (float)(i + 1) * dth;

        hpfx3d_vec3 p0 = { sx + radius * cosf(th0), sy + radius * sinf(th0), -1.0f - height };
        hpfx3d_vec3 p1 = { sx + radius * cosf(th1), sy + radius * sinf(th1), -1.0f - height };

        /* Triangles are submitted with CCW winding */
        hpfx3d_draw_triangle_3d(ctx, &apex, &p0, &p1, color);
    }
}

static void draw_site_marker(hpfx3d_context *ctx, float sx, float sy)
{
    float r = 0.022f;
    int segments = 24;
    float dth = 2.0f * PI / (float)segments;
    hpfx3d_vec3 center = { sx, sy, -0.98f }; /* Placed slightly in front of apex */

    for (int i = 0; i < segments; i++) {
        float th0 = (float)i * dth;
        float th1 = (float)(i + 1) * dth;

        hpfx3d_vec3 p0 = { sx + r * cosf(th0), sy + r * sinf(th0), -0.98f };
        hpfx3d_vec3 p1 = { sx + r * cosf(th1), sy + r * sinf(th1), -0.98f };

        hpfx3d_draw_triangle_3d(ctx, &center, &p0, &p1, 0x00ffffff);
    }
}

static void save_frame_ppm(const char *filename, const hpfx3d_context *ctx)
{
    if (!ctx || !ctx->fb_mem) return;
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", ctx->width, ctx->height);
    const uint32_t *fb = (const uint32_t*)ctx->fb_mem;

    for (uint32_t y = 0; y < ctx->height; y++) {
        for (uint32_t x = 0; x < ctx->width; x++) {
            uint32_t p = fb[y * ctx->width + x];
            fputc((p >> 16) & 0xff, f);
            fputc((p >> 8) & 0xff, f);
            fputc(p & 0xff, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    int width = 1920;
    int height = 1080;
    const char *out_ppm = "hpfx_render_voronoi.ppm";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_ppm = argv[++i];
    }

    printf("===================================================================\n");
    printf(" HP Visualize FX Hardware Z-Buffer 3D Computational Geometry Demo\n");
    printf(" Target: HP Visualize FX5 / FX10 Architecture (Lego Z-Buffer ASIC)\n");
    printf(" Algorithm: Hoff et al. SIGGRAPH 1999 (GPU Hardware Voronoi)\n");
    printf("===================================================================\n");
    printf("Resolution: %dx%d | Submitting 3D conical geometry to GPU FIFO...\n", width, height);

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to open 3D context\n");
        return 1;
    }
    hpfx3d_resize(ctx, width, height);

    /* Set Orthographic Top-Down Camera with 16:9 widescreen aspect ratio */
    float aspect = (float)width / (float)height;
    hpfx3d_mat4_ortho(&ctx->projection, -aspect, aspect, -1.0f, 1.0f, 0.5f, 10.0f);
    hpfx3d_mat4_identity(&ctx->modelview);
    hpfx3d_update_mvp(ctx);

    /* Configure Native GPU Hardware State:
     * - Depth test ENABLED with LESS condition
     * - Face culling NONE so cone facets rasterize completely
     */
    hpfx3d_set_depth_test(ctx, true, HPFX_DEPTH_LESS);
    hpfx3d_set_cull_mode(ctx, HPFX_CULL_NONE);

    /* Clear framebuffer to background dark navy and Z-buffer to max depth */
    hpfx3d_clear(ctx, 0x000b101b, 0x00ffffff);

    /* Define 16 site coordinates distributed across 16:9 widescreen domain */
    voronoi_site_t sites[NUM_SITES] = {
        { -1.35f, -0.65f, s_palette[0]  },
        { -0.85f, -0.72f, s_palette[1]  },
        { -0.20f, -0.60f, s_palette[2]  },
        {  0.55f, -0.68f, s_palette[3]  },
        {  1.25f, -0.55f, s_palette[4]  },
        { -1.40f, -0.10f, s_palette[5]  },
        { -0.65f, -0.18f, s_palette[6]  },
        {  0.10f, -0.15f, s_palette[7]  },
        {  0.80f, -0.05f, s_palette[8]  },
        {  1.40f,  0.10f, s_palette[9]  },
        { -1.25f,  0.48f, s_palette[10] },
        { -0.50f,  0.35f, s_palette[11] },
        {  0.25f,  0.42f, s_palette[12] },
        {  0.95f,  0.55f, s_palette[13] },
        { -0.90f,  0.78f, s_palette[14] },
        {  0.40f,  0.80f, s_palette[15] }
    };

    /* Submit 3D right circular cones to the GPU.
     * The hardware 24-bit Z-buffer computes the Voronoi tessellation in silicon! */
    for (int i = 0; i < NUM_SITES; i++) {
        draw_site_cone(ctx, sites[i].x, sites[i].y, 2.6f, 2.6f, sites[i].color);
    }

    /* Draw white site locator markers at cone apexes */
    for (int i = 0; i < NUM_SITES; i++) {
        draw_site_marker(ctx, sites[i].x, sites[i].y);
    }

    /* Flush command FIFO to trigger hardware rasterization */
    hpfx3d_flush(ctx);
    hpfx3d_sync(ctx);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);

    printf("[Export] Writing rendered Voronoi diagram to: %s\n", out_ppm);
    save_frame_ppm(out_ppm, ctx);
    printf("[Checksum] IEEE 802.3 Framebuffer CRC32: 0x%08X\n", crc);

    hpfx3d_close(ctx);
    printf("Hardware Z-Buffer 3D Voronoi demonstration completed successfully.\n");
    return 0;
}
