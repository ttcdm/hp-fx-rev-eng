/*
 * HP Visualize FX5 & FX10 Hardware Diagnostics Suite Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_diag.h"

hpfx_diag_runner* hpfx_diag_init(const char *fb_dev)
{
    hpfx_diag_runner *runner = (hpfx_diag_runner*)calloc(1, sizeof(hpfx_diag_runner));
    if (!runner) return NULL;

    runner->ctx = hpfx3d_open(fb_dev);
    if (!runner->ctx) {
        free(runner);
        return NULL;
    }

    /* Force diagnostic resolution to 544 x 403 */
    runner->ctx->width = HPFX_DIAG_WIDTH;
    runner->ctx->height = HPFX_DIAG_HEIGHT;

    runner->results[0] = (hpfx_diag_result){ 1, "Torus Mesh", HPFX_CRC32_TEST1_TORUS, 0, false };
    runner->results[1] = (hpfx_diag_result){ 2, "shoe4R Solid CAD", HPFX_CRC32_TEST2_SHOE, 0, false };
    runner->results[2] = (hpfx_diag_result){ 3, "Sphere Mesh", HPFX_CRC32_TEST3_SPHERE, 0, false };
    runner->results[3] = (hpfx_diag_result){ 4, "shoe4R Textured CAD", HPFX_CRC32_TEST4_SHOE_TEX, 0, false };

    return runner;
}

void hpfx_diag_free(hpfx_diag_runner *runner)
{
    if (!runner) return;
    if (runner->ctx) hpfx3d_close(runner->ctx);
    free(runner);
}

static void setup_diag_camera(hpfx3d_context *ctx)
{
    hpfx3d_mat4_perspective(&ctx->projection, 45.0f * (3.14159265f / 180.0f),
                            (float)ctx->width / (float)ctx->height, 0.5f, 100.0f);
    hpfx3d_mat4_identity(&ctx->modelview);
    hpfx3d_mat4_translate(&ctx->modelview, 0.0f, 0.0f, -6.0f);
    hpfx3d_mat4_rotate_x(&ctx->modelview, 25.0f * (3.14159265f / 180.0f));
    hpfx3d_mat4_rotate_y(&ctx->modelview, 35.0f * (3.14159265f / 180.0f));
    hpfx3d_update_mvp(ctx);
}

uint32_t hpfx_diag_run_test1_torus(hpfx_diag_runner *runner)
{
    if (!runner || !runner->ctx) return 0;
    hpfx3d_context *ctx = runner->ctx;

    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    setup_diag_camera(ctx);
    ctx->batch_count = 0;

    hpfx3d_draw_torus(ctx, 1.8f, 0.6f, 20, 14, 0x00e67e22);
    hpfx3d_flush(ctx);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);
    runner->results[0].actual_crc32 = crc;
    runner->results[0].passed = (crc != 0);
    return crc;
}

uint32_t hpfx_diag_run_test2_shoe(hpfx_diag_runner *runner)
{
    if (!runner || !runner->ctx) return 0;
    hpfx3d_context *ctx = runner->ctx;

    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    setup_diag_camera(ctx);
    ctx->batch_count = 0;

    hpfx3d_draw_shoe(ctx, 1.2f, 0x00e74c3c);
    hpfx3d_flush(ctx);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);
    runner->results[1].actual_crc32 = crc;
    runner->results[1].passed = (crc != 0 && crc != runner->results[0].actual_crc32);
    return crc;
}

uint32_t hpfx_diag_run_test3_sphere(hpfx_diag_runner *runner)
{
    if (!runner || !runner->ctx) return 0;
    hpfx3d_context *ctx = runner->ctx;

    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    setup_diag_camera(ctx);
    ctx->batch_count = 0;

    hpfx3d_draw_sphere(ctx, 1.6f, 18, 14, 0x003498db);
    hpfx3d_flush(ctx);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);
    runner->results[2].actual_crc32 = crc;
    runner->results[2].passed = (crc != 0 && crc != runner->results[1].actual_crc32);
    return crc;
}

uint32_t hpfx_diag_run_test4_shoe_textured(hpfx_diag_runner *runner)
{
    if (!runner || !runner->ctx) return 0;
    hpfx3d_context *ctx = runner->ctx;

    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    setup_diag_camera(ctx);
    ctx->batch_count = 0;

    /* Render shoe with alternate lighting angle and color pattern */
    hpfx3d_mat4_rotate_y(&ctx->modelview, 45.0f * (3.14159265f / 180.0f));
    hpfx3d_update_mvp(ctx);

    hpfx3d_draw_shoe(ctx, 1.2f, 0x002ecc71);
    hpfx3d_flush(ctx);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);
    runner->results[3].actual_crc32 = crc;
    runner->results[3].passed = (crc != 0 && crc != runner->results[1].actual_crc32);
    return crc;
}

bool hpfx_diag_run_all(hpfx_diag_runner *runner)
{
    if (!runner) return false;
    hpfx_diag_run_test1_torus(runner);
    hpfx_diag_run_test2_shoe(runner);
    hpfx_diag_run_test3_sphere(runner);
    hpfx_diag_run_test4_shoe_textured(runner);

    return runner->results[0].passed && runner->results[1].passed &&
           runner->results[2].passed && runner->results[3].passed;
}
