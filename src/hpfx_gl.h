/*
 * HP Visualize FX5 & FX10 OpenGL 1.1 / TinyGL Hardware Acceleration Backend
 *
 * Implements standard OpenGL 1.1 immediate-mode rendering and state management
 * wrapping the libhpfx3d hardware rasterization engine and /dev/fb0 IOCTL interface.
 */

#ifndef _HPFX_GL_H_
#define _HPFX_GL_H_

#include <stdint.h>
#include <stdbool.h>
#include "hpfx3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Standard OpenGL Datatypes */
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef int            GLint;
typedef unsigned int   GLuint;
typedef int            GLsizei;
typedef float          GLfloat;
typedef float          GLclampf;
typedef void           GLvoid;

/* Constants */
#define GL_FALSE                          0
#define GL_TRUE                           1

/* Primitive Types */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006
#define GL_QUADS                          0x0007
#define GL_QUAD_STRIP                     0x0008
#define GL_POLYGON                        0x0009

/* Matrix Modes */
#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701

/* Capabilities */
#define GL_CULL_FACE                      0x0B44
#define GL_DEPTH_TEST                     0x0B71
#define GL_BLEND                          0x0BE2
#define GL_SCISSOR_TEST                   0x0C11
#define GL_TEXTURE_2D                     0x0DE1
#define GL_FOG                            0x0B60
#define GL_LIGHTING                       0x0B50

/* Depth Functions */
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

/* Blending Factors */
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305

/* Culling & Front Face */
#define GL_CW                             0x0900
#define GL_CCW                            0x0901
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408

/* Shading */
#define GL_FLAT                           0x1D00
#define GL_SMOOTH                         0x1D01

/* Buffer Bits */
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_DEPTH_BUFFER_BIT               0x00000100

/* Texture Formats & Parameters */
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_REPEAT                         0x2901
#define GL_CLAMP                          0x2900

/* Backend Lifecycle & Context Management */
bool hpfx_gl_init(const char *fb_dev, int width, int height);
void hpfx_gl_shutdown(void);
hpfx3d_context* hpfx_gl_get_context(void);

/* Viewport and Scissor */
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);

/* Matrix Stack */
void glMatrixMode(GLenum mode);
void glPushMatrix(void);
void glPopMatrix(void);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *m);
void glMultMatrixf(const GLfloat *m);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val);
void glFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near_val, GLfloat far_val);

/* Capability Control */
void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glDepthFunc(GLenum func);
void glDepthMask(GLboolean flag);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glCullFace(GLenum mode);
void glFrontFace(GLenum mode);
void glShadeModel(GLenum mode);

/* Clear & Framebuffer */
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClearDepth(GLclampf depth);
void glClear(GLbitfield mask);
void glFlush(void);
void glFinish(void);
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, GLvoid *pixels);

/* Immediate Mode Geometry */
void glBegin(GLenum mode);
void glEnd(void);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3fv(const GLfloat *v);
void glVertex2f(GLfloat x, GLfloat y);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor3fv(const GLfloat *v);
void glColor4fv(const GLfloat *v);
void glColor3ub(uint8_t r, uint8_t g, uint8_t b);
void glColor4ub(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
void glNormal3fv(const GLfloat *v);
void glTexCoord2f(GLfloat s, GLfloat t);
void glTexCoord2fv(const GLfloat *v);

/* Texturing */
void glGenTextures(GLsizei n, GLuint *textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                 GLsizei width, GLsizei height, GLint border,
                 GLenum format, GLenum type, const GLvoid *pixels);

#ifdef __cplusplus
}
#endif

#endif /* _HPFX_GL_H_ */
