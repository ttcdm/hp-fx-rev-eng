/*
 * HP Visualize FX5 & FX10 Linux DRM/KMS Kernel Driver Header
 *
 * Defines the Direct Rendering Manager (DRM) and Kernel Mode Setting (KMS)
 * user-space ABI, GEM buffer management, and hardware execution dispatch.
 */

#ifndef _HPFX_DRM_H_
#define _HPFX_DRM_H_

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int32_t  __s32;
#endif

#define DRM_HPFX_GEM_CREATE    0x00
#define DRM_HPFX_EXEC          0x01
#define DRM_HPFX_WAIT_IDLE     0x02

#define DRM_COMMAND_BASE       0x40

struct drm_hpfx_gem_create {
    __u32 width;
    __u32 height;
    __u32 bpp;
    __u32 flags;
    __u32 handle;
    __u32 pad;
    __u64 size;
    __u64 offset;
};

struct drm_hpfx_exec {
    __u32 handle;
    __u32 cmd_count;
    __u64 cmds_ptr;
    __u32 flags;
    __u32 pad;
};

struct drm_hpfx_wait_idle {
    __u32 flags;
    __u32 timeout_ms;
};

#define DRM_IOCTL_HPFX_GEM_CREATE  _IOWR('d', DRM_COMMAND_BASE + DRM_HPFX_GEM_CREATE, struct drm_hpfx_gem_create)
#define DRM_IOCTL_HPFX_EXEC        _IOWR('d', DRM_COMMAND_BASE + DRM_HPFX_EXEC, struct drm_hpfx_exec)
#define DRM_IOCTL_HPFX_WAIT_IDLE   _IOWR('d', DRM_COMMAND_BASE + DRM_HPFX_WAIT_IDLE, struct drm_hpfx_wait_idle)

#endif /* _HPFX_DRM_H_ */
