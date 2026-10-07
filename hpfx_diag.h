/*
 * HP Visualize FX5 & FX10 Hardware Diagnostics Suite (FX5DIAG / FX5CRC Emulation)
 *
 * Reverse-engineered from official HP Windows 2000 diagnostics:
 * - FX5DIAG.W2K (PE32 diagnostics binary)
 * - FX5CRC.W2K  (Official golden CRC reference vector)
 *
 * Test Specifications:
 * - Render dimensions: 544 x 403 (0x220 x 0x193)
 * - Test 1: 3D Torus mesh (Golden CRC 16-bit: 0xa25c6a86, 32-bit: 0x3411fabb)
 * - Test 2: 3D shoe4R CAD model solid (Golden CRC 16-bit: 0x9861656d, 32-bit: 0x4b161f67)
 * - Test 3: 3D Sphere mesh (Golden CRC 16-bit: 0x9df8c3a9, 32-bit: 0x846c6000)
 * - Test 4: 3D shoe4R CAD model textured (Golden CRC 16-bit: 0x2d16310a, 32-bit: 0x1135eb4e)
 */

#ifndef _HPFX_DIAG_H_
#define _HPFX_DIAG_H_

#include <stdint.h>
#include <stdbool.h>
#include "hpfx3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HPFX_DIAG_WIDTH   544
#define HPFX_DIAG_HEIGHT  403

/* HP Official Golden CRCs from FX5CRC.W2K */
#define HPFX_CRC16_TEST1_TORUS       0xa25c6a86
#define HPFX_CRC16_TEST2_SHOE        0x9861656d
#define HPFX_CRC16_TEST3_SPHERE      0x9df8c3a9
#define HPFX_CRC16_TEST4_SHOE_TEX    0x2d16310a

#define HPFX_CRC32_TEST1_TORUS       0x3411fabb
#define HPFX_CRC32_TEST2_SHOE        0x4b161f67
#define HPFX_CRC32_TEST3_SPHERE      0x846c6000
#define HPFX_CRC32_TEST4_SHOE_TEX    0x1135eb4e

typedef struct {
    uint32_t test_id;
    const char *name;
    uint32_t expected_crc32;
    uint32_t actual_crc32;
    bool passed;
} hpfx_diag_result;

typedef struct {
    hpfx3d_context *ctx;
    hpfx_diag_result results[4];
} hpfx_diag_runner;

hpfx_diag_runner* hpfx_diag_init(const char *fb_dev);
void hpfx_diag_free(hpfx_diag_runner *runner);

/* Execute individual diagnostic tests */
uint32_t hpfx_diag_run_test1_torus(hpfx_diag_runner *runner);
uint32_t hpfx_diag_run_test2_shoe(hpfx_diag_runner *runner);
uint32_t hpfx_diag_run_test3_sphere(hpfx_diag_runner *runner);
uint32_t hpfx_diag_run_test4_shoe_textured(hpfx_diag_runner *runner);

/* Execute entire suite */
bool hpfx_diag_run_all(hpfx_diag_runner *runner);

#ifdef __cplusplus
}
#endif

#endif /* _HPFX_DIAG_H_ */
