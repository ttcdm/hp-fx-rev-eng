/*
 * HP Visualize FX5 & FX10 Hardware Gears Demo
 *
 * Implements the classic 3D Gears benchmark running on top of hpfx_gl.h
 * rendering animated interlocking gears via HP Visualize FX hardware rasterization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_gl.h"

#define PI 3.14159265358979323846f

/* Gear generation function */
static void draw_gear(float inner_radius, float outer_radius, float width,
                      int teeth, float tooth_depth, uint32_t color)
{
    float r0 = inner_radius;
    float r1 = outer_radius - tooth_depth / 2.0f;
    float r2 = outer_radius + tooth_depth / 2.0f;
    float da = 2.0f * PI / teeth / 4.0f;

    uint8_t r = (color >> 16) & 0xff;
    uint8_t g = (color >> 8) & 0xff;
    uint8_t b = color & 0xff;

    /* Draw front face */
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= teeth; i++) {
        float angle = i * 2.0f * PI / teeth;
        glColor3ub(r, g, b);
        glVertex3f(r0 * cosf(angle), r0 * sinf(angle), width * 0.5f);
        glVertex3f(r1 * cosf(angle), r1 * sinf(angle), width * 0.5f);
        if (i < teeth) {
            glVertex3f(r0 * cosf(angle), r0 * sinf(angle), width * 0.5f);
            glVertex3f(r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), width * 0.5f);
        }
    }
    glEnd();

    /* Draw front tooth tips */
    glBegin(GL_QUADS);
    for (int i = 0; i < teeth; i++) {
        float angle = i * 2.0f * PI / teeth;
        glColor3ub((uint8_t)(r * 0.9f), (uint8_t)(g * 0.9f), (uint8_t)(b * 0.9f));
        glVertex3f(r1 * cosf(angle), r1 * sinf(angle), width * 0.5f);
        glVertex3f(r2 * cosf(angle + da), r2 * sinf(angle + da), width * 0.5f);
        glVertex3f(r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), width * 0.5f);
        glVertex3f(r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), width * 0.5f);
    }
    glEnd();

    /* Draw back face */
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= teeth; i++) {
        float angle = i * 2.0f * PI / teeth;
        glColor3ub((uint8_t)(r * 0.7f), (uint8_t)(g * 0.7f), (uint8_t)(b * 0.7f));
        glVertex3f(r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f);
        glVertex3f(r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f);
        if (i < teeth) {
            glVertex3f(r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), -width * 0.5f);
            glVertex3f(r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f);
        }
    }
    glEnd();

    /* Draw back tooth tips */
    glBegin(GL_QUADS);
    for (int i = 0; i < teeth; i++) {
        float angle = i * 2.0f * PI / teeth;
        glColor3ub((uint8_t)(r * 0.6f), (uint8_t)(g * 0.6f), (uint8_t)(b * 0.6f));
        glVertex3f(r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), -width * 0.5f);
        glVertex3f(r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), -width * 0.5f);
        glVertex3f(r2 * cosf(angle + da), r2 * sinf(angle + da), -width * 0.5f);
        glVertex3f(r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f);
    }
    glEnd();

    /* Draw outer tooth cylinders */
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i < teeth; i++) {
        float angle = i * 2.0f * PI / teeth;
        glColor3ub((uint8_t)(r * 0.8f), (uint8_t)(g * 0.8f), (uint8_t)(b * 0.8f));
        glVertex3f(r1 * cosf(angle), r1 * sinf(angle), width * 0.5f);
        glVertex3f(r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f);
        glVertex3f(r2 * cosf(angle + da), r2 * sinf(angle + da), width * 0.5f);
        glVertex3f(r2 * cosf(angle + da), r2 * sinf(angle + da), -width * 0.5f);
        glVertex3f(r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), width * 0.5f);
        glVertex3f(r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da), -width * 0.5f);
        glVertex3f(r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), width * 0.5f);
        glVertex3f(r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da), -width * 0.5f);
    }
    glEnd();

    /* Draw inside cylinder */
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= teeth; i++) {
        float angle = i * 2.0f * PI / teeth;
        glColor3ub((uint8_t)(r * 0.5f), (uint8_t)(g * 0.5f), (uint8_t)(b * 0.5f));
        glVertex3f(r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f);
        glVertex3f(r0 * cosf(angle), r0 * sinf(angle), width * 0.5f);
    }
    glEnd();
}

static void save_ppm(const char *filename, int w, int h)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", w, h);
    uint32_t *pixels = (uint32_t*)malloc(w * h * sizeof(uint32_t));
    if (pixels) {
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint32_t p = pixels[y * w + x];
                uint8_t r = (p >> 16) & 0xff;
                uint8_t g = (p >> 8) & 0xff;
                uint8_t b = p & 0xff;
                fputc(r, f); fputc(g, f); fputc(b, f);
            }
        }
        free(pixels);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *fb_dev = NULL;
    int frames = 60;
    const char *out_ppm = "hpfx_gears_render.ppm";

    int width = 1920;
    int height = 1080;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d") && i + 1 < argc) fb_dev = argv[++i];
        else if (!strcmp(argv[i], "-f") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_ppm = argv[++i];
        else if (!strcmp(argv[i], "-w") && i + 1 < argc) width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) height = atoi(argv[++i]);
    }

    printf("===================================================================\n");
    printf(" HP Visualize FX5/FX10 OpenGL Gears Demo (libhpfx_gl)\n");
    printf("===================================================================\n");
    printf("Initializing OpenGL backend on %s (%dx%d)...\n",
           fb_dev ? fb_dev : "Software Fallback Memory", width, height);

    if (!hpfx_gl_init(fb_dev, width, height)) {
        fprintf(stderr, "Failed to initialize hpfx_gl backend\n");
        return 1;
    }

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0f, 1.0f, -(float)height / (float)width, (float)height / (float)width, 2.0f, 60.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.1f, -0.5f, -9.5f);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.10f, 0.15f, 1.0f);

    float angle = 0.0f;
    for (int f = 0; f < frames; f++) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushMatrix();
        glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(30.0f, 0.0f, 1.0f, 0.0f);

        /* Gear 1: Red */
        glPushMatrix();
        glTranslatef(-3.0f, -2.0f, 0.0f);
        glRotatef(angle, 0.0f, 0.0f, 1.0f);
        draw_gear(1.0f, 4.0f, 1.0f, 20, 0.7f, 0x00e74c3c);
        glPopMatrix();

        /* Gear 2: Green */
        glPushMatrix();
        glTranslatef(3.1f, -2.0f, 0.0f);
        glRotatef(-2.0f * angle - 9.0f, 0.0f, 0.0f, 1.0f);
        draw_gear(0.5f, 2.0f, 2.0f, 10, 0.7f, 0x002ecc71);
        glPopMatrix();

        /* Gear 3: Blue */
        glPushMatrix();
        glTranslatef(-3.1f, 4.2f, 0.0f);
        glRotatef(-2.0f * angle - 25.0f, 0.0f, 0.0f, 1.0f);
        draw_gear(1.3f, 2.0f, 0.5f, 10, 0.7f, 0x003498db);
        glPopMatrix();

        glPopMatrix();

        glFlush();
        angle += 2.0f;
    }

    if (out_ppm) {
        printf("Writing final frame screenshot to %s...\n", out_ppm);
        save_ppm(out_ppm, width, height);
    }

    hpfx_gl_shutdown();
    printf("HP Visualize FX OpenGL Gears Demo finished successfully (%d frames).\n", frames);
    return 0;
}
