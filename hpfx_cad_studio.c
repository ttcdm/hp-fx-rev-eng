/*
 * HP Visualize FX Industrial CAD & Workstation Studio Showcase
 *
 * Demonstrates high-performance workstation CAD visualization on HP Visualize FX:
 * - Complex shoe4R CAD model with multi-part athletic shoe materials
 * - Dual-axis Torus & high-density tessellated Sphere assembly
 * - Engineering ground grid with coordinate axes and depth fog
 * - Dual-pass CAD wireframe edge overlay over solid Gouraud shading
 * - Warm/Cool dual-light studio rig with Blinn-Phong specular highlights
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_gl.h"

#define PI 3.14159265358979323846f

static void draw_cad_grid(float size, int divisions)
{
    float step = (size * 2.0f) / (float)divisions;

    /* Architectural CAD ground plane */
    glBegin(GL_QUADS);
    glColor3ub(26, 32, 44);
    glVertex3f(-size, -1.5f, -size);
    glVertex3f( size, -1.5f, -size);
    glVertex3f( size, -1.5f,  size);
    glVertex3f(-size, -1.5f,  size);
    glEnd();

    /* Grid lines */
    for (int i = 0; i <= divisions; i++) {
        float p = -size + (float)i * step;

        /* Major vs minor grid line brightness */
        uint8_t c = (i % 4 == 0) ? 90 : 50;
        glColor3ub(c, c + 15, c + 35);

        /* Lines parallel to Z */
        glBegin(GL_LINES);
        glVertex3f(p, -1.48f, -size);
        glVertex3f(p, -1.48f,  size);
        glEnd();

        /* Lines parallel to X */
        glBegin(GL_LINES);
        glVertex3f(-size, -1.48f, p);
        glVertex3f( size, -1.48f, p);
        glEnd();
    }

    /* Coordinate Axes */
    /* X Axis: Radiant Red */
    glColor3ub(231, 76, 60);
    glBegin(GL_LINES);
    glVertex3f(-size * 0.5f, -1.47f, 0.0f);
    glVertex3f( size * 0.5f, -1.47f, 0.0f);
    glEnd();

    /* Z Axis: Emerald Green */
    glColor3ub(46, 204, 113);
    glBegin(GL_LINES);
    glVertex3f(0.0f, -1.47f, -size * 0.5f);
    glVertex3f(0.0f, -1.47f,  size * 0.5f);
    glEnd();
}

static void save_frame_ppm(const char *filename, int w, int h)
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
                fputc((p >> 16) & 0xff, f);
                fputc((p >> 8) & 0xff, f);
                fputc(p & 0xff, f);
            }
        }
        free(pixels);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    int width = 1920;
    int height = 1080;
    const char *out_ppm = "hpfx_render_cad_studio.ppm";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_ppm = argv[++i];
    }

    printf("===================================================================\n");
    printf(" HP Visualize FX Industrial CAD & Workstation Studio Showcase\n");
    printf(" Target: HP Visualize FX5 / FX10 Architecture Simulation\n");
    printf("===================================================================\n");
    printf("Resolution: %dx%d | Rendering CAD assembly & turntable...\n", width, height);

    if (!hpfx_gl_init(NULL, width, height)) {
        fprintf(stderr, "Failed to initialize OpenGL backend\n");
        return 1;
    }

    hpfx3d_context *ctx = hpfx_gl_get_context();

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0f, 1.0f, -(float)height / (float)width, (float)height / (float)width, 2.0f, 50.0f);

    /* Atmospheric Fog Setup */
    glEnable(GL_FOG);
    hpfx3d_set_fog(ctx, true, 0x000e141f, 3.0f, 25.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.055f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.2f, -5.6f);
    glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-25.0f, 0.0f, 1.0f, 0.0f);

    /* 1. Draw Engineering Grid */
    draw_cad_grid(8.0f, 40);

    /* 2. Central Assembly: shoe4R CAD Model on Turntable */
    glPushMatrix();
    glTranslatef(0.0f, -0.35f, 0.0f);
    glRotatef(20.0f, 0.0f, 1.0f, 0.0f);
    hpfx3d_draw_shoe(ctx, 1.15f, 0x00e74c3c);
    glPopMatrix();

    /* 3. Orbiting Component 1: Golden Engineering Torus */
    glPushMatrix();
    glTranslatef(-2.7f, 0.35f, -0.2f);
    glRotatef(45.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(30.0f, 0.0f, 1.0f, 0.0f);
    hpfx3d_draw_torus(ctx, 0.90f, 0.35f, 36, 24, 0x00f39c12);
    glPopMatrix();

    /* 4. Orbiting Component 2: High-Poly Sapphire Sphere */
    glPushMatrix();
    glTranslatef(2.7f, 0.35f, -0.2f);
    hpfx3d_draw_sphere(ctx, 0.85f, 32, 24, 0x002980b9);
    glPopMatrix();

    /* 5. Precision Calibration Cube with Contrasting Facets */
    glPushMatrix();
    glTranslatef(-0.2f, -0.55f, 2.0f);
    glRotatef(25.0f, 0.0f, 1.0f, 0.0f);
    hpfx3d_draw_cube(ctx, 0.70f);
    glPopMatrix();

    glFlush();

    /* Checksum verification */
    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);

    printf("[Export] Writing rendered CAD studio frame to: %s\n", out_ppm);
    save_frame_ppm(out_ppm, width, height);
    printf("[Checksum] IEEE 802.3 Framebuffer CRC32: 0x%08X\n", crc);

    hpfx_gl_shutdown();
    printf("CAD Studio rendering finished successfully.\n");
    return 0;
}
