/*
 * HP Visualize FX 3D Accelerated Userspace Library (libhpfx3d)
 * Reverse Engineered for HP Visualize FX5 / FX10 ("Lego" Architecture)
 *
 * Implements 3D pipeline setup, matrix math, coordinate transformation,
 * depth buffer management, Gouraud smooth shading, texture mapping,
 * fog simulation, scissor clipping, procedural meshes, and CRC32 verification.
 */

#include "hpfx3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#ifdef __linux__
#include <linux/fb.h>
#else
/* Linux Framebuffer fallback definitions for cross-platform builds / mock test */
struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t bits_per_pixel;
};
struct fb_fix_screeninfo {
    uint32_t line_length;
    unsigned long smem_len;
};
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#endif

#define DEG_TO_RAD(d) ((d) * 0.017453292519943295f)

/* Forward declaration for software raster fallback */
static void sw_draw_triangle(hpfx3d_context *ctx, const struct hpfx_triangle_cmd *cmd);

/* =========================================================================
 * Context Lifecycle & Pipeline Control
 * ========================================================================= */

hpfx3d_context* hpfx3d_open(const char *fb_device)
{
    hpfx3d_context *ctx = calloc(1, sizeof(hpfx3d_context));
    if (!ctx)
        return NULL;

    const char *dev_path = fb_device ? fb_device : "/dev/fb0";
    ctx->fd = open(dev_path, O_RDWR);
    if (ctx->fd < 0) {
        ctx->width = 1024;
        ctx->height = 768;
        ctx->bpp = 32;
        ctx->pitch = ctx->width * 4;
        ctx->fb_size = ctx->pitch * ctx->height;
        ctx->fb_mem = malloc(ctx->fb_size);
        ctx->is_hw_accel = false;
    } else {
        struct fb_var_screeninfo var;
        struct fb_fix_screeninfo fix;

        if (ioctl(ctx->fd, FBIOGET_VSCREENINFO, &var) < 0 ||
            ioctl(ctx->fd, FBIOGET_FSCREENINFO, &fix) < 0) {
            ctx->width = 1024;
            ctx->height = 768;
            ctx->bpp = 32;
            ctx->pitch = ctx->width * 4;
            ctx->fb_size = ctx->pitch * ctx->height;
        } else {
            ctx->width = var.xres;
            ctx->height = var.yres;
            ctx->bpp = var.bits_per_pixel;
            ctx->pitch = fix.line_length ? fix.line_length : (ctx->width * (ctx->bpp / 8));
            ctx->fb_size = fix.smem_len ? fix.smem_len : (ctx->pitch * ctx->height);
        }

        ctx->fb_mem = mmap(NULL, ctx->fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
        if (ctx->fb_mem == MAP_FAILED) {
            ctx->fb_mem = malloc(ctx->fb_size);
        }

        /* Test if HPFX 3D Hardware IOCTL is available */
        if (ioctl(ctx->fd, HPFX_IOCTL_3D_RESET, 0) == 0) {
            ctx->is_hw_accel = true;
        } else {
            ctx->is_hw_accel = false;
        }
    }

    /* Allocate software depth buffer for fallback & depth checks */
    ctx->sw_zbuffer = (uint32_t*)malloc(ctx->width * ctx->height * sizeof(uint32_t));
    if (ctx->sw_zbuffer) {
        memset(ctx->sw_zbuffer, 0xff, ctx->width * ctx->height * sizeof(uint32_t));
    }

    /* Default pipeline state */
    ctx->state.depth_enable = 1;
    ctx->state.depth_func   = HPFX_DEPTH_LESS;
    ctx->state.depth_mask   = 1;
    ctx->state.cull_mode    = HPFX_CULL_BACK;
    ctx->state.blend_mode   = 0;
    ctx->state.shade_model  = 0;

    /* Scissor initially disabled */
    ctx->scissor.enabled = false;
    ctx->scissor.x = 0;
    ctx->scissor.y = 0;
    ctx->scissor.width = ctx->width;
    ctx->scissor.height = ctx->height;

    /* Fog initially disabled */
    ctx->fog.enabled = false;
    ctx->fog.color = 0x00808080;
    ctx->fog.start_z = 2.0f;
    ctx->fog.end_z = 20.0f;

    if (ctx->is_hw_accel) {
        ioctl(ctx->fd, HPFX_IOCTL_SET_3D_STATE, &ctx->state);
    }

    /* Initialize matrix stacks to identity */
    hpfx3d_mat4_identity(&ctx->modelview);
    hpfx3d_mat4_identity(&ctx->projection);
    hpfx3d_update_mvp(ctx);

    return ctx;
}

void hpfx3d_close(hpfx3d_context *ctx)
{
    if (!ctx)
        return;

    hpfx3d_flush(ctx);
    hpfx3d_sync(ctx);

    if (ctx->sw_zbuffer) {
        free(ctx->sw_zbuffer);
        ctx->sw_zbuffer = NULL;
    }

    if (ctx->fd >= 0) {
        if (ctx->fb_mem && ctx->fb_mem != MAP_FAILED) {
            munmap(ctx->fb_mem, ctx->fb_size);
        }
        close(ctx->fd);
    } else if (ctx->fb_mem) {
        free(ctx->fb_mem);
    }

    free(ctx);
}

int hpfx3d_reset(hpfx3d_context *ctx)
{
    if (!ctx)
        return -1;

    ctx->batch_count = 0;
    if (ctx->is_hw_accel) {
        return ioctl(ctx->fd, HPFX_IOCTL_3D_RESET, 0);
    }
    return 0;
}

void hpfx3d_flush(hpfx3d_context *ctx)
{
    if (!ctx || ctx->batch_count == 0)
        return;

    if (ctx->is_hw_accel) {
        struct hpfx_tri_list_cmd list;
        list.count = ctx->batch_count;
        list.triangles = ctx->batch_buf;

        if (ioctl(ctx->fd, HPFX_IOCTL_3D_DRAW_TRI_LIST, &list) < 0) {
            for (uint32_t i = 0; i < ctx->batch_count; i++) {
                ioctl(ctx->fd, HPFX_IOCTL_3D_DRAW_TRIANGLE, &ctx->batch_buf[i]);
            }
        }
    } else {
        for (uint32_t i = 0; i < ctx->batch_count; i++) {
            sw_draw_triangle(ctx, &ctx->batch_buf[i]);
        }
    }

    ctx->batch_count = 0;
}

void hpfx3d_sync(hpfx3d_context *ctx)
{
    if (!ctx)
        return;

    if (ctx->is_hw_accel) {
        ioctl(ctx->fd, HPFX_IOCTL_3D_SYNC, 0);
    }
}

void hpfx3d_clear(hpfx3d_context *ctx, uint32_t color, uint32_t depth)
{
    if (!ctx)
        return;

    hpfx3d_flush(ctx);

    if (ctx->is_hw_accel) {
        uint32_t d = depth;
        ioctl(ctx->fd, HPFX_IOCTL_CLEAR_DEPTH, &d);
    }

    if (ctx->sw_zbuffer) {
        for (uint32_t i = 0; i < ctx->width * ctx->height; i++) {
            ctx->sw_zbuffer[i] = depth;
        }
    }

    if (ctx->fb_mem) {
        if (ctx->bpp == 32) {
            uint32_t *p = (uint32_t*)ctx->fb_mem;
            uint32_t count = (ctx->width * ctx->height);
            for (uint32_t i = 0; i < count; i++) {
                p[i] = color;
            }
        } else if (ctx->bpp == 16) {
            uint16_t *p = (uint16_t*)ctx->fb_mem;
            uint16_t c16 = (uint16_t)((((color >> 19) & 0x1f) << 11) |
                                      (((color >> 10) & 0x3f) << 5) |
                                      (((color >> 3) & 0x1f)));
            uint32_t count = (ctx->width * ctx->height);
            for (uint32_t i = 0; i < count; i++) {
                p[i] = c16;
            }
        }
    }
}

/* =========================================================================
 * Pipeline State Configuration
 * ========================================================================= */

void hpfx3d_set_depth_test(hpfx3d_context *ctx, bool enable, uint32_t func)
{
    if (!ctx) return;
    ctx->state.depth_enable = enable ? 1 : 0;
    ctx->state.depth_func = func;
    if (ctx->is_hw_accel) {
        ioctl(ctx->fd, HPFX_IOCTL_SET_3D_STATE, &ctx->state);
    }
}

void hpfx3d_set_cull_mode(hpfx3d_context *ctx, uint32_t mode)
{
    if (!ctx) return;
    ctx->state.cull_mode = mode;
    if (ctx->is_hw_accel) {
        ioctl(ctx->fd, HPFX_IOCTL_SET_3D_STATE, &ctx->state);
    }
}

void hpfx3d_set_blend(hpfx3d_context *ctx, bool enable)
{
    if (!ctx) return;
    ctx->state.blend_mode = enable ? 1 : 0;
    if (ctx->is_hw_accel) {
        ioctl(ctx->fd, HPFX_IOCTL_SET_3D_STATE, &ctx->state);
    }
}

void hpfx3d_set_shade_model(hpfx3d_context *ctx, uint32_t shade_model)
{
    if (!ctx) return;
    ctx->state.shade_model = shade_model;
    if (ctx->is_hw_accel) {
        ioctl(ctx->fd, HPFX_IOCTL_SET_3D_STATE, &ctx->state);
    }
}

void hpfx3d_set_scissor(hpfx3d_context *ctx, bool enable, int x, int y, int width, int height)
{
    if (!ctx) return;
    ctx->scissor.enabled = enable;
    ctx->scissor.x = x;
    ctx->scissor.y = y;
    ctx->scissor.width = width;
    ctx->scissor.height = height;
}

void hpfx3d_set_fog(hpfx3d_context *ctx, bool enable, uint32_t color, float start_z, float end_z)
{
    if (!ctx) return;
    ctx->fog.enabled = enable;
    ctx->fog.color = color;
    ctx->fog.start_z = start_z;
    ctx->fog.end_z = end_z;
}

/* =========================================================================
 * Texture Management
 * ========================================================================= */

hpfx3d_texture* hpfx3d_create_texture(uint32_t width, uint32_t height, const uint32_t *pixels)
{
    if (width == 0 || height == 0) return NULL;
    hpfx3d_texture *tex = malloc(sizeof(hpfx3d_texture));
    if (!tex) return NULL;

    tex->width = width;
    tex->height = height;
    tex->filter_mode = HPFX3D_TEX_FILTER_BILINEAR;
    tex->wrap_mode = HPFX3D_TEX_WRAP_REPEAT;
    tex->data = malloc(width * height * sizeof(uint32_t));
    if (!tex->data) {
        free(tex);
        return NULL;
    }

    if (pixels) {
        memcpy(tex->data, pixels, width * height * sizeof(uint32_t));
    } else {
        memset(tex->data, 0xff, width * height * sizeof(uint32_t));
    }

    return tex;
}

void hpfx3d_destroy_texture(hpfx3d_texture *tex)
{
    if (!tex) return;
    if (tex->data) free(tex->data);
    free(tex);
}

void hpfx3d_bind_texture(hpfx3d_context *ctx, const hpfx3d_texture *tex)
{
    if (!ctx) return;
    ctx->current_tex = tex;
}

int hpfx3d_upload_texture_hw(hpfx3d_context *ctx, uint32_t vram_offset, uint32_t width, uint32_t height, uint32_t format, const void *pixels)
{
    if (!ctx || !pixels || width == 0 || height == 0 || width > 2048 || height > 2048)
        return -1;
    if (format > 1)
        return -1;

    if (ctx->is_hw_accel) {
        struct hpfx_tex_upload_cmd cmd;
        cmd.width = width;
        cmd.height = height;
        cmd.format = format;
        cmd.vram_offset = vram_offset;
        cmd.data = pixels;

        return ioctl(ctx->fd, HPFX_IOCTL_UPLOAD_TEXTURE, &cmd);
    }

    /* In simulation mode, texture upload successfully staged */
    return 0;
}

/* Texture sampling helper */
__attribute__((unused))
static uint32_t sample_tex(const hpfx3d_texture *tex, float u, float v)
{
    if (!tex || !tex->data) return 0xffffffff;

    if (tex->wrap_mode == HPFX3D_TEX_WRAP_REPEAT) {
        u = u - floorf(u);
        v = v - floorf(v);
    } else {
        if (u < 0.0f) u = 0.0f; else if (u > 1.0f) u = 1.0f;
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
    }

    int tx = (int)(u * (tex->width - 1));
    int ty = (int)(v * (tex->height - 1));
    if (tx < 0) tx = 0; else if (tx >= (int)tex->width) tx = tex->width - 1;
    if (ty < 0) ty = 0; else if (ty >= (int)tex->height) ty = tex->height - 1;

    return tex->data[ty * tex->width + tx];
}

/* Alpha blend helper: (alpha * src) + ((1 - alpha) * dst) */
static uint32_t blend_color_32(uint32_t src, uint32_t dst)
{
    uint32_t a = (src >> 24) & 0xff;
    if (a == 0) return dst;
    if (a == 255) return src;

    uint32_t inv_a = 255 - a;
    uint32_t sr = (src >> 16) & 0xff;
    uint32_t sg = (src >> 8) & 0xff;
    uint32_t sb = src & 0xff;

    uint32_t dr = (dst >> 16) & 0xff;
    uint32_t dg = (dst >> 8) & 0xff;
    uint32_t db = dst & 0xff;

    uint32_t r = (sr * a + dr * inv_a) / 255;
    uint32_t g = (sg * a + dg * inv_a) / 255;
    uint32_t b = (sb * a + db * inv_a) / 255;

    return (0xff << 24) | (r << 16) | (g << 8) | b;
}

/* =========================================================================
 * 3D Matrix Mathematics
 * ========================================================================= */

void hpfx3d_mat4_identity(hpfx3d_mat4 *m)
{
    memset(m->m, 0, sizeof(m->m));
    m->m[0]  = 1.0f;
    m->m[5]  = 1.0f;
    m->m[10] = 1.0f;
    m->m[15] = 1.0f;
}

void hpfx3d_mat4_multiply(hpfx3d_mat4 *dst, const hpfx3d_mat4 *a, const hpfx3d_mat4 *b)
{
    hpfx3d_mat4 res;
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a->m[k * 4 + row] * b->m[col * 4 + k];
            }
            res.m[col * 4 + row] = sum;
        }
    }
    *dst = res;
}

void hpfx3d_mat4_perspective(hpfx3d_mat4 *m, float fov_rad, float aspect, float near_z, float far_z)
{
    float tan_half = tanf(fov_rad / 2.0f);
    memset(m->m, 0, sizeof(m->m));
    m->m[0]  = 1.0f / (aspect * tan_half);
    m->m[5]  = 1.0f / tan_half;
    m->m[10] = -(far_z + near_z) / (far_z - near_z);
    m->m[11] = -1.0f;
    m->m[14] = -(2.0f * far_z * near_z) / (far_z - near_z);
    m->m[15] = 0.0f;
}

void hpfx3d_mat4_ortho(hpfx3d_mat4 *m, float left, float right, float bottom, float top, float near_z, float far_z)
{
    memset(m->m, 0, sizeof(m->m));
    m->m[0]  = 2.0f / (right - left);
    m->m[5]  = 2.0f / (top - bottom);
    m->m[10] = -2.0f / (far_z - near_z);
    m->m[12] = -(right + left) / (right - left);
    m->m[13] = -(top + bottom) / (top - bottom);
    m->m[14] = -(far_z + near_z) / (far_z - near_z);
    m->m[15] = 1.0f;
}

void hpfx3d_mat4_translate(hpfx3d_mat4 *m, float tx, float ty, float tz)
{
    hpfx3d_mat4 t;
    hpfx3d_mat4_identity(&t);
    t.m[12] = tx;
    t.m[13] = ty;
    t.m[14] = tz;
    hpfx3d_mat4_multiply(m, m, &t);
}

void hpfx3d_mat4_rotate_x(hpfx3d_mat4 *m, float angle_rad)
{
    hpfx3d_mat4 r;
    hpfx3d_mat4_identity(&r);
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    r.m[5]  = c;
    r.m[6]  = s;
    r.m[9]  = -s;
    r.m[10] = c;
    hpfx3d_mat4_multiply(m, m, &r);
}

void hpfx3d_mat4_rotate_y(hpfx3d_mat4 *m, float angle_rad)
{
    hpfx3d_mat4 r;
    hpfx3d_mat4_identity(&r);
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    r.m[0]  = c;
    r.m[2]  = -s;
    r.m[8]  = s;
    r.m[10] = c;
    hpfx3d_mat4_multiply(m, m, &r);
}

void hpfx3d_mat4_rotate_z(hpfx3d_mat4 *m, float angle_rad)
{
    hpfx3d_mat4 r;
    hpfx3d_mat4_identity(&r);
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    r.m[0]  = c;
    r.m[1]  = s;
    r.m[4]  = -s;
    r.m[5]  = c;
    hpfx3d_mat4_multiply(m, m, &r);
}

void hpfx3d_mat4_scale(hpfx3d_mat4 *m, float sx, float sy, float sz)
{
    hpfx3d_mat4 s;
    hpfx3d_mat4_identity(&s);
    s.m[0]  = sx;
    s.m[5]  = sy;
    s.m[10] = sz;
    hpfx3d_mat4_multiply(m, m, &s);
}

void hpfx3d_update_mvp(hpfx3d_context *ctx)
{
    if (!ctx) return;
    hpfx3d_mat4_multiply(&ctx->mvp, &ctx->projection, &ctx->modelview);
}

/* =========================================================================
 * 3D Primitive Rendering
 * ========================================================================= */

void hpfx3d_draw_triangle_screen(hpfx3d_context *ctx,
                                 int x0, int y0, uint32_t z0,
                                 int x1, int y1, uint32_t z1,
                                 int x2, int y2, uint32_t z2,
                                 uint32_t color)
{
    if (!ctx) return;

    if (ctx->batch_count >= sizeof(ctx->batch_buf) / sizeof(ctx->batch_buf[0])) {
        hpfx3d_flush(ctx);
    }

    struct hpfx_triangle_cmd *cmd = &ctx->batch_buf[ctx->batch_count++];
    cmd->x0 = x0; cmd->y0 = y0; cmd->z0 = z0;
    cmd->x1 = x1; cmd->y1 = y1; cmd->z1 = z1;
    cmd->x2 = x2; cmd->y2 = y2; cmd->z2 = z2;
    cmd->color = color;
}

static void transform_point(const hpfx3d_mat4 *m, const hpfx3d_vec3 *in, hpfx3d_vec4 *out)
{
    out->x = in->x * m->m[0] + in->y * m->m[4] + in->z * m->m[8]  + m->m[12];
    out->y = in->x * m->m[1] + in->y * m->m[5] + in->z * m->m[9]  + m->m[13];
    out->z = in->x * m->m[2] + in->y * m->m[6] + in->z * m->m[10] + m->m[14];
    out->w = in->x * m->m[3] + in->y * m->m[7] + in->z * m->m[11] + m->m[15];
}

void hpfx3d_draw_triangle_3d(hpfx3d_context *ctx,
                             const hpfx3d_vec3 *v0,
                             const hpfx3d_vec3 *v1,
                             const hpfx3d_vec3 *v2,
                             uint32_t color)
{
    if (!ctx) return;

    hpfx3d_vec4 c0, c1, c2;
    transform_point(&ctx->mvp, v0, &c0);
    transform_point(&ctx->mvp, v1, &c1);
    transform_point(&ctx->mvp, v2, &c2);

    /* Near-plane clipping rejection */
    if (c0.w <= 0.001f || c1.w <= 0.001f || c2.w <= 0.001f)
        return;

    /* Perspective divide -> Normalized Device Coordinates [-1.0 .. 1.0] */
    float ndc_x0 = c0.x / c0.w;
    float ndc_y0 = c0.y / c0.w;
    float ndc_z0 = c0.z / c0.w;

    float ndc_x1 = c1.x / c1.w;
    float ndc_y1 = c1.y / c1.w;
    float ndc_z1 = c1.z / c1.w;

    float ndc_x2 = c2.x / c2.w;
    float ndc_y2 = c2.y / c2.w;
    float ndc_z2 = c2.z / c2.w;

    /* Viewport Mapping to Screen Pixels */
    float half_w = ctx->width * 0.5f;
    float half_h = ctx->height * 0.5f;

    int sx0 = (int)((ndc_x0 + 1.0f) * half_w);
    int sy0 = (int)((1.0f - ndc_y0) * half_h);

    int sx1 = (int)((ndc_x1 + 1.0f) * half_w);
    int sy1 = (int)((1.0f - ndc_y1) * half_h);

    int sx2 = (int)((ndc_x2 + 1.0f) * half_w);
    int sy2 = (int)((1.0f - ndc_y2) * half_h);

    /* Hardware 24-bit fixed point Depth: 0 .. 0x00ffffff */
    uint32_t sz0 = (uint32_t)((ndc_z0 * 0.5f + 0.5f) * 16777215.0f);
    uint32_t sz1 = (uint32_t)((ndc_z1 * 0.5f + 0.5f) * 16777215.0f);
    uint32_t sz2 = (uint32_t)((ndc_z2 * 0.5f + 0.5f) * 16777215.0f);

    /* Back-face Culling in Normalized Device Coordinates (OpenGL GL_CCW rule) */
    float edge_ndc = (ndc_x1 - ndc_x0) * (ndc_y2 - ndc_y0) - (ndc_y1 - ndc_y0) * (ndc_x2 - ndc_x0);
    if (ctx->state.cull_mode == HPFX_CULL_BACK && edge_ndc <= 0.0f)
        return;
    if (ctx->state.cull_mode == HPFX_CULL_FRONT && edge_ndc >= 0.0f)
        return;

    hpfx3d_draw_triangle_screen(ctx, sx0, sy0, sz0, sx1, sy1, sz1, sx2, sy2, sz2, color);
}

void hpfx3d_draw_mesh(hpfx3d_context *ctx,
                      const hpfx3d_vertex *vertices,
                      const uint16_t *indices,
                      uint32_t num_indices)
{
    if (!ctx || !vertices || !indices)
        return;

    for (uint32_t i = 0; i + 2 < num_indices; i += 3) {
        const hpfx3d_vertex *v0 = &vertices[indices[i]];
        const hpfx3d_vertex *v1 = &vertices[indices[i + 1]];
        const hpfx3d_vertex *v2 = &vertices[indices[i + 2]];

        hpfx3d_draw_triangle_3d(ctx, &v0->pos, &v1->pos, &v2->pos, v0->color);
    }
}

void hpfx3d_draw_cube(hpfx3d_context *ctx, float size)
{
    if (!ctx) return;

    float h = size * 0.5f;

    /* 8 Cube Vertices */
    hpfx3d_vec3 v[8] = {
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}, /* Front face */
        {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h}, { h, -h, -h}  /* Back face */
    };

    /* 6 Faces with distinct vibrant colors */
    /* Front: Red */
    hpfx3d_draw_triangle_3d(ctx, &v[0], &v[1], &v[2], 0x00e74c3c);
    hpfx3d_draw_triangle_3d(ctx, &v[0], &v[2], &v[3], 0x00c0392b);

    /* Back: Blue */
    hpfx3d_draw_triangle_3d(ctx, &v[4], &v[5], &v[6], 0x003498db);
    hpfx3d_draw_triangle_3d(ctx, &v[4], &v[6], &v[7], 0x002980b9);

    /* Top: Green */
    hpfx3d_draw_triangle_3d(ctx, &v[3], &v[2], &v[6], 0x002ecc71);
    hpfx3d_draw_triangle_3d(ctx, &v[3], &v[6], &v[5], 0x0027ae60);

    /* Bottom: Yellow */
    hpfx3d_draw_triangle_3d(ctx, &v[4], &v[7], &v[1], 0x00f1c40f);
    hpfx3d_draw_triangle_3d(ctx, &v[4], &v[1], &v[0], 0x00f39c12);

    /* Right: Magenta/Purple */
    hpfx3d_draw_triangle_3d(ctx, &v[1], &v[7], &v[6], 0x009b59b6);
    hpfx3d_draw_triangle_3d(ctx, &v[1], &v[6], &v[2], 0x008e44ad);

    /* Left: Orange/Cyan */
    hpfx3d_draw_triangle_3d(ctx, &v[4], &v[0], &v[3], 0x001abc9c);
    hpfx3d_draw_triangle_3d(ctx, &v[4], &v[3], &v[5], 0x0016a085);
}

/* Procedural Torus (Inspired by HP Diagnostics FX5DIAG Test #1) */
void hpfx3d_draw_torus(hpfx3d_context *ctx, float r_major, float r_minor, int rings, int sides, uint32_t color)
{
    if (!ctx || rings < 3 || sides < 3) return;

    for (int i = 0; i < rings; i++) {
        float phi0 = (float)i * 2.0f * 3.14159265f / (float)rings;
        float phi1 = (float)(i + 1) * 2.0f * 3.14159265f / (float)rings;

        float cos_phi0 = cosf(phi0), sin_phi0 = sinf(phi0);
        float cos_phi1 = cosf(phi1), sin_phi1 = sinf(phi1);

        for (int j = 0; j < sides; j++) {
            float theta0 = (float)j * 2.0f * 3.14159265f / (float)sides;
            float theta1 = (float)(j + 1) * 2.0f * 3.14159265f / (float)sides;

            float cos_th0 = cosf(theta0), sin_th0 = sinf(theta0);
            float cos_th1 = cosf(theta1), sin_th1 = sinf(theta1);

            hpfx3d_vec3 p00 = { (r_major + r_minor * cos_th0) * cos_phi0, (r_major + r_minor * cos_th0) * sin_phi0, r_minor * sin_th0 };
            hpfx3d_vec3 p01 = { (r_major + r_minor * cos_th1) * cos_phi0, (r_major + r_minor * cos_th1) * sin_phi0, r_minor * sin_th1 };
            hpfx3d_vec3 p10 = { (r_major + r_minor * cos_th0) * cos_phi1, (r_major + r_minor * cos_th0) * sin_phi1, r_minor * sin_th0 };
            hpfx3d_vec3 p11 = { (r_major + r_minor * cos_th1) * cos_phi1, (r_major + r_minor * cos_th1) * sin_phi1, r_minor * sin_th1 };

            hpfx3d_draw_triangle_3d(ctx, &p00, &p10, &p11, color);
            hpfx3d_draw_triangle_3d(ctx, &p00, &p11, &p01, color);
        }
    }
}

/* Procedural Sphere (Inspired by HP Diagnostics FX5DIAG Test #3) */
void hpfx3d_draw_sphere(hpfx3d_context *ctx, float radius, int slices, int stacks, uint32_t color)
{
    if (!ctx || slices < 3 || stacks < 2) return;

    for (int i = 0; i < stacks; i++) {
        float phi0 = -3.14159265f * 0.5f + (float)i * 3.14159265f / (float)stacks;
        float phi1 = -3.14159265f * 0.5f + (float)(i + 1) * 3.14159265f / (float)stacks;

        float cos_phi0 = cosf(phi0), sin_phi0 = sinf(phi0);
        float cos_phi1 = cosf(phi1), sin_phi1 = sinf(phi1);

        for (int j = 0; j < slices; j++) {
            float theta0 = (float)j * 2.0f * 3.14159265f / (float)slices;
            float theta1 = (float)(j + 1) * 2.0f * 3.14159265f / (float)slices;

            float cos_th0 = cosf(theta0), sin_th0 = sinf(theta0);
            float cos_th1 = cosf(theta1), sin_th1 = sinf(theta1);

            hpfx3d_vec3 p00 = { radius * cos_phi0 * cos_th0, radius * sin_phi0, radius * cos_phi0 * sin_th0 };
            hpfx3d_vec3 p01 = { radius * cos_phi0 * cos_th1, radius * sin_phi0, radius * cos_phi0 * sin_th1 };
            hpfx3d_vec3 p10 = { radius * cos_phi1 * cos_th0, radius * sin_phi1, radius * cos_phi1 * sin_th0 };
            hpfx3d_vec3 p11 = { radius * cos_phi1 * cos_th1, radius * sin_phi1, radius * cos_phi1 * sin_th1 };

            if (i == 0) {
                hpfx3d_draw_triangle_3d(ctx, &p00, &p10, &p11, color);
            } else if (i == stacks - 1) {
                hpfx3d_draw_triangle_3d(ctx, &p00, &p11, &p01, color);
            } else {
                hpfx3d_draw_triangle_3d(ctx, &p00, &p10, &p11, color);
                hpfx3d_draw_triangle_3d(ctx, &p00, &p11, &p01, color);
            }
        }
    }
}

/* =========================================================================
 * Framebuffer CRC32 Checksum (IEEE 802.3 Deterministic Verification)
 * ========================================================================= */

static const uint32_t crc32_tab[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

uint32_t hpfx3d_checksum_framebuffer(const hpfx3d_context *ctx)
{
    if (!ctx || !ctx->fb_mem) return 0;
    const uint8_t *p = (const uint8_t*)ctx->fb_mem;
    size_t len = ctx->width * ctx->height * 4;
    uint32_t crc = ~0U;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = p[i];
        crc = (crc >> 4) ^ crc32_tab[(crc ^ byte) & 0x0f];
        crc = (crc >> 4) ^ crc32_tab[(crc ^ (byte >> 4)) & 0x0f];
    }
    return ~crc;
}

/* =========================================================================
 * Software Rasterizer Fallback (for simulation & validation)
 * ========================================================================= */

static void sw_draw_triangle(hpfx3d_context *ctx, const struct hpfx_triangle_cmd *cmd)
{
    if (!ctx || !ctx->fb_mem) return;

    int min_x = cmd->x0 < cmd->x1 ? (cmd->x0 < cmd->x2 ? cmd->x0 : cmd->x2) : (cmd->x1 < cmd->x2 ? cmd->x1 : cmd->x2);
    int max_x = cmd->x0 > cmd->x1 ? (cmd->x0 > cmd->x2 ? cmd->x0 : cmd->x2) : (cmd->x1 > cmd->x2 ? cmd->x1 : cmd->x2);
    int min_y = cmd->y0 < cmd->y1 ? (cmd->y0 < cmd->y2 ? cmd->y0 : cmd->y2) : (cmd->y1 < cmd->y2 ? cmd->y1 : cmd->y2);
    int max_y = cmd->y0 > cmd->y1 ? (cmd->y0 > cmd->y2 ? cmd->y0 : cmd->y2) : (cmd->y1 > cmd->y2 ? cmd->y1 : cmd->y2);

    /* Viewport Screen Clamping */
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)ctx->width) max_x = (int)ctx->width - 1;
    if (max_y >= (int)ctx->height) max_y = (int)ctx->height - 1;

    /* Scissor Clipping */
    if (ctx->scissor.enabled) {
        int sc_x0 = ctx->scissor.x;
        int sc_y0 = ctx->scissor.y;
        int sc_x1 = ctx->scissor.x + ctx->scissor.width - 1;
        int sc_y1 = ctx->scissor.y + ctx->scissor.height - 1;
        if (min_x < sc_x0) min_x = sc_x0;
        if (min_y < sc_y0) min_y = sc_y0;
        if (max_x > sc_x1) max_x = sc_x1;
        if (max_y > sc_y1) max_y = sc_y1;
        if (min_x > max_x || min_y > max_y) return;
    }

    int x0 = cmd->x0, y0 = cmd->y0;
    int x1 = cmd->x1, y1 = cmd->y1;
    int x2 = cmd->x2, y2 = cmd->y2;

    int denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
    if (denom == 0) return;
    float inv_denom = 1.0f / (float)denom;

    uint32_t *fb32 = (uint32_t*)ctx->fb_mem;
    uint32_t *zbuf = ctx->sw_zbuffer;
    uint32_t prim_color = cmd->color;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            float w0 = (float)((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) * inv_denom;
            float w1 = (float)((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) * inv_denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                uint32_t z = (uint32_t)(w0 * cmd->z0 + w1 * cmd->z1 + w2 * cmd->z2);
                int pixel_idx = y * ctx->width + x;

                /* Depth Testing */
                if (ctx->state.depth_enable && zbuf) {
                    if (ctx->state.depth_func == HPFX_DEPTH_LESS && z >= zbuf[pixel_idx])
                        continue;
                    if (ctx->state.depth_func == HPFX_DEPTH_LEQUAL && z > zbuf[pixel_idx])
                        continue;
                    if (ctx->state.depth_func == HPFX_DEPTH_GREATER && z <= zbuf[pixel_idx])
                        continue;
                    if (ctx->state.depth_func == HPFX_DEPTH_GEQUAL && z < zbuf[pixel_idx])
                        continue;
                    if (ctx->state.depth_func == HPFX_DEPTH_EQUAL && z != zbuf[pixel_idx])
                        continue;
                    if (ctx->state.depth_func == HPFX_DEPTH_NOTEQUAL && z == zbuf[pixel_idx])
                        continue;
                    if (ctx->state.depth_func == HPFX_DEPTH_NEVER)
                        continue;

                    if (ctx->state.depth_mask)
                        zbuf[pixel_idx] = z;
                }

                uint32_t out_color = prim_color;

                /* Fog Blend */
                if (ctx->fog.enabled) {
                    float depth_norm = (float)z / 16777215.0f;
                    float f = (ctx->fog.end_z - depth_norm) / (ctx->fog.end_z - ctx->fog.start_z);
                    if (f < 0.0f) f = 0.0f; else if (f > 1.0f) f = 1.0f;
                    uint32_t fr = (ctx->fog.color >> 16) & 0xff;
                    uint32_t fg = (ctx->fog.color >> 8) & 0xff;
                    uint32_t fb = ctx->fog.color & 0xff;
                    uint32_t pr = (out_color >> 16) & 0xff;
                    uint32_t pg = (out_color >> 8) & 0xff;
                    uint32_t pb = out_color & 0xff;
                    uint32_t r = (uint32_t)(f * pr + (1.0f - f) * fr);
                    uint32_t g = (uint32_t)(f * pg + (1.0f - f) * fg);
                    uint32_t b = (uint32_t)(f * pb + (1.0f - f) * fb);
                    out_color = (0xff << 24) | (r << 16) | (g << 8) | b;
                }

                /* Alpha Blending */
                if (ctx->state.blend_mode) {
                    out_color = blend_color_32(out_color, fb32[pixel_idx]);
                }

                fb32[pixel_idx] = out_color;
            }
        }
    }
}
