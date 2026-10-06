/*
 * HP Visualize FX 3D Acceleration Demonstration Program
 * Reverse Engineered for HP Visualize FX5 / FX10 ("Lego" Architecture)
 *
 * Demonstrates:
 * - Hardware 3D pipeline initialization & IOCTL verification
 * - Matrix mathematics (Perspective Projection, View, Model Rotation)
 * - Hardware Z-Buffering (Depth test GL_LESS)
 * - Hardware Back-face Culling (OpenGL GL_CCW rule)
 * - Fast batch triangle submission (HPFP14 DMA commands)
 * - HP Diagnostics Geometric Meshes: Torus, Sphere, Cube, Pyramid
 * - Real-time rendering performance metrics & PPM screenshot export
 */

#include "hpfx3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void save_ppm(const char *filename, const uint32_t *pixels, uint32_t width, uint32_t height)
{
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot open output image file %s\n", filename);
        return;
    }

    fprintf(f, "P6\n%u %u\n255\n", width, height);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t c = pixels[y * width + x];
            unsigned char rgb[3];
            rgb[0] = (c >> 16) & 0xff; /* Red */
            rgb[1] = (c >> 8) & 0xff;  /* Green */
            rgb[2] = c & 0xff;         /* Blue */
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    printf("[Demo] Saved rendered frame screenshot to: %s (%ux%u)\n", filename, width, height);
}

/* Draw a 3D Pyramid */
static void draw_pyramid(hpfx3d_context *ctx, float base, float height)
{
    float b = base * 0.5f;
    hpfx3d_vec3 apex = {0.0f, height * 0.6f, 0.0f};
    hpfx3d_vec3 v0 = {-b, -height * 0.4f,  b};
    hpfx3d_vec3 v1 = { b, -height * 0.4f,  b};
    hpfx3d_vec3 v2 = { b, -height * 0.4f, -b};
    hpfx3d_vec3 v3 = {-b, -height * 0.4f, -b};

    /* Front: Gold/Yellow */
    hpfx3d_draw_triangle_3d(ctx, &apex, &v0, &v1, 0x00f39c12);
    /* Right: Magenta */
    hpfx3d_draw_triangle_3d(ctx, &apex, &v1, &v2, 0x009b59b6);
    /* Back: Cyan */
    hpfx3d_draw_triangle_3d(ctx, &apex, &v2, &v3, 0x001abc9c);
    /* Left: Emerald Green */
    hpfx3d_draw_triangle_3d(ctx, &apex, &v3, &v0, 0x002ecc71);
    /* Base: Dark Violet */
    hpfx3d_draw_triangle_3d(ctx, &v0, &v2, &v1, 0x0034495e);
    hpfx3d_draw_triangle_3d(ctx, &v0, &v3, &v2, 0x002c3e50);
}

enum demo_mesh_mode {
    MESH_ALL = 0,
    MESH_CUBE,
    MESH_TORUS,
    MESH_SPHERE
};

int main(int argc, char **argv)
{
    const char *fb_dev = "/dev/fb0";
    const char *ppm_out = "hpfx_3d_render.ppm";
    int num_frames = 60;
    bool export_image = true;
    enum demo_mesh_mode mesh_mode = MESH_ALL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            fb_dev = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            num_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            ppm_out = argv[++i];
        } else if (strcmp(argv[i], "--torus") == 0) {
            mesh_mode = MESH_TORUS;
        } else if (strcmp(argv[i], "--sphere") == 0) {
            mesh_mode = MESH_SPHERE;
        } else if (strcmp(argv[i], "--cube") == 0) {
            mesh_mode = MESH_CUBE;
        } else if (strcmp(argv[i], "--no-ppm") == 0) {
            export_image = false;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("HP Visualize FX 3D Hardware Demo\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("  -d <device>  Framebuffer device (default: /dev/fb0)\n");
            printf("  -f <frames>  Number of frames to render (default: 60)\n");
            printf("  -o <file>    Output screenshot file (default: hpfx_3d_render.ppm)\n");
            printf("  --torus      Render HP Diagnostic Torus mesh\n");
            printf("  --sphere     Render HP Diagnostic Sphere mesh\n");
            printf("  --cube       Render 3D reference Cube\n");
            printf("  --no-ppm     Do not output PPM screenshot\n");
            return 0;
        }
    }

    printf("=========================================================\n");
    printf(" HP Visualize FX 3D Pipeline Hardware Demonstration\n");
    printf(" Target: HP Visualize FX5 / FX10 Workstation Graphics\n");
    printf("=========================================================\n");

    hpfx3d_context *ctx = hpfx3d_open(fb_dev);
    if (!ctx) {
        fprintf(stderr, "Fatal error: Failed to create 3D context\n");
        return 1;
    }

    printf("[Demo] Resolution: %ux%u @ %ubpp (Pitch: %u bytes)\n",
           ctx->width, ctx->height, ctx->bpp, ctx->pitch);
    printf("[Demo] Acceleration Status: %s\n",
           ctx->is_hw_accel ? "3D HARDWARE ACCELERATED (HP Visualize FX)" : "Software Rasterizer Fallback");

    /* Setup Camera & Perspective Matrix */
    float aspect = (float)ctx->width / (float)ctx->height;
    hpfx3d_mat4_perspective(&ctx->projection, 45.0f * (3.14159265f / 180.0f), aspect, 0.5f, 100.0f);

    /* Setup 3D Hardware Render State */
    hpfx3d_set_depth_test(ctx, true, HPFX_DEPTH_LESS);
    hpfx3d_set_cull_mode(ctx, HPFX_CULL_BACK);

    printf("[Demo] Beginning rendering loop (%d frames)...\n", num_frames);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    float angle_x = 25.0f;
    float angle_y = 35.0f;
    uint32_t total_triangles = 0;

    for (int frame = 0; frame < num_frames; frame++) {
        hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);

        if (mesh_mode == MESH_TORUS) {
            /* Full Screen 3D Torus (HP Diag Test #1) */
            hpfx3d_mat4_identity(&ctx->modelview);
            hpfx3d_mat4_translate(&ctx->modelview, 0.0f, 0.0f, -4.5f);
            hpfx3d_mat4_rotate_x(&ctx->modelview, angle_x * (3.14159265f / 180.0f));
            hpfx3d_mat4_rotate_y(&ctx->modelview, angle_y * (3.14159265f / 180.0f));
            hpfx3d_update_mvp(ctx);

            hpfx3d_draw_torus(ctx, 1.3f, 0.45f, 24, 20, 0x00e67e22);
            total_triangles += (24 * 20 * 2);
        } else if (mesh_mode == MESH_SPHERE) {
            /* Full Screen 3D Sphere (HP Diag Test #3) */
            hpfx3d_mat4_identity(&ctx->modelview);
            hpfx3d_mat4_translate(&ctx->modelview, 0.0f, 0.0f, -5.0f);
            hpfx3d_mat4_rotate_y(&ctx->modelview, angle_y * (3.14159265f / 180.0f));
            hpfx3d_mat4_rotate_x(&ctx->modelview, 25.0f * (3.14159265f / 180.0f));
            hpfx3d_update_mvp(ctx);

            hpfx3d_draw_sphere(ctx, 1.6f, 20, 16, 0x003498db);
            total_triangles += (20 * 16 * 2);
        } else if (mesh_mode == MESH_CUBE) {
            /* 3D Cube */
            hpfx3d_mat4_identity(&ctx->modelview);
            hpfx3d_mat4_translate(&ctx->modelview, 0.0f, 0.0f, -5.0f);
            hpfx3d_mat4_rotate_x(&ctx->modelview, angle_x * (3.14159265f / 180.0f));
            hpfx3d_mat4_rotate_y(&ctx->modelview, angle_y * (3.14159265f / 180.0f));
            hpfx3d_update_mvp(ctx);

            hpfx3d_draw_cube(ctx, 2.0f);
            total_triangles += 12;
        } else {
            /* MESH_ALL: Multi-Object Showcase (Torus, Sphere, Cube, Pyramid) */
            /* 1. Spinning 3D Cube (Upper Left: x = -1.8, y = +0.8) */
            hpfx3d_mat4_identity(&ctx->modelview);
            hpfx3d_mat4_translate(&ctx->modelview, -1.8f, 0.9f, -6.5f);
            hpfx3d_mat4_rotate_x(&ctx->modelview, angle_x * (3.14159265f / 180.0f));
            hpfx3d_mat4_rotate_y(&ctx->modelview, angle_y * (3.14159265f / 180.0f));
            hpfx3d_update_mvp(ctx);
            hpfx3d_draw_cube(ctx, 1.4f);
            total_triangles += 12;

            /* 2. Spinning 3D Pyramid (Lower Left: x = -1.8, y = -1.0) */
            hpfx3d_mat4_identity(&ctx->modelview);
            hpfx3d_mat4_translate(&ctx->modelview, -1.8f, -1.0f, -6.5f);
            hpfx3d_mat4_rotate_y(&ctx->modelview, -angle_y * 1.2f * (3.14159265f / 180.0f));
            hpfx3d_mat4_rotate_x(&ctx->modelview, 20.0f * (3.14159265f / 180.0f));
            hpfx3d_update_mvp(ctx);
            draw_pyramid(ctx, 1.5f, 1.7f);
            total_triangles += 6;

            /* 3. Spinning 3D Torus (Right: x = +1.6, y = 0.0) */
            hpfx3d_mat4_identity(&ctx->modelview);
            hpfx3d_mat4_translate(&ctx->modelview, 1.7f, 0.0f, -6.0f);
            hpfx3d_mat4_rotate_x(&ctx->modelview, angle_x * 0.9f * (3.14159265f / 180.0f));
            hpfx3d_mat4_rotate_y(&ctx->modelview, angle_y * 1.1f * (3.14159265f / 180.0f));
            hpfx3d_update_mvp(ctx);
            hpfx3d_draw_torus(ctx, 1.1f, 0.38f, 18, 14, 0x00e67e22);
            total_triangles += (18 * 14 * 2);
        }

        /* Flush batch and sync hardware pipeline */
        hpfx3d_flush(ctx);
        hpfx3d_sync(ctx);

        /* Advance rotation animation */
        angle_x += 1.2f;
        angle_y += 1.8f;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_sec = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
    double fps = num_frames / elapsed_sec;
    double tri_per_sec = total_triangles / elapsed_sec;

    printf("[Demo] Rendered %d frames (%u triangles) in %.3f seconds.\n",
           num_frames, total_triangles, elapsed_sec);
    printf("[Demo] Performance: %.2f FPS | %.0f Triangles/sec\n", fps, tri_per_sec);

    uint32_t final_crc = hpfx3d_checksum_framebuffer(ctx);
    printf("[Demo] Final Framebuffer Checksum (CRC32): 0x%08X\n", final_crc);

    if (export_image && ctx->fb_mem) {
        save_ppm(ppm_out, (const uint32_t*)ctx->fb_mem, ctx->width, ctx->height);
    }

    hpfx3d_close(ctx);
    printf("[Demo] 3D demonstration completed successfully.\n");
    return 0;
}
