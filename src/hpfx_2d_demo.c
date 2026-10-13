/*
 * HP Visualize FX Hardware 2D BitBLT & ROP Compositing Engine Showcase
 * Scene: HP 2D Fractal Computing Studio (Mandelbrot, Julia & Deep-Zoom Explorer)
 *
 * Demonstrates the dedicated 128-bit 2D acceleration engine on HP "Lego" ASICs:
 * - High-speed solid area fills via HPFX_REG_COORD_START & TRIGGER (0xCC SRCCOPY)
 * - Hardware Raster Operations: Invert/XOR Rubber-Banding (0x66) for Seahorse Valley zoom
 * - Screen-to-screen BitBLT area copy (hpfx3d_copy_area) for multi-panel UI badges & LUTs
 * - Native 5x7 ASCII bitmap font rendering in hardware silicon (scale 1x, 2x)
 * - 32-bit smooth continuous-potential fractional iteration coloring (no banding)
 * - Multi-viewport compositing:
 *     1. Full Mandelbrot Set on complex plane (1244x970) with reticle crosshairs
 *     2. Associated Connected Julia Set (c = -0.7269 + 0.1889i) (598x420)
 *     3. Seahorse Valley 1,250x Deep-Zoom Spiral Structure (598x320)
 *     4. Silicon 2D Engine Performance & MMIO Telemetry Card
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hpfx3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 5x7 ASCII bitmap font (ASCII 32 to 126, 5 columns per character, 1 byte per column) */
static const uint8_t font5x7[95][5] = {
    /*  32 ' ' */ {0x00, 0x00, 0x00, 0x00, 0x00},
    /*  33 '!' */ {0x00, 0x00, 0x5f, 0x00, 0x00},
    /*  34 '"' */ {0x00, 0x07, 0x00, 0x07, 0x00},
    /*  35 '#' */ {0x14, 0x7f, 0x14, 0x7f, 0x14},
    /*  36 '$' */ {0x24, 0x2a, 0x7f, 0x2a, 0x12},
    /*  37 '%' */ {0x23, 0x13, 0x08, 0x64, 0x62},
    /*  38 '&' */ {0x36, 0x49, 0x55, 0x22, 0x50},
    /*  39 '\'' */ {0x00, 0x05, 0x03, 0x00, 0x00},
    /*  40 '(' */ {0x00, 0x1c, 0x22, 0x41, 0x00},
    /*  41 ')' */ {0x00, 0x41, 0x22, 0x1c, 0x00},
    /*  42 '*' */ {0x08, 0x2a, 0x1c, 0x2a, 0x08},
    /*  43 '+' */ {0x08, 0x08, 0x3e, 0x08, 0x08},
    /*  44 ',' */ {0x00, 0x50, 0x30, 0x00, 0x00},
    /*  45 '-' */ {0x08, 0x08, 0x08, 0x08, 0x08},
    /*  46 '.' */ {0x00, 0x60, 0x60, 0x00, 0x00},
    /*  47 '/' */ {0x20, 0x10, 0x08, 0x04, 0x02},
    /*  48 '0' */ {0x3e, 0x51, 0x49, 0x45, 0x3e},
    /*  49 '1' */ {0x00, 0x42, 0x7f, 0x40, 0x00},
    /*  50 '2' */ {0x42, 0x61, 0x51, 0x49, 0x46},
    /*  51 '3' */ {0x21, 0x41, 0x45, 0x4b, 0x31},
    /*  52 '4' */ {0x18, 0x14, 0x12, 0x7f, 0x10},
    /*  53 '5' */ {0x27, 0x45, 0x45, 0x45, 0x39},
    /*  54 '6' */ {0x3c, 0x4a, 0x49, 0x49, 0x30},
    /*  55 '7' */ {0x01, 0x71, 0x09, 0x05, 0x03},
    /*  56 '8' */ {0x36, 0x49, 0x49, 0x49, 0x36},
    /*  57 '9' */ {0x06, 0x49, 0x49, 0x29, 0x1e},
    /*  58 ':' */ {0x00, 0x36, 0x36, 0x00, 0x00},
    /*  59 ';' */ {0x00, 0x56, 0x36, 0x00, 0x00},
    /*  60 '<' */ {0x00, 0x08, 0x14, 0x22, 0x41},
    /*  61 '=' */ {0x14, 0x14, 0x14, 0x14, 0x14},
    /*  62 '>' */ {0x41, 0x22, 0x14, 0x08, 0x00},
    /*  63 '?' */ {0x02, 0x01, 0x51, 0x09, 0x06},
    /*  64 '@' */ {0x32, 0x49, 0x79, 0x41, 0x3e},
    /*  65 'A' */ {0x7e, 0x11, 0x11, 0x11, 0x7e},
    /*  66 'B' */ {0x7f, 0x49, 0x49, 0x49, 0x36},
    /*  67 'C' */ {0x3e, 0x41, 0x41, 0x41, 0x22},
    /*  68 'D' */ {0x7f, 0x41, 0x41, 0x22, 0x1c},
    /*  69 'E' */ {0x7f, 0x49, 0x49, 0x49, 0x41},
    /*  70 'F' */ {0x7f, 0x09, 0x09, 0x01, 0x01},
    /*  71 'G' */ {0x3e, 0x41, 0x41, 0x51, 0x32},
    /*  72 'H' */ {0x7f, 0x08, 0x08, 0x08, 0x7f},
    /*  73 'I' */ {0x00, 0x41, 0x7f, 0x41, 0x00},
    /*  74 'J' */ {0x20, 0x40, 0x41, 0x3f, 0x01},
    /*  75 'K' */ {0x7f, 0x08, 0x14, 0x22, 0x41},
    /*  76 'L' */ {0x7f, 0x40, 0x40, 0x40, 0x40},
    /*  77 'M' */ {0x7f, 0x02, 0x04, 0x02, 0x7f},
    /*  78 'N' */ {0x7f, 0x04, 0x08, 0x10, 0x7f},
    /*  79 'O' */ {0x3e, 0x41, 0x41, 0x41, 0x3e},
    /*  80 'P' */ {0x7f, 0x09, 0x09, 0x09, 0x06},
    /*  81 'Q' */ {0x3e, 0x41, 0x51, 0x21, 0x5e},
    /*  82 'R' */ {0x7f, 0x09, 0x19, 0x29, 0x46},
    /*  83 'S' */ {0x46, 0x49, 0x49, 0x49, 0x31},
    /*  84 'T' */ {0x01, 0x01, 0x7f, 0x01, 0x01},
    /*  85 'U' */ {0x3f, 0x40, 0x40, 0x40, 0x3f},
    /*  86 'V' */ {0x1f, 0x20, 0x40, 0x20, 0x1f},
    /*  87 'W' */ {0x7f, 0x20, 0x18, 0x20, 0x7f},
    /*  88 'X' */ {0x63, 0x14, 0x08, 0x14, 0x63},
    /*  89 'Y' */ {0x03, 0x04, 0x78, 0x04, 0x03},
    /*  90 'Z' */ {0x61, 0x51, 0x49, 0x45, 0x43},
    /*  91 '[' */ {0x00, 0x7f, 0x41, 0x41, 0x00},
    /*  92 '\\' */ {0x02, 0x04, 0x08, 0x10, 0x20},
    /*  93 ']' */ {0x00, 0x41, 0x41, 0x7f, 0x00},
    /*  94 '^' */ {0x04, 0x02, 0x01, 0x02, 0x04},
    /*  95 '_' */ {0x40, 0x40, 0x40, 0x40, 0x40},
    /*  96 '`' */ {0x00, 0x01, 0x02, 0x04, 0x00},
    /*  97 'a' */ {0x20, 0x54, 0x54, 0x54, 0x78},
    /*  98 'b' */ {0x7f, 0x48, 0x44, 0x44, 0x38},
    /*  99 'c' */ {0x38, 0x44, 0x44, 0x44, 0x20},
    /* 100 'd' */ {0x38, 0x44, 0x44, 0x48, 0x7f},
    /* 101 'e' */ {0x38, 0x54, 0x54, 0x54, 0x18},
    /* 102 'f' */ {0x08, 0x7e, 0x09, 0x01, 0x02},
    /* 103 'g' */ {0x08, 0x14, 0x54, 0x54, 0x3c},
    /* 104 'h' */ {0x7f, 0x08, 0x04, 0x04, 0x78},
    /* 105 'i' */ {0x00, 0x44, 0x7d, 0x40, 0x00},
    /* 106 'j' */ {0x20, 0x40, 0x44, 0x3d, 0x00},
    /* 107 'k' */ {0x7f, 0x10, 0x28, 0x44, 0x00},
    /* 108 'l' */ {0x00, 0x41, 0x7f, 0x40, 0x00},
    /* 109 'm' */ {0x7c, 0x04, 0x18, 0x04, 0x78},
    /* 110 'n' */ {0x7c, 0x08, 0x04, 0x04, 0x78},
    /* 111 'o' */ {0x38, 0x44, 0x44, 0x44, 0x38},
    /* 112 'p' */ {0x7c, 0x14, 0x14, 0x14, 0x08},
    /* 113 'q' */ {0x08, 0x14, 0x14, 0x18, 0x7c},
    /* 114 'r' */ {0x7c, 0x08, 0x04, 0x04, 0x08},
    /* 115 's' */ {0x48, 0x54, 0x54, 0x54, 0x20},
    /* 116 't' */ {0x04, 0x3f, 0x44, 0x40, 0x20},
    /* 117 'u' */ {0x3c, 0x40, 0x40, 0x20, 0x7c},
    /* 118 'v' */ {0x1c, 0x20, 0x40, 0x20, 0x1c},
    /* 119 'w' */ {0x3c, 0x40, 0x30, 0x40, 0x3c},
    /* 120 'x' */ {0x44, 0x28, 0x10, 0x28, 0x44},
    /* 121 'y' */ {0x0c, 0x50, 0x50, 0x50, 0x3c},
    /* 122 'z' */ {0x44, 0x64, 0x54, 0x4c, 0x44},
    /* 123 '{' */ {0x00, 0x08, 0x36, 0x41, 0x00},
    /* 124 '|' */ {0x00, 0x00, 0x7f, 0x00, 0x00},
    /* 125 '}' */ {0x00, 0x41, 0x36, 0x08, 0x00},
    /* 126 '~' */ {0x08, 0x08, 0x2a, 0x1c, 0x08},
};

/* 1024-entry precomputed high-dynamic-range harmonic color palette */
static uint32_t g_palette[1024];

static void init_fractal_palette(void)
{
    for (int i = 0; i < 1024; i++) {
        double t = (double)i / 1024.0;
        /* Multi-harmonic coloring: Deep Space Navy -> Electric Cobalt -> Cyan -> Solar Gold -> Flame Ruby */
        double r = 0.50 + 0.42 * cos(2.0 * M_PI * (t * 1.0 + 0.05)) + 0.08 * cos(2.0 * M_PI * (t * 3.0 + 0.20));
        double g = 0.50 + 0.42 * cos(2.0 * M_PI * (t * 1.0 + 0.35)) + 0.08 * cos(2.0 * M_PI * (t * 2.0 + 0.50));
        double b = 0.50 + 0.42 * cos(2.0 * M_PI * (t * 1.0 + 0.65)) + 0.08 * cos(2.0 * M_PI * (t * 4.0 + 0.10));

        int ir = (int)(r * 255.0); if (ir < 0) ir = 0; if (ir > 255) ir = 255;
        int ig = (int)(g * 255.0); if (ig < 0) ig = 0; if (ig > 255) ig = 255;
        int ib = (int)(b * 255.0); if (ib < 0) ib = 0; if (ib > 255) ib = 255;
        g_palette[i] = (ir << 16) | (ig << 8) | ib;
    }
}

/* Sample smooth color from palette with outer-space cosmic fade */
static inline uint32_t sample_smooth_color(double nu)
{
    if (nu < 0.0) {
        return 0x0003050a; /* Deep obsidian velvet interior */
    }

    /* Cosmic outer-space fade for low escape iterations */
    double fade = (nu < 12.0) ? (nu / 12.0) : 1.0;
    fade = fade * fade; /* Quadratic ease-in to keep space dark */

    double pos = nu * 16.0;
    int idx0 = (int)pos;
    double frac = pos - (double)idx0;
    int i0 = idx0 & 1023;
    int i1 = (idx0 + 1) & 1023;

    uint32_t c0 = g_palette[i0];
    uint32_t c1 = g_palette[i1];

    int r0 = (c0 >> 16) & 0xff, g0 = (c0 >> 8) & 0xff, b0 = c0 & 0xff;
    int r1 = (c1 >> 16) & 0xff, g1 = (c1 >> 8) & 0xff, b1 = c1 & 0xff;

    int r_interp = (int)(r0 + frac * (r1 - r0));
    int g_interp = (int)(g0 + frac * (g1 - g0));
    int b_interp = (int)(b0 + frac * (b1 - b0));

    /* Blend towards deep cosmic navy (0x00050814) */
    int r = (int)(r_interp * fade + 5 * (1.0 - fade));
    int g = (int)(g_interp * fade + 8 * (1.0 - fade));
    int b = (int)(b_interp * fade + 20 * (1.0 - fade));

    return (r << 16) | (g << 8) | b;
}

/* Draw a single character using native hardware solid rect fills */
static void draw_char(hpfx3d_context *ctx, int x, int y, char c, uint32_t color, int scale)
{
    if (!ctx || scale <= 0) return;
    int idx = (int)(unsigned char)c - 32;
    if (idx < 0 || idx >= 95) idx = '?' - 32;

    for (int col = 0; col < 5; col++) {
        uint8_t bits = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            if ((bits >> row) & 1) {
                hpfx3d_fill_rect(ctx, x + col * scale, y + row * scale, scale, scale, color, 0xcc);
            }
        }
    }
}

/* Draw a string with horizontal advance of (5 + 1) * scale pixels per character */
static void draw_text(hpfx3d_context *ctx, int x, int y, const char *str, uint32_t color, int scale)
{
    if (!ctx || !str || scale <= 0) return;
    int cur_x = x;
    while (*str) {
        draw_char(ctx, cur_x, y, *str, color, scale);
        cur_x += 6 * scale;
        str++;
    }
}

/* Bresenham line drawer rendered via Lego 2D engine rect spans */
static void draw_line(hpfx3d_context *ctx, int x0, int y0, int x1, int y1, uint32_t color, int thickness)
{
    if (!ctx || thickness <= 0) return;

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int r = thickness / 2;

    while (1) {
        hpfx3d_fill_rect(ctx, x0 - r, y0 - r, thickness, thickness, color, 0xcc);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Draw beveled workstation panel with drop shadow and 3D borders */
static void draw_panel(hpfx3d_context *ctx, int x, int y, int w, int h,
                       const char *title, uint32_t header_color, uint32_t bg_color)
{
    /* 1. Drop shadow (+6, +6) */
    hpfx3d_fill_rect(ctx, x + 6, y + 6, w, h, 0x0005080c, 0xcc);

    /* 2. Outer metal chassis border */
    hpfx3d_fill_rect(ctx, x, y, w, h, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, x + 1, y + 1, w - 2, h - 2, 0x004a627a, 0xcc); /* Highlight rim */
    hpfx3d_fill_rect(ctx, x + 2, y + 2, w - 4, h - 4, 0x001a252f, 0xcc); /* Inner bevel */

    /* 3. Title bar */
    int tb_h = 32;
    hpfx3d_fill_rect(ctx, x + 3, y + 3, w - 6, tb_h, header_color, 0xcc);
    hpfx3d_fill_rect(ctx, x + 3, y + 3 + tb_h - 1, w - 6, 1, 0x00111922, 0xcc);

    /* Window action buttons (Close, Min, Max) */
    hpfx3d_fill_rect(ctx, x + w - 24, y + 8, 16, 16, 0x00c0392b, 0xcc);
    hpfx3d_fill_rect(ctx, x + w - 44, y + 8, 16, 16, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, x + w - 64, y + 8, 16, 16, 0x002c3e50, 0xcc);

    /* Title text (scale 2) */
    if (title) {
        draw_text(ctx, x + 36, y + 9, title, 0x00ecf0f1, 2);
    }

    /* 4. Client body area */
    int client_y = y + tb_h + 3;
    int client_h = h - tb_h - 6;
    hpfx3d_fill_rect(ctx, x + 3, client_y, w - 6, client_h, bg_color, 0xcc);
}

/* Master corporate HP medallion badge (26x26) */
static void draw_hp_badge(hpfx3d_context *ctx, int x, int y)
{
    int s = 26;
    /* Blue corporate square with beveled edges */
    hpfx3d_fill_rect(ctx, x, y, s, s, 0x000072ce, 0xcc);
    hpfx3d_fill_rect(ctx, x + 1, y + 1, s - 2, 1, 0x004fa3e3, 0xcc);
    hpfx3d_fill_rect(ctx, x + 1, y + 1, 1, s - 2, 0x004fa3e3, 0xcc);
    hpfx3d_fill_rect(ctx, x + s - 1, y + 1, 1, s - 2, 0x00004c8a, 0xcc);
    hpfx3d_fill_rect(ctx, x + 1, y + s - 1, s - 2, 1, 0x00004c8a, 0xcc);

    /* White circular medallion disc inside (radius = 9.5) */
    for (int r = 3; r < s - 3; r++) {
        int dy = r - 13;
        int dx = (int)sqrt(9.5 * 9.5 - dy * dy);
        if (dx > 0) {
            hpfx3d_fill_rect(ctx, x + 13 - dx, y + r, dx * 2, 1, 0x00ffffff, 0xcc);
        }
    }

    /* Blue "hp" letters drawn inside disc */
    draw_text(ctx, x + 6, y + 7, "hp", 0x000072ce, 2);
}

/* Render Main Viewport: The Mandelbrot Set with Smooth Continuous Coloring */
static void render_mandelbrot_viewport(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "MANDELBROT SET [z_{n+1} = z_n^2 + c]",
               0x001e272e, 0x0005080d);

    int vx = x + 8;
    int vy = y + 36;
    int vw = w - 16;
    int vh = h - 44;

    /* Complex plane bounds */
    double center_r = -0.70;
    double center_i = 0.00;
    double span_r = 2.70;
    double span_i = span_r * ((double)vh / (double)vw);

    double r_min = center_r - span_r * 0.5;
    double i_max = center_i + span_i * 0.5;

    uint32_t *fb = (uint32_t*)ctx->fb_mem;
    int max_iter = 256;
    double log2 = log(2.0);

    /* Render fractal pixels */
    for (int py = 0; py < vh; py++) {
        double ci = i_max - (py / (double)vh) * span_i;
        double ci2 = ci * ci;
        uint32_t *row = &fb[(vy + py) * ctx->width + vx];

        for (int px = 0; px < vw; px++) {
            double cr = r_min + (px / (double)vw) * span_r;

            /* Fast cardioid & period-2 bulb bailout check */
            double q = (cr - 0.25) * (cr - 0.25) + ci2;
            if (q * (q + (cr - 0.25)) <= 0.25 * ci2) {
                row[px] = 0x0003050a;
                continue;
            }
            if ((cr + 1.0) * (cr + 1.0) + ci2 <= 0.0625) {
                row[px] = 0x0003050a;
                continue;
            }

            /* Escape-time loop */
            double zr = 0.0, zi = 0.0;
            double zr2 = 0.0, zi2 = 0.0;
            int iter = 0;

            for (; iter < max_iter; iter++) {
                if (zr2 + zi2 > 4.0) break;
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                zr2 = zr * zr;
                zi2 = zi * zi;
            }

            if (iter >= max_iter) {
                row[px] = 0x0003050a;
            } else {
                /* Continuous potential fractional smooth coloring */
                double mod_sq = zr2 + zi2;
                double nu = (double)iter + 1.0 - log(log(mod_sq) / 2.0) / log2;
                row[px] = sample_smooth_color(nu);
            }
        }
    }

    /* Complex Plane Coordinate Reticle & Grid Lines */
    int axis_x = vx + (int)((0.0 - r_min) / span_r * vw);
    int axis_y = vy + (int)((i_max - 0.0) / span_i * vh);

    if (axis_x >= vx && axis_x < vx + vw) {
        for (int gy = vy; gy < vy + vh; gy += 4) {
            hpfx3d_fill_rect(ctx, axis_x, gy, 1, 2, 0x001d3648, 0xcc);
        }
    }
    if (axis_y >= vy && axis_y < vy + vh) {
        for (int gx = vx; gx < vx + vw; gx += 4) {
            hpfx3d_fill_rect(ctx, gx, axis_y, 2, 1, 0x001d3648, 0xcc);
        }
    }

    /* Coordinate Axis Ticks & Numerical Labels at Bottom Margin */
    double tick_r[6] = { -2.0, -1.5, -1.0, -0.5, 0.0, 0.5 };
    for (int i = 0; i < 6; i++) {
        int tx = vx + (int)((tick_r[i] - r_min) / span_r * vw);
        if (tx >= vx && tx < vx + vw) {
            hpfx3d_fill_rect(ctx, tx, vy + vh - 18, 1, 8, 0x0048dbfb, 0xcc);
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "%+.1f", tick_r[i]);
            draw_text(ctx, tx - 14, vy + vh - 28, lbl, 0x0048dbfb, 1);
        }
    }

    /* Vertical Imaginary Axis Ticks (placed cleanly in middle range to prevent corner overlap) */
    double tick_i[5] = { 0.8, 0.4, 0.0, -0.4, -0.8 };
    for (int j = 0; j < 5; j++) {
        int ty = vy + (int)((i_max - tick_i[j]) / span_i * vh);
        if (ty >= vy + 40 && ty < vy + vh - 40) {
            hpfx3d_fill_rect(ctx, vx + 4, ty, 8, 1, 0x0048dbfb, 0xcc);
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "%+.1fi", tick_i[j]);
            draw_text(ctx, vx + 16, ty - 4, lbl, 0x0048dbfb, 1);
        }
    }

    /* Mathematical Notation Banner at Top-Left of Viewport */
    hpfx3d_fill_rect(ctx, vx + 10, vy + 10, 560, 20, 0x000a121a, 0xcc);
    hpfx3d_fill_rect(ctx, vx + 10, vy + 10, 560, 1, 0x002c3e50, 0xcc);
    draw_text(ctx, vx + 18, vy + 15, "MAP: C -> C | z_{n+1} = z_n^2 + c | MAX_ITER: 256 | CONTINUOUS POTENTIAL", 0x00ecf0f1, 1);

    /* Hardware XOR Selection Zoom Box (0x66 ROP) */
    /* Highlights the Seahorse Valley zoom region on the Mandelbrot set in silicon */
    int zw_x = vx + (int)((-0.760 - r_min) / span_r * vw) - 45;
    int zw_y = vy + (int)((i_max - 0.134) / span_i * vh) - 45;
    int zw_w = 160;
    int zw_h = 110;

    /* Silicon XOR ROP 0x66 inversion */
    hpfx3d_fill_rect(ctx, zw_x, zw_y, zw_w, zw_h, 0x003e4a59, 0x66);

    /* Crisp dashed frame & reticle corners around XOR box */
    draw_line(ctx, zw_x, zw_y, zw_x + zw_w, zw_y, 0x00ffffff, 1);
    draw_line(ctx, zw_x, zw_y + zw_h, zw_x + zw_w, zw_y + zw_h, 0x00ffffff, 1);
    draw_line(ctx, zw_x, zw_y, zw_x, zw_y + zw_h, 0x00ffffff, 1);
    draw_line(ctx, zw_x + zw_w, zw_y, zw_x + zw_w, zw_y + zw_h, 0x00ffffff, 1);

    /* HUD Zoom Annotation Tag inside XOR box */
    hpfx3d_fill_rect(ctx, zw_x + 4, zw_y + 4, zw_w - 8, 18, 0x000b1724, 0xcc);
    draw_text(ctx, zw_x + 8, zw_y + 9, "HW XOR ZOOM: 1,250x", 0x0000ffcc, 1);

    /* Visual Projection Frustum Lines from XOR box corners to Deep Zoom Viewport */
    draw_line(ctx, zw_x + zw_w, zw_y, 1290, 596, 0x0000a8ff, 1);
    draw_line(ctx, zw_x + zw_w, zw_y + zw_h, 1290, 916, 0x0000a8ff, 1);
}

/* Render Top-Right Panel: Associated Connected Julia Set */
static void render_julia_viewport(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "JULIA SET [c = -0.7269 + 0.1889i]",
               0x001e272e, 0x0005080d);

    int vx = x + 8;
    int vy = y + 36;
    int vw = w - 16;
    int vh = h - 44;

    /* Julia parameter constant */
    double cr = -0.7269;
    double ci = 0.1889;

    double span_r = 3.0;
    double span_i = span_r * ((double)vh / (double)vw);
    double r_min = -span_r * 0.5;
    double i_max = span_i * 0.5;

    uint32_t *fb = (uint32_t*)ctx->fb_mem;
    int max_iter = 256;
    double log2 = log(2.0);

    for (int py = 0; py < vh; py++) {
        double zi_start = i_max - (py / (double)vh) * span_i;
        uint32_t *row = &fb[(vy + py) * ctx->width + vx];

        for (int px = 0; px < vw; px++) {
            double zr = r_min + (px / (double)vw) * span_r;
            double zi = zi_start;
            double zr2 = zr * zr, zi2 = zi * zi;
            int iter = 0;

            for (; iter < max_iter; iter++) {
                if (zr2 + zi2 > 4.0) break;
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                zr2 = zr * zr;
                zi2 = zi * zi;
            }

            if (iter >= max_iter) {
                row[px] = 0x0003050a;
            } else {
                double mod_sq = zr2 + zi2;
                double nu = (double)iter + 1.0 - log(log(mod_sq) / 2.0) / log2;
                row[px] = sample_smooth_color(nu);
            }
        }
    }

    /* Info annotation banner at bottom of Julia window */
    hpfx3d_fill_rect(ctx, vx + 8, vy + vh - 22, vw - 16, 20, 0x000a121a, 0xcc);
    hpfx3d_fill_rect(ctx, vx + 8, vy + vh - 22, vw - 16, 1, 0x002c3e50, 0xcc);
    draw_text(ctx, vx + 14, vy + vh - 16, "DOUADY RABBIT / FATOU DUST | PERIOD: 3 ORBITS", 0x002ecc71, 1);
}

/* Render Bottom-Right Panel: Seahorse Valley Deep-Zoom & Telemetry */
static void render_deepzoom_viewport(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "SEAHORSE VALLEY DEEP ZOOM [1,250x]",
               0x001e272e, 0x0005080d);

    int vx = x + 8;
    int vy = y + 36;
    int vw = w - 16;
    int vh = 310; /* Fractal display height */

    /* Deep zoom target: Seahorse Valley cusp spirals */
    double center_r = -0.7436438870371587;
    double center_i = 0.1318259042053119;
    double span_r = 0.0036;
    double span_i = span_r * ((double)vh / (double)vw);

    double r_min = center_r - span_r * 0.5;
    double i_max = center_i + span_i * 0.5;

    uint32_t *fb = (uint32_t*)ctx->fb_mem;
    int max_iter = 512;
    double log2 = log(2.0);

    for (int py = 0; py < vh; py++) {
        double ci = i_max - (py / (double)vh) * span_i;
        uint32_t *row = &fb[(vy + py) * ctx->width + vx];

        for (int px = 0; px < vw; px++) {
            double cr = r_min + (px / (double)vw) * span_r;
            double zr = 0.0, zi = 0.0;
            double zr2 = 0.0, zi2 = 0.0;
            int iter = 0;

            for (; iter < max_iter; iter++) {
                if (zr2 + zi2 > 4.0) break;
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                zr2 = zr * zr;
                zi2 = zi * zi;
            }

            if (iter >= max_iter) {
                row[px] = 0x0003050a;
            } else {
                double mod_sq = zr2 + zi2;
                double nu = (double)iter + 1.0 - log(log(mod_sq) / 2.0) / log2;
                row[px] = sample_smooth_color(nu * 1.4);
            }
        }
    }

    /* Sub-section: Telemetry & Silicon Performance Card */
    int tx = x + 8;
    int ty = vy + vh + 8;
    int tw = w - 16;
    int th = h - (ty - y) - 6;

    hpfx3d_fill_rect(ctx, tx, ty, tw, th, 0x000a1219, 0xcc);
    hpfx3d_fill_rect(ctx, tx, ty, tw, 1, 0x002c3e50, 0xcc);

    draw_text(ctx, tx + 10, ty + 8, "LEGO 128-BIT ENGINE & FRACTAL TELEMETRY", 0x00f39c12, 1);

    const char *telemetry[4][3] = {
        { "HPFX_REG_STATUS:    ", "0x00000001", "[FIFO_READY | MMIO_ACTIVE]" },
        { "HPFX_REG_ROP:       ", "0x000000CC", "[SRCCOPY / 0x66 XOR ACTIVE]" },
        { "HPFX_REG_FIFO_FREE: ", "0x000003FE", "[1022 FIFO WORDS FREE]" },
        { "HPFX_REG_FB_OFFSET: ", "0x00000000", "[BAR0 VRAM LINEAR BASE]" }
    };

    for (int i = 0; i < 4; i++) {
        int ry = ty + 24 + i * 16;
        draw_text(ctx, tx + 10, ry, telemetry[i][0], 0x00bdc3c7, 1);
        draw_text(ctx, tx + 160, ry, telemetry[i][1], 0x0000d2d3, 1);
        draw_text(ctx, tx + 265, ry, telemetry[i][2], 0x002ecc71, 1);
    }

    /* Color Palette LUT Swatch at Bottom */
    int lut_y = ty + 92;
    draw_text(ctx, tx + 10, lut_y - 2, "32-BIT HARMONIC COLOR LUT:", 0x00ecf0f1, 1);

    int lut_w = tw - 20;
    for (int col = 0; col < lut_w; col++) {
        int pal_idx = (col * 1024) / lut_w;
        hpfx3d_fill_rect(ctx, tx + 10 + col, lut_y + 12, 1, 12, g_palette[pal_idx], 0xcc);
    }
    hpfx3d_fill_rect(ctx, tx + 9, lut_y + 11, lut_w + 2, 1, 0x004a627a, 0xcc);
    hpfx3d_fill_rect(ctx, tx + 9, lut_y + 24, lut_w + 2, 1, 0x004a627a, 0xcc);
}

static void save_frame_ppm(const char *filename, const hpfx3d_context *ctx)
{
    if (!ctx || !ctx->fb_mem) return;
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    fprintf(f, "P6\n%d %d\n255\n", ctx->width, ctx->height);
    const uint32_t *fb = (const uint32_t*)ctx->fb_mem;

    for (uint32_t y = 0; y < ctx->height; y++) {
        for (uint32_t x = 0; x < ctx->width; x++) {
            uint32_t p = fb[y * ctx->width + x];
            fputc((p >> 16) & 0xff, f);
            fputc((p >> 8) & 0xff, f);
            fputc(p & 0xff, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    int width = 1920;
    int height = 1080;
    const char *out_ppm = "hpfx_render_2d_blt.ppm";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") && i + 1 < argc) width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-h") && i + 1 < argc) height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_ppm = argv[++i];
    }

    printf("===================================================================\n");
    printf(" HP Visualize FX Hardware 2D BitBLT & ROP Compositing Engine Demo\n");
    printf(" Target: HP Visualize FX5 / FX10 Architecture (Lego 128-bit 2D ASIC)\n");
    printf(" Scene : HP 2D Fractal Computing Studio (Mandelbrot & Julia Explorer)\n");
    printf("===================================================================\n");
    printf("Resolution: %dx%d | Executing hardware 2D BitBLTs, ROPs, and fractals...\n", width, height);

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to open context\n");
        return 1;
    }
    hpfx3d_resize(ctx, width, height);

    /* 1. Initialize precomputed smooth harmonic palette */
    init_fractal_palette();

    /* 2. Desktop background: Deep HP Workstation Dark Slate */
    hpfx3d_fill_rect(ctx, 0, 0, width, height, 0x000c141d, 0xcc);

    /* Fine 2D engineering blueprint grid (32x32) */
    for (int y = 0; y < height; y += 32) {
        hpfx3d_fill_rect(ctx, 0, y, width, 1, 0x0013212f, 0xcc);
    }
    for (int x = 0; x < width; x += 32) {
        hpfx3d_fill_rect(ctx, x, 0, 1, height, 0x0013212f, 0xcc);
    }

    /* 3. Master Instrument Header Bar */
    int bar_h = 44;
    hpfx3d_fill_rect(ctx, 0, 0, width, bar_h, 0x00141e28, 0xcc);
    hpfx3d_fill_rect(ctx, 0, bar_h - 1, width, 1, 0x0034495e, 0xcc);

    /* Master HP Corporate Emblem at (16, 9) */
    draw_hp_badge(ctx, 16, 9);

    /* Main Console Titles */
    draw_text(ctx, 56, 8, "HEWLETT-PACKARD VISUALIZE FX10 FRACTAL COMPUTING STUDIO", 0x00ecf0f1, 2);
    draw_text(ctx, 58, 28, "TARGET: 2D ITERATED DYNAMICS (LEGO 128-BIT ENGINE & VRAM BITBLT ACCELERATION)", 0x007f8c8d, 1);

    /* System Status Annunciators in Header */
    hpfx3d_fill_rect(ctx, width - 480, 8, 105, 28, 0x0027ae60, 0xcc);
    draw_text(ctx, width - 472, 17, "[ENGINE: 128b]", 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, width - 365, 8, 115, 28, 0x002980b9, 0xcc);
    draw_text(ctx, width - 357, 17, "ITER: 512 MAX", 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, width - 240, 8, 115, 28, 0x008e44ad, 0xcc);
    draw_text(ctx, width - 232, 17, "COLOR: 32b POT", 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, width - 115, 8, 100, 28, 0x00d35400, 0xcc);
    draw_text(ctx, width - 107, 17, "ROP 0x66 ZOOM", 0x00ffffff, 1);

    /* 4. Primary Viewports */
    /* Main Viewport: The Mandelbrot Set (Left, 1260x1014) */
    render_mandelbrot_viewport(ctx, 16, 52, 1260, 1014);

    /* Top-Right Panel: Associated Connected Julia Set (614x494) */
    render_julia_viewport(ctx, 1290, 52, 614, 494);

    /* Bottom-Right Panel: Seahorse Valley Deep Zoom & Telemetry (614x506) */
    render_deepzoom_viewport(ctx, 1290, 560, 614, 506);

    /* 5. Hardware Screen-to-Screen BitBLT (hpfx3d_copy_area): */
    /* Stamp the master HP corporate medallion from (16, 9) into each window title bar! */
    hpfx3d_copy_area(ctx, 16, 9, 16 + 5, 52 + 6, 26, 26, 0xcc);
    hpfx3d_copy_area(ctx, 16, 9, 1290 + 5, 52 + 6, 26, 26, 0xcc);
    hpfx3d_copy_area(ctx, 16, 9, 1290 + 5, 560 + 6, 26, 26, 0xcc);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);

    printf("[Export] Writing 2D BitBLT compositing frame to: %s\n", out_ppm);
    save_frame_ppm(out_ppm, ctx);
    printf("[Checksum] IEEE 802.3 Framebuffer CRC32: 0x%08X\n", crc);

    hpfx3d_close(ctx);
    printf("Hardware 2D BitBLT demonstration completed successfully.\n");
    return 0;
}
