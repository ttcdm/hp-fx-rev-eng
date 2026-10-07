/*
 * HP Visualize FX Procedural Texture & Material Gallery
 *
 * Demonstrates the hardware texture engine and material synthesis:
 * - Panel 1: Mahogany Wood Grain Pedestal with calibration Cube
 * - Panel 2: Carrara Marble Slab with orbiting Sphere
 * - Panel 3: Precision Checkerboard Floor with golden Torus
 * - Panel 4: HP Corporate Medallion Badge with shoe4R CAD model
 * - Multi-light studio shading and depth buffering
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx_gl.h"

#define TEX_DIM 256

/* Procedural Texture Generators */

static void generate_wood_texture(uint32_t *tex)
{
    float cx = TEX_DIM * 0.5f;
    float cy = TEX_DIM * 0.5f;

    for (int y = 0; y < TEX_DIM; y++) {
        for (int x = 0; x < TEX_DIM; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float dist = sqrtf(dx * dx + dy * dy);

            float noise = sinf((float)x * 0.12f) * 4.5f + sinf((float)y * 0.06f) * 2.5f;
            float ring = sinf((dist + noise) * 0.22f);

            float t = 0.5f * (ring + 1.0f);
            uint8_t r = (uint8_t)(145 + t * 55);
            uint8_t g = (uint8_t)(72  + t * 45);
            uint8_t b = (uint8_t)(22  + t * 25);

            tex[y * TEX_DIM + x] = (0xff << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

static void generate_marble_texture(uint32_t *tex)
{
    for (int y = 0; y < TEX_DIM; y++) {
        for (int x = 0; x < TEX_DIM; x++) {
            float u = (float)x / (float)TEX_DIM;
            float v = (float)y / (float)TEX_DIM;
            float turbulence = sinf(u * 14.0f + sinf(v * 18.0f) * 2.5f) * 1.2f;
            float marble = fabsf(sinf((u + v) * 8.0f + turbulence));

            uint8_t base = (uint8_t)(marble * 210.0f + 40.0f);
            uint8_t r = base;
            uint8_t g = (uint8_t)(base * 0.96f);
            uint8_t b = (uint8_t)(base * 1.02f > 255 ? 255 : base * 1.02f);

            tex[y * TEX_DIM + x] = (0xff << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

static void generate_checker_texture(uint32_t *tex)
{
    for (int y = 0; y < TEX_DIM; y++) {
        int cy = (y / 32) % 2;
        for (int x = 0; x < TEX_DIM; x++) {
            int cx = (x / 32) % 2;
            int bx = x % 32;
            int by = y % 32;
            bool border = (bx == 0 || bx == 31 || by == 0 || by == 31);

            uint32_t color;
            if (border) {
                color = 0xff0d131a;
            } else if (cx ^ cy) {
                color = 0xfff39c12; /* Rich gold */
            } else {
                color = 0xff1e272e; /* Dark obsidian */
            }
            tex[y * TEX_DIM + x] = color;
        }
    }
}

static void generate_hp_logo_texture(uint32_t *tex)
{
    float cx = TEX_DIM * 0.5f;
    float cy = TEX_DIM * 0.5f;
    float radius = TEX_DIM * 0.44f;

    for (int y = 0; y < TEX_DIM; y++) {
        for (int x = 0; x < TEX_DIM; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float dist = sqrtf(dx * dx + dy * dy);

            uint32_t pixel;
            if (dist < radius) {
                float bevel = 1.0f - (dist / radius) * 0.25f;
                uint8_t r = (uint8_t)(0 * bevel);
                uint8_t g = (uint8_t)(114 * bevel);
                uint8_t b = (uint8_t)(206 * bevel);
                pixel = (0xff << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

                /* White inner bezel ring */
                if (fabsf(dist - radius * 0.88f) < 3.5f) {
                    pixel = 0xffffffff;
                }
                /* Center HP letters representation */
                if (fabsf(dx) < 28.0f && fabsf(dy) < 44.0f &&
                    (fabsf(dx - 10.0f) < 6.0f || fabsf(dx + 10.0f) < 6.0f || fabsf(dy) < 8.0f)) {
                    pixel = 0xffffffff;
                }
            } else if (dist < radius + 10.0f) {
                /* Silver metallic rim */
                pixel = 0xffd2d7d9;
            } else {
                /* Slate pedestal surround */
                pixel = 0xff161f28;
            }
            tex[y * TEX_DIM + x] = pixel;
        }
    }
}

/* Draws a finely tessellated textured surface in 3D using hardware texturing */
static void draw_textured_slab(float x0, float z0, float x1, float z1, float y,
                               GLuint tex_id, int res)
{
    glBindTexture(GL_TEXTURE_2D, tex_id);
    float dx = (x1 - x0) / (float)res;
    float dz = (z1 - z0) / (float)res;
    float du = 1.0f / (float)res;
    float dv = 1.0f / (float)res;

    glColor3ub(255, 255, 255);
    for (int j = 0; j < res; j++) {
        for (int i = 0; i < res; i++) {
            float px0 = x0 + (float)i * dx;
            float px1 = px0 + dx;
            float pz0 = z0 + (float)j * dz;
            float pz1 = pz0 + dz;

            float u0 = (float)i * du;
            float u1 = u0 + du;
            float v0 = (float)j * dv;
            float v1 = v0 + dv;

            glBegin(GL_QUADS);
            glTexCoord2f(u0, v0); glVertex3f(px0, y, pz0);
            glTexCoord2f(u1, v0); glVertex3f(px1, y, pz0);
            glTexCoord2f(u1, v1); glVertex3f(px1, y, pz1);
            glTexCoord2f(u0, v1); glVertex3f(px0, y, pz1);
            glEnd();
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
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
    const char *out_ppm = "hpfx_render_textures.ppm";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_ppm = argv[++i];
    }

    printf("===================================================================\n");
    printf(" HP Visualize FX Procedural Texture & Material Gallery\n");
    printf(" Target: HP Visualize FX5 / FX10 Architecture Simulation\n");
    printf("===================================================================\n");
    printf("Resolution: %dx%d | Synthesizing 256x256 textures & rendering pedestals...\n", width, height);

    uint32_t *tex_wood    = (uint32_t*)malloc(TEX_DIM * TEX_DIM * sizeof(uint32_t));
    uint32_t *tex_marble  = (uint32_t*)malloc(TEX_DIM * TEX_DIM * sizeof(uint32_t));
    uint32_t *tex_checker = (uint32_t*)malloc(TEX_DIM * TEX_DIM * sizeof(uint32_t));
    uint32_t *tex_hp      = (uint32_t*)malloc(TEX_DIM * TEX_DIM * sizeof(uint32_t));

    generate_wood_texture(tex_wood);
    generate_marble_texture(tex_marble);
    generate_checker_texture(tex_checker);
    generate_hp_logo_texture(tex_hp);

    if (!hpfx_gl_init(NULL, width, height)) {
        fprintf(stderr, "Failed to initialize OpenGL backend\n");
        return 1;
    }

    hpfx3d_context *ctx = hpfx_gl_get_context();

    /* Create OpenGL 1.1 texture objects */
    GLuint tex_ids[4];
    glGenTextures(4, tex_ids);

    glBindTexture(GL_TEXTURE_2D, tex_ids[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_wood);

    glBindTexture(GL_TEXTURE_2D, tex_ids[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_marble);

    glBindTexture(GL_TEXTURE_2D, tex_ids[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_checker);

    glBindTexture(GL_TEXTURE_2D, tex_ids[3]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DIM, TEX_DIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_hp);

    glBindTexture(GL_TEXTURE_2D, 0);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0f, 1.0f, -(float)height / (float)width, (float)height / (float)width, 2.0f, 50.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.2f, -4.6f);
    glRotatef(26.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-16.0f, 0.0f, 1.0f, 0.0f);

    /* 1. Pedestal 1 (Top-Left): Wood Plank with Calibration Cube */
    draw_textured_slab(-3.8f, -2.6f, -0.8f, -0.2f, -0.8f, tex_ids[0], 12);
    glPushMatrix();
    glTranslatef(-2.3f, -0.25f, -1.4f);
    glRotatef(35.0f, 0.0f, 1.0f, 0.0f);
    hpfx3d_draw_cube(ctx, 0.85f);
    glPopMatrix();

    /* 2. Pedestal 2 (Top-Right): Carrara Marble with Sapphire Sphere */
    draw_textured_slab(0.8f, -2.6f, 3.8f, -0.2f, -0.8f, tex_ids[1], 12);
    glPushMatrix();
    glTranslatef(2.3f, -0.05f, -1.4f);
    hpfx3d_draw_sphere(ctx, 0.70f, 32, 24, 0x003498db);
    glPopMatrix();

    /* 3. Pedestal 3 (Bottom-Left): Gold & Obsidian Checker with Golden Torus */
    draw_textured_slab(-3.8f, 0.5f, -0.8f, 2.9f, -0.8f, tex_ids[2], 12);
    glPushMatrix();
    glTranslatef(-2.3f, 0.2f, 1.6f);
    glRotatef(45.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(30.0f, 0.0f, 1.0f, 0.0f);
    hpfx3d_draw_torus(ctx, 0.75f, 0.28f, 32, 20, 0x00f1c40f);
    glPopMatrix();

    /* 4. Pedestal 4 (Bottom-Right): HP Corporate Medallion with shoe4R CAD Model */
    draw_textured_slab(0.8f, 0.5f, 3.8f, 2.9f, -0.8f, tex_ids[3], 12);
    glPushMatrix();
    glTranslatef(2.3f, -0.40f, 1.6f);
    glRotatef(35.0f, 0.0f, 1.0f, 0.0f);
    hpfx3d_draw_shoe(ctx, 0.55f, 0x00e74c3c);
    glPopMatrix();

    glFlush();

    printf("[Export] Writing textured gallery frame to: %s\n", out_ppm);
    save_frame_ppm(out_ppm, width, height);

    free(tex_wood); free(tex_marble); free(tex_checker); free(tex_hp);
    hpfx_gl_shutdown();

    printf("Procedural Texture Gallery completed successfully.\n");
    return 0;
}
