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

## HP Diagnostics Architecture & Golden CRC Verification

Reverse-engineered directly from official HP Windows 2000 diagnostic files (`FX5DIAG.W2K` and `FX5CRC.W2K`):
* **Target Viewport**: $544 \times 403$ window (`0x220 x 0x193`).
* **Test #1 (3D Torus Mesh)**: Rotated parametric torus evaluating Gouraud specular lighting and depth occlusion.
  * Official Golden CRC: `0xa25c6a86` (16-bit) / `0x3411fabb` (32-bit).
* **Test #2 (3D shoe4R Solid CAD Model)**: Industrial CAD running shoe benchmark geometry evaluating multi-patch assembly and boundary clipping.
  * Official Golden CRC: `0x9861656d` (16-bit) / `0x4b161f67` (32-bit).
* **Test #3 (3D Sphere Mesh)**: Tessellated UV sphere evaluating high-vertex density rasterization.
  * Official Golden CRC: `0x9df8c3a9` (16-bit) / `0x846c6000` (32-bit).
* **Test #4 (3D shoe4R Textured CAD Model)**: Running shoe model rendered with hardware texture mapping and alternate lighting angle.
  * Official Golden CRC: `0x2d16310a` (16-bit) / `0x1135eb4e` (32-bit).

---

## Pre-Shader GPGPU & Numerical Compute Framework (`hpfx_compute.h`)

Maps high-performance numerical computation onto fixed-function graphics pipelines:
1. **2D PDE / Heat Diffusion & Discrete Laplacian**:
   - Solves $\frac{\partial u}{\partial t} = \alpha \nabla^2 u$ on 2D grids via 5-point discrete Laplacian stencil and ping-pong hardware buffers.
2. **Spatial Image Convolutions**:
   - $3 \times 3$ horizontal and vertical Sobel gradient edge detector ($G_x, G_y, |G|$) and Gaussian smoothing filters.
3. **Discrete Voronoi & Euclidean Distance Transform (EDT)**:
   - Evaluates nearest seed sites and continuous Euclidean distance fields via 3D cone rendering into the hardware Z-buffer (`GL_LESS`), computing exact Voronoi partitions in $O(N)$ geometry time rather than $O(N \cdot W \cdot H)$ CPU brute force.
4. **Cellular Automata (Conway's Game of Life)**:
   - High-throughput cellular automata evaluated at memory bandwidth speeds using 2D Blitter bitwise ROP SIMD operations.
5. **FP14 Blocked Matrix Multiplication (GEMM)**:
   - Decomposes general $M \times K \times N$ matrix multiplications into $4 \times 4$ tile blocks executed through FP14 geometry transform units.

---

## OpenGL 1.1 / TinyGL Hardware Backend (`hpfx_gl.h` & `hpfx_gears`)

Provides standard OpenGL 1.1 immediate-mode APIs mapping directly onto the Visualize FX hardware acceleration pipeline:
* **Immediate Mode**: `glBegin` (`GL_TRIANGLES`, `GL_QUADS`, `GL_TRIANGLE_STRIP`, `GL_TRIANGLE_FAN`), `glEnd`, `glVertex3f`, `glColor3f/4f`, `glNormal3f`, `glTexCoord2f`.
* **Matrix Stack**: Hierarchical 32-level ModelView and 8-level Projection matrix stacks (`glMatrixMode`, `glPushMatrix`, `glPopMatrix`, `glTranslatef`, `glRotatef`, `glScalef`, `glFrustum`, `glOrtho`).
* **State Management**: `glEnable`/`glDisable` (`GL_DEPTH_TEST`, `GL_CULL_FACE`, `GL_BLEND`, `GL_FOG`, `GL_TEXTURE_2D`, `GL_SCISSOR_TEST`), `glDepthFunc`, `glBlendFunc`, `glClear`.
* **3D Gears Benchmark (`hpfx_gears`)**: Classic Brian Paul gears demo running natively on top of `hpfx_gl`.

---

## Modern Linux DRM/KMS Kernel Driver (`hpfx_drm.c`)

Extends beyond legacy `fbdev` with a modern Linux Direct Rendering Manager (DRM) and Kernel Mode Setting (KMS) driver:
* **Atomic Modesetting**: CRTC, Primary Plane, Connector, and Encoder pipeline programming HP Visualize FX timing registers (`0x000044`, `0x00004c`).
* **GEM Dumb Buffers**: 128-byte cache-aligned linear VRAM allocations with write-combining mapping.
* **Render Node IOCTLs**: Hardware command buffer execution dispatch (`DRM_IOCTL_HPFX_EXEC`, `DRM_IOCTL_HPFX_WAIT_IDLE`).

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
make
```
Builds userspace acceleration libraries (`libhpfx3d.a`), 3D multi-mesh demo (`hpfx_3d_demo`), OpenGL gears demo (`hpfx_gears`), test suite (`test_hpfx_suite`), and kernel modules (`hpfx_fb.ko`, `hpfx_drm.ko`).

### 3. Run the Automated Hardware & Pipeline Test Suite
```bash
make test
```
Executes the comprehensive 377-test regression suite across 20 domains covering PCI specs, timing tables, FP14 3D registers, IOCTL ABI, 3D matrix mathematics, Z-buffer occlusion, textures, alpha blending, fog, scissor clipping, Torus/Sphere diagnostics, fuzzing, VRAM staging, HP Diagnostics golden verification, OpenGL 1.1 backend, GPGPU compute, and DRM/KMS GEM ABI.

### 4. Run the Demos
```bash
# Run multi-mesh 3D animation demo
make test-demo

# Run classic OpenGL Gears demo
make test-gears
```

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
