/*
 * Linux Framebuffer Driver for HP Visualize FX5 & FX10 ("Lego" Architecture)
 * Fully reverse engineered from HP Windows 2000 Driver (fx_w2k_118b)
 *
 * Supports PCI / AGP:
 *   Vendor ID: 0x103c (Hewlett-Packard)
 *   Device ID: 0x100a (Visualize FX Series)
 *
 * Includes 2D and 3D hardware acceleration functions and ioctls.
 * Compatible with Linux kernels 4.x, 5.x, and 6.x on x86/x86_64
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include "hpfx_regs.h"

#define DRIVER_NAME "hpfx_fb"
#define DRIVER_DESC "HP Visualize FX5/FX10 Framebuffer Driver with 3D Acceleration"

/* Module Parameters */
static char *mode_option = "1024x768-32@60";
module_param(mode_option, charp, 0444);
MODULE_PARM_DESC(mode_option, "Default video mode (e.g. 1024x768-32@60, 1280x1024-32@60)");

static bool hw_accel = true;
module_param(hw_accel, bool, 0444);
MODULE_PARM_DESC(hw_accel, "Enable 2D and 3D hardware acceleration (default: true)");

/* Extracted Video Timing Table from fx_w2k_118b (20 entries) */
static const struct hpfx_timing_entry hpfx_timings[20] = {
	/*  0: 640x480 @ 60Hz */
	{ 0x02f17c0f, 0x00012001, 0x00200109, 0x00002c23, 0x00000403, 0 },
	/*  1: 640x480 @ 75Hz */
	{ 0x0770fc0f, 0x00012001, 0x000f0200, 0x00000d08, 0x00000d08, 0 },
	/*  2: 640x480 @ 85Hz */
	{ 0x04f0dc37, 0x00012001, 0x00180200, 0x00002f1a, 0x00000f08, 0 },
	/*  3: 640x480 @ 120Hz */
	{ 0x02f17c0f, 0x0001202b, 0x00180200, 0x00006228, 0x00006228, 0 },
	/*  4: 800x600 @ 60Hz */
	{ 0x0571fc27, 0x00012001, 0x00160300, 0x00005028, 0x00005028, 0 },
	/*  5: 800x600 @ 75Hz */
	{ 0x09f13c0f, 0x00012001, 0x00140200, 0x00001508, 0x00001508, 0 },
	/*  6: 800x600 @ 85Hz */
	{ 0x0970fc1f, 0x00012001, 0x001a0200, 0x00003111, 0x00001808, 0 },
	/*  7: 800x600 @ 120Hz */
	{ 0x0571fc27, 0x0001204d, 0x001a0200, 0x00004e13, 0x00004e13, 0 },
	/*  8: 1024x768 @ 60Hz */
	{ 0x09f21c17, 0x00012001, 0x001c0502, 0x00002f0e, 0x00001c08, 0 },
	/*  9: 1024x768 @ 75Hz */
	{ 0x0af17c0f, 0x00012028, 0x001b0200, 0x00002208, 0x00002208, 0 },
	/* 10: 1024x768 @ 85Hz */
	{ 0x0cf17c2f, 0x00012029, 0x00230200, 0x0000370b, 0x00001b05, 0 },
	/* 11: 1024x768 @ 120Hz */
	{ 0x09f21c17, 0x000120dd, 0x00230200, 0x00002c06, 0x00003908, 0 },
	/* 12: 1280x1024 @ 60Hz */
	{ 0x0f71bc2f, 0x0001202a, 0x00250200, 0x00003f0b, 0x00001f05, 0 },
	/* 13: 1280x1024 @ 75Hz */
	{ 0x0f723c0f, 0x0001209a, 0x00250200, 0x00004f0b, 0x00002705, 0 },
	/* 14: 1280x1024 @ 85Hz */
	{ 0x0df27c3f, 0x000120d0, 0x002b0200, 0x00004508, 0x00004508, 0 },
	/* 15: 1600x1024 @ 76Hz */
	{ 0x12f27c1f, 0x000120d6, 0x00270202, 0x00003a06, 0x00003a06, 0 },
	/* 16: 1600x1200 @ 60Hz */
	{ 0x12f2fc3f, 0x00012073, 0x002d0200, 0x00003f07, 0x00001f03, 0 },
	/* 17: 1600x1200 @ 75Hz */
	{ 0x12f2fc3f, 0x0001211b, 0x002d0200, 0x00004f07, 0x00002703, 0 },
	/* 18: 1920x1080 @ 60Hz */
	{ 0x10f23c1f, 0x000120da, 0x00260202, 0x00004608, 0x00004608, 0 },
	/* 19: 1920x1200 @ 76Hz */
	{ 0x14d3fc3f, 0x00012170, 0x002b0202, 0x00006007, 0x00006007, 0 }
};

/* Driver private structure */
struct hpfx_par {
	struct pci_dev *pdev;
	void __iomem *mmio_base;      /* BAR 0 mapped */
	void __iomem *fb_base;        /* BAR 2 mapped */
	resource_size_t mmio_start;
	resource_size_t mmio_len;
	resource_size_t fb_start;
	resource_size_t fb_len;

	u32 pseudo_palette[16];
	int current_timing_idx;

	/* 3D Engine State */
	bool in_3d_mode;
	bool depth_test_enabled;
	u32  depth_func;
	u32  cull_mode;
};

/* Register Access Helpers */
static inline u32 hpfx_read32(struct hpfx_par *par, u32 reg)
{
	return readl(par->mmio_base + reg);
}

static inline void hpfx_write32(struct hpfx_par *par, u32 reg, u32 val)
{
	writel(val, par->mmio_base + reg);
}

/* Wait for Command FIFO space */
static int hpfx_wait_fifo(struct hpfx_par *par, u32 entries)
{
	int timeout = 100000;
	while (timeout--) {
		u32 status = hpfx_read32(par, HPFX_REG_FIFO_STATUS);
		u32 free_slots = (status >> 1) > 10 ? ((status >> 1) - 10) : 0;
		if (free_slots >= entries)
			return 0;
		udelay(1);
	}
	return -EBUSY;
}

/* Wait for Engine Idle */
static int hpfx_wait_idle(struct hpfx_par *par)
{
	int timeout = 500000;
	while (timeout--) {
		if (!(hpfx_read32(par, HPFX_REG_STATUS) & HPFX_STATUS_BUSY))
			return 0;
		udelay(2);
	}
	return -EBUSY;
}

/* =========================================================================
 * 3D Hardware Processing Functions
 * ========================================================================= */

/* Synchronize 3D Pipeline and wait for completion */
static int hpfx_3d_sync(struct hpfx_par *par)
{
	int timeout = 500000;

	/* Trigger flush */
	hpfx_write32(par, HPFX_REG_3D_FLUSH, HPFX_3D_FLUSH_TRIGGER);

	/* Wait until 3D engine busy bit 31 clears */
	while (timeout--) {
		if (!(hpfx_read32(par, HPFX_REG_3D_SYNC) & HPFX_3D_SYNC_BUSY))
			return 0;
		udelay(2);
	}

	pr_warn(DRIVER_NAME ": 3D pipeline sync timed out\n");
	return -EBUSY;
}

/* Initialize 3D Hardware Engine & Z-Buffer */
static int hpfx_3d_init(struct hpfx_par *par, u32 width, u32 height)
{
	pr_info(DRIVER_NAME ": Initializing 3D geometry & rasterizer pipeline...\n");

	/* Enable 3D Hardware Pipeline Mode */
	hpfx_write32(par, HPFX_REG_3D_PIPELINE_CTRL,
		     hpfx_read32(par, HPFX_REG_3D_PIPELINE_CTRL) | HPFX_3D_MODE_ENABLE);

	/* Configure Z-Buffer (Depth Buffer) */
	hpfx_write32(par, HPFX_REG_Z_FORMAT, 0x00000001);          /* 24-bit Z-Buffer enable */
	hpfx_write32(par, HPFX_REG_Z_STRIDE, width * 4);           /* Z-stride in bytes */
	hpfx_write32(par, HPFX_REG_DEPTH_FUNC, HPFX_DEPTH_LESS);   /* Default: GL_LESS */
	hpfx_write32(par, HPFX_REG_DEPTH_MASK, 0x00000001);        /* Z-write enabled */
	hpfx_write32(par, HPFX_REG_CLEAR_DEPTH, 0x00ffffff);       /* Clear depth maximum */

	/* Setup Viewport & Clip Box */
	hpfx_write32(par, HPFX_REG_CLIP_MIN, 0);
	hpfx_write32(par, HPFX_REG_CLIP_MAX, ((height - 1) << 16) | (width - 1));
	hpfx_write32(par, HPFX_REG_3D_CLIP, ((height - 1) << 16) | (width - 1));

	/* Default Face Culling (None) */
	hpfx_write32(par, HPFX_REG_3D_RASTER_STATE, HPFX_CULL_NONE);

	/* Flush and sync */
	hpfx_3d_sync(par);

	par->in_3d_mode = true;
	par->depth_test_enabled = true;
	par->depth_func = HPFX_DEPTH_LESS;
	par->cull_mode = HPFX_CULL_NONE;

	pr_info(DRIVER_NAME ": 3D hardware pipeline ready\n");
	return 0;
}

/* Set 3D State: Depth Test, Culling, Blending */
static void hpfx_3d_set_state(struct hpfx_par *par, bool depth_enable, u32 depth_func, u32 cull_mode)
{
	if (depth_enable) {
		hpfx_write32(par, HPFX_REG_Z_FORMAT, 0x00000001);
		hpfx_write32(par, HPFX_REG_DEPTH_FUNC, depth_func);
		hpfx_write32(par, HPFX_REG_DEPTH_MASK, 0x00000001);
	} else {
		hpfx_write32(par, HPFX_REG_Z_FORMAT, 0x00000000);
		hpfx_write32(par, HPFX_REG_DEPTH_MASK, 0x00000000);
	}

	hpfx_write32(par, HPFX_REG_3D_RASTER_STATE, cull_mode);

	par->depth_test_enabled = depth_enable;
	par->depth_func = depth_func;
	par->cull_mode = cull_mode;
}

/* Draw Hardware-Accelerated 3D Triangle */
static int hpfx_3d_draw_triangle(struct hpfx_par *par, struct hpfx_triangle_cmd *cmd)
{
	if (hpfx_wait_fifo(par, 12) < 0)
		return -EBUSY;

	/* Ensure 3D pipeline mode is active */
	if (!par->in_3d_mode) {
		hpfx_write32(par, HPFX_REG_3D_PIPELINE_CTRL,
			     hpfx_read32(par, HPFX_REG_3D_PIPELINE_CTRL) | HPFX_3D_MODE_ENABLE);
		par->in_3d_mode = true;
	}

	/* Set Primitive Color and Plane Write Mask */
	hpfx_write32(par, HPFX_REG_FG_COLOR, cmd->color);
	hpfx_write32(par, HPFX_REG_PLANE_MASK, 0xffffffff);
	hpfx_write32(par, HPFX_REG_ROP, 0xcc); /* SRCCOPY */

	/* Setup Vertex 0: (Y0 << 16) | X0 */
	hpfx_write32(par, HPFX_REG_TRI_V0, ((cmd->y0 & 0xffff) << 16) | (cmd->x0 & 0xffff));

	/* Setup Vertex 1: (Y1 << 16) | X1 */
	hpfx_write32(par, HPFX_REG_TRI_V1, ((cmd->y1 & 0xffff) << 16) | (cmd->x1 & 0xffff));

	/* Setup Vertex 2 & Trigger Hardware Rasterization: (Y2 << 16) | X2 */
	hpfx_write32(par, HPFX_REG_TRI_V2_TRIGGER, ((cmd->y2 & 0xffff) << 16) | (cmd->x2 & 0xffff));

	return 0;
}

/* =========================================================================
 * Core Reset & Initialization
 * ========================================================================= */

static int hpfx_hw_reset(struct hpfx_par *par)
{
	int timeout = 100000;

	pr_info(DRIVER_NAME ": Initiating hardware engine reset...\n");

	/* Trigger soft reset */
	hpfx_write32(par, HPFX_REG_RESET, HPFX_RESET_TRIGGER);
	while (timeout--) {
		if (!(hpfx_read32(par, HPFX_REG_RESET) & HPFX_RESET_BUSY))
			break;
		udelay(10);
	}

	if (timeout <= 0) {
		pr_warn(DRIVER_NAME ": Soft reset timed out, proceeding anyway\n");
	}

	/* Wait until engine reports idle */
	hpfx_wait_idle(par);

	/* Initialize internal sub-engine blocks */
	hpfx_write32(par, HPFX_SUBENG_0 + 0x600, 0x01010101);
	hpfx_write32(par, HPFX_SUBENG_0 + 0xe00, 0x01010101);

	/* Enable video output and power */
	hpfx_write32(par, HPFX_REG_VIDEO_CTRL,
		     hpfx_read32(par, HPFX_REG_VIDEO_CTRL) | HPFX_VIDEO_CTRL_ENABLE | HPFX_VIDEO_CTRL_UNBLANK);

	/* Turn on DPMS sync signals (Normal On) */
	hpfx_write32(par, HPFX_REG_DPMS_CTRL, HPFX_DPMS_ON);

	pr_info(DRIVER_NAME ": Hardware initialization complete (Caps: 0x%08x)\n",
		hpfx_read32(par, HPFX_REG_CAPS));
	return 0;
}

/* Set DPMS display power state */
static int hpfx_fb_blank(int blank, struct fb_info *info)
{
	struct hpfx_par *par = info->par;
	u32 dpms_val;
	u32 video_ctrl = hpfx_read32(par, HPFX_REG_VIDEO_CTRL);

	switch (blank) {
	case FB_BLANK_UNBLANK:
		dpms_val = HPFX_DPMS_ON;
		video_ctrl |= HPFX_VIDEO_CTRL_UNBLANK;
		break;
	case FB_BLANK_NORMAL:
		dpms_val = HPFX_DPMS_ON;
		video_ctrl &= ~HPFX_VIDEO_CTRL_UNBLANK;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		dpms_val = HPFX_DPMS_STANDBY;
		video_ctrl &= ~HPFX_VIDEO_CTRL_UNBLANK;
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		dpms_val = HPFX_DPMS_SUSPEND;
		video_ctrl &= ~HPFX_VIDEO_CTRL_UNBLANK;
		break;
	case FB_BLANK_POWERDOWN:
	default:
		dpms_val = HPFX_DPMS_OFF;
		video_ctrl &= ~HPFX_VIDEO_CTRL_UNBLANK;
		break;
	}

	hpfx_write32(par, HPFX_REG_DPMS_CTRL, dpms_val);
	hpfx_write32(par, HPFX_REG_VIDEO_CTRL, video_ctrl);
	return 0;
}

/* Find timing entry matching resolution and refresh rate */
static int hpfx_find_timing_index(u32 xres, u32 yres, u32 refresh)
{
	if (xres == 640 && yres == 480) {
		if (refresh >= 115) return 3;
		if (refresh >= 80)  return 2;
		if (refresh >= 70)  return 1;
		return 0;
	}
	if (xres == 800 && yres == 600) {
		if (refresh >= 115) return 7;
		if (refresh >= 80)  return 6;
		if (refresh >= 70)  return 5;
		return 4;
	}
	if (xres == 1024 && yres == 768) {
		if (refresh >= 115) return 11;
		if (refresh >= 80)  return 10;
		if (refresh >= 70)  return 9;
		return 8;
	}
	if (xres == 1280 && yres == 1024) {
		if (refresh >= 80)  return 14;
		if (refresh >= 70)  return 13;
		return 12;
	}
	if (xres == 1600 && yres == 1024)
		return 15;
	if (xres == 1600 && yres == 1200) {
		if (refresh >= 70)  return 17;
		return 16;
	}
	if (xres == 1920 && yres == 1080)
		return 18;
	if (xres == 1920 && yres == 1200)
		return 19;

	return 8;
}

/* Validate and check display mode */
static int hpfx_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	if (var->xres < 640 || var->yres < 480) {
		var->xres = 640;
		var->yres = 480;
	}

	if (var->xres > 1920 || var->yres > 1200) {
		var->xres = 1920;
		var->yres = 1200;
	}

	if (var->bits_per_pixel <= 8) {
		var->bits_per_pixel = 8;
		var->red.offset = 0;   var->red.length = 8;
		var->green.offset = 0; var->green.length = 8;
		var->blue.offset = 0;  var->blue.length = 8;
		var->transp.offset = 0; var->transp.length = 0;
	} else if (var->bits_per_pixel <= 16) {
		var->bits_per_pixel = 16;
		var->red.offset = 11;  var->red.length = 5;
		var->green.offset = 5; var->green.length = 6;
		var->blue.offset = 0;  var->blue.length = 5;
		var->transp.offset = 0; var->transp.length = 0;
	} else {
		var->bits_per_pixel = 32;
		var->red.offset = 16;  var->red.length = 8;
		var->green.offset = 8; var->green.length = 8;
		var->blue.offset = 0;  var->blue.length = 8;
		var->transp.offset = 24; var->transp.length = 8;
	}

	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres;
	var->xoffset = 0;
	var->yoffset = 0;

	return 0;
}

/* Apply video mode to hardware */
static int hpfx_fb_set_par(struct fb_info *info)
{
	struct hpfx_par *par = info->par;
	u32 refresh = 60;
	int t_idx;

	info->fix.line_length = info->var.xres * (info->var.bits_per_pixel / 8);
	info->fix.visual = (info->var.bits_per_pixel == 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;

	/* Determine target timing index */
	t_idx = hpfx_find_timing_index(info->var.xres, info->var.yres, refresh);
	par->current_timing_idx = t_idx;

	pr_info(DRIVER_NAME ": Setting mode %dx%d-%dbpp (timing index %d)\n",
		info->var.xres, info->var.yres, info->var.bits_per_pixel, t_idx);

	/* Set 2D pixel format in hardware */
	if (info->var.bits_per_pixel == 16)
		hpfx_write32(par, HPFX_REG_PIXEL_FORMAT, HPFX_FMT_16BPP);
	else if (info->var.bits_per_pixel == 32)
		hpfx_write32(par, HPFX_REG_PIXEL_FORMAT, HPFX_FMT_32BPP);
	else
		hpfx_write32(par, HPFX_REG_PIXEL_FORMAT, HPFX_FMT_8BPP);

	/* Initialize 3D subsystem for current resolution */
	hpfx_3d_init(par, info->var.xres, info->var.yres);

	return 0;
}

/* Color Palette Setup */
static int hpfx_fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			     u_int transp, struct fb_info *info)
{
	if (regno >= 16)
		return -EINVAL;

	if (info->var.grayscale) {
		red = green = blue = (19595 * red + 38470 * green + 7471 * blue) >> 16;
	}

	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 *pal = info->pseudo_palette;
		u32 v = 0;

		if (info->var.bits_per_pixel == 16) {
			v = ((red >> 11) << 11) |
			    ((green >> 10) << 5) |
			    (blue >> 11);
		} else if (info->var.bits_per_pixel == 32) {
			v = ((red >> 8) << 16) |
			    ((green >> 8) << 8) |
			    (blue >> 8);
		}
		pal[regno] = v;
	}

	return 0;
}

/* Hardware-Accelerated 2D Rectangle Fill */
static void hpfx_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct hpfx_par *par = info->par;

	if (!hw_accel) {
		cfb_fillrect(info, rect);
		return;
	}

	if (hpfx_wait_fifo(par, 8) < 0) {
		cfb_fillrect(info, rect);
		return;
	}

	/* Disable 3D mode for 2D blit */
	if (par->in_3d_mode) {
		hpfx_write32(par, HPFX_REG_3D_PIPELINE_CTRL,
			     hpfx_read32(par, HPFX_REG_3D_PIPELINE_CTRL) & HPFX_3D_MODE_DISABLE_MASK);
		par->in_3d_mode = false;
	}

	hpfx_write32(par, HPFX_REG_FG_COLOR, rect->color);
	hpfx_write32(par, HPFX_REG_ROP, 0xcc); /* SRCCOPY */

	hpfx_write32(par, HPFX_REG_COORD_START, (rect->dy << 16) | rect->dx);
	hpfx_write32(par, HPFX_REG_COORD_TRIGGER,
		     ((rect->dy + rect->height - 1) << 16) | (rect->dx + rect->width - 1));
}

/* Hardware-Accelerated 2D Copy Area */
static void hpfx_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	struct hpfx_par *par = info->par;

	if (!hw_accel) {
		cfb_copyarea(info, area);
		return;
	}

	if (hpfx_wait_fifo(par, 8) < 0) {
		cfb_copyarea(info, area);
		return;
	}

	if (par->in_3d_mode) {
		hpfx_write32(par, HPFX_REG_3D_PIPELINE_CTRL,
			     hpfx_read32(par, HPFX_REG_3D_PIPELINE_CTRL) & HPFX_3D_MODE_DISABLE_MASK);
		par->in_3d_mode = false;
	}

	hpfx_write32(par, HPFX_REG_ROP, 0xcc); /* SRCCOPY */
	hpfx_write32(par, HPFX_REG_COORD_START, (area->sy << 16) | area->sx);
	hpfx_write32(par, HPFX_REG_COORD_TRIGGER,
		     ((area->dy + area->height - 1) << 16) | (area->dx + area->width - 1));
}

/* 3D IOCTL Dispatch Handler */
static int hpfx_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct hpfx_par *par = info->par;
	struct hpfx_triangle_cmd tri;
	u32 depth_val;
	int ret = 0;

	switch (cmd) {
	case HPFX_IOCTL_3D_RESET:
		ret = hpfx_hw_reset(par);
		if (!ret)
			ret = hpfx_3d_init(par, info->var.xres, info->var.yres);
		break;

	case HPFX_IOCTL_3D_SYNC:
		ret = hpfx_3d_sync(par);
		break;

	case HPFX_IOCTL_3D_DRAW_TRIANGLE:
		if (copy_from_user(&tri, (void __user *)arg, sizeof(tri)))
			return -EFAULT;
		ret = hpfx_3d_draw_triangle(par, &tri);
		break;

	case HPFX_IOCTL_CLEAR_DEPTH:
		if (copy_from_user(&depth_val, (void __user *)arg, sizeof(depth_val)))
			return -EFAULT;
		hpfx_write32(par, HPFX_REG_CLEAR_DEPTH, depth_val);
		hpfx_3d_sync(par);
		break;

	case HPFX_IOCTL_SET_3D_STATE: {
		struct hpfx_3d_state_cmd state;
		if (copy_from_user(&state, (void __user *)arg, sizeof(state)))
			return -EFAULT;
		hpfx_3d_set_state(par, state.depth_enable, state.depth_func, state.cull_mode);
		if (state.depth_mask)
			hpfx_write32(par, HPFX_REG_DEPTH_MASK, 1);
		else
			hpfx_write32(par, HPFX_REG_DEPTH_MASK, 0);
		if (state.blend_mode)
			hpfx_write32(par, HPFX_REG_BLEND_MODE, 0x00000001);
		else
			hpfx_write32(par, HPFX_REG_BLEND_MODE, 0x00000000);
		break;
	}

	case HPFX_IOCTL_3D_DRAW_TRI_LIST: {
		struct hpfx_tri_list_cmd list;
		struct hpfx_triangle_cmd batch[64];
		u32 remaining, chunk, i;
		struct hpfx_triangle_cmd __user *uptr;

		if (copy_from_user(&list, (void __user *)arg, sizeof(list)))
			return -EFAULT;
		if (list.count > 16384)
			return -EINVAL;

		remaining = list.count;
		uptr = list.triangles;
		while (remaining > 0) {
			chunk = remaining > 64 ? 64 : remaining;
			if (copy_from_user(batch, uptr, chunk * sizeof(struct hpfx_triangle_cmd)))
				return -EFAULT;
			for (i = 0; i < chunk; i++) {
				ret = hpfx_3d_draw_triangle(par, &batch[i]);
				if (ret)
					return ret;
			}
			remaining -= chunk;
			uptr += chunk;
		}
		break;
	}

	case HPFX_IOCTL_UPLOAD_TEXTURE: {
		struct hpfx_tex_upload_cmd tex;
		u32 bpp, size;

		if (copy_from_user(&tex, (void __user *)arg, sizeof(tex)))
			return -EFAULT;
		if (tex.width == 0 || tex.height == 0 || tex.width > 2048 || tex.height > 2048)
			return -EINVAL;
		if (tex.format > 1)
			return -EINVAL;
		if (!tex.data)
			return -EINVAL;

		bpp = (tex.format == 0) ? 2 : 4;
		size = tex.width * tex.height * bpp;

		/* Verify VRAM bounds (must fit within total VRAM capacity) */
		if (tex.vram_offset + size > par->vram_len)
			return -EINVAL;

		if (copy_from_user((u8 *)info->screen_base + tex.vram_offset, tex.data, size))
			return -EFAULT;

		/* Program hardware texture staging registers */
		hpfx_write32(par, HPFX_REG_TEX_BASE, tex.vram_offset);
		hpfx_write32(par, HPFX_REG_TEX_FORMAT, tex.format);
		hpfx_write32(par, HPFX_REG_TEX_PITCH, tex.width * bpp);
		hpfx_write32(par, HPFX_REG_TEX_CTRL, 0x00000001); /* Enable texture engine */
		break;
	}

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

/* Framebuffer Operations */
static const struct fb_ops hpfx_fb_ops = {
	.owner          = THIS_MODULE,
	.fb_check_var   = hpfx_fb_check_var,
	.fb_set_par     = hpfx_fb_set_par,
	.fb_setcolreg   = hpfx_fb_setcolreg,
	.fb_blank       = hpfx_fb_blank,
	.fb_fillrect    = hpfx_fb_fillrect,
	.fb_copyarea    = hpfx_fb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_ioctl       = hpfx_fb_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = hpfx_fb_ioctl,
#endif
};

/* PCI Driver Probe */
static int hpfx_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct fb_info *info;
	struct hpfx_par *par;
	int ret;

	pr_info(DRIVER_NAME ": Found HP Visualize FX device [%04x:%04x] Subsystem [%04x:%04x]\n",
		pdev->vendor, pdev->device, pdev->subsystem_vendor, pdev->subsystem_device);

	ret = pci_enable_device(pdev);
	if (ret) {
		pr_err(DRIVER_NAME ": Failed to enable PCI device\n");
		return ret;
	}

	ret = pci_request_regions(pdev, DRIVER_NAME);
	if (ret) {
		pr_err(DRIVER_NAME ": Cannot request PCI regions\n");
		goto err_disable;
	}

	pci_set_master(pdev);

	info = framebuffer_alloc(sizeof(struct hpfx_par), &pdev->dev);
	if (!info) {
		ret = -ENOMEM;
		goto err_release_regions;
	}

	par = info->par;
	par->pdev = pdev;

	/* Resource inspection */
	par->mmio_start = pci_resource_start(pdev, HPFX_BAR_MMIO);
	par->mmio_len   = pci_resource_len(pdev, HPFX_BAR_MMIO);
	par->fb_start   = pci_resource_start(pdev, HPFX_BAR_FB);
	par->fb_len     = pci_resource_len(pdev, HPFX_BAR_FB);

	pr_info(DRIVER_NAME ": MMIO (BAR 0): 0x%pa (len 0x%zx)\n", &par->mmio_start, (size_t)par->mmio_len);
	pr_info(DRIVER_NAME ": FB   (BAR 2): 0x%pa (len 0x%zx)\n", &par->fb_start, (size_t)par->fb_len);

	/* Map MMIO registers */
	par->mmio_base = pci_iomap(pdev, HPFX_BAR_MMIO, 0);
	if (!par->mmio_base) {
		pr_err(DRIVER_NAME ": Failed to map MMIO registers\n");
		ret = -ENOMEM;
		goto err_free_fb;
	}

	/* Map Framebuffer with Write-Combining for maximum throughput */
	par->fb_base = ioremap_wc(par->fb_start, par->fb_len);
	if (!par->fb_base) {
		pr_err(DRIVER_NAME ": Failed to map Framebuffer\n");
		ret = -ENOMEM;
		goto err_unmap_mmio;
	}

	/* Reset and initialize hardware */
	hpfx_hw_reset(par);

	/* Clear framebuffer memory (black screen) */
	memset_io(par->fb_base, 0, par->fb_len > 0x1000000 ? 0x1000000 : par->fb_len);

	/* Setup fb_info */
	info->fbops = &hpfx_fb_ops;
	info->pseudo_palette = par->pseudo_palette;
	info->screen_base = par->fb_base;
	info->screen_size = par->fb_len;

	/* Fix information */
	strscpy(info->fix.id, "HP VisualizeFX", sizeof(info->fix.id));
	info->fix.smem_start = par->fb_start;
	info->fix.smem_len = par->fb_len;
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.mmio_start = par->mmio_start;
	info->fix.mmio_len = par->mmio_len;

	/* Default Var settings */
	info->var.xres = 1024;
	info->var.yres = 768;
	info->var.bits_per_pixel = 32;
	info->var.activate = FB_ACTIVATE_NOW;

	hpfx_fb_check_var(&info->var, info);
	hpfx_fb_set_par(info);

	pci_set_drvdata(pdev, info);

	ret = register_framebuffer(info);
	if (ret < 0) {
		pr_err(DRIVER_NAME ": Failed to register framebuffer\n");
		goto err_unmap_fb;
	}

	fb_info(info, "%s frame buffer device at 0x%pa with 3D support\n", info->fix.id, &par->fb_start);
	return 0;

err_unmap_fb:
	iounmap(par->fb_base);
err_unmap_mmio:
	pci_iounmap(pdev, par->mmio_base);
err_free_fb:
	framebuffer_release(info);
err_release_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

/* PCI Driver Remove */
static void hpfx_pci_remove(struct pci_dev *pdev)
{
	struct fb_info *info = pci_get_drvdata(pdev);

	if (info) {
		struct hpfx_par *par = info->par;

		unregister_framebuffer(info);
		if (par->fb_base)
			iounmap(par->fb_base);
		if (par->mmio_base)
			pci_iounmap(pdev, par->mmio_base);
		framebuffer_release(info);
	}

	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pr_info(DRIVER_NAME ": Driver unloaded\n");
}

/* PCI ID Table */
static const struct pci_device_id hpfx_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_VISUALIZE), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, hpfx_pci_tbl);

static struct pci_driver hpfx_pci_driver = {
	.name       = DRIVER_NAME,
	.id_table   = hpfx_pci_tbl,
	.probe      = hpfx_pci_probe,
	.remove     = hpfx_pci_remove,
};

module_pci_driver(hpfx_pci_driver);

MODULE_AUTHOR("Reverse Engineered from HP fx_w2k_118b");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
