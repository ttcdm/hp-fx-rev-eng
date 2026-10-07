/*
 * HP Visualize FX Hardware 2D BitBLT & ROP Compositing Engine Showcase
 *
 * Demonstrates the dedicated 128-bit 2D acceleration engine on HP "Lego" ASICs:
 * - High-speed solid area fills via HPFX_REG_COORD_START & TRIGGER (0xCC SRCCOPY)
 * - Hardware Raster Operations: Invert/XOR Rubber-Banding (0x66) for waveform glitch zoom
 * - Screen-to-screen BitBLT area copy (hpfx3d_copy_area) for multi-panel UI badges
 * - Native 5x7 ASCII bitmap font rendering in hardware silicon (scale 1x, 2x, 3x)
 * - Authentic HP 16700A Logic Analysis System & HP 54845A Infiniium Oscilloscope Console
 * - 4-channel real-time analog waveforms with transmission-line ringing and impedance physics
 * - 8-channel digital logic analyzer with decoded bus packet protocol ribbons
 * - 32-bin RF spectral power distribution (FFT) with harmonic peaks & automated telemetry
 * - Live HP Visualize FX MMIO register dump & silicon hardware utilization meters
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

/* Render the complete HP 54845A Infiniium Oscilloscope Screen */
static void render_oscilloscope(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "HP 54845A INFINIIUM OSCILLOSCOPE [4-CH REAL-TIME]",
               0x001e272e, 0x000a1016);

    /* Scope screen graticule inner area */
    int gx = x + 65;
    int gy = y + 42;
    int gw = w - 75;
    int gh = h - 60;

    /* Deep CRT phosphor screen background */
    hpfx3d_fill_rect(ctx, gx, gy, gw, gh, 0x0003070b, 0xcc);
    hpfx3d_fill_rect(ctx, gx - 1, gy - 1, gw + 2, 1, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, gx - 1, gy - 1, 1, gh + 2, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, gx + gw, gy - 1, 1, gh + 2, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, gx - 1, gy + gh, gw + 2, 1, 0x002c3e50, 0xcc);

    /* 10 x 8 Major division grid */
    for (int i = 0; i <= 10; i++) {
        int line_x = gx + (i * gw) / 10;
        uint32_t gc = (i == 5) ? 0x001d3648 : 0x0011212d;
        hpfx3d_fill_rect(ctx, line_x, gy, 1, gh, gc, 0xcc);
        /* Sub-division tick marks on center axes */
        for (int ty = gy; ty < gy + gh; ty += 8) {
            hpfx3d_fill_rect(ctx, line_x - 2, ty, 5, 1, gc, 0xcc);
        }
    }
    for (int j = 0; j <= 8; j++) {
        int line_y = gy + (j * gh) / 8;
        uint32_t gc = (j == 4) ? 0x001d3648 : 0x0011212d;
        hpfx3d_fill_rect(ctx, gx, line_y, gw, 1, gc, 0xcc);
        /* Sub-division tick marks on center axes */
        for (int tx = gx; tx < gx + gw; tx += 8) {
            hpfx3d_fill_rect(ctx, tx, line_y - 2, 1, 5, gc, 0xcc);
        }
    }

    /* Left margin voltage division labels */
    const char *v_labels[9] = { "+4.0V", "+3.0V", "+2.0V", "+1.0V", " 0.0V", "-1.0V", "-2.0V", "-3.0V", "-4.0V" };
    for (int j = 0; j <= 8; j++) {
        int line_y = gy + (j * gh) / 8;
        draw_text(ctx, x + 8, line_y - 4, v_labels[j], 0x0048dbfb, 1);
    }

    /* Bottom margin timebase labels */
    const char *t_labels[11] = { "-50ns", "-40ns", "-30ns", "-20ns", "-10ns", "  0ns", "+10ns", "+20ns", "+30ns", "+40ns", "+50ns" };
    for (int i = 0; i <= 10; i++) {
        int line_x = gx + (i * gw) / 10;
        draw_text(ctx, line_x - 14, gy + gh + 4, t_labels[i], 0x00f39c12, 1);
    }

    /* Waveform Channels */
    /* CH1 (Amber Gold): PCI 66.67 MHz System Clock with high-speed transmission ringing */
    int prev_x1 = gx, prev_y1 = gy + (4 * gh) / 8;
    for (int col = 0; col < gw; col++) {
        double t_ns = ((double)col / (double)gw - 0.5) * 100.0; /* -50ns to +50ns */
        double period = 15.0; /* 66.67 MHz = 15.0 ns period */
        double phase = fmod(t_ns + 1000.0, period);
        double v = 0.0;

        if (phase < 7.5) {
            /* High state with rising edge ringing */
            double tr = phase;
            if (tr < 0.6) {
                v = 3.3 * (tr / 0.6);
            } else {
                /* Damped overshoot oscillation */
                double overshoot = 0.45 * exp(-tr / 1.8) * cos(2.0 * M_PI * tr / 1.6);
                v = 3.3 + overshoot;
            }
        } else {
            /* Low state with falling edge undershoot */
            double tf = phase - 7.5;
            if (tf < 0.6) {
                v = 3.3 * (1.0 - tf / 0.6);
            } else {
                double undershoot = -0.35 * exp(-tf / 1.8) * cos(2.0 * M_PI * tf / 1.6);
                v = 0.0 + undershoot;
            }
        }

        int py = gy + (int)((4.0 - v) * (gh / 8.0));
        if (py < gy) py = gy;
        if (py >= gy + gh) py = gy + gh - 1;

        int px = gx + col;
        if (col > 0) {
            draw_line(ctx, prev_x1, prev_y1, px, py, 0x00f39c12, 2);
        }
        prev_x1 = px;
        prev_y1 = py;
    }

    /* CH2 (Cyan): AGP 4X 4-byte Burst Data Strobe AD_STB[0] */
    int prev_x2 = gx, prev_y2 = gy + (gh * 5) / 8;
    for (int col = 0; col < gw; col++) {
        double t_ns = ((double)col / (double)gw - 0.5) * 100.0;
        double period = 7.5; /* 133.3 MHz Strobe (QDR AGP 4X) */
        double phase = fmod(t_ns + 1000.0, period);
        double v = (phase < 3.75) ? 2.5 : 0.0;
        if (fmod(phase, 3.75) < 0.5) {
            v = 1.25 + 1.25 * sin((fmod(phase, 3.75) / 0.5 - 0.5) * M_PI);
        }
        /* Burst gating: active only during burst windows */
        if (fmod(t_ns + 50.0, 30.0) > 22.0) v = 1.25; /* Tri-state bias */

        int py = gy + (int)((3.5 - v) * (gh / 8.0));
        int px = gx + col;
        if (col > 0) {
            draw_line(ctx, prev_x2, prev_y2, px, py, 0x0000d2d3, 2);
        }
        prev_x2 = px;
        prev_y2 = py;
    }

    /* CH3 (Neon Green): Lego ASIC Command FIFO Ready FIFO_READY# */
    int prev_x3 = gx, prev_y3 = gy + (gh * 6) / 8;
    for (int col = 0; col < gw; col++) {
        double t_ns = ((double)col / (double)gw - 0.5) * 100.0;
        double v = 3.3;
        /* Pulses low every 30 ns for 12 ns (active batch drain) */
        double cycle = fmod(t_ns + 1000.0, 30.0);
        if (cycle < 12.0) v = 0.2;

        int py = gy + (int)((3.0 - v) * (gh / 8.0));
        int px = gx + col;
        if (col > 0) {
            draw_line(ctx, prev_x3, prev_y3, px, py, 0x001dd1a1, 2);
        }
        prev_x3 = px;
        prev_y3 = py;
    }

    /* CH4 (Magenta): Ramdac 360 MHz Video DAC Output & Raster Sync */
    int prev_x4 = gx, prev_y4 = gy + (gh * 7) / 8;
    for (int col = 0; col < gw; col++) {
        double t_ns = ((double)col / (double)gw - 0.5) * 100.0;
        double sweep = fmod(t_ns + 1000.0, 40.0);
        double v = 0.0;
        if (sweep < 6.0) v = -0.4; /* Sync pulse */
        else if (sweep < 10.0) v = 0.0; /* Back porch */
        else v = (sweep - 10.0) / 30.0 * 0.7; /* Active video ramp */

        int py = gy + (int)((2.0 - v * 2.0) * (gh / 8.0));
        int px = gx + col;
        if (col > 0) {
            draw_line(ctx, prev_x4, prev_y4, px, py, 0x00ff7675, 2);
        }
        prev_x4 = px;
        prev_y4 = py;
    }

    /* Vertical Time Measurement Cursors T1 and T2 */
    int t1_x = gx + (int)(gw * 0.35);
    int t2_x = gx + (int)(gw * 0.65);
    for (int cy = gy; cy < gy + gh; cy += 6) {
        hpfx3d_fill_rect(ctx, t1_x, cy, 1, 3, 0x00f1c40f, 0xcc);
        hpfx3d_fill_rect(ctx, t2_x, cy, 1, 3, 0x00f1c40f, 0xcc);
    }

    /* Cursor Delta Header Centered at gy + 8 */
    hpfx3d_fill_rect(ctx, gx + (gw / 2) - 175, gy + 8, 350, 20, 0x00141f29, 0xcc);
    hpfx3d_fill_rect(ctx, gx + (gw / 2) - 175, gy + 8, 350, 1, 0x002c3e50, 0xcc);
    draw_text(ctx, gx + (gw / 2) - 165, gy + 14, "dT: 30.00ns | 1/dT: 33.33MHz | dV(CH1): 3.32V", 0x00ffffff, 1);

    /* Cursor readout flags located cleanly below the header at gy + 34 */
    hpfx3d_fill_rect(ctx, t1_x - 30, gy + 34, 60, 18, 0x002c3e50, 0xcc);
    draw_text(ctx, t1_x - 24, gy + 39, "T1:-15ns", 0x00f1c40f, 1);
    hpfx3d_fill_rect(ctx, t2_x - 30, gy + 34, 60, 18, 0x002c3e50, 0xcc);
    draw_text(ctx, t2_x - 24, gy + 39, "T2:+15ns", 0x00f1c40f, 1);

    /* Hardware XOR ROP Glitch Zoom Selection Box (0x66) */
    /* Demonstrates genuine silicon XOR rubber-band box inversion! */
    int zw_x = gx + (int)(gw * 0.44);
    int zw_y = gy + (int)(gh * 0.22);
    int zw_w = 340;
    int zw_h = 170;
    hpfx3d_fill_rect(ctx, zw_x, zw_y, zw_w, zw_h, 0x003e4a59, 0x66);

    /* Beveled XOR Box Border & Banner */
    draw_line(ctx, zw_x, zw_y, zw_x + zw_w, zw_y, 0x00ffffff, 1);
    draw_line(ctx, zw_x, zw_y + zw_h, zw_x + zw_w, zw_y + zw_h, 0x00ffffff, 1);
    draw_line(ctx, zw_x, zw_y, zw_x, zw_y + zw_h, 0x00ffffff, 1);
    draw_line(ctx, zw_x + zw_w, zw_y, zw_x + zw_w, zw_y + zw_h, 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, zw_x + 6, zw_y + 6, zw_w - 12, 18, 0x000b1724, 0xcc);
    draw_text(ctx, zw_x + 12, zw_y + 11, "HW XOR GLITCH ZOOM [ROP 0x66] 10x MAG", 0x0000ffcc, 1);

    /* Channel Legends on Top Right */
    hpfx3d_fill_rect(ctx, gx + gw - 460, gy + gh - 26, 110, 20, 0x00f39c12, 0xcc);
    draw_text(ctx, gx + gw - 454, gy + gh - 21, "CH1:CLK 1V/d", 0x00000000, 1);

    hpfx3d_fill_rect(ctx, gx + gw - 345, gy + gh - 26, 110, 20, 0x0000d2d3, 0xcc);
    draw_text(ctx, gx + gw - 339, gy + gh - 21, "CH2:STB 1V/d", 0x00000000, 1);

    hpfx3d_fill_rect(ctx, gx + gw - 230, gy + gh - 26, 110, 20, 0x001dd1a1, 0xcc);
    draw_text(ctx, gx + gw - 224, gy + gh - 21, "CH3:RDY .5V", 0x00000000, 1);

    hpfx3d_fill_rect(ctx, gx + gw - 115, gy + gh - 26, 110, 20, 0x00ff7675, 0xcc);
    draw_text(ctx, gx + gw - 109, gy + gh - 21, "CH4:DAC .5V", 0x00000000, 1);
}

/* Render the 8-Channel HP 16700A Digital Logic Analyzer Screen */
static void render_logic_analyzer(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "HP 16700A 16.7 GHz TIMING LOGIC ANALYZER [8-CH BUS]",
               0x001e272e, 0x000a1016);

    int lx = x + 185;
    int ly = y + 42;
    int lw = w - 195;
    int lh = h - 110;

    /* Background for digital traces */
    hpfx3d_fill_rect(ctx, lx, ly, lw, lh, 0x0003070b, 0xcc);
    hpfx3d_fill_rect(ctx, lx - 1, ly - 1, lw + 2, 1, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, lx - 1, ly - 1, 1, lh + 2, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, lx + lw, ly - 1, 1, lh + 2, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, lx - 1, ly + lh, lw + 2, 1, 0x002c3e50, 0xcc);

    /* 8 Channel names and waveforms */
    const char *ch_names[8] = {
        "D0: PCI_FRAME#",
        "D1: PCI_IRDY#",
        "D2: PCI_TRDY#",
        "D3: PCI_DEVSEL#",
        "D4: LEGO_FIFO_FULL",
        "D5: SUMMIT_TL_BUSY",
        "D6: AGP_SBA[0]",
        "D7: VRAM_RAS#"
    };
    const uint32_t ch_colors[8] = {
        0x00f1c40f, 0x00f1c40f, 0x00f1c40f, 0x00f1c40f,
        0x0000d2d3, 0x0000d2d3, 0x00fd79a8, 0x002ecc71
    };

    int lane_h = lh / 8;

    for (int ch = 0; ch < 8; ch++) {
        int lane_y = ly + ch * lane_h;

        /* Channel label pod */
        hpfx3d_fill_rect(ctx, x + 10, lane_y + 4, 165, 26, 0x00131e29, 0xcc);
        hpfx3d_fill_rect(ctx, x + 10, lane_y + 4, 4, 26, ch_colors[ch], 0xcc);
        draw_text(ctx, x + 18, lane_y + 12, ch_names[ch], ch_colors[ch], 1);

        /* Lane grid separator line */
        hpfx3d_fill_rect(ctx, lx, lane_y + lane_h - 1, lw, 1, 0x00121d27, 0xcc);

        /* Digital waveform synthesis */
        int prev_wx = lx;
        int prev_state = 0;
        int y_low = lane_y + lane_h - 8;
        int y_high = lane_y + 8;

        for (int col = 0; col < lw; col++) {
            double t = (double)col / 32.0; /* 32 pixels per clock period */
            int state = 0;

            switch (ch) {
                case 0: /* PCI_FRAME#: Active Low during bursts */
                    state = ((int)t % 16 >= 2 && (int)t % 16 <= 10) ? 0 : 1;
                    break;
                case 1: /* PCI_IRDY#: Active Low */
                    state = ((int)t % 16 >= 3 && (int)t % 16 <= 11) ? 0 : 1;
                    break;
                case 2: /* PCI_TRDY#: Target ready */
                    state = ((int)t % 16 >= 4 && (int)t % 16 <= 11) ? 0 : 1;
                    break;
                case 3: /* PCI_DEVSEL# */
                    state = ((int)t % 16 >= 3 && (int)t % 16 <= 12) ? 0 : 1;
                    break;
                case 4: /* LEGO_FIFO_FULL */
                    state = ((int)t % 24 >= 14 && (int)t % 24 <= 17) ? 1 : 0;
                    break;
                case 5: /* SUMMIT_TL_BUSY */
                    state = ((int)t % 20 >= 4 && (int)t % 20 <= 16) ? 1 : 0;
                    break;
                case 6: /* AGP_SBA[0] Sideband address strobe */
                    state = ((int)(t * 2.0) % 2 == 0) ? 1 : 0;
                    break;
                case 7: /* VRAM_RAS# */
                    state = ((int)t % 8 == 2 || (int)t % 8 == 6) ? 0 : 1;
                    break;
            }

            if (col == 0) prev_state = state;

            int cur_y = state ? y_high : y_low;
            int px = lx + col;

            if (state != prev_state) {
                /* Vertical transition edge */
                draw_line(ctx, px, y_low, px, y_high, ch_colors[ch], 2);
                prev_state = state;
            } else {
                /* Horizontal level */
                hpfx3d_fill_rect(ctx, px, cur_y, 1, 2, ch_colors[ch], 0xcc);
            }
            prev_wx = px;
            (void)prev_wx;
        }
    }

    /* Decoded Protocol State Bus Ribbon at Bottom */
    int ry = y + h - 58;
    int rw = w - 20;
    hpfx3d_fill_rect(ctx, x + 10, ry, rw, 48, 0x000e1822, 0xcc);
    hpfx3d_fill_rect(ctx, x + 10, ry, rw, 1, 0x002c3e50, 0xcc);

    draw_text(ctx, x + 20, ry + 18, "BUS DECODE:", 0x00ffffff, 2);

    /* Protocol packet capsules */
    struct {
        const char *label;
        uint32_t color;
        int pw;
    } packets[6] = {
        { "IDLE [BUS RESET]", 0x007f8c8d, 150 },
        { "CMD: 0x101826 (TRI_BATCH)", 0x0027ae60, 230 },
        { "VRAM WR: 0xFAC57B", 0x002980b9, 180 },
        { "FIFO_SYNC [WAIT]", 0x00d35400, 160 },
        { "BURST ACK (QDR 4X)", 0x008e44ad, 180 },
        { "FLUSH / EOB", 0x0016a085, 140 }
    };

    int cur_px = x + 175;
    for (int p = 0; p < 6; p++) {
        hpfx3d_fill_rect(ctx, cur_px, ry + 8, packets[p].pw, 32, packets[p].color, 0xcc);
        hpfx3d_fill_rect(ctx, cur_px + 1, ry + 9, packets[p].pw - 2, 1, 0x00ffffff, 0xcc);
        hpfx3d_fill_rect(ctx, cur_px + 1, ry + 8 + 30, packets[p].pw - 2, 1, 0x00000000, 0xcc);
        draw_text(ctx, cur_px + 8, ry + 19, packets[p].label, 0x00ffffff, 1);
        cur_px += packets[p].pw + 10;
    }
}

/* Render Real-Time FFT Spectrum Analyzer & Parameter Telemetry */
static void render_spectrum_analyzer(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "HP 8720D RF SPECTRUM ANALYZER",
               0x001e272e, 0x000a1016);

    int fx = x + 45;
    int fy = y + 42;
    int fw = w - 55;
    int fh = 210;

    /* Dark spectrum display area */
    hpfx3d_fill_rect(ctx, fx, fy, fw, fh, 0x00020508, 0xcc);
    hpfx3d_fill_rect(ctx, fx - 1, fy - 1, fw + 2, 1, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, fx - 1, fy - 1, 1, fh + 2, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, fx + fw, fy - 1, 1, fh + 2, 0x002c3e50, 0xcc);
    hpfx3d_fill_rect(ctx, fx - 1, fy + fh, fw + 2, 1, 0x002c3e50, 0xcc);

    /* Grid lines & dBm scale */
    const char *db_labels[5] = { "  0dBm", "-20dBm", "-40dBm", "-60dBm", "-80dBm" };
    for (int i = 0; i < 5; i++) {
        int gy = fy + (i * fh) / 4;
        hpfx3d_fill_rect(ctx, fx, gy, fw, 1, 0x00112233, 0xcc);
        draw_text(ctx, x + 6, gy - 4, db_labels[i], 0x0000cec9, 1);
    }

    /* 32 Frequency Bins */
    int num_bins = 32;
    int bar_width = (fw - (num_bins * 2)) / num_bins;
    double bin_db[32] = {
        -65.0, -74.0, -78.0, -72.0,   /* 0-48 MHz */
          0.0, -75.0, -79.0, -76.0,   /* 4: 66.67 MHz Fundamental Peak (0 dBm) */
        -18.5, -77.0, -79.0, -75.0,   /* 8: 133.3 MHz 2nd Harmonic (-18.5 dBm) */
        -26.2, -78.0, -80.0, -77.0,   /* 12: 200.0 MHz 3rd Harmonic (-26.2 dBm) */
        -34.8, -79.0, -80.0, -78.0,   /* 16: 266.7 MHz 4th Harmonic (-34.8 dBm) */
        -42.5, -78.0, -79.0, -77.0,   /* 20: 333.3 MHz 5th Harmonic (-42.5 dBm) */
        -51.0, -79.0, -80.0, -78.0,   /* 24: 400.0 MHz 6th Harmonic (-51.0 dBm) */
        -58.4, -80.0, -79.0, -78.0    /* 28: 466.7 MHz 7th Harmonic (-58.4 dBm) */
    };

    for (int b = 0; b < num_bins; b++) {
        double db = bin_db[b];
        if (db < -80.0) db = -80.0;
        int bar_h = (int)((db + 80.0) / 80.0 * fh);
        int bx = fx + 2 + b * (bar_width + 2);
        int by = fy + fh - bar_h;

        /* Gradient colored bars */
        uint32_t bar_col = (db > -10.0) ? 0x002ecc71 : ((db > -30.0) ? 0x000984e3 : 0x006c5ce7);
        hpfx3d_fill_rect(ctx, bx, by, bar_width, bar_h, bar_col, 0xcc);

        /* Peak indicator cap */
        hpfx3d_fill_rect(ctx, bx, by - 2, bar_width, 2, 0x00f1c40f, 0xcc);
    }

    /* Frequency labels below FFT */
    const char *f_ticks[9] = { "DC", "66M", "133M", "200M", "266M", "333M", "400M", "466M", "500M" };
    for (int i = 0; i < 9; i++) {
        int lx = fx + (i * fw) / 8;
        if (i == 8) lx -= 20;
        draw_text(ctx, lx, fy + fh + 4, f_ticks[i], 0x00f39c12, 1);
    }

    /* Telemetry Measurement Card */
    int tx = x + 12;
    int ty = fy + fh + 24;
    int tw = w - 24;
    int th = h - (ty - y) - 10;

    hpfx3d_fill_rect(ctx, tx, ty, tw, th, 0x000d161f, 0xcc);
    hpfx3d_fill_rect(ctx, tx, ty, tw, 1, 0x002c3e50, 0xcc);

    draw_text(ctx, tx + 10, ty + 10, "AUTOMATED TELEMETRY PARAMETERS", 0x0000d2d3, 2);

    const char *telemetry[8][3] = {
        { "Vpp (Peak-to-Peak):",  "3.342 V",    "[PASS - NOMINAL]" },
        { "Vrms (True RMS):",     "1.651 V",    "[PASS - 50% DUTY]" },
        { "Freq (CH1 System):",   "66.667 MHz", "[PLL LOCKED]" },
        { "Duty Cycle:",          "50.12 %",    "[BALANCED]" },
        { "Rise Time (10-90%):",  "1.418 ns",   "[HIGH SLEW RATE]" },
        { "Fall Time (90-10%):",  "1.385 ns",   "[HIGH SLEW RATE]" },
        { "Peak Phase Jitter:",   "38.4 ps",    "[LOW NOISE FLOOR]" },
        { "Total Harm Distortion:","-24.6 dB",  "[COMPLIANT]" }
    };

    for (int row = 0; row < 8; row++) {
        int ry = ty + 38 + row * 18;
        draw_text(ctx, tx + 12, ry, telemetry[row][0], 0x00bdc3c7, 1);
        draw_text(ctx, tx + 200, ry, telemetry[row][1], 0x00f1c40f, 1);
        draw_text(ctx, tx + 310, ry, telemetry[row][2], 0x002ecc71, 1);
    }
}

/* Render HP Visualize FX Hardware Registers & Silicon Engine Profiler */
static void render_register_profiler(hpfx3d_context *ctx, int x, int y, int w, int h)
{
    draw_panel(ctx, x, y, w, h,
               "HP VISUALIZE FX10 PROFILER",
               0x001e272e, 0x000a1016);

    int px = x + 12;
    int py = y + 42;
    int pw = w - 24;

    /* Section A: Live Hardware Registers */
    hpfx3d_fill_rect(ctx, px, py, pw, 200, 0x000b1219, 0xcc);
    hpfx3d_fill_rect(ctx, px, py, pw, 1, 0x002c3e50, 0xcc);
    draw_text(ctx, px + 10, py + 8, "LEGO 128-BIT & SUMMIT T&L MMIO REGISTERS", 0x00f39c12, 1);

    const char *reg_dump[8][3] = {
        { "HPFX_REG_STATUS:    ", "0x00000001", "[FIFO_READY | MMIO_ACTIVE]" },
        { "HPFX_REG_CONFIG:    ", "0x00001004", "[AGP_4X | FAST_WRITES]" },
        { "HPFX_REG_FIFO_FREE: ", "0x000003FE", "[1022 FIFO WORDS FREE]" },
        { "HPFX_REG_ROP:       ", "0x000000CC", "[SRCCOPY SOLID / BITBLT]" },
        { "HPFX_REG_FB_OFFSET: ", "0x00000000", "[BAR0 FRAMEBUFFER BASE]" },
        { "HPFX_REG_Z_OFFSET:  ", "0x00400000", "[24-BIT LINEAR Z-BUFFER]" },
        { "HPFX_REG_TEX_OFFSET:", "0x00800000", "[16MB VRAM TEXTURE STAGE]" },
        { "HPFX_REG_CHIP_ID:   ", "0x103C104B", "[HP LEGO B2 WORKSTATION]" }
    };

    for (int i = 0; i < 8; i++) {
        int ry = py + 28 + i * 20;
        draw_text(ctx, px + 12, ry, reg_dump[i][0], 0x00bdc3c7, 1);
        draw_text(ctx, px + 180, ry, reg_dump[i][1], 0x0000d2d3, 1);
        draw_text(ctx, px + 285, ry, reg_dump[i][2], 0x002ecc71, 1);
    }

    /* Section B: Silicon Utilization Gauges */
    int my = py + 212;
    int mh = h - (my - y) - 10;
    hpfx3d_fill_rect(ctx, px, my, pw, mh, 0x000b1219, 0xcc);
    hpfx3d_fill_rect(ctx, px, my, pw, 1, 0x002c3e50, 0xcc);
    draw_text(ctx, px + 10, my + 10, "SILICON ENGINE UTILIZATION GAUGES", 0x0000d2d3, 2);

    struct {
        const char *name;
        double pct;
        const char *rate;
        uint32_t color;
    } meters[4] = {
        { "LEGO 128-BIT 2D ENGINE (BLT/ROP):", 0.864, "1.38 GB/s (9.2 Mblit/s)", 0x00e67e22 },
        { "SUMMIT T&L GEOMETRY VECTOR CORE:", 0.912, "11.2 Mtri/s (PEAK RATE)", 0x002ecc71 },
        { "SDRAM VRAM BUS BANDWIDTH (128b):", 0.785, "3.14 GB/s (78.5% BUS LOAD)", 0x003498db },
        { "AGP 4X COMMAND STREAM FIFO:",      0.640, "682 MB/s (QDR STROBE)", 0x009b59b6 }
    };

    int gauge_y = my + 38;
    for (int m = 0; m < 4; m++) {
        int gy = gauge_y + m * 52;
        draw_text(ctx, px + 12, gy, meters[m].name, 0x00ecf0f1, 1);

        char pct_str[32];
        snprintf(pct_str, sizeof(pct_str), "%5.1f%%", meters[m].pct * 100.0);
        draw_text(ctx, px + 360, gy, pct_str, meters[m].color, 1);
        draw_text(ctx, px + 420, gy, meters[m].rate, 0x0095a5a6, 1);

        /* Progress Bar Chassis */
        int bar_x = px + 12;
        int bar_y = gy + 14;
        int bar_w = pw - 24;
        int bar_h = 16;
        hpfx3d_fill_rect(ctx, bar_x, bar_y, bar_w, bar_h, 0x00141f29, 0xcc);
        hpfx3d_fill_rect(ctx, bar_x - 1, bar_y - 1, bar_w + 2, 1, 0x002c3e50, 0xcc);
        hpfx3d_fill_rect(ctx, bar_x - 1, bar_y + bar_h, bar_w + 2, 1, 0x002c3e50, 0xcc);

        /* Fill segments */
        int fill_w = (int)(bar_w * meters[m].pct);
        for (int seg = 0; seg < fill_w; seg += 6) {
            int seg_w = (seg + 4 <= fill_w) ? 4 : (fill_w - seg);
            hpfx3d_fill_rect(ctx, bar_x + seg, bar_y + 2, seg_w, bar_h - 4, meters[m].color, 0xcc);
        }
    }
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
    printf(" Scene : HP 16700A Logic Analyzer & HP 54845A Infiniium Workstation\n");
    printf("===================================================================\n");
    printf("Resolution: %dx%d | Executing hardware 2D BitBLTs, ROPs, and fonts...\n", width, height);

    hpfx3d_context *ctx = hpfx3d_open(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to open context\n");
        return 1;
    }
    hpfx3d_resize(ctx, width, height);

    /* 1. Desktop background: Deep HP Workstation Dark Slate */
    hpfx3d_fill_rect(ctx, 0, 0, width, height, 0x000c141d, 0xcc);

    /* Fine 2D engineering blueprint grid (32x32) */
    for (int y = 0; y < height; y += 32) {
        hpfx3d_fill_rect(ctx, 0, y, width, 1, 0x0013212f, 0xcc);
    }
    for (int x = 0; x < width; x += 32) {
        hpfx3d_fill_rect(ctx, x, 0, 1, height, 0x0013212f, 0xcc);
    }

    /* 2. Master Instrument Header Bar */
    int bar_h = 44;
    hpfx3d_fill_rect(ctx, 0, 0, width, bar_h, 0x00141e28, 0xcc);
    hpfx3d_fill_rect(ctx, 0, bar_h - 1, width, 1, 0x0034495e, 0xcc);

    /* Master HP Corporate Emblem at (16, 9) */
    draw_hp_badge(ctx, 16, 9);

    /* Main Console Titles */
    draw_text(ctx, 56, 8, "HEWLETT-PACKARD 16700A LOGIC ANALYSIS SYSTEM & 54845A INFINIIUM OSCILLOSCOPE", 0x00ecf0f1, 2);
    draw_text(ctx, 58, 28, "TARGET: HP VISUALIZE FX10 HARDWARE VERIFICATION (AGP 4X / PCI 66MHz / LEGO 128-BIT 2D ENGINE)", 0x007f8c8d, 1);

    /* System Status Annunciators in Header */
    hpfx3d_fill_rect(ctx, width - 480, 8, 105, 28, 0x0027ae60, 0xcc);
    draw_text(ctx, width - 472, 17, "[TRIG'D RUN]", 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, width - 365, 8, 115, 28, 0x002980b9, 0xcc);
    draw_text(ctx, width - 357, 17, "TB: 10.0 ns/d", 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, width - 240, 8, 115, 28, 0x008e44ad, 0xcc);
    draw_text(ctx, width - 232, 17, "SR: 2.00 GSa/s", 0x00ffffff, 1);

    hpfx3d_fill_rect(ctx, width - 115, 8, 100, 28, 0x00d35400, 0xcc);
    draw_text(ctx, width - 107, 17, "2D ENGINE ON", 0x00ffffff, 1);

    /* 3. Primary Panels */
    /* Left Side: Oscilloscope (Top) & Logic Analyzer (Bottom) */
    render_oscilloscope(ctx, 16, 52, 1276, 550);
    render_logic_analyzer(ctx, 16, 612, 1276, 456);

    /* Right Side: Spectrum Analyzer (Top) & Hardware Register Profiler (Bottom) */
    render_spectrum_analyzer(ctx, 1302, 52, 602, 490);
    render_register_profiler(ctx, 1302, 552, 602, 516);

    /* 4. Hardware Screen-to-Screen BitBLT (hpfx3d_copy_area): */
    /* Stamp the master HP corporate medallion from (16, 9) into each sub-window title bar! */
    hpfx3d_copy_area(ctx, 16, 9, 16 + 5, 52 + 6, 26, 26, 0xcc);
    hpfx3d_copy_area(ctx, 16, 9, 16 + 5, 612 + 6, 26, 26, 0xcc);
    hpfx3d_copy_area(ctx, 16, 9, 1302 + 5, 52 + 6, 26, 26, 0xcc);
    hpfx3d_copy_area(ctx, 16, 9, 1302 + 5, 552 + 6, 26, 26, 0xcc);

    uint32_t crc = hpfx3d_checksum_framebuffer(ctx);

    printf("[Export] Writing 2D BitBLT compositing frame to: %s\n", out_ppm);
    save_frame_ppm(out_ppm, ctx);
    printf("[Checksum] IEEE 802.3 Framebuffer CRC32: 0x%08X\n", crc);

    hpfx3d_close(ctx);
    printf("Hardware 2D BitBLT demonstration completed successfully.\n");
    return 0;
}
