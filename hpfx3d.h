/*
 * HP Visualize FX 3D Accelerated Userspace Library (libhpfx3d)
 * Reverse Engineered for HP Visualize FX5 / FX10 ("Lego" Architecture)
 *
 * Provides 3D pipeline setup, matrix math, coordinate transformation,
 * depth buffer management, Gouraud smooth shading, texturing, fog,
 * procedural meshes (Torus, Sphere, Cube), and hardware triangle dispatch.
 */

#ifndef _HPFX3D_H_
#define _HPFX3D_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hpfx_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Texture Wrap & Filter Modes */
#define HPFX3D_TEX_FILTER_NEAREST   0
#define HPFX3D_TEX_FILTER_BILINEAR  1
#define HPFX3D_TEX_WRAP_CLAMP       0
#define HPFX3D_TEX_WRAP_REPEAT      1

/* 3D Vector types */
typedef struct {
    float x, y, z;
} hpfx3d_vec3;

typedef struct {
    float x, y, z, w;
} hpfx3d_vec4;

/* 4x4 Matrix (Column-Major order) */
typedef struct {
    float m[16];
} hpfx3d_mat4;

/* 3D Vertex structure */
typedef struct {
    hpfx3d_vec3 pos;
    uint32_t color;  /* 0xRRGGBB or 0xAARRGGBB */
    float u, v;      /* Texture coordinates */
} hpfx3d_vertex;

/* Texture Structure */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t *data;
    int filter_mode;
    int wrap_mode;
} hpfx3d_texture;

/* Scissor Boundary */
typedef struct {
    bool enabled;
    int x, y;
    int width, height;
} hpfx3d_scissor;

/* Fog Parameters */
typedef struct {
    bool enabled;
    uint32_t color;
    float start_z;
    float end_z;
} hpfx3d_fog;

/* 3D Context */
typedef struct {
    int fd;                       /* Framebuffer /dev/fb* descriptor */
    void *fb_mem;                 /* Memory mapped framebuffer */
    size_t fb_size;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    
    bool is_hw_accel;             /* True if HPFX hardware IOCTLs succeed */
    
    /* Current Matrix Transforms */
    hpfx3d_mat4 modelview;
    hpfx3d_mat4 projection;
    hpfx3d_mat4 mvp;              /* Cached MVP = Projection * ModelView */
    
    /* Hardware 3D State */
    struct hpfx_3d_state_cmd state;
    
    /* Scissor and Fog Settings */
    hpfx3d_scissor scissor;
    hpfx3d_fog fog;
    
    /* Active Texture */
    const hpfx3d_texture *current_tex;
    
    /* Batch triangle submission buffer */
    struct hpfx_triangle_cmd batch_buf[1024];
    uint32_t batch_count;
    
    /* Software depth buffer fallback if running without HW */
    uint32_t *sw_zbuffer;
} hpfx3d_context;

/* =========================================================================
 * Context Lifecycle & Pipeline Control
 * ========================================================================= */

hpfx3d_context* hpfx3d_open(const char *fb_device);
void hpfx3d_close(hpfx3d_context *ctx);
int hpfx3d_reset(hpfx3d_context *ctx);
void hpfx3d_flush(hpfx3d_context *ctx);
void hpfx3d_sync(hpfx3d_context *ctx);
void hpfx3d_clear(hpfx3d_context *ctx, uint32_t color, uint32_t depth);

/* =========================================================================
 * Pipeline State Configuration
 * ========================================================================= */

void hpfx3d_set_depth_test(hpfx3d_context *ctx, bool enable, uint32_t func);
void hpfx3d_set_cull_mode(hpfx3d_context *ctx, uint32_t mode);
void hpfx3d_set_blend(hpfx3d_context *ctx, bool enable);
void hpfx3d_set_shade_model(hpfx3d_context *ctx, uint32_t shade_model);

void hpfx3d_set_scissor(hpfx3d_context *ctx, bool enable, int x, int y, int width, int height);
void hpfx3d_set_fog(hpfx3d_context *ctx, bool enable, uint32_t color, float start_z, float end_z);

/* =========================================================================
 * Texture Management
 * ========================================================================= */

hpfx3d_texture* hpfx3d_create_texture(uint32_t width, uint32_t height, const uint32_t *pixels);
void hpfx3d_destroy_texture(hpfx3d_texture *tex);
void hpfx3d_bind_texture(hpfx3d_context *ctx, const hpfx3d_texture *tex);
int hpfx3d_upload_texture_hw(hpfx3d_context *ctx, uint32_t vram_offset, uint32_t width, uint32_t height, uint32_t format, const void *pixels);

/* =========================================================================
 * 3D Matrix Mathematics
 * ========================================================================= */

void hpfx3d_mat4_identity(hpfx3d_mat4 *m);
void hpfx3d_mat4_multiply(hpfx3d_mat4 *dst, const hpfx3d_mat4 *a, const hpfx3d_mat4 *b);
void hpfx3d_mat4_perspective(hpfx3d_mat4 *m, float fov_rad, float aspect, float near_z, float far_z);
void hpfx3d_mat4_ortho(hpfx3d_mat4 *m, float left, float right, float bottom, float top, float near_z, float far_z);
void hpfx3d_mat4_translate(hpfx3d_mat4 *m, float tx, float ty, float tz);
void hpfx3d_mat4_rotate_x(hpfx3d_mat4 *m, float angle_rad);
void hpfx3d_mat4_rotate_y(hpfx3d_mat4 *m, float angle_rad);
void hpfx3d_mat4_rotate_z(hpfx3d_mat4 *m, float angle_rad);
void hpfx3d_mat4_scale(hpfx3d_mat4 *m, float sx, float sy, float sz);
void hpfx3d_update_mvp(hpfx3d_context *ctx);

/* =========================================================================
 * 3D Primitive Rendering
 * ========================================================================= */

void hpfx3d_draw_triangle_screen(hpfx3d_context *ctx,
                                 int x0, int y0, uint32_t z0,
                                 int x1, int y1, uint32_t z1,
                                 int x2, int y2, uint32_t z2,
                                 uint32_t color);

void hpfx3d_draw_triangle_3d(hpfx3d_context *ctx,
                             const hpfx3d_vec3 *v0,
                             const hpfx3d_vec3 *v1,
                             const hpfx3d_vec3 *v2,
                             uint32_t color);

void hpfx3d_draw_triangle_smooth(hpfx3d_context *ctx,
                                const hpfx3d_vertex *v0,
                                const hpfx3d_vertex *v1,
                                const hpfx3d_vertex *v2);

void hpfx3d_draw_mesh(hpfx3d_context *ctx,
                      const hpfx3d_vertex *vertices,
                      const uint16_t *indices,
                      uint32_t num_indices);

/* Procedural Meshes */
void hpfx3d_draw_cube(hpfx3d_context *ctx, float size);
void hpfx3d_draw_torus(hpfx3d_context *ctx, float r_major, float r_minor, int rings, int sides, uint32_t color);
void hpfx3d_draw_sphere(hpfx3d_context *ctx, float radius, int slices, int stacks, uint32_t color);

/* CRC32 Framebuffer Verification (deterministic regression testing) */
uint32_t hpfx3d_checksum_framebuffer(const hpfx3d_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _HPFX3D_H_ */
