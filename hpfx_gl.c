/*
 * HP Visualize FX5 & FX10 OpenGL 1.1 / TinyGL Hardware Backend Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_gl.h"

#define MATRIX_STACK_DEPTH_MV   32
#define MATRIX_STACK_DEPTH_PROJ 8
#define MAX_IMMEDIATE_VERTICES  2048
#define MAX_TEXTURES            32

typedef struct {
    hpfx3d_context *ctx;

    /* Matrix Stacks */
    GLenum matrix_mode;
    hpfx3d_mat4 mv_stack[MATRIX_STACK_DEPTH_MV];
    int mv_top;
    hpfx3d_mat4 proj_stack[MATRIX_STACK_DEPTH_PROJ];
    int proj_top;

    /* Current State */
    uint32_t current_color;
    float current_u, current_v;
    float current_nx, current_ny, current_nz;
    uint32_t clear_color;
    uint32_t clear_depth;

    /* Immediate Mode Primitives */
    bool in_begin;
    GLenum begin_mode;
    hpfx3d_vertex imm_vertices[MAX_IMMEDIATE_VERTICES];
    int imm_count;

    /* Textures */
    GLuint current_texture_id;
    hpfx3d_texture *textures[MAX_TEXTURES];
} hpfx_gl_state;

static hpfx_gl_state g_gl;

bool hpfx_gl_init(const char *fb_dev, int width, int height)
{
    memset(&g_gl, 0, sizeof(hpfx_gl_state));

    g_gl.ctx = hpfx3d_open(fb_dev);
    if (!g_gl.ctx) return false;

    if (width > 0 && height > 0) {
        hpfx3d_resize(g_gl.ctx, width, height);
    }

    g_gl.matrix_mode = GL_MODELVIEW;
    hpfx3d_mat4_identity(&g_gl.mv_stack[0]);
    g_gl.mv_top = 0;
    hpfx3d_mat4_identity(&g_gl.proj_stack[0]);
    g_gl.proj_top = 0;

    g_gl.ctx->modelview = g_gl.mv_stack[0];
    g_gl.ctx->projection = g_gl.proj_stack[0];
    hpfx3d_update_mvp(g_gl.ctx);

    g_gl.current_color = 0x00ffffff; /* Opaque white */
    g_gl.clear_color = 0x00000000;   /* Black */
    g_gl.clear_depth = 0x00ffffff;   /* Max depth 24-bit */
    g_gl.current_nx = 0.0f;
    g_gl.current_ny = 0.0f;
    g_gl.current_nz = 1.0f;

    /* OpenGL default: Face culling is disabled until glEnable(GL_CULL_FACE) */
    hpfx3d_set_cull_mode(g_gl.ctx, HPFX_CULL_NONE);

    return true;
}

void hpfx_gl_shutdown(void)
{
    if (g_gl.ctx) {
        hpfx3d_close(g_gl.ctx);
        g_gl.ctx = NULL;
    }
    for (int i = 0; i < MAX_TEXTURES; i++) {
        if (g_gl.textures[i]) {
            hpfx3d_destroy_texture(g_gl.textures[i]);
            g_gl.textures[i] = NULL;
        }
    }
}

hpfx3d_context* hpfx_gl_get_context(void)
{
    return g_gl.ctx;
}

/* =========================================================================
 * Viewport and Scissor
 * ========================================================================= */

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    (void)x; (void)y;
    if (!g_gl.ctx) return;
    if (width > 0 && height > 0) {
        g_gl.ctx->width = (uint32_t)width;
        g_gl.ctx->height = (uint32_t)height;
    }
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (!g_gl.ctx) return;
    hpfx3d_set_scissor(g_gl.ctx, g_gl.ctx->scissor.enabled, x, y, width, height);
}

/* =========================================================================
 * Matrix Stack Operations
 * ========================================================================= */

void glMatrixMode(GLenum mode)
{
    g_gl.matrix_mode = mode;
}

static hpfx3d_mat4* current_matrix(void)
{
    if (g_gl.matrix_mode == GL_PROJECTION) {
        return &g_gl.proj_stack[g_gl.proj_top];
    }
    return &g_gl.mv_stack[g_gl.mv_top];
}

static void sync_matrices_to_context(void)
{
    if (!g_gl.ctx) return;
    g_gl.ctx->modelview = g_gl.mv_stack[g_gl.mv_top];
    g_gl.ctx->projection = g_gl.proj_stack[g_gl.proj_top];
    hpfx3d_update_mvp(g_gl.ctx);
}

void glPushMatrix(void)
{
    if (g_gl.matrix_mode == GL_PROJECTION) {
        if (g_gl.proj_top < MATRIX_STACK_DEPTH_PROJ - 1) {
            g_gl.proj_stack[g_gl.proj_top + 1] = g_gl.proj_stack[g_gl.proj_top];
            g_gl.proj_top++;
        }
    } else {
        if (g_gl.mv_top < MATRIX_STACK_DEPTH_MV - 1) {
            g_gl.mv_stack[g_gl.mv_top + 1] = g_gl.mv_stack[g_gl.mv_top];
            g_gl.mv_top++;
        }
    }
}

void glPopMatrix(void)
{
    if (g_gl.matrix_mode == GL_PROJECTION) {
        if (g_gl.proj_top > 0) g_gl.proj_top--;
    } else {
        if (g_gl.mv_top > 0) g_gl.mv_top--;
    }
    sync_matrices_to_context();
}

void glLoadIdentity(void)
{
    hpfx3d_mat4_identity(current_matrix());
    sync_matrices_to_context();
}

void glLoadMatrixf(const GLfloat *m)
{
    if (!m) return;
    memcpy(current_matrix()->m, m, 16 * sizeof(float));
    sync_matrices_to_context();
}

void glMultMatrixf(const GLfloat *m)
{
    if (!m) return;
    hpfx3d_mat4 mat;
    memcpy(mat.m, m, 16 * sizeof(float));
    hpfx3d_mat4 *cur = current_matrix();
    hpfx3d_mat4 res;
    hpfx3d_mat4_multiply(&res, cur, &mat);
    *cur = res;
    sync_matrices_to_context();
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    hpfx3d_mat4_translate(current_matrix(), x, y, z);
    sync_matrices_to_context();
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    float rad = angle * (3.14159265f / 180.0f);
    hpfx3d_mat4 *cur = current_matrix();

    /* Normalize rotation axis */
    float len = sqrtf(x*x + y*y + z*z);
    if (len > 1e-6f) {
        x /= len; y /= len; z /= len;
    }

    if (fabsf(x - 1.0f) < 1e-4f && fabsf(y) < 1e-4f && fabsf(z) < 1e-4f) {
        hpfx3d_mat4_rotate_x(cur, rad);
    } else if (fabsf(y - 1.0f) < 1e-4f && fabsf(x) < 1e-4f && fabsf(z) < 1e-4f) {
        hpfx3d_mat4_rotate_y(cur, rad);
    } else if (fabsf(z - 1.0f) < 1e-4f && fabsf(x) < 1e-4f && fabsf(z) < 1e-4f) {
        hpfx3d_mat4_rotate_z(cur, rad);
    } else {
        /* Arbitrary axis rotation matrix */
        float c = cosf(rad);
        float s = sinf(rad);
        float t = 1.0f - c;
        hpfx3d_mat4 rot;
        rot.m[0] = t*x*x + c;    rot.m[1] = t*x*y - s*z;  rot.m[2] = t*x*z + s*y;  rot.m[3] = 0.0f;
        rot.m[4] = t*x*y + s*z;  rot.m[5] = t*y*y + c;    rot.m[6] = t*y*z - s*x;  rot.m[7] = 0.0f;
        rot.m[8] = t*x*z - s*y;  rot.m[9] = t*y*z + s*x;  rot.m[10]= t*z*z + c;    rot.m[11]= 0.0f;
        rot.m[12]= 0.0f;         rot.m[13]= 0.0f;         rot.m[14]= 0.0f;         rot.m[15]= 1.0f;
        hpfx3d_mat4 res;
        hpfx3d_mat4_multiply(&res, cur, &rot);
        *cur = res;
    }
    sync_matrices_to_context();
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    hpfx3d_mat4_scale(current_matrix(), x, y, z);
    sync_matrices_to_context();
}

void glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val)
{
    hpfx3d_mat4_ortho(current_matrix(), left, right, bottom, top, near_val, far_val);
    sync_matrices_to_context();
}

void glFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val)
{
    hpfx3d_mat4 m;
    memset(&m, 0, sizeof(m));
    m.m[0] = (2.0f * near_val) / (right - left);
    m.m[2] = (right + left) / (right - left);
    m.m[5] = (2.0f * near_val) / (top - bottom);
    m.m[6] = (top + bottom) / (top - bottom);
    m.m[10] = -(far_val + near_val) / (far_val - near_val);
    m.m[11] = -(2.0f * far_val * near_val) / (far_val - near_val);
    m.m[14] = -1.0f;

    hpfx3d_mat4 *cur = current_matrix();
    hpfx3d_mat4 res;
    hpfx3d_mat4_multiply(&res, cur, &m);
    *cur = res;
    sync_matrices_to_context();
}

/* =========================================================================
 * Capability Control
 * ========================================================================= */

void glEnable(GLenum cap)
{
    if (!g_gl.ctx) return;
    switch (cap) {
        case GL_DEPTH_TEST:
            hpfx3d_set_depth_test(g_gl.ctx, true, g_gl.ctx->state.depth_func);
            break;
        case GL_CULL_FACE:
            hpfx3d_set_cull_mode(g_gl.ctx, HPFX_CULL_BACK);
            break;
        case GL_BLEND:
            hpfx3d_set_blend(g_gl.ctx, true);
            break;
        case GL_SCISSOR_TEST:
            g_gl.ctx->scissor.enabled = true;
            break;
        case GL_FOG:
            hpfx3d_set_fog(g_gl.ctx, true, 0x00808080, 2.0f, 20.0f);
            break;
        default: break;
    }
}

void glDisable(GLenum cap)
{
    if (!g_gl.ctx) return;
    switch (cap) {
        case GL_DEPTH_TEST:
            hpfx3d_set_depth_test(g_gl.ctx, false, g_gl.ctx->state.depth_func);
            break;
        case GL_CULL_FACE:
            hpfx3d_set_cull_mode(g_gl.ctx, HPFX_CULL_NONE);
            break;
        case GL_BLEND:
            hpfx3d_set_blend(g_gl.ctx, false);
            break;
        case GL_SCISSOR_TEST:
            g_gl.ctx->scissor.enabled = false;
            break;
        case GL_FOG:
            g_gl.ctx->fog.enabled = false;
            break;
        default: break;
    }
}

void glDepthFunc(GLenum func)
{
    if (!g_gl.ctx) return;
    uint32_t f = HPFX_DEPTH_LESS;
    switch (func) {
        case GL_NEVER:    f = HPFX_DEPTH_NEVER; break;
        case GL_LESS:     f = HPFX_DEPTH_LESS; break;
        case GL_EQUAL:    f = HPFX_DEPTH_EQUAL; break;
        case GL_LEQUAL:   f = HPFX_DEPTH_LEQUAL; break;
        case GL_GREATER:  f = HPFX_DEPTH_GREATER; break;
        case GL_NOTEQUAL: f = HPFX_DEPTH_NOTEQUAL; break;
        case GL_GEQUAL:   f = HPFX_DEPTH_GEQUAL; break;
        case GL_ALWAYS:   f = HPFX_DEPTH_ALWAYS; break;
    }
    hpfx3d_set_depth_test(g_gl.ctx, g_gl.ctx->state.depth_enable != 0, f);
}

void glDepthMask(GLboolean flag)
{
    if (!g_gl.ctx) return;
    g_gl.ctx->state.depth_mask = (flag != 0) ? 1 : 0;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    (void)sfactor; (void)dfactor;
    if (!g_gl.ctx) return;
    hpfx3d_set_blend(g_gl.ctx, true);
}

void glCullFace(GLenum mode)
{
    if (!g_gl.ctx) return;
    if (mode == GL_FRONT) hpfx3d_set_cull_mode(g_gl.ctx, HPFX_CULL_FRONT);
    else if (mode == GL_BACK) hpfx3d_set_cull_mode(g_gl.ctx, HPFX_CULL_BACK);
    else hpfx3d_set_cull_mode(g_gl.ctx, HPFX_CULL_NONE);
}

void glFrontFace(GLenum mode)
{
    (void)mode; /* HP FP14 geometry engine defaults to GL_CCW */
}

void glShadeModel(GLenum mode)
{
    if (!g_gl.ctx) return;
    hpfx3d_set_shade_model(g_gl.ctx, (mode == GL_SMOOTH) ? 1 : 0);
}

/* =========================================================================
 * Clear & Buffer Operations
 * ========================================================================= */

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    (void)alpha;
    uint32_t r = (uint32_t)(red * 255.0f) & 0xff;
    uint32_t g = (uint32_t)(green * 255.0f) & 0xff;
    uint32_t b = (uint32_t)(blue * 255.0f) & 0xff;
    g_gl.clear_color = (r << 16) | (g << 8) | b;
}

void glClearDepth(GLclampf depth)
{
    g_gl.clear_depth = (uint32_t)(depth * 16777215.0f) & 0x00ffffff;
}

void glClear(GLbitfield mask)
{
    if (!g_gl.ctx) return;
    uint32_t c = (mask & GL_COLOR_BUFFER_BIT) ? g_gl.clear_color : 0;
    uint32_t z = (mask & GL_DEPTH_BUFFER_BIT) ? g_gl.clear_depth : 0x00ffffff;
    hpfx3d_clear(g_gl.ctx, c, z);
}

void glFlush(void)
{
    if (g_gl.ctx) hpfx3d_flush(g_gl.ctx);
}

void glFinish(void)
{
    if (g_gl.ctx) hpfx3d_sync(g_gl.ctx);
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, GLvoid *pixels)
{
    (void)format; (void)type;
    if (!g_gl.ctx || !g_gl.ctx->fb_mem || !pixels) return;

    uint32_t *fb = (uint32_t*)g_gl.ctx->fb_mem;
    uint32_t *dst = (uint32_t*)pixels;
    for (int row = 0; row < height; row++) {
        int src_y = y + row;
        if (src_y < 0 || src_y >= (int)g_gl.ctx->height) continue;
        for (int col = 0; col < width; col++) {
            int src_x = x + col;
            if (src_x < 0 || src_x >= (int)g_gl.ctx->width) continue;
            dst[row * width + col] = fb[src_y * g_gl.ctx->width + src_x];
        }
    }
}

/* =========================================================================
 * Immediate Mode Geometry
 * ========================================================================= */

void glBegin(GLenum mode)
{
    g_gl.in_begin = true;
    g_gl.begin_mode = mode;
    g_gl.imm_count = 0;
}

static void emit_triangle(const hpfx3d_vertex *v0, const hpfx3d_vertex *v1, const hpfx3d_vertex *v2)
{
    hpfx3d_draw_triangle_smooth(g_gl.ctx, v0, v1, v2);
}

void glEnd(void)
{
    if (!g_gl.in_begin || !g_gl.ctx) return;
    g_gl.in_begin = false;

    int n = g_gl.imm_count;
    const hpfx3d_vertex *v = g_gl.imm_vertices;

    switch (g_gl.begin_mode) {
        case GL_LINES:
            for (int i = 0; i + 1 < n; i += 2) {
                hpfx3d_vertex va = v[i];
                hpfx3d_vertex vb = v[i+1];
                float dx = vb.pos.x - va.pos.x;
                float dz = vb.pos.z - va.pos.z;
                float len = sqrtf(dx * dx + dz * dz);
                float nx = 0.0f, nz = 0.0f;
                if (len > 0.0001f) {
                    nx = -dz / len * 0.02f;
                    nz =  dx / len * 0.02f;
                } else {
                    nx = 0.02f;
                }
                hpfx3d_vertex v0 = va, v1 = va, v2 = vb, v3 = vb;
                v0.pos.x -= nx; v0.pos.z -= nz;
                v1.pos.x += nx; v1.pos.z += nz;
                v2.pos.x += nx; v2.pos.z += nz;
                v3.pos.x -= nx; v3.pos.z -= nz;
                emit_triangle(&v0, &v1, &v2);
                emit_triangle(&v0, &v2, &v3);
            }
            break;
        case GL_TRIANGLES:
            for (int i = 0; i + 2 < n; i += 3) {
                emit_triangle(&v[i], &v[i+1], &v[i+2]);
            }
            break;
        case GL_QUADS:
            for (int i = 0; i + 3 < n; i += 4) {
                emit_triangle(&v[i], &v[i+1], &v[i+2]);
                emit_triangle(&v[i], &v[i+2], &v[i+3]);
            }
            break;
        case GL_QUAD_STRIP:
            for (int i = 0; i + 3 < n; i += 2) {
                emit_triangle(&v[i], &v[i+1], &v[i+3]);
                emit_triangle(&v[i], &v[i+3], &v[i+2]);
            }
            break;
        case GL_TRIANGLE_FAN:
        case GL_POLYGON:
            for (int i = 1; i + 1 < n; i++) {
                emit_triangle(&v[0], &v[i], &v[i+1]);
            }
            break;
        case GL_TRIANGLE_STRIP:
            for (int i = 0; i + 2 < n; i++) {
                if (i & 1) emit_triangle(&v[i+1], &v[i], &v[i+2]);
                else       emit_triangle(&v[i], &v[i+1], &v[i+2]);
            }
            break;
    }

    g_gl.imm_count = 0;
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (!g_gl.in_begin || g_gl.imm_count >= MAX_IMMEDIATE_VERTICES) return;

    hpfx3d_vertex *vert = &g_gl.imm_vertices[g_gl.imm_count++];
    vert->pos.x = x;
    vert->pos.y = y;
    vert->pos.z = z;
    vert->color = g_gl.current_color;
    vert->u = g_gl.current_u;
    vert->v = g_gl.current_v;
}

void glVertex3fv(const GLfloat *v)
{
    if (v) glVertex3f(v[0], v[1], v[2]);
}

void glVertex2f(GLfloat x, GLfloat y)
{
    glVertex3f(x, y, 0.0f);
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    uint32_t ir = (uint32_t)(r * 255.0f) & 0xff;
    uint32_t ig = (uint32_t)(g * 255.0f) & 0xff;
    uint32_t ib = (uint32_t)(b * 255.0f) & 0xff;
    uint32_t ia = (uint32_t)(a * 255.0f) & 0xff;
    g_gl.current_color = (ia << 24) | (ir << 16) | (ig << 8) | ib;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b)
{
    glColor4f(r, g, b, 1.0f);
}

void glColor3fv(const GLfloat *v)
{
    if (v) glColor3f(v[0], v[1], v[2]);
}

void glColor4fv(const GLfloat *v)
{
    if (v) glColor4f(v[0], v[1], v[2], v[3]);
}

void glColor3ub(uint8_t r, uint8_t g, uint8_t b)
{
    g_gl.current_color = (0xff000000) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void glColor4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    g_gl.current_color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    g_gl.current_nx = nx;
    g_gl.current_ny = ny;
    g_gl.current_nz = nz;
}

void glNormal3fv(const GLfloat *v)
{
    if (v) glNormal3f(v[0], v[1], v[2]);
}

void glTexCoord2f(GLfloat s, GLfloat t)
{
    g_gl.current_u = s;
    g_gl.current_v = t;
}

void glTexCoord2fv(const GLfloat *v)
{
    if (v) glTexCoord2f(v[0], v[1]);
}

/* =========================================================================
 * Texturing Operations
 * ========================================================================= */

void glGenTextures(GLsizei n, GLuint *textures)
{
    if (!textures) return;
    int allocated = 0;
    for (int i = 1; i < MAX_TEXTURES && allocated < n; i++) {
        if (!g_gl.textures[i]) {
            textures[allocated++] = (GLuint)i;
        }
    }
}

void glBindTexture(GLenum target, GLuint texture)
{
    (void)target;
    if (texture < MAX_TEXTURES) {
        g_gl.current_texture_id = texture;
        if (g_gl.ctx && g_gl.textures[texture]) {
            hpfx3d_bind_texture(g_gl.ctx, g_gl.textures[texture]);
        }
    }
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    (void)target; (void)pname; (void)param;
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                 GLsizei width, GLsizei height, GLint border,
                 GLenum format, GLenum type, const GLvoid *pixels)
{
    (void)target; (void)level; (void)internalformat; (void)border; (void)format; (void)type;
    GLuint id = g_gl.current_texture_id;
    if (id >= MAX_TEXTURES || !pixels) return;

    if (g_gl.textures[id]) {
        hpfx3d_destroy_texture(g_gl.textures[id]);
        g_gl.textures[id] = NULL;
    }

    g_gl.textures[id] = hpfx3d_create_texture((uint32_t)width, (uint32_t)height, (const uint32_t*)pixels);
    if (g_gl.ctx && g_gl.textures[id]) {
        hpfx3d_bind_texture(g_gl.ctx, g_gl.textures[id]);
    }
}
