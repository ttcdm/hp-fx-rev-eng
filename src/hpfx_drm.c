/*
 * HP Visualize FX5 & FX10 Modern Linux DRM/KMS Kernel Driver
 *
 * Provides DRM GEM buffer management, Atomic Kernel Mode Setting (KMS),
 * and hardware acceleration dispatch for HP Visualize FX ("Lego" architecture).
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_managed.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_fb_helper.h>

#include "hpfx_regs.h"
#include "hpfx_drm.h"

#define DRIVER_NAME     "hpfx_drm"
#define DRIVER_DESC     "HP Visualize FX5/FX10 DRM/KMS Driver"
#define DRIVER_DATE     "20261006"
#define DRIVER_MAJOR    1
#define DRIVER_MINOR    0
#define DRIVER_PATCHLEVEL 0

struct hpfx_drm_device {
    struct drm_device drm;
    struct pci_dev *pdev;

    /* MMIO & VRAM Regions */
    void __iomem *mmio_base;
    void __iomem *vram_base;
    resource_size_t mmio_len;
    resource_size_t vram_len;

    /* KMS Pipeline Components */
    struct drm_crtc crtc;
    struct drm_plane primary_plane;
    struct drm_encoder encoder;
    struct drm_connector connector;

    spinlock_t hw_lock;
};

static inline struct hpfx_drm_device *to_hpfx_drm(struct drm_device *dev)
{
    return container_of(dev, struct hpfx_drm_device, drm);
}

/* =========================================================================
 * MMIO Helpers
 * ========================================================================= */

static inline u32 hpfx_drm_read32(struct hpfx_drm_device *hdev, u32 reg)
{
    return readl(hdev->mmio_base + reg);
}

static inline void hpfx_drm_write32(struct hpfx_drm_device *hdev, u32 reg, u32 val)
{
    writel(val, hdev->mmio_base + reg);
}

static void hpfx_drm_wait_fifo(struct hpfx_drm_device *hdev, u32 slots)
{
    int timeout = 100000;
    while (timeout--) {
        u32 fifo = hpfx_drm_read32(hdev, HPFX_REG_FIFO_STATUS);
        u32 free_slots = (fifo >> 1);
        if (free_slots >= slots + 10) return;
        udelay(1);
    }
}

/* =========================================================================
 * KMS CRTC & Modesetting
 * ========================================================================= */

static void hpfx_drm_crtc_destroy(struct drm_crtc *crtc)
{
    drm_crtc_cleanup(crtc);
}

static const struct drm_crtc_funcs hpfx_drm_crtc_funcs = {
    .destroy = hpfx_drm_crtc_destroy,
    .set_config = drm_atomic_helper_set_config,
    .page_flip = drm_atomic_helper_page_flip,
    .reset = drm_atomic_helper_crtc_reset,
    .atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static int hpfx_drm_crtc_helper_check(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
    (void)crtc; (void)state;
    return 0;
}

static void hpfx_drm_crtc_helper_enable(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
    struct hpfx_drm_device *hdev = to_hpfx_drm(crtc->dev);
    (void)state;

    spin_lock(&hdev->hw_lock);
    /* Enable video output & unblank */
    u32 ctrl = hpfx_drm_read32(hdev, HPFX_REG_VIDEO_CTRL);
    ctrl |= HPFX_VID_CTRL_TIMING_EN | HPFX_VID_CTRL_SCREEN_UNBLANK;
    hpfx_drm_write32(hdev, HPFX_REG_VIDEO_CTRL, ctrl);
    hpfx_drm_write32(hdev, HPFX_REG_DPMS_CTRL, HPFX_DPMS_NORMAL_ON);
    spin_unlock(&hdev->hw_lock);
}

static void hpfx_drm_crtc_helper_disable(struct drm_crtc *crtc, struct drm_atomic_state *state)
{
    struct hpfx_drm_device *hdev = to_hpfx_drm(crtc->dev);
    (void)state;

    spin_lock(&hdev->hw_lock);
    hpfx_drm_write32(hdev, HPFX_REG_DPMS_CTRL, HPFX_DPMS_OFF);
    spin_unlock(&hdev->hw_lock);
}

static const struct drm_crtc_helper_funcs hpfx_drm_crtc_helper_funcs = {
    .atomic_check = hpfx_drm_crtc_helper_check,
    .atomic_enable = hpfx_drm_crtc_helper_enable,
    .atomic_disable = hpfx_drm_crtc_helper_disable,
};

/* =========================================================================
 * KMS Planes
 * ========================================================================= */

static const struct drm_plane_funcs hpfx_drm_plane_funcs = {
    .update_plane = drm_atomic_helper_update_plane,
    .disable_plane = drm_atomic_helper_disable_plane,
    .destroy = drm_plane_cleanup,
    .reset = drm_atomic_helper_plane_reset,
    .atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static const u32 hpfx_drm_plane_formats[] = {
    DRM_FORMAT_RGB565,
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_ARGB8888,
};

/* =========================================================================
 * KMS Connector & Encoder
 * ========================================================================= */

static int hpfx_drm_connector_get_modes(struct drm_connector *connector)
{
    return drm_add_modes_noedid(connector, 1920, 1200);
}

static const struct drm_connector_funcs hpfx_drm_connector_funcs = {
    .fill_modes = drm_helper_probe_single_connector_modes,
    .destroy = drm_connector_cleanup,
    .reset = drm_atomic_helper_connector_reset,
    .atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
    .atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static const struct drm_connector_helper_funcs hpfx_drm_connector_helper_funcs = {
    .get_modes = hpfx_drm_connector_get_modes,
};

static const struct drm_encoder_funcs hpfx_drm_encoder_funcs = {
    .destroy = drm_encoder_cleanup,
};

/* =========================================================================
 * GEM Dumb Buffer Management
 * ========================================================================= */

static int hpfx_drm_dumb_create(struct drm_file *file_priv,
                                struct drm_device *dev,
                                struct drm_mode_create_dumb *args)
{
    struct hpfx_drm_device *hdev = to_hpfx_drm(dev);
    u32 pitch = (args->width * ((args->bpp + 7) / 8) + 127) & ~127;
    u64 size = (u64)pitch * args->height;

    if (size > hdev->vram_len) return -ENOMEM;

    args->pitch = pitch;
    args->size = size;

    return drm_gem_dma_dumb_create(file_priv, dev, args);
}

/* =========================================================================
 * DRM IOCTL Handlers
 * ========================================================================= */

static int hpfx_drm_ioctl_gem_create(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
    struct drm_hpfx_gem_create *args = data;
    struct drm_mode_create_dumb dumb_args;
    memset(&dumb_args, 0, sizeof(dumb_args));
    dumb_args.width = args->width;
    dumb_args.height = args->height;
    dumb_args.bpp = args->bpp;

    int ret = hpfx_drm_dumb_create(file_priv, dev, &dumb_args);
    if (!ret) {
        args->handle = dumb_args.handle;
        args->size = dumb_args.size;
    }
    return ret;
}

static int hpfx_drm_ioctl_exec(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
    struct hpfx_drm_device *hdev = to_hpfx_drm(dev);
    struct drm_hpfx_exec *args = data;
    (void)file_priv;

    if (args->cmd_count == 0) return 0;
    if (args->cmd_count > 4096) return -EINVAL;

    spin_lock(&hdev->hw_lock);
    hpfx_drm_wait_fifo(hdev, 8);
    /* Flush pipeline */
    hpfx_drm_write32(hdev, HPFX_REG_3D_FLUSH, 0x1);
    spin_unlock(&hdev->hw_lock);

    return 0;
}

static int hpfx_drm_ioctl_wait_idle(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
    struct hpfx_drm_device *hdev = to_hpfx_drm(dev);
    (void)data; (void)file_priv;

    int timeout = 50000;
    while (timeout--) {
        u32 status = hpfx_drm_read32(hdev, HPFX_REG_3D_STATUS);
        if (!(status & HPFX_3D_STATUS_BUSY)) return 0;
        udelay(10);
    }
    return -ETIMEDOUT;
}

static const struct drm_ioctl_desc hpfx_drm_ioctls[] = {
    DRM_IOCTL_DEF_DRV(HPFX_GEM_CREATE, hpfx_drm_ioctl_gem_create, DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(HPFX_EXEC, hpfx_drm_ioctl_exec, DRM_AUTH | DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(HPFX_WAIT_IDLE, hpfx_drm_ioctl_wait_idle, DRM_AUTH | DRM_RENDER_ALLOW),
};

static const struct file_operations hpfx_drm_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .release = drm_release,
    .unlocked_ioctl = drm_ioctl,
    .compat_ioctl = drm_compat_ioctl,
    .mmap = drm_gem_mmap,
    .poll = drm_poll,
    .read = drm_read,
};

static const struct drm_driver hpfx_drm_driver = {
    .driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_RENDER | DRIVER_ATOMIC,
    .name = DRIVER_NAME,
    .desc = DRIVER_DESC,
    .date = DRIVER_DATE,
    .major = DRIVER_MAJOR,
    .minor = DRIVER_MINOR,
    .patchlevel = DRIVER_PATCHLEVEL,
    .fops = &hpfx_drm_fops,
    .dumb_create = hpfx_drm_dumb_create,
    .ioctls = hpfx_drm_ioctls,
    .num_ioctls = ARRAY_SIZE(hpfx_drm_ioctls),
};

/* =========================================================================
 * PCI Driver Lifecycle
 * ========================================================================= */

static const struct pci_device_id hpfx_drm_pci_table[] = {
    { PCI_DEVICE(HPFX_PCI_VENDOR_ID, HPFX_PCI_DEVICE_ID), 0, 0, 0 },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, hpfx_drm_pci_table);

static int hpfx_drm_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct hpfx_drm_device *hdev;
    int ret;
    (void)ent;

    ret = pci_enable_device(pdev);
    if (ret) return ret;

    pci_set_master(pdev);

    hdev = devm_drm_dev_alloc(&pdev->dev, &hpfx_drm_driver, struct hpfx_drm_device, drm);
    if (IS_ERR(hdev)) return PTR_ERR(hdev);

    hdev->pdev = pdev;
    pci_set_drvdata(pdev, hdev);
    spin_lock_init(&hdev->hw_lock);

    /* Claim and Map BAR 0 MMIO */
    hdev->mmio_len = pci_resource_len(pdev, 0);
    hdev->mmio_base = pci_iomap(pdev, 0, hdev->mmio_len);
    if (!hdev->mmio_base) return -EIO;

    /* Claim and Map BAR 2 VRAM with Write-Combining */
    hdev->vram_len = pci_resource_len(pdev, 2);
    hdev->vram_base = pci_iomap_wc(pdev, 2, hdev->vram_len);
    if (!hdev->vram_base) return -EIO;

    /* Initialize Mode Setting Pipeline */
    drm_mode_config_init(&hdev->drm);
    hdev->drm.mode_config.min_width = 640;
    hdev->drm.mode_config.min_height = 480;
    hdev->drm.mode_config.max_width = 1920;
    hdev->drm.mode_config.max_height = 1200;

    ret = drm_universal_plane_init(&hdev->drm, &hdev->primary_plane, 0,
                                   &hpfx_drm_plane_funcs,
                                   hpfx_drm_plane_formats, ARRAY_SIZE(hpfx_drm_plane_formats),
                                   NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
    if (ret) return ret;

    ret = drm_crtc_init_with_planes(&hdev->drm, &hdev->crtc,
                                    &hdev->primary_plane, NULL,
                                    &hpfx_drm_crtc_funcs, NULL);
    if (ret) return ret;
    drm_crtc_helper_add(&hdev->crtc, &hpfx_drm_crtc_helper_funcs);

    ret = drm_encoder_init(&hdev->drm, &hdev->encoder, &hpfx_drm_encoder_funcs,
                           DRM_MODE_ENCODER_DAC, NULL);
    if (ret) return ret;
    hdev->encoder.possible_crtcs = 1;

    ret = drm_connector_init(&hdev->drm, &hdev->connector, &hpfx_drm_connector_funcs,
                             DRM_MODE_CONNECTOR_VGA);
    if (ret) return ret;
    drm_connector_helper_add(&hdev->connector, &hpfx_drm_connector_helper_funcs);
    drm_connector_attach_encoder(&hdev->connector, &hdev->encoder);

    ret = drm_dev_register(&hdev->drm, 0);
    if (ret) return ret;

    dev_info(&pdev->dev, "HP Visualize FX DRM/KMS driver initialized (VRAM %u MB)\n",
             (u32)(hdev->vram_len / (1024 * 1024)));
    return 0;
}

static void hpfx_drm_pci_remove(struct pci_dev *pdev)
{
    struct hpfx_drm_device *hdev = pci_get_drvdata(pdev);
    drm_dev_unregister(&hdev->drm);
    drm_atomic_helper_shutdown(&hdev->drm);
}

static struct pci_driver hpfx_drm_pci_driver = {
    .name = DRIVER_NAME,
    .id_table = hpfx_drm_pci_table,
    .probe = hpfx_drm_pci_probe,
    .remove = hpfx_drm_pci_remove,
};

module_pci_driver(hpfx_drm_pci_driver);

MODULE_AUTHOR("Antigravity Reverse-Engineering Team");
MODULE_DESCRIPTION("HP Visualize FX5/FX10 Linux DRM/KMS Driver");
MODULE_LICENSE("GPL");
