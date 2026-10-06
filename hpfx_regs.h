/*
 * HP Visualize FX5 / FX10 ("Lego" Architecture) Graphics Hardware Definitions
 * Reverse engineered from fx_w2k_118b (HP Windows 2000 Driver v1.18)
 *
 * (C) 2026 - Reverse engineered for Linux Kernel Driver support
 */

#ifndef _HPFX_REGS_H_
#define _HPFX_REGS_H_

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#ifndef __user
#define __user
#endif
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

/* PCI Identification */
#define PCI_VENDOR_ID_HP            0x103c
#define PCI_DEVICE_ID_HP_VISUALIZE  0x100a

#define PCI_SUBDEVICE_ID_HP_FX5     0x10d4
#define PCI_SUBDEVICE_ID_HP_FX10    0x10d5
#define PCI_SUBDEVICE_ID_HP_FX5_ALT 0x10d6
#define PCI_SUBDEVICE_ID_HP_FX10_ALT 0x10d7
#define PCI_SUBDEVICE_ID_HP_FX_GEN  0x10d8

/* PCI BAR Assignments */
#define HPFX_BAR_MMIO               0  /* BAR 0: MMIO & Control Registers (32 MB) */
#define HPFX_BAR_FB                 2  /* BAR 2: Linear Framebuffer Aperture (32 MB) */

#define HPFX_MMIO_SIZE              (32 * 1024 * 1024)
#define HPFX_FB_SIZE                (32 * 1024 * 1024)

/* PCI Configuration Registers */
#define HPFX_PCI_AGP_STATUS         0x44
#define HPFX_PCI_AGP_COMMAND        0x48

/* =========================================================================
 * BAR 0: Core Control & Status Registers (Offset 0x000000 .. 0x000500)
 * ========================================================================= */

/* Video Control Register (Offset 0x44) */
#define HPFX_REG_VIDEO_CTRL         0x000044
#define HPFX_VIDEO_CTRL_ENABLE      (1 << 9)   /* Bit 9: Display Timing / Video Enable */
#define HPFX_VIDEO_CTRL_UNBLANK     (1 << 14)  /* Bit 14: Screen Unblank */

/* DPMS / Sync Control Register (Offset 0x4c) */
#define HPFX_REG_DPMS_CTRL          0x00004c
#define HPFX_DPMS_ON                0x0000000c /* HSync ON, VSync ON (Normal Display) */
#define HPFX_DPMS_STANDBY           0x0000000d /* HSync OFF, VSync ON */
#define HPFX_DPMS_SUSPEND           0x0000000e /* HSync ON, VSync OFF */
#define HPFX_DPMS_OFF               0x0000000f /* HSync OFF, VSync OFF */
#define HPFX_DPMS_RESET             0x00000000 /* All Sync Disabled / Reset */

/* I2C / DDC Monitor Detection (Offsets 0x70, 0xd4) */
#define HPFX_REG_I2C_STATUS         0x000070
#define HPFX_REG_I2C_CTRL           0x0000d4
#define HPFX_I2C_SCL_OUT            (1 << 0)
#define HPFX_I2C_SDA_OUT            (1 << 1)
#define HPFX_I2C_SDA_IN             (1 << 4)
#define HPFX_I2C_SCL_IN             (1 << 5)

/* Soft Reset Register (Offset 0x100) */
#define HPFX_REG_RESET              0x000100
#define HPFX_RESET_TRIGGER          0x00000100 /* Write 0x100 to trigger reset */
#define HPFX_RESET_BUSY             0x00000100 /* Bit 8 is 1 while resetting */

/* Engine Busy / Status Register (Offset 0x400) */
#define HPFX_REG_STATUS             0x000400
#define HPFX_STATUS_BUSY            0x01000000 /* Bit 24: Engine Busy */

/* Hardware Capabilities (Offset 0x440) */
#define HPFX_REG_CAPS               0x000440

/* Sub-Engines Aperture Offsets inside BAR 0 */
#define HPFX_SUBENG_0               0x006a0000
#define HPFX_SUBENG_1               0x00800000
#define HPFX_SUBENG_2               0x00a41000
#define HPFX_SUBENG_3               0x00aa0000

/* =========================================================================
 * BAR 0: 2D Graphics Acceleration & FIFO (Offsets 0x600000 .. 0xb30000)
 * ========================================================================= */

/* Command FIFO Free Slot Register (Offset 0x641440) */
#define HPFX_REG_FIFO_STATUS        0x00641440
/* Free FIFO words = ((readl(reg) >> 1) - 10) */

/* 2D Raster Color Format Register (Offset 0x921110) */
#define HPFX_REG_PIXEL_FORMAT       0x00921110
#define HPFX_FMT_8BPP               (0x02000000 | 0x01)
#define HPFX_FMT_16BPP              (0x02000000 | 0x02)
#define HPFX_FMT_32BPP              (0x02000000 | 0x04)

/* 2D Drawing Registers */
#define HPFX_REG_FG_COLOR           0x00a00808
#define HPFX_REG_ROP                0x00a0080c
#define HPFX_REG_BG_COLOR           0x00a008a4
#define HPFX_REG_PLANE_MASK         0x00a008a8

/* Clipping Window */
#define HPFX_REG_CLIP_MIN           0x00ac1050 /* (Ymin << 16) | Xmin */
#define HPFX_REG_CLIP_MAX           0x00ac1054 /* (Ymax << 16) | Xmax */

/* 2D Coordinate & Trigger Registers */
#define HPFX_REG_COORD_START        0x00b20000 /* (Y0 << 16) | X0 */
#define HPFX_REG_COORD_TRIGGER      0x00b25400 /* (Y1 << 16) | X1 (triggers raster op) */

/* =========================================================================
 * BAR 0: 3D Hardware Processing & Geometry Pipeline Registers
 * ========================================================================= */

/* 3D Geometry Pipeline Mode & Control (Offset 0x800048) */
#define HPFX_REG_3D_PIPELINE_CTRL   0x00800048
#define HPFX_3D_MODE_ENABLE         0x60000000 /* Enable 3D hardware pipeline mode */
#define HPFX_3D_MODE_DISABLE_MASK   0x9fffffff /* Clear 3D mode (return to 2D) */

/* 3D Pipeline Sync & Completion Status (Offset 0x8000c8) */
#define HPFX_REG_3D_SYNC            0x008000c8
#define HPFX_3D_SYNC_BUSY           0x80000000 /* Bit 31: 3D Engine Busy */
#define HPFX_3D_SYNC_RESET_VAL      0x00ffffff /* Write to init sync counter */

/* 3D Pipeline Flush Trigger (Offset 0x8000cc) */
#define HPFX_REG_3D_FLUSH           0x008000cc
#define HPFX_3D_FLUSH_TRIGGER       0x00000001 /* Trigger pipeline flush */

/* 3D Raster State & Face Culling (Offset 0x8e5800) */
#define HPFX_REG_3D_RASTER_STATE    0x008e5800
#define HPFX_CULL_NONE              0x00000000
#define HPFX_CULL_FRONT             0x00000001
#define HPFX_CULL_BACK              0x00000002

/* Z-Buffer (Depth Buffer) Control Registers */
#define HPFX_REG_Z_FORMAT           0x00920404 /* Z-Buffer format & enable (0: off, 1: 24-bit Z) */
#define HPFX_REG_Z_STRIDE           0x00920808 /* Z-Buffer stride/pitch in bytes */
#define HPFX_REG_DEPTH_FUNC         0x0092083c /* Depth compare func (GL_LESS, etc.) */
#define HPFX_REG_DEPTH_MASK         0x0092084c /* Depth buffer write mask (1: enable, 0: disable) */
#define HPFX_REG_CLEAR_DEPTH        0x009208a4 /* Fast Z-Clear depth value (e.g. 0x00ffffff) */
#define HPFX_REG_STENCIL_CTRL       0x009208ac /* Stencil test & operation */

/* Depth Comparison Functions */
#define HPFX_DEPTH_NEVER            0
#define HPFX_DEPTH_LESS             1
#define HPFX_DEPTH_EQUAL            2
#define HPFX_DEPTH_LEQUAL           3
#define HPFX_DEPTH_GREATER          4
#define HPFX_DEPTH_NOTEQUAL         5
#define HPFX_DEPTH_GEQUAL           6
#define HPFX_DEPTH_ALWAYS           7

/* Alpha Blending, Fog & Texture Unit Control */
#define HPFX_REG_BLEND_MODE         0x00921120 /* Blend mode / alpha blend enable */
#define HPFX_REG_FOG_CTRL           0x00921124 /* Fog enable & fog color */
#define HPFX_REG_TEX_BASE           0x00921180 /* Texture base offset in VRAM */
#define HPFX_REG_TEX_CTRL           0x00921184 /* Texture unit enable & filtering */
#define HPFX_REG_TEX_FORMAT         0x00921188 /* Texture format (0: RGB565, 1: RGBA8888) */
#define HPFX_REG_TEX_PITCH          0x0092118c /* Texture row pitch/stride in bytes */
#define HPFX_REG_3D_CLIP            0x00ac0858 /* 3D Viewport / Scissor Rect */

/* 3D Triangle Vertex Coordinate & Rasterization Triggers */
#define HPFX_REG_TRI_V0             0x00b20000 /* Vertex 0: (Y0 << 16) | X0 */
#define HPFX_REG_TRI_V1             0x00b24c00 /* Vertex 1: (Y1 << 16) | X1 */
#define HPFX_REG_TRI_V2_TRIGGER     0x00b3c010 /* Vertex 2: (Y2 << 16) | X2 (Triggers 3D Rasterize) */

/* FP14 Lego Rasterizer Alternate Offsets */
#define HPFX_REG_FP14_V0            0x00cb0008 /* FP14 Vertex 0 coordinate */
#define HPFX_REG_FP14_V1            0x00cb0010 /* FP14 Vertex 1 coordinate */
#define HPFX_REG_FP14_V2            0x00cb0020 /* FP14 Vertex 2 coordinate */
#define HPFX_REG_FP14_PRIM_MODE     0x00e00120 /* 2 = Triangle primitive */
#define HPFX_REG_FP14_RASTER_SETUP  0x00e0040c /* Rasterizer setup & interpolation */

/* DMA Command Stream Packet Format */
#define HPFX_DMA_CMD_REG_WRITE      0x08000000 /* DMA Opcode: (0x08000000 | RegOffset) */

/* =========================================================================
 * 3D IOCTL Interfaces (for user-space acceleration via /dev/fb0)
 * ========================================================================= */

struct hpfx_triangle_cmd {
	int x0, y0;
	int x1, y1;
	int x2, y2;
	u32 color;      /* 32-bit RGBA / BGRX color */
	u32 z0, z1, z2; /* Fixed-point 24-bit depth values (0 .. 0x00ffffff) */
};

struct hpfx_3d_state_cmd {
	u32 depth_enable; /* 0: Disable, 1: Enable */
	u32 depth_func;   /* HPFX_DEPTH_* (LESS, LEQUAL, etc.) */
	u32 depth_mask;   /* 0: Read-only, 1: Write enabled */
	u32 cull_mode;    /* HPFX_CULL_* (NONE, FRONT, BACK) */
	u32 blend_mode;   /* 0: Replace, 1: Alpha blend */
	u32 shade_model;  /* 0: Flat, 1: Smooth/Gouraud */
};

struct hpfx_tri_list_cmd {
	u32 count;        /* Number of triangles in the buffer (max 4096 per batch) */
	struct hpfx_triangle_cmd __user *triangles; /* Pointer to user triangle array */
};

struct hpfx_tex_upload_cmd {
	u32 width;        /* Texture width in pixels */
	u32 height;       /* Texture height in pixels */
	u32 format;       /* 0: RGB565 (2 bytes/px), 1: RGBA8888 (4 bytes/px) */
	u32 vram_offset;  /* Byte offset within VRAM texture staging region */
	const void __user *data; /* Userspace pointer to texel pixel array */
};

#define HPFX_IOCTL_MAGIC            'H'
#define HPFX_IOCTL_3D_RESET         _IO(HPFX_IOCTL_MAGIC, 0x20)
#define HPFX_IOCTL_3D_SYNC          _IO(HPFX_IOCTL_MAGIC, 0x21)
#define HPFX_IOCTL_3D_DRAW_TRIANGLE _IOW(HPFX_IOCTL_MAGIC, 0x22, struct hpfx_triangle_cmd)
#define HPFX_IOCTL_CLEAR_DEPTH      _IOW(HPFX_IOCTL_MAGIC, 0x23, u32)
#define HPFX_IOCTL_SET_3D_STATE     _IOW(HPFX_IOCTL_MAGIC, 0x24, struct hpfx_3d_state_cmd)
#define HPFX_IOCTL_3D_DRAW_TRI_LIST _IOW(HPFX_IOCTL_MAGIC, 0x25, struct hpfx_tri_list_cmd)
#define HPFX_IOCTL_UPLOAD_TEXTURE   _IOW(HPFX_IOCTL_MAGIC, 0x26, struct hpfx_tex_upload_cmd)

/* =========================================================================
 * Video Timing Structures & Mode Definitions
 * ========================================================================= */

struct hpfx_timing_entry {
	u32 dw0;       /* Packed Horizontal / Vertical Timings */
	u32 dw1;       /* PLL / Clock Multiplier & Divider */
	u32 dw2;       /* Sync & Active Polarity Flags */
	u32 dw3;       /* Blank Timings */
	u32 dw4;       /* Front Porch / Border */
	u32 dw5;       /* Reserved / Extension */
};

struct hpfx_mode_info {
	u16 width;
	u16 height;
	u16 pitch;
	u16 pitch_alt;
	u16 flags;
	u16 bpp;
	u8  refresh_rates[4]; /* In Hz: e.g. 60, 75, 85, 120 */
	u8  timing_indices[4];
};

#endif /* _HPFX_REGS_H_ */
