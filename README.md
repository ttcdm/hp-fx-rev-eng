# HP Visualize FX5 & FX10 Linux Driver & 3D Acceleration Suite

This repository contains the reverse-engineered hardware specifications, open-source Linux kernel framebuffer driver (`hpfx_fb`), userspace 3D acceleration library (`libhpfx3d`), and 3D demonstration program (`hpfx_3d_demo`) for the **Hewlett-Packard Visualize FX5 and FX10** series workstation graphics accelerators ("Lego" architecture).

Extracted and reverse-engineered directly from the official HP Windows 2000 driver suite (`fx_w2k_118b.exe` / `hpfxlmp.w2k` / `hpfxlgdi.w2k` / `hpfxlkds.w2k` / `hpfxluds.w2k` / `hpfxlicd.w2k`).

---

## Hardware Identification

* **PCI Vendor ID**: `0x103C` (Hewlett-Packard)
* **PCI Device ID**: `0x100A` (HP Visualize FX Series)
* **Subsystem IDs**:
  * `0x10D4103C`: Visualize FX5 (32 MB)
  * `0x10D5103C`: Visualize FX10 (64 MB)
  * `0x10D6103C`: Visualize FX5 Dual/Alternate
  * `0x10D7103C`: Visualize FX10 Dual/Alternate
  * `0x10D8103C`: Visualize FX Generic Variant
* **Bus Type**: AGP 4x / PCI 66MHz
* **ASICs**: HP FP14 Geometry Accelerator / Setup Engine + GraFiX / Pinnacle Rasterizers

---

## Memory Map & Register Architecture

### PCI BAR 0 (32 MB - MMIO & Control Registers)

#### 2D Display & Video Core
* `0x000044`: **Video Control** (Bit 9: Display Timing Enable, Bit 14: Screen Unblank)
* `0x00004c`: **DPMS Control** (`0x0c` = Normal On, `0x0d` = Standby, `0x0e` = Suspend, `0x0f` = Off)
* `0x000070` / `0x0000d4`: **I2C / DDC Bus** (Monitor detection, SCL/SDA bitbanging)
* `0x000100`: **Soft Reset** (Write `0x100` to trigger reset, bit 8 clears when complete)
* `0x000400`: **Engine Status** (Bit 24: Engine Busy flag)
* `0x000440`: **Hardware Capabilities**
* `0x641440`: **2D Command FIFO Status** (`((read >> 1) - 10)` free slots)
* `0x921110`: **Raster Pixel Format** (`0x02000000 | format`)
* `0xa00808` / `0xa0080c`: **Foreground Color & ROP**
* `0xa008a4` / `0xa008a8`: **Background Color & Plane Mask**
* `0xac1050` / `0xac1054`: **Hardware Clip Window** (`(Y << 16) | X`)
* `0xb20000`: **2D Start Coordinate** (`(Y0 << 16) | X0`)
* `0xb25400`: **2D End Coordinate & Execution Trigger** (`(Y1 << 16) | X1`)

#### 3D Pipeline & FP14 Rasterizer Registers
* `0x800048`: **3D Pipeline Control** (`|= 0x60000000` enables 3D mode, `&= 0x9fffffff` restores 2D mode)
* `0x8000c8`: **3D Engine Status** (Bit 31: 3D Rasterizer Busy)
* `0x8000cc`: **3D Pipeline Flush** (Write `0x1` to force pending primitive flush)
* `0x8e5800`: **3D Raster State & Face Culling** (`0`: None, `1`: Front, `2`: Back)
* `0x920404`: **Z-Buffer Format & Enable** (`0x00000001` = 24-bit hardware Z-buffer)
* `0x920808`: **Z-Buffer Stride** (Stride in bytes = `width * 4`)
* `0x92083c`: **Depth Comparison Function** (`0`: NEVER, `1`: LESS, `2`: EQUAL, `3`: LEQUAL, `4`: GREATER, `5`: NOTEQUAL, `6`: GEQUAL, `7`: ALWAYS)
* `0x92084c`: **Depth Write Mask** (`1`: Write enabled, `0`: Read-only)
* `0x9208a4`: **Fast Depth Clear** (Hardware Z-Clear value, e.g. `0x00ffffff`)
* `0x921120`: **Alpha Blending Mode**
* `0x921124`: **Hardware Fog Control**
* `0x921184`: **Texture Unit Control**
* `0xac0858`: **3D Viewport / Scissor Box**
* `0xb20000` / `0xcb0008`: **3D Triangle Vertex 0** (`(Y0 << 16) | X0`)
* `0xb24c00` / `0xcb0010`: **3D Triangle Vertex 1** (`(Y1 << 16) | X1`)
* `0xb3c010` / `0xcb0020`: **3D Triangle Vertex 2 & Trigger** (`(Y2 << 16) | X2` - Triggers FP14 rasterization)

### PCI BAR 2 (32 MB - Framebuffer Memory)
* Direct linear byte-addressable framebuffer mapped with Write-Combining cache mode.
* Formats: 8bpp pseudocolor, 16bpp RGB565, 32bpp BGRX8888.

---

## Supported Video Modes

Reverse-engineered from the driver timing table:
1. `640x480` @ 60Hz, 75Hz, 85Hz, 120Hz
2. `800x600` @ 60Hz, 75Hz, 85Hz, 120Hz
3. `1024x768` @ 60Hz, 75Hz, 85Hz, 120Hz
4. `1280x1024` @ 60Hz, 75Hz, 85Hz
5. `1600x1024` @ 76Hz
6. `1600x1200` @ 60Hz, 75Hz
7. `1920x1080` @ 60Hz
8. `1920x1200` @ 76Hz

---

## 3D IOCTL Interface (`/dev/fb0`)

The driver provides direct hardware 3D acceleration IOCTLs defined in `hpfx_regs.h`:

| IOCTL | Description |
|---|---|
| `HPFX_IOCTL_3D_RESET` | Resets the 3D pipeline and sets default depth buffer states. |
| `HPFX_IOCTL_3D_SYNC` | Blocks until the 3D rasterization engine and FIFO become idle. |
| `HPFX_IOCTL_CLEAR_DEPTH` | Initiates single-cycle hardware depth buffer clear. |
| `HPFX_IOCTL_SET_3D_STATE` | Configures depth test, depth func, culling mode, and blending. |
| `HPFX_IOCTL_3D_DRAW_TRIANGLE` | Submits a single 3D triangle `(x0,y0,z0)`, `(x1,y1,z1)`, `(x2,y2,z2)`. |
| `HPFX_IOCTL_3D_DRAW_TRI_LIST` | Batch submits an array of up to 4096 3D triangles with minimal syscall overhead. |
| `HPFX_IOCTL_UPLOAD_TEXTURE` | Staged upload of custom texture bitmap to GPU SDRAM with bounds protection. |

---

## Historical Documentation & Literature Dossier

The development and reverse-engineering of this driver suite were synthesized from archived literature and reverse engineering of `fx_w2k_118b.exe`:

* **HP Journal (May 1998, Vol. 49, No. 2, pp. 28–34)**:
  * *"An Overview of the VISUALIZE fx Graphics Accelerator Hardware"* (Noel D. Scott, Daniel M. Olsen, Ethan W. Gannett): Architectural breakdown of Summit ASICs and evolution to the Lego unified raster/texture engine.
  * *"HP Kayak: A PC Workstation with Advanced Graphics Performance"*: Architecture of the Intel IA-32 Kayak workstations, AGP 2X / AGP Pro bus, and OpenGL acceleration.
* **HP Official Manuals & Part Numbers**:
  * **HP Data Sheet `5980-1411E`** (2000): *HP Visualize fx5pro/fx10pro UNIX Graphics Accelerators Data Sheet*.
  * **HP Configuration Guide `A5021-90015`** (2000): *HP VISUALIZE fx5 and fx10 Configuration Guide* / *HP fx Graphics Card Installation and Configuration Guide (fxe, fx5, and fx10)*.
  * **HP White Paper**: *HP IA-32 visualize fx5 and fx10 Windows graphics accelerators*.
* **Open-Source PA-RISC Linux Heritage**:
  * Community documentation at [OpenPA.net](https://www.openpa.net) and [parisc.docs.kernel.org](https://parisc.docs.kernel.org).
  * Historical driver efforts by Kyle McMartin and Sven Schnelle's 2021 `[PATCH/RFT] fbdev driver for HP Visualize FX cards` on `dri-devel` / `freedesktop.org`.

---

## Building and Installing on Linux

### 1. Prerequisites
On your target Linux distribution (Ubuntu, Debian, Fedora, Arch, CentOS, etc.):
```bash
# Debian / Ubuntu / Mint:
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r)

# Fedora / RHEL / Alma:
sudo dnf install kernel-devel kernel-headers gcc make

# Arch Linux:
sudo pacman -S linux-headers base-devel
```

### 2. Compile Everything
```bash
cd hp_visualize_fx_linux
make
```
This builds both the kernel module (`hpfx_fb.ko`) and the userspace 3D library and demo (`hpfx_3d_demo`).

### 3. Run the Automated Hardware & Pipeline Test Suite
```bash
make test
```
Executes the comprehensive 303-test regression suite across 16 domains covering PCI specs, timing tables, FP14 3D registers, IOCTL ABI, 3D matrix mathematics, Z-buffer occlusion, textures, alpha blending, fog, scissor clipping, Torus/Sphere diagnostics, fuzzing, and VRAM staging.

### 4. Load the Driver
```bash
sudo insmod hpfx_fb.ko
```
Or with custom mode setting:
```bash
sudo insmod hpfx_fb.ko mode_option="1280x1024-32@60"
```

Verify in kernel logs:
```bash
dmesg | grep hpfx_fb
```

### 5. Run the 3D Demonstration Program
```bash
./hpfx_3d_demo -f 120
```
Command-line options:
* `-d <device>`: Framebuffer device path (default: `/dev/fb0`)
* `-f <frames>`: Number of animation frames to render (default: 60)
* `-o <file.ppm>`: Save final rendered frame to PPM screenshot
* `--no-ppm`: Disable screenshot writing

### 6. Running X11 Desktop
Copy the Xorg configuration snippet to `/etc/X11/xorg.conf.d/`:
```bash
sudo cp 10-hpfx.conf /etc/X11/xorg.conf.d/
```
Ensure `xserver-xorg-video-fbdev` is installed and start Xorg.
