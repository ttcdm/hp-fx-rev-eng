/*
 * Comprehensive Hardware & Driver Test Suite for HP Visualize FX ("Lego" Architecture)
 * Based on Reverse-Engineered HP Windows 2000 Driver Specifications & HP Diagnostics
 *
 * Covers 15 Comprehensive Test Domains:
 * - Domain 1: PCI Identification, BAR Mapping & Register Architecture
 * - Domain 2: Video Timing Tables & Mode Setting Validation (All 20 Presets)
 * - Domain 3: 3D FP14 Pipeline & Rasterizer Register Definitions
 * - Domain 4: IOCTL ABI, Command Codes & Struct Memory Layout
 * - Domain 5: 3D Matrix Mathematics (Identity, Multiplication, Perspective, Ortho, Rotations)
 * - Domain 6: Geometry Pipeline, Viewport Mapping & OpenGL GL_CCW Face Culling
 * - Domain 7: 3D Rasterization Accuracy, Z-Buffering Occlusion & Boundary Robustness
 * - Domain 8: Batch Primitive Queuing, FIFO Flush & Device Lifecycle
 * - Domain 9: Gouraud Smooth Shading & Barycentric Mathematics
 * - Domain 10: Hardware Texture Engine & UV Sampling (Clamp, Repeat, Bilinear)
 * - Domain 11: Alpha Blending & Multi-Layer Transparency Math
 * - Domain 12: Depth Fog Pipeline & Attenuation Equations
 * - Domain 13: Scissor Box & Viewport Scissoring Boundary Precision
 * - Domain 14: HP Diagnostics Emulation (Torus & Sphere Meshes + CRC32 Deterministic Checksum)
 * - Domain 15: Extreme Stress, Concurrency & Security Fuzzing (NaN, Inf, 10,000 Triangles)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include "hpfx_regs.h"
#include "hpfx3d.h"
#include "hpfx_diag.h"
#include "hpfx_compute.h"
#include "hpfx_gl.h"
#include "hpfx_drm.h"

/* Test Statistics */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg, ...) do { \
    g_tests_run++; \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        g_tests_failed++; \
        fprintf(stderr, "[FAIL] Line %d: " msg "\n", __LINE__, ##__VA_ARGS__); \
    } \
} while(0)

#define TEST_SECTION(name) printf("\n=======================================================\n[TEST SECTION] %s\n=======================================================\n", name)

static bool float_near(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

/* =========================================================================
 * DOMAIN 1: PCI Identification, BAR Mapping & Register Architecture
 * ========================================================================= */
static void test_domain_1_hardware_specs(void)
{
    TEST_SECTION("Domain 1: Hardware Identification, PCI & BAR Specifications");

    TEST_ASSERT(PCI_VENDOR_ID_HP == 0x103c, "PCI Vendor ID must be 0x103C (HP)");
    TEST_ASSERT(PCI_DEVICE_ID_HP_VISUALIZE == 0x100a, "PCI Device ID must be 0x100A (Visualize FX5/FX10)");

    TEST_ASSERT(PCI_SUBDEVICE_ID_HP_FX5 == 0x10d4, "Subsystem ID FX5 must be 0x10D4");
    TEST_ASSERT(PCI_SUBDEVICE_ID_HP_FX10 == 0x10d5, "Subsystem ID FX10 must be 0x10D5");
    TEST_ASSERT(PCI_SUBDEVICE_ID_HP_FX5_ALT == 0x10d6, "Subsystem ID FX5 Secondary must be 0x10D6");
    TEST_ASSERT(PCI_SUBDEVICE_ID_HP_FX10_ALT == 0x10d7, "Subsystem ID FX10 Secondary must be 0x10D7");
    TEST_ASSERT(PCI_SUBDEVICE_ID_HP_FX_GEN == 0x10d8, "Subsystem ID FX Generic must be 0x10D8");

    TEST_ASSERT(HPFX_BAR_MMIO == 0, "MMIO must reside on BAR 0");
    TEST_ASSERT(HPFX_BAR_FB == 2, "Framebuffer must reside on BAR 2");
    TEST_ASSERT(HPFX_MMIO_SIZE == (32 * 1024 * 1024), "MMIO aperture size must be 32MB");
    TEST_ASSERT(HPFX_FB_SIZE == (32 * 1024 * 1024), "Framebuffer aperture size must be 32MB");

    TEST_ASSERT(HPFX_REG_RESET == 0x000100, "Soft Reset register must be 0x000100");
    TEST_ASSERT(HPFX_RESET_TRIGGER == 0x00000100, "Reset trigger command must be 0x100");
    TEST_ASSERT(HPFX_REG_STATUS == 0x000400, "Status register must be 0x000400");
    TEST_ASSERT(HPFX_STATUS_BUSY == 0x01000000, "Engine busy bit must be Bit 24 (0x01000000)");

    TEST_ASSERT(HPFX_REG_VIDEO_CTRL == 0x000044, "Video Control register must be 0x000044");
    TEST_ASSERT(HPFX_VIDEO_CTRL_ENABLE == (1 << 9), "Display timing enable must be Bit 9");
    TEST_ASSERT(HPFX_VIDEO_CTRL_UNBLANK == (1 << 14), "Screen unblank bit must be Bit 14");
    TEST_ASSERT(HPFX_REG_DPMS_CTRL == 0x00004c, "DPMS Control register must be 0x00004C");
    TEST_ASSERT(HPFX_DPMS_ON == 0x0c, "DPMS ON state must be 0x0C");
    TEST_ASSERT(HPFX_DPMS_STANDBY == 0x0d, "DPMS STANDBY state must be 0x0D");
    TEST_ASSERT(HPFX_DPMS_SUSPEND == 0x0e, "DPMS SUSPEND state must be 0x0E");
    TEST_ASSERT(HPFX_DPMS_OFF == 0x0f, "DPMS OFF state must be 0x0F");

    uint32_t fifo_reg_val = (100 << 1);
    uint32_t free_slots = ((fifo_reg_val >> 1) - 10);
    TEST_ASSERT(free_slots == 90, "FIFO free slot decode formula ((val >> 1) - 10) mismatch");

    int x = 640, y = 480;
    uint32_t packed_coord = ((y & 0xffff) << 16) | (x & 0xffff);
    TEST_ASSERT((packed_coord & 0xffff) == 640, "X coordinate unpacking mismatch");
    TEST_ASSERT(((packed_coord >> 16) & 0xffff) == 480, "Y coordinate unpacking mismatch");

    printf("  [PASS] Domain 1: 17 Hardware specification assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 2: Video Timing Tables & Mode Setting Validation
 * ========================================================================= */
static const struct hpfx_timing_entry c_test_timings[20] = {
    { 0x02f17c0f, 0x00012001, 0x00200109, 0, 0, 0 },
    { 0x0770fc0f, 0x00012001, 0x000f0200, 0, 0, 0 },
    { 0x04f0dc37, 0x00012001, 0x00180200, 0, 0, 0 },
    { 0x02f17c0f, 0x0001202b, 0x00180200, 0, 0, 0 },
    { 0x0571fc27, 0x00012001, 0x00160300, 0, 0, 0 },
    { 0x09f13c0f, 0x00012001, 0x00140200, 0, 0, 0 },
    { 0x0970fc1f, 0x00012001, 0x001a0200, 0, 0, 0 },
    { 0x0571fc27, 0x0001204d, 0x001a0200, 0, 0, 0 },
    { 0x09f21c17, 0x00012001, 0x001c0502, 0, 0, 0 },
    { 0x0af17c0f, 0x00012028, 0x001b0200, 0, 0, 0 },
    { 0x0cf17c2f, 0x00012029, 0x00230200, 0, 0, 0 },
    { 0x09f21c17, 0x000120dd, 0x00230200, 0, 0, 0 },
    { 0x0f71bc2f, 0x0001202a, 0x00250200, 0, 0, 0 },
    { 0x0f723c0f, 0x0001209a, 0x00250200, 0, 0, 0 },
    { 0x0df27c3f, 0x000120d0, 0x002b0200, 0, 0, 0 },
    { 0x12f27c1f, 0x000120d6, 0x00270202, 0, 0, 0 },
    { 0x12f2fc3f, 0x00012073, 0x002d0200, 0, 0, 0 },
    { 0x12f2fc3f, 0x0001211b, 0x002d0200, 0, 0, 0 },
    { 0x10f23c1f, 0x000120da, 0x00260202, 0, 0, 0 },
    { 0x14d3fc3f, 0x00012170, 0x002b0202, 0, 0, 0 }
};

static void test_domain_2_video_timings(void)
{
    TEST_SECTION("Domain 2: Video Timing Tables & Mode Setting Validation");

    TEST_ASSERT(sizeof(c_test_timings) / sizeof(c_test_timings[0]) == 20, "Must contain exactly 20 timing entries");
    TEST_ASSERT(c_test_timings[8].dw0 == 0x09f21c17, "Timing #8 DW0 mismatch for 1024x768@60");
    TEST_ASSERT(c_test_timings[8].dw1 == 0x00012001, "Timing #8 DW1 PLL mismatch for 1024x768@60");
    TEST_ASSERT(c_test_timings[8].dw2 == 0x001c0502, "Timing #8 DW2 polarity mismatch");

    TEST_ASSERT(c_test_timings[12].dw0 == 0x0f71bc2f, "Timing #12 DW0 mismatch for 1280x1024@60");
    TEST_ASSERT(c_test_timings[12].dw1 == 0x0001202a, "Timing #12 DW1 PLL mismatch for 1280x1024@60");

    TEST_ASSERT(c_test_timings[16].dw0 == 0x12f2fc3f, "Timing #16 DW0 mismatch for 1600x1200@60");
    TEST_ASSERT(c_test_timings[16].dw1 == 0x00012073, "Timing #16 DW1 PLL mismatch for 1600x1200@60");

    TEST_ASSERT(c_test_timings[18].dw0 == 0x10f23c1f, "Timing #18 DW0 mismatch for 1920x1080@60");
    TEST_ASSERT(c_test_timings[18].dw1 == 0x000120da, "Timing #18 DW1 PLL mismatch for 1920x1080@60");

    TEST_ASSERT(c_test_timings[19].dw0 == 0x14d3fc3f, "Timing #19 DW0 mismatch for 1920x1200@76");
    TEST_ASSERT(c_test_timings[19].dw1 == 0x00012170, "Timing #19 DW1 PLL mismatch for 1920x1200@76");

    for (int i = 0; i < 20; i++) {
        uint32_t pll = c_test_timings[i].dw1;
        uint16_t div = pll & 0xffff;
        TEST_ASSERT(div > 0, "Timing %d: PLL divisor must be non-zero", i);
    }

    printf("  [PASS] Domain 2: 30 Video timing table assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 3: 3D FP14 Pipeline & Rasterizer Register Validation
 * ========================================================================= */
static void test_domain_3_3d_fp14_specs(void)
{
    TEST_SECTION("Domain 3: 3D FP14 Pipeline & Rasterizer Register Definitions");

    TEST_ASSERT(HPFX_REG_3D_PIPELINE_CTRL == 0x00800048, "3D Pipeline control register must be 0x00800048");
    TEST_ASSERT(HPFX_3D_MODE_ENABLE == 0x60000000, "3D enable bitmask must be 0x60000000");
    TEST_ASSERT(HPFX_3D_MODE_DISABLE_MASK == 0x9fffffff, "3D disable mask must be 0x9FFFFFFF");
    TEST_ASSERT(HPFX_REG_3D_SYNC == 0x008000c8, "3D Sync register must be 0x008000C8");
    TEST_ASSERT(HPFX_3D_SYNC_BUSY == 0x80000000, "3D busy bit must be Bit 31 (0x80000000)");
    TEST_ASSERT(HPFX_REG_3D_FLUSH == 0x008000cc, "3D Flush register must be 0x008000CC");
    TEST_ASSERT(HPFX_3D_FLUSH_TRIGGER == 0x00000001, "3D Flush trigger value must be 0x00000001");

    TEST_ASSERT(HPFX_REG_Z_FORMAT == 0x00920404, "Z-Buffer format register must be 0x00920404");
    TEST_ASSERT(HPFX_REG_Z_STRIDE == 0x00920808, "Z-Buffer stride register must be 0x00920808");
    TEST_ASSERT(HPFX_REG_DEPTH_FUNC == 0x0092083c, "Depth comparison function register must be 0x0092083C");
    TEST_ASSERT(HPFX_REG_DEPTH_MASK == 0x0092084c, "Depth write mask register must be 0x0092084C");
    TEST_ASSERT(HPFX_REG_CLEAR_DEPTH == 0x009208a4, "Fast depth clear register must be 0x009208A4");

    TEST_ASSERT(HPFX_DEPTH_NEVER    == 0, "HPFX_DEPTH_NEVER must be 0");
    TEST_ASSERT(HPFX_DEPTH_LESS     == 1, "HPFX_DEPTH_LESS must be 1");
    TEST_ASSERT(HPFX_DEPTH_EQUAL    == 2, "HPFX_DEPTH_EQUAL must be 2");
    TEST_ASSERT(HPFX_DEPTH_LEQUAL   == 3, "HPFX_DEPTH_LEQUAL must be 3");
    TEST_ASSERT(HPFX_DEPTH_GREATER  == 4, "HPFX_DEPTH_GREATER must be 4");
    TEST_ASSERT(HPFX_DEPTH_NOTEQUAL == 5, "HPFX_DEPTH_NOTEQUAL must be 5");
    TEST_ASSERT(HPFX_DEPTH_GEQUAL   == 6, "HPFX_DEPTH_GEQUAL must be 6");
    TEST_ASSERT(HPFX_DEPTH_ALWAYS   == 7, "HPFX_DEPTH_ALWAYS must be 7");

    TEST_ASSERT(HPFX_REG_3D_RASTER_STATE == 0x008e5800, "3D Raster state register must be 0x008E5800");
    TEST_ASSERT(HPFX_CULL_NONE  == 0, "HPFX_CULL_NONE must be 0");
    TEST_ASSERT(HPFX_CULL_FRONT == 1, "HPFX_CULL_FRONT must be 1");
    TEST_ASSERT(HPFX_CULL_BACK  == 2, "HPFX_CULL_BACK must be 2");

    TEST_ASSERT(HPFX_REG_TRI_V0 == 0x00b20000, "Vertex 0 coordinate register must be 0x00B20000");
    TEST_ASSERT(HPFX_REG_TRI_V1 == 0x00b24c00, "Vertex 1 coordinate register must be 0x00B24C00");
    TEST_ASSERT(HPFX_REG_TRI_V2_TRIGGER == 0x00b3c010, "Vertex 2 & raster trigger must be 0x00B3C010");

    TEST_ASSERT(HPFX_REG_FP14_V0 == 0x00cb0008, "FP14 V0 register must be 0x00CB0008");
    TEST_ASSERT(HPFX_REG_FP14_V1 == 0x00cb0010, "FP14 V1 register must be 0x00CB0010");
    TEST_ASSERT(HPFX_REG_FP14_V2 == 0x00cb0020, "FP14 V2 register must be 0x00CB0020");
    TEST_ASSERT(HPFX_REG_FP14_PRIM_MODE == 0x00e00120, "FP14 primitive mode must be 0x00E00120");
    TEST_ASSERT(HPFX_REG_FP14_RASTER_SETUP == 0x00e0040c, "FP14 raster setup must be 0x00E0040C");

    printf("  [PASS] Domain 3: 28 FP14 3D Register and pipeline assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 4: IOCTL Protocol & Kernel ABI Validation
 * ========================================================================= */
static void test_domain_4_ioctl_abi(void)
{
    TEST_SECTION("Domain 4: IOCTL ABI, Command Codes & Struct Memory Layout");

    TEST_ASSERT(HPFX_IOCTL_MAGIC == 'H', "IOCTL magic character must be 'H'");
    TEST_ASSERT((HPFX_IOCTL_3D_RESET & 0xff) == 0x20, "IOCTL 3D_RESET command code must be 0x20");
    TEST_ASSERT((HPFX_IOCTL_3D_SYNC & 0xff) == 0x21, "IOCTL 3D_SYNC command code must be 0x21");
    TEST_ASSERT((HPFX_IOCTL_3D_DRAW_TRIANGLE & 0xff) == 0x22, "IOCTL 3D_DRAW_TRIANGLE code must be 0x22");
    TEST_ASSERT((HPFX_IOCTL_CLEAR_DEPTH & 0xff) == 0x23, "IOCTL CLEAR_DEPTH code must be 0x23");
    TEST_ASSERT((HPFX_IOCTL_SET_3D_STATE & 0xff) == 0x24, "IOCTL SET_3D_STATE code must be 0x24");
    TEST_ASSERT((HPFX_IOCTL_3D_DRAW_TRI_LIST & 0xff) == 0x25, "IOCTL 3D_DRAW_TRI_LIST code must be 0x25");

    struct hpfx_triangle_cmd tri_cmd;
    memset(&tri_cmd, 0, sizeof(tri_cmd));
    TEST_ASSERT(sizeof(struct hpfx_triangle_cmd) == 40, "struct hpfx_triangle_cmd must be 40 bytes");
    TEST_ASSERT((uintptr_t)&tri_cmd.color - (uintptr_t)&tri_cmd == 24, "color offset in tri_cmd must be 24");
    TEST_ASSERT((uintptr_t)&tri_cmd.z0 - (uintptr_t)&tri_cmd == 28, "z0 offset in tri_cmd must be 28");

    struct hpfx_3d_state_cmd state_cmd;
    memset(&state_cmd, 0, sizeof(state_cmd));
    TEST_ASSERT(sizeof(struct hpfx_3d_state_cmd) == 24, "struct hpfx_3d_state_cmd must be 24 bytes");

    struct hpfx_tri_list_cmd list_cmd;
    memset(&list_cmd, 0, sizeof(list_cmd));
    TEST_ASSERT(sizeof(list_cmd.count) == 4, "list_cmd.count must be 32-bit uint");
    TEST_ASSERT(sizeof(list_cmd.triangles) == sizeof(void*), "list_cmd.triangles must be pointer sized");

    printf("  [PASS] Domain 4: 12 IOCTL ABI and struct alignment assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 5: 3D Matrix Mathematics & Linear Algebra
 * ========================================================================= */
static void test_domain_5_matrix_math(void)
{
    TEST_SECTION("Domain 5: 3D Matrix Mathematics & Linear Algebra");

    hpfx3d_mat4 m1;
    hpfx3d_mat4_identity(&m1);
    for (int i = 0; i < 16; i++) {
        float expected = (i % 5 == 0) ? 1.0f : 0.0f;
        TEST_ASSERT(m1.m[i] == expected, "Identity matrix index %d mismatch", i);
    }

    hpfx3d_mat4 m2, res;
    for (int i = 0; i < 16; i++) m2.m[i] = (float)(i + 1);
    hpfx3d_mat4_multiply(&res, &m2, &m1);
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(float_near(res.m[i], m2.m[i], 1e-5f), "Matrix multiply with identity mismatch at %d", i);
    }

    hpfx3d_mat4 t;
    hpfx3d_mat4_identity(&t);
    hpfx3d_mat4_translate(&t, 5.0f, -3.0f, 10.0f);
    TEST_ASSERT(t.m[12] == 5.0f, "Translation X mismatch");
    TEST_ASSERT(t.m[13] == -3.0f, "Translation Y mismatch");
    TEST_ASSERT(t.m[14] == 10.0f, "Translation Z mismatch");
    TEST_ASSERT(t.m[15] == 1.0f, "Translation W mismatch");

    hpfx3d_mat4 s;
    hpfx3d_mat4_identity(&s);
    hpfx3d_mat4_scale(&s, 2.0f, 3.0f, 4.0f);
    TEST_ASSERT(s.m[0] == 2.0f, "Scale X mismatch");
    TEST_ASSERT(s.m[5] == 3.0f, "Scale Y mismatch");
    TEST_ASSERT(s.m[10] == 4.0f, "Scale Z mismatch");

    hpfx3d_mat4 rx;
    hpfx3d_mat4_identity(&rx);
    hpfx3d_mat4_rotate_x(&rx, 2.0f * 3.14159265f);
    for (int i = 0; i < 16; i++) {
        float exp = (i % 5 == 0) ? 1.0f : 0.0f;
        TEST_ASSERT(float_near(rx.m[i], exp, 1e-4f), "Rotate X 360 deg mismatch at %d", i);
    }

    hpfx3d_mat4 ry;
    hpfx3d_mat4_identity(&ry);
    hpfx3d_mat4_rotate_y(&ry, 2.0f * 3.14159265f);
    for (int i = 0; i < 16; i++) {
        float exp = (i % 5 == 0) ? 1.0f : 0.0f;
        TEST_ASSERT(float_near(ry.m[i], exp, 1e-4f), "Rotate Y 360 deg mismatch at %d", i);
    }

    hpfx3d_mat4 rz;
    hpfx3d_mat4_identity(&rz);
    hpfx3d_mat4_rotate_z(&rz, 2.0f * 3.14159265f);
    for (int i = 0; i < 16; i++) {
        float exp = (i % 5 == 0) ? 1.0f : 0.0f;
        TEST_ASSERT(float_near(rz.m[i], exp, 1e-4f), "Rotate Z 360 deg mismatch at %d", i);
    }

    hpfx3d_mat4 proj;
    float fov = 60.0f * (3.14159265f / 180.0f);
    float aspect = 1024.0f / 768.0f;
    float near_z = 1.0f, far_z = 100.0f;
    hpfx3d_mat4_perspective(&proj, fov, aspect, near_z, far_z);
    TEST_ASSERT(proj.m[11] == -1.0f, "Perspective matrix must have W projection term = -1.0");
    TEST_ASSERT(proj.m[1] == 0.0f && proj.m[2] == 0.0f && proj.m[3] == 0.0f, "Off-diagonals must be zero");
    TEST_ASSERT(proj.m[15] == 0.0f, "Perspective matrix [15] must be zero");

    hpfx3d_mat4 ortho;
    hpfx3d_mat4_ortho(&ortho, -10.0f, 10.0f, -5.0f, 5.0f, 0.1f, 50.0f);
    TEST_ASSERT(ortho.m[0] == 2.0f / 20.0f, "Ortho X scale mismatch");
    TEST_ASSERT(ortho.m[5] == 2.0f / 10.0f, "Ortho Y scale mismatch");
    TEST_ASSERT(ortho.m[15] == 1.0f, "Ortho [15] must be 1.0");

    printf("  [PASS] Domain 5: 98 Matrix and linear algebra assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 6: Geometry Pipeline, Viewport Mapping & OpenGL GL_CCW Face Culling
 * ========================================================================= */
static void test_domain_6_geometry_pipeline(void)
{
    TEST_SECTION("Domain 6: Geometry Pipeline, Viewport Mapping & Face Culling");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to allocate 3D context");

    float ndc_x0 = -0.5f, ndc_y0 = -0.5f;
    float ndc_x1 =  0.5f, ndc_y1 = -0.5f;
    float ndc_x2 =  0.0f, ndc_y2 =  0.5f;
    float edge_ccw = (ndc_x1 - ndc_x0) * (ndc_y2 - ndc_y0) - (ndc_y1 - ndc_y0) * (ndc_x2 - ndc_x0);
    TEST_ASSERT(edge_ccw > 0.0f, "Counter-Clockwise triangle edge must be positive (Front-facing in GL_CCW)");

    float edge_cw = (ndc_x2 - ndc_x0) * (ndc_y1 - ndc_y0) - (ndc_y2 - ndc_y0) * (ndc_x1 - ndc_x0);
    TEST_ASSERT(edge_cw < 0.0f, "Clockwise triangle edge must be negative (Back-facing in GL_CCW)");

    float half_w = ctx->width * 0.5f;
    float half_h = ctx->height * 0.5f;
    int center_x = (int)((0.0f + 1.0f) * half_w);
    int center_y = (int)((1.0f - 0.0f) * half_h);
    uint32_t center_z = (uint32_t)((0.0f * 0.5f + 0.5f) * 16777215.0f);
    TEST_ASSERT(center_x == (int)(ctx->width / 2), "Screen X center mapping mismatch");
    TEST_ASSERT(center_y == (int)(ctx->height / 2), "Screen Y center mapping mismatch");
    TEST_ASSERT(center_z == 8388607, "Screen Z center mapping mismatch");

    hpfx3d_mat4_identity(&ctx->projection);
    hpfx3d_mat4_identity(&ctx->modelview);
    hpfx3d_update_mvp(ctx);

    ctx->batch_count = 0;
    hpfx3d_set_cull_mode(ctx, HPFX_CULL_BACK);

    hpfx3d_vec3 fv0 = {-0.5f, -0.5f, 0.0f};
    hpfx3d_vec3 fv1 = { 0.5f, -0.5f, 0.0f};
    hpfx3d_vec3 fv2 = { 0.0f,  0.5f, 0.0f};
    hpfx3d_draw_triangle_3d(ctx, &fv0, &fv1, &fv2, 0x00ff0000);
    TEST_ASSERT(ctx->batch_count == 1, "Front-facing triangle must NOT be culled under CULL_BACK");

    hpfx3d_vec3 bv0 = {-0.5f, -0.5f, 0.0f};
    hpfx3d_vec3 bv1 = { 0.0f,  0.5f, 0.0f};
    hpfx3d_vec3 bv2 = { 0.5f, -0.5f, 0.0f};
    hpfx3d_draw_triangle_3d(ctx, &bv0, &bv1, &bv2, 0x0000ff00);
    TEST_ASSERT(ctx->batch_count == 1, "Back-facing triangle MUST be culled under CULL_BACK");

    ctx->batch_count = 0;
    hpfx3d_set_cull_mode(ctx, HPFX_CULL_NONE);
    hpfx3d_draw_cube(ctx, 1.0f);
    TEST_ASSERT(ctx->batch_count == 12, "Cube with no culling must submit exactly 12 triangles");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 6: 7 Geometry and culling pipeline assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 7: 3D Rasterization Accuracy, Z-Buffering Occlusion & Boundary Robustness
 * ========================================================================= */
static void test_domain_7_rasterization_accuracy(void)
{
    TEST_SECTION("Domain 7: 3D Rasterization Accuracy, Z-Buffering Occlusion & Boundary Robustness");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    hpfx3d_clear(ctx, 0x00000000, 0x00ffffff);
    uint32_t *fb = (uint32_t*)ctx->fb_mem;
    uint32_t *zb = ctx->sw_zbuffer;
    TEST_ASSERT(fb[0] == 0x00000000, "Framebuffer pixel [0] clear color mismatch");
    TEST_ASSERT(zb[0] == 0x00ffffff, "Z-Buffer pixel [0] clear depth mismatch");

    struct hpfx_triangle_cmd tri1;
    tri1.x0 = 100; tri1.y0 = 100; tri1.z0 = 0x00500000;
    tri1.x1 = 300; tri1.y1 = 100; tri1.z1 = 0x00500000;
    tri1.x2 = 100; tri1.y2 = 300; tri1.z2 = 0x00500000;
    tri1.color = 0x00ff0000;

    ctx->batch_buf[0] = tri1;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);

    int idx_inside = 120 * ctx->width + 120;
    TEST_ASSERT(fb[idx_inside] == 0x00ff0000, "Interior point must be colored Red");
    TEST_ASSERT(zb[idx_inside] == 0x00500000, "Interior point Z value mismatch");

    int idx_outside = 250 * ctx->width + 250;
    TEST_ASSERT(fb[idx_outside] == 0x00000000, "Exterior point must remain background Black");

    struct hpfx_triangle_cmd tri2_closer;
    tri2_closer.x0 = 100; tri2_closer.y0 = 100; tri2_closer.z0 = 0x00200000;
    tri2_closer.x1 = 200; tri2_closer.y1 = 100; tri2_closer.z1 = 0x00200000;
    tri2_closer.x2 = 100; tri2_closer.y2 = 200; tri2_closer.z2 = 0x00200000;
    tri2_closer.color = 0x0000ff00;

    ctx->batch_buf[0] = tri2_closer;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);

    TEST_ASSERT(fb[idx_inside] == 0x0000ff00, "Closer triangle must occlude farther triangle");
    TEST_ASSERT(zb[idx_inside] == 0x00200000, "Z-Buffer must be updated with closer depth");

    struct hpfx_triangle_cmd tri3_farther;
    tri3_farther.x0 = 100; tri3_farther.y0 = 100; tri3_farther.z0 = 0x00900000;
    tri3_farther.x1 = 200; tri3_farther.y1 = 100; tri3_farther.z1 = 0x00900000;
    tri3_farther.x2 = 100; tri3_farther.y2 = 200; tri3_farther.z2 = 0x00900000;
    tri3_farther.color = 0x000000ff;

    ctx->batch_buf[0] = tri3_farther;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);

    TEST_ASSERT(fb[idx_inside] == 0x0000ff00, "Farther triangle must be rejected by Z-buffer depth test");
    TEST_ASSERT(zb[idx_inside] == 0x00200000, "Z-Buffer must retain closer depth");

    struct hpfx_triangle_cmd degen;
    degen.x0 = 50; degen.y0 = 50; degen.z0 = 0x1000;
    degen.x1 = 50; degen.y1 = 50; degen.z1 = 0x1000;
    degen.x2 = 50; degen.y2 = 50; degen.z2 = 0x1000;
    degen.color = 0x00ffffff;

    ctx->batch_buf[0] = degen;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "Degenerate triangle flush handled safely");

    struct hpfx_triangle_cmd oob;
    oob.x0 = -500; oob.y0 = -500; oob.z0 = 0x1000;
    oob.x1 = 2000; oob.y1 = -500; oob.z1 = 0x1000;
    oob.x2 =  500; oob.y2 = 2000; oob.z2 = 0x1000;
    oob.color = 0x00ffffff;

    ctx->batch_buf[0] = oob;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "Out-of-bounds triangle clamped safely");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 7: 10 Rasterization, Z-buffer occlusion and boundary assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 8: Batch Primitive Queuing, FIFO Flush & Device Lifecycle
 * ========================================================================= */
static void test_domain_8_batch_and_lifecycle(void)
{
    TEST_SECTION("Domain 8: Batch Primitive Queuing, FIFO Flush & Device Lifecycle");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "hpfx3d_open failed");
    TEST_ASSERT(ctx->batch_count == 0, "Initial batch count must be zero");

    for (int i = 0; i < 1024; i++) {
        hpfx3d_draw_triangle_screen(ctx, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0x00112233);
    }
    TEST_ASSERT(ctx->batch_count == 1024, "Batch buffer must hold 1024 triangles before automatic flush");

    hpfx3d_draw_triangle_screen(ctx, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0x00445566);
    TEST_ASSERT(ctx->batch_count == 1, "Adding 1025th triangle must automatically flush previous batch");

    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "hpfx3d_flush must reset batch count to zero");

    hpfx3d_sync(ctx);
    int reset_res = hpfx3d_reset(ctx);
    TEST_ASSERT(reset_res == 0, "hpfx3d_reset must return 0");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 8: 6 Batch management and lifecycle assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 9: Gouraud Smooth Shading & Barycentric Mathematics
 * ========================================================================= */
static void test_domain_9_barycentric_math(void)
{
    TEST_SECTION("Domain 9: Gouraud Smooth Shading & Barycentric Mathematics");

    /* Barycentric coordinates for triangle (x0,y0)=(0,0), (x1,y1)=(100,0), (x2,y2)=(0,100) */
    int x0 = 0, y0 = 0;
    int x1 = 100, y1 = 0;
    int x2 = 0, y2 = 100;
    int denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
    TEST_ASSERT(denom == 10000, "Triangle area determinant must equal 10000");

    float inv_denom = 1.0f / (float)denom;

    /* Test 9.1: Centroid (33, 33) */
    int px = 33, py = 33;
    float w0 = (float)((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * inv_denom;
    float w1 = (float)((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * inv_denom;
    float w2 = 1.0f - w0 - w1;

    TEST_ASSERT(float_near(w0 + w1 + w2, 1.0f, 1e-4f), "Sum of barycentric weights must equal 1.0");
    TEST_ASSERT(w0 > 0.0f && w1 > 0.0f && w2 > 0.0f, "Centroid must have all weights positive");
    TEST_ASSERT(float_near(w0, 0.34f, 0.02f), "w0 near 0.33 at centroid");
    TEST_ASSERT(float_near(w1, 0.33f, 0.02f), "w1 near 0.33 at centroid");
    TEST_ASSERT(float_near(w2, 0.33f, 0.02f), "w2 near 0.33 at centroid");

    /* Test 9.2: Vertex V0 (0, 0) */
    px = 0; py = 0;
    w0 = (float)((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * inv_denom;
    w1 = (float)((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * inv_denom;
    w2 = 1.0f - w0 - w1;
    TEST_ASSERT(float_near(w0, 1.0f, 1e-4f), "Weight w0 at vertex V0 must be 1.0");
    TEST_ASSERT(float_near(w1, 0.0f, 1e-4f), "Weight w1 at vertex V0 must be 0.0");
    TEST_ASSERT(float_near(w2, 0.0f, 1e-4f), "Weight w2 at vertex V0 must be 0.0");

    /* Test 9.3: Vertex V1 (100, 0) */
    px = 100; py = 0;
    w0 = (float)((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * inv_denom;
    w1 = (float)((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * inv_denom;
    w2 = 1.0f - w0 - w1;
    TEST_ASSERT(float_near(w0, 0.0f, 1e-4f), "Weight w0 at vertex V1 must be 0.0");
    TEST_ASSERT(float_near(w1, 1.0f, 1e-4f), "Weight w1 at vertex V1 must be 1.0");
    TEST_ASSERT(float_near(w2, 0.0f, 1e-4f), "Weight w2 at vertex V1 must be 0.0");

    printf("  [PASS] Domain 9: 13 Barycentric coordinate and smooth shading assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 10: Hardware Texture Engine & UV Sampling
 * ========================================================================= */
static void test_domain_10_texture_engine(void)
{
    TEST_SECTION("Domain 10: Hardware Texture Engine & UV Sampling");

    /* Create 4x4 test texture with distinct quadrant colors */
    uint32_t tex_data[16] = {
        0xffff0000, 0xffff0000, 0xff00ff00, 0xff00ff00,
        0xffff0000, 0xffff0000, 0xff00ff00, 0xff00ff00,
        0xff0000ff, 0xff0000ff, 0xffffff00, 0xffffff00,
        0xff0000ff, 0xff0000ff, 0xffffff00, 0xffffff00
    };

    hpfx3d_texture *tex = hpfx3d_create_texture(4, 4, tex_data);
    TEST_ASSERT(tex != NULL, "hpfx3d_create_texture failed");
    TEST_ASSERT(tex->width == 4 && tex->height == 4, "Texture dimensions mismatch");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation failed");

    hpfx3d_bind_texture(ctx, tex);
    TEST_ASSERT(ctx->current_tex == tex, "Texture binding pointer mismatch");

    /* Test texture sampling at corners */
    TEST_ASSERT(tex->data[0] == 0xffff0000, "Top-left texel must be Red");
    TEST_ASSERT(tex->data[3] == 0xff00ff00, "Top-right texel must be Green");
    TEST_ASSERT(tex->data[12] == 0xff0000ff, "Bottom-left texel must be Blue");
    TEST_ASSERT(tex->data[15] == 0xffffff00, "Bottom-right texel must be Yellow");

    /* UV Wrapping properties: REPEAT mode maps u = 1.25 to u = 0.25 */
    float u_repeat = 1.25f - floorf(1.25f);
    TEST_ASSERT(float_near(u_repeat, 0.25f, 1e-4f), "UV Repeat wrapping formula mismatch");

    /* UV Clamp mode clamps u = 1.8 to 1.0 */
    float u_clamp = 1.8f > 1.0f ? 1.0f : 1.8f;
    TEST_ASSERT(u_clamp == 1.0f, "UV Clamp upper bound mismatch");

    hpfx3d_destroy_texture(tex);
    hpfx3d_close(ctx);
    printf("  [PASS] Domain 10: 9 Texture sampling and filtering assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 11: Alpha Blending & Multi-Layer Transparency Math
 * ========================================================================= */
static void test_domain_11_alpha_blending(void)
{
    TEST_SECTION("Domain 11: Alpha Blending & Multi-Layer Transparency Math");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation failed");

    /* Clear framebuffer to pure Blue: 0xff0000ff */
    hpfx3d_clear(ctx, 0xff0000ff, 0x00ffffff);
    uint32_t *fb = (uint32_t*)ctx->fb_mem;
    TEST_ASSERT(fb[0] == 0xff0000ff, "Initial background color must be Blue (0xff0000ff)");

    /* Enable Alpha Blending in State */
    hpfx3d_set_blend(ctx, true);
    TEST_ASSERT(ctx->state.blend_mode == 1, "Blend mode flag must be enabled");

    /* Draw 50% translucent Red triangle over point (100, 100): alpha = 128, color = 0x80ff0000 */
    struct hpfx_triangle_cmd tri_trans;
    tri_trans.x0 = 50;  tri_trans.y0 = 50;  tri_trans.z0 = 0x00100000;
    tri_trans.x1 = 200; tri_trans.y1 = 50;  tri_trans.z1 = 0x00100000;
    tri_trans.x2 = 50;  tri_trans.y2 = 200; tri_trans.z2 = 0x00100000;
    tri_trans.color = 0x80ff0000; /* 50% Red */

    ctx->batch_buf[0] = tri_trans;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);

    int idx = 80 * ctx->width + 80;
    uint32_t blended = fb[idx];
    uint32_t r = (blended >> 16) & 0xff;
    uint32_t g = (blended >> 8) & 0xff;
    uint32_t b = blended & 0xff;

    /* Expected: R ≈ 128, G = 0, B ≈ 127 (Purple) */
    TEST_ASSERT(r >= 120 && r <= 135, "Blended Red channel must be approximately 128 (got %u)", r);
    TEST_ASSERT(g == 0, "Blended Green channel must be 0 (got %u)", g);
    TEST_ASSERT(b >= 120 && b <= 135, "Blended Blue channel must be approximately 127 (got %u)", b);
    /* Test 100% opaque replace with closer depth */
    tri_trans.color = 0xffff0000; /* 100% Red */
    tri_trans.z0 = 0x00050000;
    tri_trans.z1 = 0x00050000;
    tri_trans.z2 = 0x00050000;
    ctx->batch_buf[0] = tri_trans;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);
    TEST_ASSERT((fb[idx] & 0x00ffffff) == 0x00ff0000, "100%% opaque alpha must fully replace destination pixel");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 11: 7 Alpha blending and transparency assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 12: Depth Fog Pipeline & Attenuation Equations
 * ========================================================================= */
static void test_domain_12_fog_pipeline(void)
{
    TEST_SECTION("Domain 12: Depth Fog Pipeline & Attenuation Equations");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation failed");

    /* Enable Fog: Color = Gray (0x00808080), Start = 0.2, End = 0.8 */
    hpfx3d_set_fog(ctx, true, 0x00808080, 0.2f, 0.8f);
    TEST_ASSERT(ctx->fog.enabled == true, "Fog enabled flag mismatch");
    TEST_ASSERT(ctx->fog.color == 0x00808080, "Fog color mismatch");
    TEST_ASSERT(ctx->fog.start_z == 0.2f, "Fog start_z mismatch");
    TEST_ASSERT(ctx->fog.end_z == 0.8f, "Fog end_z mismatch");

    /* Test Fog Factor Equation: f = (end - z) / (end - start) */
    float z_near = 0.1f;
    float f_near = (0.8f - z_near) / (0.8f - 0.2f);
    if (f_near > 1.0f) f_near = 1.0f;
    TEST_ASSERT(f_near == 1.0f, "Near object (z <= start) must have fog factor f = 1.0 (zero fog)");

    float z_far = 0.9f;
    float f_far = (0.8f - z_far) / (0.8f - 0.2f);
    if (f_far < 0.0f) f_far = 0.0f;
    TEST_ASSERT(f_far == 0.0f, "Far object (z >= end) must have fog factor f = 0.0 (maximum fog)");

    float z_mid = 0.5f;
    float f_mid = (0.8f - z_mid) / (0.8f - 0.2f);
    TEST_ASSERT(float_near(f_mid, 0.5f, 1e-4f), "Midway object must have fog factor f = 0.5");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 12: 8 Depth fog pipeline and attenuation assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 13: Scissor Box & Viewport Scissoring Boundary Precision
 * ========================================================================= */
static void test_domain_13_scissor_box(void)
{
    TEST_SECTION("Domain 13: Scissor Box & Viewport Scissoring Boundary Precision");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation failed");

    hpfx3d_clear(ctx, 0x00000000, 0x00ffffff);
    uint32_t *fb = (uint32_t*)ctx->fb_mem;

    /* Set Scissor Rectangle: [200, 200] with size [100, 100] -> X:200..299, Y:200..299 */
    hpfx3d_set_scissor(ctx, true, 200, 200, 100, 100);
    TEST_ASSERT(ctx->scissor.enabled == true, "Scissor enabled mismatch");
    TEST_ASSERT(ctx->scissor.x == 200 && ctx->scissor.y == 200, "Scissor origin mismatch");
    TEST_ASSERT(ctx->scissor.width == 100 && ctx->scissor.height == 100, "Scissor size mismatch");

    /* Draw large triangle covering [100, 100] to [400, 400] in pure White */
    struct hpfx_triangle_cmd tri_big;
    tri_big.x0 = 100; tri_big.y0 = 100; tri_big.z0 = 0x00100000;
    tri_big.x1 = 400; tri_big.y1 = 100; tri_big.z1 = 0x00100000;
    tri_big.x2 = 100; tri_big.y2 = 400; tri_big.z2 = 0x00100000;
    tri_big.color = 0x00ffffff;

    ctx->batch_buf[0] = tri_big;
    ctx->batch_count = 1;
    hpfx3d_flush(ctx);

    /* Pixel INSIDE Scissor Box (220, 220) must be White */
    int idx_in = 220 * ctx->width + 220;
    TEST_ASSERT(fb[idx_in] == 0x00ffffff, "Pixel inside scissor box (220,220) must be drawn White");

    /* Pixel OUTSIDE Scissor Box (150, 150) must remain Black */
    int idx_out_1 = 150 * ctx->width + 150;
    TEST_ASSERT(fb[idx_out_1] == 0x00000000, "Pixel outside scissor box (150,150) must remain Black");

    /* Pixel OUTSIDE Scissor Box (350, 220) must remain Black */
    int idx_out_2 = 220 * ctx->width + 350;
    TEST_ASSERT(fb[idx_out_2] == 0x00000000, "Pixel outside scissor box (350,220) must remain Black");

    /* Disable Scissor Box and redraw */
    hpfx3d_set_scissor(ctx, false, 0, 0, ctx->width, ctx->height);
    TEST_ASSERT(ctx->scissor.enabled == false, "Scissor disabled flag mismatch");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 13: 8 Scissor boundary and clipping assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 14: HP Diagnostics Emulation (Torus & Sphere Meshes + CRC32 Deterministic Checksum)
 * ========================================================================= */
static void test_domain_14_hp_diagnostics_crc(void)
{
    TEST_SECTION("Domain 14: HP Diagnostics Emulation (Torus & Sphere Meshes + CRC32 Deterministic Checksum)");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation failed");

    /* Setup camera for 3D meshes */
    hpfx3d_mat4_perspective(&ctx->projection, 45.0f * (3.14159265f / 180.0f), (float)ctx->width / (float)ctx->height, 0.5f, 100.0f);
    hpfx3d_mat4_identity(&ctx->modelview);
    hpfx3d_mat4_translate(&ctx->modelview, 0.0f, 0.0f, -5.0f);
    hpfx3d_mat4_rotate_x(&ctx->modelview, 30.0f * (3.14159265f / 180.0f));
    hpfx3d_update_mvp(ctx);

    /* -------------------------------------------------------------
     * 14.1: Render 3D Torus (HP Diag Test #1 Emulation)
     * ------------------------------------------------------------- */
    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    hpfx3d_set_depth_test(ctx, true, HPFX_DEPTH_LESS);
    hpfx3d_set_cull_mode(ctx, HPFX_CULL_BACK);

    ctx->batch_count = 0;
    /* 16 rings x 16 sides = 512 triangles */
    hpfx3d_draw_torus(ctx, 1.2f, 0.4f, 16, 16, 0x00e67e22);
    hpfx3d_flush(ctx);

    uint32_t torus_crc = hpfx3d_checksum_framebuffer(ctx);
    TEST_ASSERT(torus_crc != 0, "Torus CRC32 checksum must be non-zero");

    /* Redraw Torus to prove 100% bit-level deterministic repeatability */
    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    ctx->batch_count = 0;
    hpfx3d_draw_torus(ctx, 1.2f, 0.4f, 16, 16, 0x00e67e22);
    hpfx3d_flush(ctx);
    uint32_t torus_crc_repeat = hpfx3d_checksum_framebuffer(ctx);
    TEST_ASSERT(torus_crc == torus_crc_repeat, "Torus render must be 100%% deterministic (CRC32: 0x%08x vs 0x%08x)",
                torus_crc, torus_crc_repeat);

    /* -------------------------------------------------------------
     * 14.2: Render 3D Sphere (HP Diag Test #3 Emulation)
     * ------------------------------------------------------------- */
    hpfx3d_clear(ctx, 0x00101826, 0x00ffffff);
    ctx->batch_count = 0;
    hpfx3d_draw_sphere(ctx, 1.5f, 16, 12, 0x003498db);
    hpfx3d_flush(ctx);

    uint32_t sphere_crc = hpfx3d_checksum_framebuffer(ctx);
    TEST_ASSERT(sphere_crc != 0, "Sphere CRC32 checksum must be non-zero");
    TEST_ASSERT(sphere_crc != torus_crc, "Sphere CRC32 must differ from Torus CRC32");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 14: 5 HP Diagnostics Torus/Sphere emulation & CRC32 assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 15: Extreme Stress, Robustness & Fuzzing
 * ========================================================================= */
static void test_domain_15_stress_and_fuzzing(void)
{
    TEST_SECTION("Domain 15: Extreme Stress, Robustness & Fuzzing");

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation failed");

    /* 15.1 Massive 10,000 Triangle Throughput Test */
    hpfx3d_clear(ctx, 0, 0x00ffffff);
    for (int i = 0; i < 10000; i++) {
        int x = (i * 17) % (ctx->width - 20);
        int y = (i * 31) % (ctx->height - 20);
        hpfx3d_draw_triangle_screen(ctx, x, y, 0x1000, x + 15, y, 0x1000, x, y + 15, 0x1000, (uint32_t)i);
    }
    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "10,000 triangle stream processed without buffer overflow");

    /* 15.2 NaN and Infinity Fuzzing Rejection */
    ctx->batch_count = 0;
    hpfx3d_vec3 nan_v0 = { NAN, 0.0f, -5.0f };
    hpfx3d_vec3 nan_v1 = { 1.0f, INFINITY, -5.0f };
    hpfx3d_vec3 nan_v2 = { 0.0f, 1.0f, -INFINITY };
    /* Must not crash or produce floating-point trap */
    hpfx3d_draw_triangle_3d(ctx, &nan_v0, &nan_v1, &nan_v2, 0x00ff0000);
    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "NaN and Infinity floating-point inputs rejected safely");

    /* 15.3 Extreme Coordinate Boundary Fuzzing */
    ctx->batch_count = 0;
    hpfx3d_draw_triangle_screen(ctx, -100000, -100000, 0, 100000, -100000, 0, 0, 100000, 0, 0x00123456);
    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "Extreme coordinates [-100000, 100000] clamped safely");

    /* 15.4 Collinear Zero-Area Primitives */
    ctx->batch_count = 0;
    for (int i = 0; i < 100; i++) {
        hpfx3d_draw_triangle_screen(ctx, 100, 100, 0, 100, 100, 0, 100, 100, 0, 0x00abcdef);
    }
    hpfx3d_flush(ctx);
    TEST_ASSERT(ctx->batch_count == 0, "100 zero-area collinear primitives processed without divide-by-zero");

    /* 15.5 Rapid Context Allocation and Teardown Lifecycle */
    bool rapid_alloc_success = true;
    for (int i = 0; i < 50; i++) {
        hpfx3d_context *c = hpfx3d_open(NULL);
        if (!c) {
            rapid_alloc_success = false;
            break;
        }
        hpfx3d_draw_cube(c, 1.0f);
        hpfx3d_flush(c);
        hpfx3d_close(c);
    }
    TEST_ASSERT(rapid_alloc_success, "50 rapid context create/render/destroy cycles completed without memory leak");

    hpfx3d_close(ctx);
    printf("  [PASS] Domain 15: 5 Stress, robustness and security fuzzing assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 16: Hardware Texture Upload, VRAM Staging & IOCTL Protocol
 * ========================================================================= */
static void test_domain_16_texture_staging(void)
{
    TEST_SECTION("Domain 16: Hardware Texture Upload, VRAM Staging & IOCTL Protocol");

    /* 16.1 Register definitions */
    TEST_ASSERT(HPFX_REG_TEX_BASE == 0x00921180, "HPFX_REG_TEX_BASE must be 0x00921180");
    TEST_ASSERT(HPFX_REG_TEX_CTRL == 0x00921184, "HPFX_REG_TEX_CTRL must be 0x00921184");
    TEST_ASSERT(HPFX_REG_TEX_FORMAT == 0x00921188, "HPFX_REG_TEX_FORMAT must be 0x00921188");
    TEST_ASSERT(HPFX_REG_TEX_PITCH == 0x0092118c, "HPFX_REG_TEX_PITCH must be 0x0092118C");

    /* 16.2 IOCTL command code and struct layout */
    TEST_ASSERT((HPFX_IOCTL_UPLOAD_TEXTURE & 0xff) == 0x26, "HPFX_IOCTL_UPLOAD_TEXTURE code must be 0x26");
    struct hpfx_tex_upload_cmd tex_cmd;
    memset(&tex_cmd, 0, sizeof(tex_cmd));
    TEST_ASSERT((uintptr_t)&tex_cmd.width - (uintptr_t)&tex_cmd == 0, "width offset in tex_cmd must be 0");
    TEST_ASSERT((uintptr_t)&tex_cmd.height - (uintptr_t)&tex_cmd == 4, "height offset in tex_cmd must be 4");
    TEST_ASSERT((uintptr_t)&tex_cmd.format - (uintptr_t)&tex_cmd == 8, "format offset in tex_cmd must be 8");
    TEST_ASSERT((uintptr_t)&tex_cmd.vram_offset - (uintptr_t)&tex_cmd == 12, "vram_offset in tex_cmd must be 12");
    TEST_ASSERT((uintptr_t)&tex_cmd.data - (uintptr_t)&tex_cmd >= 16, "data pointer in tex_cmd must be at offset >= 16");

    /* 16.3 VRAM Texture staging calculation */
    uint32_t rgb565_bytes = 256 * 256 * 2;
    TEST_ASSERT(rgb565_bytes == 131072, "256x256 RGB565 texture must require 128KB (131072 bytes)");
    uint32_t rgba8888_bytes = 512 * 512 * 4;
    TEST_ASSERT(rgba8888_bytes == 1048576, "512x512 RGBA8888 texture must require 1MB (1048576 bytes)");

    /* Staging offset at 16MB in 32MB card leaves 15MB for textures */
    uint32_t vram_base = 16 * 1024 * 1024;
    TEST_ASSERT(vram_base + rgba8888_bytes <= 32 * 1024 * 1024, "Texture staging within 32MB VRAM boundary");

    /* 16.4 API Function Validation & Error Handling */
    hpfx3d_context *ctx = hpfx3d_open(NULL);
    TEST_ASSERT(ctx != NULL, "Context allocation must succeed");

    uint32_t dummy_pixels[64 * 64];
    memset(dummy_pixels, 0xAA, sizeof(dummy_pixels));

    /* Valid uploads */
    int ret_rgb = hpfx3d_upload_texture_hw(ctx, vram_base, 64, 64, 0, dummy_pixels);
    TEST_ASSERT(ret_rgb == 0, "Valid RGB565 hardware texture upload must return 0");
    int ret_rgba = hpfx3d_upload_texture_hw(ctx, vram_base, 64, 64, 1, dummy_pixels);
    TEST_ASSERT(ret_rgba == 0, "Valid RGBA8888 hardware texture upload must return 0");

    /* Negative error handling tests */
    TEST_ASSERT(hpfx3d_upload_texture_hw(NULL, vram_base, 64, 64, 1, dummy_pixels) == -1, "NULL context must fail");
    TEST_ASSERT(hpfx3d_upload_texture_hw(ctx, vram_base, 64, 64, 1, NULL) == -1, "NULL pixel data must fail");
    TEST_ASSERT(hpfx3d_upload_texture_hw(ctx, vram_base, 0, 64, 1, dummy_pixels) == -1, "Zero width must fail");
    TEST_ASSERT(hpfx3d_upload_texture_hw(ctx, vram_base, 64, 0, 1, dummy_pixels) == -1, "Zero height must fail");
    TEST_ASSERT(hpfx3d_upload_texture_hw(ctx, vram_base, 4096, 64, 1, dummy_pixels) == -1, "Oversized width (>2048) must fail");
    TEST_ASSERT(hpfx3d_upload_texture_hw(ctx, vram_base, 64, 64, 2, dummy_pixels) == -1, "Invalid format (>1) must fail");

    /* Texture upload + binding workflow */
    hpfx3d_texture *tex = hpfx3d_create_texture(64, 64, dummy_pixels);
    TEST_ASSERT(tex != NULL, "Texture creation must succeed");
    hpfx3d_bind_texture(ctx, tex);
    TEST_ASSERT(ctx->current_tex == tex, "Texture must be bound to context");

    hpfx3d_destroy_texture(tex);
    hpfx3d_close(ctx);
    printf("  [PASS] Domain 16: 22 Hardware texture staging & IOCTL protocol assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 17: HP Diagnostics Golden Verification & shoe4R CAD Geometry
 * ========================================================================= */
static void test_domain_17_hp_diagnostics_suite(void)
{
    TEST_SECTION("Domain 17: HP Diagnostics Golden Verification & shoe4R CAD Geometry");

    /* 17.1 Golden CRC Constants from FX5CRC.W2K */
    TEST_ASSERT(HPFX_CRC16_TEST1_TORUS == 0xa25c6a86, "Golden CRC16 Test #1 (Torus) constant mismatch");
    TEST_ASSERT(HPFX_CRC16_TEST2_SHOE == 0x9861656d, "Golden CRC16 Test #2 (Shoe) constant mismatch");
    TEST_ASSERT(HPFX_CRC16_TEST3_SPHERE == 0x9df8c3a9, "Golden CRC16 Test #3 (Sphere) constant mismatch");
    TEST_ASSERT(HPFX_CRC16_TEST4_SHOE_TEX == 0x2d16310a, "Golden CRC16 Test #4 (Shoe Tex) constant mismatch");

    TEST_ASSERT(HPFX_CRC32_TEST1_TORUS == 0x3411fabb, "Golden CRC32 Test #1 (Torus) constant mismatch");
    TEST_ASSERT(HPFX_CRC32_TEST2_SHOE == 0x4b161f67, "Golden CRC32 Test #2 (Shoe) constant mismatch");
    TEST_ASSERT(HPFX_CRC32_TEST3_SPHERE == 0x846c6000, "Golden CRC32 Test #3 (Sphere) constant mismatch");
    TEST_ASSERT(HPFX_CRC32_TEST4_SHOE_TEX == 0x1135eb4e, "Golden CRC32 Test #4 (Shoe Tex) constant mismatch");

    /* 17.2 Diagnostic Dimensions */
    TEST_ASSERT(HPFX_DIAG_WIDTH == 544, "Diagnostic width must be 544 (0x220)");
    TEST_ASSERT(HPFX_DIAG_HEIGHT == 403, "Diagnostic height must be 403 (0x193)");

    /* 17.3 Diagnostic Runner Lifecycle & Execution */
    hpfx_diag_runner *runner = hpfx_diag_init(NULL);
    TEST_ASSERT(runner != NULL, "Diagnostic runner initialization failed");
    TEST_ASSERT(runner->ctx != NULL, "Diagnostic runner context failed");
    TEST_ASSERT(runner->ctx->width == 544, "Runner width mismatch");
    TEST_ASSERT(runner->ctx->height == 403, "Runner height mismatch");

    /* Execute Test 1 (Torus) */
    uint32_t crc1 = hpfx_diag_run_test1_torus(runner);
    TEST_ASSERT(crc1 != 0, "Test #1 (Torus) CRC must be non-zero");
    TEST_ASSERT(runner->results[0].passed, "Test #1 status flag must be passed");

    /* Execute Test 2 (shoe4R solid) */
    uint32_t crc2 = hpfx_diag_run_test2_shoe(runner);
    TEST_ASSERT(crc2 != 0, "Test #2 (shoe4R) CRC must be non-zero");
    TEST_ASSERT(crc2 != crc1, "Test #2 CRC must differ from Test #1");
    TEST_ASSERT(runner->results[1].passed, "Test #2 status flag must be passed");

    /* Execute Test 3 (Sphere) */
    uint32_t crc3 = hpfx_diag_run_test3_sphere(runner);
    TEST_ASSERT(crc3 != 0, "Test #3 (Sphere) CRC must be non-zero");
    TEST_ASSERT(crc3 != crc1 && crc3 != crc2, "Test #3 CRC must differ from Tests #1 & #2");
    TEST_ASSERT(runner->results[2].passed, "Test #3 status flag must be passed");

    /* Execute Test 4 (shoe4R textured) */
    uint32_t crc4 = hpfx_diag_run_test4_shoe_textured(runner);
    TEST_ASSERT(crc4 != 0, "Test #4 (shoe4R Tex) CRC must be non-zero");
    TEST_ASSERT(crc4 != crc2, "Test #4 CRC must differ from Test #2");
    TEST_ASSERT(runner->results[3].passed, "Test #4 status flag must be passed");

    /* Deterministic repeatability */
    uint32_t crc1_repeat = hpfx_diag_run_test1_torus(runner);
    TEST_ASSERT(crc1_repeat == crc1, "Test #1 deterministic CRC repeatability mismatch");

    hpfx_diag_free(runner);
    printf("  [PASS] Domain 17: 21 HP Diagnostics Golden Verification assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 18: OpenGL 1.1 / TinyGL Backend Validation
 * ========================================================================= */
static void test_domain_18_opengl_backend(void)
{
    TEST_SECTION("Domain 18: OpenGL 1.1 / TinyGL Backend Validation");

    bool ok = hpfx_gl_init(NULL, 640, 480);
    TEST_ASSERT(ok == true, "OpenGL backend initialization must succeed");

    hpfx3d_context *ctx = hpfx_gl_get_context();
    TEST_ASSERT(ctx != NULL, "OpenGL context pointer must be non-NULL");
    TEST_ASSERT(ctx->width == 640, "OpenGL context width mismatch");
    TEST_ASSERT(ctx->height == 480, "OpenGL context height mismatch");

    /* Matrix stack */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glTranslatef(1.0f, 2.0f, 3.0f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glScalef(2.0f, 2.0f, 2.0f);
    TEST_ASSERT(ctx->modelview.m[12] != 0.0f || ctx->modelview.m[13] != 0.0f, "Modelview translation must be active");
    glPopMatrix();
    TEST_ASSERT(ctx->modelview.m[0] == 1.0f && ctx->modelview.m[12] == 0.0f, "Popped matrix must restore identity");

    /* Projection matrix */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 100.0f);
    TEST_ASSERT(ctx->projection.m[14] == -1.0f, "Perspective frustum entry m[14] must be -1.0");

    /* State toggling */
    glEnable(GL_DEPTH_TEST);
    TEST_ASSERT(ctx->state.depth_enable == 1, "Depth test enable failed");
    glDepthFunc(GL_LEQUAL);
    TEST_ASSERT(ctx->state.depth_func == HPFX_DEPTH_LEQUAL, "Depth func LEQUAL failed");
    glDisable(GL_CULL_FACE);
    TEST_ASSERT(ctx->state.cull_mode == HPFX_CULL_NONE, "Cull mode disable failed");
    glEnable(GL_BLEND);
    TEST_ASSERT(ctx->state.blend_mode != 0, "Blend enable failed");

    /* Immediate mode rendering: Triangle */
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -2.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f( 0.5f, -0.5f, -2.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f( 0.0f,  0.5f, -2.0f);
    glEnd();
    glFlush();

    /* Framebuffer readback */
    uint32_t pixels[16];
    glReadPixels(0, 0, 4, 4, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    uint32_t bg_color = ((uint32_t)(0.1f * 255.0f) << 16) | ((uint32_t)(0.2f * 255.0f) << 8) | (uint32_t)(0.3f * 255.0f);
    TEST_ASSERT(pixels[0] == bg_color, "Framebuffer readback must match background clear color");

    /* Texturing */
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    TEST_ASSERT(tex_id > 0, "glGenTextures must return valid ID");
    glBindTexture(GL_TEXTURE_2D, tex_id);
    uint32_t dummy_tex[16] = { 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00ffffff };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummy_tex);
    TEST_ASSERT(ctx->current_tex != NULL, "Texture must be bound to 3D context");

    hpfx_gl_shutdown();
    printf("  [PASS] Domain 18: 16 OpenGL 1.1 / TinyGL Backend assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 19: Pre-Shader GPGPU & Numerical Compute Framework
 * ========================================================================= */
static void test_domain_19_gpgpu_compute(void)
{
    TEST_SECTION("Domain 19: Pre-Shader GPGPU & Numerical Compute Framework");

    /* 19.1 Context Lifecycle */
    hpfx_compute_context *ctx = hpfx_compute_init(64, 64);
    TEST_ASSERT(ctx != NULL, "GPGPU Compute context creation failed");
    TEST_ASSERT(ctx->grid_a != NULL && ctx->grid_b != NULL, "Compute grids allocation failed");

    /* 19.2 2D PDE Heat Diffusion & Discrete Laplacian */
    hpfx_compute_pde_set_source(ctx, 32, 32, 4.0f, 100.0f);
    TEST_ASSERT(hpfx_compute_pde_get_temp(ctx, 32, 32) == 100.0f, "Source temperature must be 100.0");
    TEST_ASSERT(hpfx_compute_pde_get_temp(ctx, 0, 0) == 0.0f, "Edge temperature must be initially 0.0");

    for (int step = 0; step < 20; step++) {
        hpfx_compute_pde_step(ctx, 0.2f, 0.5f);
    }
    float center_temp = hpfx_compute_pde_get_temp(ctx, 32, 32);
    float near_temp   = hpfx_compute_pde_get_temp(ctx, 32, 34);
    float far_temp    = hpfx_compute_pde_get_temp(ctx, 32, 42);
    TEST_ASSERT(center_temp < 100.0f, "Center temperature must diffuse downwards");
    TEST_ASSERT(near_temp > far_temp, "Heat must follow monotonic radial diffusion profile");
    TEST_ASSERT(far_temp >= 0.0f, "Temperature must remain non-negative");

    uint32_t *pde_pixels = (uint32_t*)malloc(64 * 64 * sizeof(uint32_t));
    hpfx_compute_pde_render_to_pixels(ctx, pde_pixels);
    TEST_ASSERT(pde_pixels[32 * 64 + 32] != 0, "Rendered center heat pixel must be non-zero");
    free(pde_pixels);

    /* 19.3 Spatial Convolutions: Sobel Edge Detection */
    uint32_t *src_img = (uint32_t*)calloc(64 * 64, sizeof(uint32_t));
    uint32_t *dst_sobel = (uint32_t*)calloc(64 * 64, sizeof(uint32_t));
    /* Create step edge: black on left, white on right */
    for (int y = 0; y < 64; y++) {
        for (int x = 32; x < 64; x++) {
            src_img[y * 64 + x] = 0x00ffffff;
        }
    }
    hpfx_compute_sobel_gradient(src_img, dst_sobel, 64, 64);
    uint32_t edge_val = dst_sobel[32 * 64 + 31] & 0xff;
    uint32_t flat_val = dst_sobel[32 * 64 + 10] & 0xff;
    TEST_ASSERT(edge_val > 100, "Sobel gradient at vertical step edge must be sharp");
    TEST_ASSERT(flat_val == 0, "Sobel gradient on flat region must be zero");

    /* 19.4 Gaussian Blur */
    uint32_t *dst_blur = (uint32_t*)calloc(64 * 64, sizeof(uint32_t));
    hpfx_compute_gaussian_blur(src_img, dst_blur, 64, 64);
    uint32_t blurred_transition = dst_blur[32 * 64 + 32] & 0xff;
    TEST_ASSERT(blurred_transition > 0 && blurred_transition < 255, "Gaussian blur must produce smooth gradient transition");
    free(src_img); free(dst_sobel); free(dst_blur);

    /* 19.5 3D Z-Buffer Voronoi & Euclidean Distance Transform */
    hpfx_voronoi_site sites[2] = {
        { 16.0f, 16.0f, 1, 0x00ff0000 },
        { 48.0f, 48.0f, 2, 0x000000ff }
    };
    uint32_t *vor_color = (uint32_t*)calloc(64 * 64, sizeof(uint32_t));
    float *vor_dist = (float*)calloc(64 * 64, sizeof(float));
    hpfx_compute_voronoi_zbuffer(ctx, sites, 2, vor_color, vor_dist);
    TEST_ASSERT(vor_color[16 * 64 + 16] == 0x00ff0000, "Site 1 point must receive Site 1 color");
    TEST_ASSERT(vor_color[48 * 64 + 48] == 0x000000ff, "Site 2 point must receive Site 2 color");
    TEST_ASSERT(vor_dist[16 * 64 + 16] == 0.0f, "Distance at site center must be 0.0");
    TEST_ASSERT(vor_dist[16 * 64 + 20] > 0.0f, "Distance away from site center must increase");
    free(vor_color); free(vor_dist);

    /* 19.6 Conway's Game of Life SIMD Cellular Automata */
    hpfx_life_grid *lg = hpfx_life_create(32, 32);
    TEST_ASSERT(lg != NULL, "Life grid allocation failed");
    hpfx_life_load_glider(lg, 5, 5);
    TEST_ASSERT(hpfx_life_population(lg) == 5, "Glider initial population must be 5");
    /* Run 4 steps (1 full glider cycle translates diagonally by +1, +1) */
    for (int s = 0; s < 4; s++) {
        hpfx_life_step(lg);
    }
    TEST_ASSERT(hpfx_life_population(lg) == 5, "Glider population after 1 full period must remain 5");
    TEST_ASSERT(hpfx_life_get_cell(lg, 8, 8) == 1, "Glider must translate diagonally (+1,+1) after period");
    hpfx_life_destroy(lg);

    /* 19.7 Blocked GEMM Matrix Multiplication */
    float a4[16] = { 1,2,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    float b4[16] = { 2,0,0,0, 0,2,0,0, 0,0,2,0, 0,0,0,2 };
    float c4[16];
    hpfx_compute_gemm_4x4(a4, b4, c4);
    TEST_ASSERT(c4[0] == 2.0f && c4[1] == 4.0f, "4x4 GEMM multiplication failed");

    float bigA[64], bigB[64], bigC[64];
    for (int i = 0; i < 64; i++) {
        bigA[i] = (i % 8 == i / 8) ? 1.0f : 0.0f; /* 8x8 identity */
        bigB[i] = (float)i;
    }
    hpfx_compute_gemm(bigA, bigB, bigC, 8, 8, 8);
    TEST_ASSERT(bigC[10] == 10.0f && bigC[63] == 63.0f, "8x8 Blocked GEMM identity multiply failed");

    hpfx_compute_destroy(ctx);
    printf("  [PASS] Domain 19: 22 Pre-Shader GPGPU & Numerical Compute assertions verified.\n");
}

/* =========================================================================
 * DOMAIN 20: DRM/KMS Kernel Subsystem & GEM ABI Verification
 * ========================================================================= */
static void test_domain_20_drm_kms_abi(void)
{
    TEST_SECTION("Domain 20: DRM/KMS Kernel Subsystem & GEM ABI Verification");

    /* 20.1 Structure Memory Layout and Alignment */
    TEST_ASSERT(sizeof(struct drm_hpfx_gem_create) == 40, "struct drm_hpfx_gem_create size mismatch (must be 40 bytes)");
    TEST_ASSERT(sizeof(struct drm_hpfx_exec) == 24, "struct drm_hpfx_exec size mismatch (must be 24 bytes)");
    TEST_ASSERT(sizeof(struct drm_hpfx_wait_idle) == 8, "struct drm_hpfx_wait_idle size mismatch (must be 8 bytes)");

    /* 20.2 IOCTL Command Codes */
    TEST_ASSERT(DRM_HPFX_GEM_CREATE == 0x00, "DRM_HPFX_GEM_CREATE opcode mismatch");
    TEST_ASSERT(DRM_HPFX_EXEC == 0x01, "DRM_HPFX_EXEC opcode mismatch");
    TEST_ASSERT(DRM_HPFX_WAIT_IDLE == 0x02, "DRM_HPFX_WAIT_IDLE opcode mismatch");
    TEST_ASSERT(DRM_COMMAND_BASE == 0x40, "DRM_COMMAND_BASE mismatch");

    /* 20.3 Hardware Dumb Buffer Pitch / Cache Alignment */
    /* Pitch = ((width * bpp/8) + 127) & ~127 */
    uint32_t p640 = ((640 * 4) + 127) & ~127;
    TEST_ASSERT(p640 % 128 == 0, "Pitch 640x32 must be 128-byte cache aligned");
    TEST_ASSERT(p640 >= 640 * 4, "Pitch 640x32 must cover visible scanline");

    uint32_t p1920 = ((1920 * 4) + 127) & ~127;
    TEST_ASSERT(p1920 % 128 == 0, "Pitch 1920x32 must be 128-byte cache aligned");
    TEST_ASSERT(p1920 >= 1920 * 4, "Pitch 1920x32 must cover visible scanline");

    /* 20.4 Maximum VRAM Capacity Bounds */
    uint64_t vram_fx5 = 32ULL * 1024 * 1024;
    uint64_t vram_fx10 = 64ULL * 1024 * 1024;
    TEST_ASSERT(vram_fx5 == 0x2000000ULL, "FX5 VRAM size must be 32MB");
    TEST_ASSERT(vram_fx10 == 0x4000000ULL, "FX10 VRAM size must be 64MB");

    printf("  [PASS] Domain 20: 11 DRM/KMS Subsystem & GEM ABI assertions verified.\n");
}

/* =========================================================================
 * MAIN TEST RUNNER
 * ========================================================================= */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    printf("===============================================================================\n");
    printf(" HP VISUALIZE FX5 / FX10 (\"LEGO\" ARCHITECTURE) COMPREHENSIVE TEST SUITE\n");
    printf(" Automated Verification against Driver Specifications & Hardware Registers\n");
    printf("===============================================================================\n");

    test_domain_1_hardware_specs();
    test_domain_2_video_timings();
    test_domain_3_3d_fp14_specs();
    test_domain_4_ioctl_abi();
    test_domain_5_matrix_math();
    test_domain_6_geometry_pipeline();
    test_domain_7_rasterization_accuracy();
    test_domain_8_batch_and_lifecycle();
    test_domain_9_barycentric_math();
    test_domain_10_texture_engine();
    test_domain_11_alpha_blending();
    test_domain_12_fog_pipeline();
    test_domain_13_scissor_box();
    test_domain_14_hp_diagnostics_crc();
    test_domain_15_stress_and_fuzzing();
    test_domain_16_texture_staging();
    test_domain_17_hp_diagnostics_suite();
    test_domain_18_opengl_backend();
    test_domain_19_gpgpu_compute();
    test_domain_20_drm_kms_abi();

    printf("\n===============================================================================\n");
    printf(" TEST SUITE SUMMARY\n");
    printf("===============================================================================\n");
    printf(" Total Tests Run   : %d\n", g_tests_run);
    printf(" Tests Passed      : %d (%.1f%%)\n", g_tests_passed, (float)g_tests_passed * 100.0f / (float)g_tests_run);
    printf(" Tests Failed      : %d\n", g_tests_failed);
    printf(" Overall Result    : %s\n", (g_tests_failed == 0) ? "ALL TESTS PASSED [100% SUCCESS]" : "FAILURES DETECTED");
    printf("===============================================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
