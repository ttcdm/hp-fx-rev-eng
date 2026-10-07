---
name: legacy-driver-re-to-linux
description: >-
  Use this skill when reverse-engineering legacy Windows (NT/2000/XP/9x) or DOS
  hardware drivers (.exe self-extractors, .sys miniports, .dll display drivers, .inf files)
  to extract undocumented hardware registers, memory maps, timing tables, 2D/3D acceleration
  pipelines, and develop clean-room Linux kernel drivers (fbdev, DRM/KMS) and GPGPU frameworks.
---

# Legacy Hardware Driver Reverse-Engineering to Linux

This runbook guides the systematic deconstruction of proprietary legacy drivers to recover hardware register specifications and write modern, robust Linux kernel drivers and user-space libraries.

## 8-Phase Reverse-Engineering & Porting Methodology

### Phase 1: Archive Extraction & Triage
1. **Unpack Binary Archives**:
   - For `.exe` installers / self-extractors, test with `unzip -q <file>.exe -d extracted/` or `7z x <file>.exe`.
   - Inspect `.inf` files to extract PCI Vendor IDs, Device IDs, and Subsystem IDs.
   - Extract `.txt` / `.hlp` documentation for hardware revision lineage, memory sizes, companion chips, and errata.

### Phase 2: Kernel Miniport (.SYS) Analysis
The Windows NT Video Miniport (`*mp.sys`) contains foundational low-level hardware interactions.
1. **Locate Driver Entry & Dispatch Table**:
   - Disassemble `DriverEntry` to locate `VIDEO_HW_INITIALIZATION_DATA`.
   - Find callback pointers: `HwFindAdapter`, `HwInitialize`, `HwStartIO`, `HwResetHW`, and `HwInterrupt`.
2. **Extract PCI Resource & BAR Mapping**:
   - Trace calls to `VideoPortVerifyAccessRanges` and `VideoPortGetDeviceBase`.
   - Determine BAR numbers, access lengths (e.g., 32 MB), and memory vs. I/O space.
3. **Isolate Hardware Reset & Control Registers**:
   - Trace calls to `VideoPortWriteRegisterUlong` and `VideoPortReadRegisterUlong`.
   - Identify soft reset triggers, status/busy polling masks, and engine wait loops.
   - Identify DPMS power control registers (`0x0C` Normal, `0x0D` Standby, `0x0E` Suspend, `0x0F` Off).
   - Identify video output enable and screen unblank bitmasks.
4. **Extract Video Mode & Timing Tables**:
   - Scan `.data` and `.rdata` for resolution pairs (`640x480`, `800x600`, `1024x768`, `1280x1024`, `1600x1200`, `1920x1200`).
   - Extract stride/pitch, color depths (8/16/32 bpp), and refresh rates.
   - Locate referenced timing entries containing packed horizontal/vertical blanking and PLL clock divider values.

### Phase 3: Display Driver (.DLL) Analysis (2D Blitter & Mode Setting)
The GDI driver (`*gdi.dll`) manages 2D acceleration and framebuffer mapping.
1. **Trace `DrvEnableDriver`**:
   - Extract the `DRVENABLEDATA` function table (`DrvEnablePDEV`, `DrvEnableSurface`, `DrvAssertMode`).
2. **Locate Framebuffer Mapping**:
   - Trace `EngDeviceIoControl` with `IOCTL_VIDEO_MAP_VIDEO_MEMORY` (`0x0023201C`) to confirm aperture base and offsets.
3. **Recover 2D Acceleration Engine**:
   - Identify the Command FIFO free-slot status register and threshold calculations.
   - Identify pixel format configuration registers (8bpp, 16bpp, 32bpp).
   - Recover clipping boundaries and coordinate registers (`(Y << 16) | X`).
   - Identify the trigger register that initiates drawing operations.

### Phase 4: 3D Hardware Acceleration & Rasterization Pipeline Recovery
Analyze 3D user display services (`*uds.dll`), OpenGL Installable Client Drivers (`*icd.dll`), and diagnostics binaries (`*diag.exe`).
1. **3D Pipeline Mode Enable/Disable**:
   - Identify registers toggling 2D/3D modes (e.g. `0x800048` `|= 0x60000000`).
   - Locate 3D engine sync registers (busy bits) and primitive pipeline flush triggers.
2. **Geometry & Vertex Submission**:
   - Identify vertex submission coordinates (`V0`, `V1`, `V2_TRIGGER`).
   - Determine coordinate formats (fixed-point screen space vs FP14/IEEE floating-point).
3. **Depth Buffer & Raster State**:
   - Identify Z-buffer enable register, stride/pitch, comparison functions (LESS, LEQUAL, etc.), and fast depth clear registers.
   - Identify face culling registers (`0`: None, `1`: Front, `2`: Back).
   - Identify alpha blending modes, depth fog attenuation, and viewport scissor registers.

### Phase 5: Hardware Texture Staging & VRAM Management
1. **Texture Unit Registers**:
   - Identify base texture address in VRAM, texture format (RGB565 vs RGBA8888), pitch/stride, and filtering mode.
2. **Dedicated Staging Regions**:
   - Reserve high-memory regions in VRAM (e.g. upper 16 MB or 32 MB) for texture storage to avoid corrupting visible framebuffers.
   - Design kernel IOCTLs (`HPFX_IOCTL_UPLOAD_TEXTURE`) with strict memory bounds checking.

### Phase 6: Linux Driver Implementation
1. **Hardware Register Header (`<chip>_regs.h`)**:
   - Declare all discovered register offsets, bitmasks, structures, video timings, and IOCTL interfaces.
2. **Kernel Framebuffer Driver (`<chip>_fb.c`)**:
   - `struct pci_driver` with PCI ID auto-probing.
   - Resource claiming and Write-Combining framebuffer mapping (`ioremap_wc`).
   - Hardware reset, DPMS control, and mode setting (`fb_check_var`, `fb_set_par`, `fb_blank`).
   - Accelerated 2D blit (`fb_fillrect`, `fb_copyarea`).
   - 3D & texture IOCTL handlers (`fb_ioctl` / `fb_compat_ioctl`).
3. **Out-of-Tree Build & Packaging**:
   - Clean `Makefile` supporting `make modules`, `make userspace`, and `make test`.
   - X.Org configuration snippet (`10-<chip>.conf`) using `xserver-xorg-video-fbdev`.

### Phase 7: Userspace 3D Library & Verification Test Harness
1. **Userspace 3D Library (`lib<chip>3d`)**:
   - 4x4 matrix transforms (perspective, ortho, rotate, translate, scale).
   - Batch primitive queuing buffer to minimize kernel syscall overhead.
   - Software fallback rasterizer with identical math for development without physical AGP hardware.
2. **Factory Diagnostics & Deterministic CRC Verification**:
   - Extract golden test patterns from vendor diagnostic files (e.g. Torus, Sphere, Multi-part CAD models).
   - Compute deterministic IEEE 802.3 CRC32 checksums of rendered buffers.
   - Build a comprehensive multi-domain test suite with 100% assertion pass rates.

### Phase 8: Pre-Shader GPGPU & Numerical Compute Mapping
Map general-purpose compute onto legacy fixed-function graphics pipelines:
1. **Rasterization as ALU**: Evaluate 2D linear gradient equations across grid cells.
2. **Multi-Pass Blending as Arithmetic Accumulator**: Solve 2D PDEs (heat diffusion / Laplacian) and spatial convolutions (Sobel edge detection, Gaussian blur).
3. **Hardware Z-Buffer as Parallel Search Engine**: Compute discrete Voronoi diagrams and Euclidean distance fields via 3D cone rendering with depth testing.
4. **2D Blitter ROPs as SIMD Logic Engine**: Simulate cellular automata (Conway's Game of Life) and parallel bitwise operations at memory bandwidth speeds.
5. **FP14 Geometry Engines as Linear Algebra Accelerators**: Decompose matrix-matrix multiplication (GEMM) into $4 \times 4$ blocks processed by hardware geometry transform units.

### Phase 9: High-Resolution Native Silicon Visual Showcases & Demonstration Suite
Produce authentic, high-impact visual demonstrations that strictly exercise target ASIC capabilities:
1. **Silicon Authenticity Invariant**:
   - Confine demonstrations strictly to capabilities present in physical silicon (e.g., Summit T&L, Lego rasterizer, 2D BitBLT engine).
   - Do not substitute CPU-emulated software shaders, post-2000 shadow mapping, or raytracing without clear architectural delineation.
2. **Native 2D Engine Typography & Instrumentation Consoles**:
   - Leverage 2D coordinate-triggered solid fills (`0xCC` SRCCOPY) with an embedded 95-glyph 5×7 ASCII font to render complete workstation instrumentation consoles, live telemetry, and bus packet decoders natively in VRAM.
   - Exercise hardware Raster Operations (e.g. `0x66` XOR invert) for glitch-capture / zoom rubber-banding boxes directly across rendered framebuffers.
   - Exercise screen-to-screen BitBLT area copies to stamp corporate emblems and channel badges across multi-window viewports.
3. **Unified Master Showcase & Deterministic CRC Verification**:
   - Standardize all demonstration viewports at 1920×1080 Full HD (32bpp).
   - Build a unified gallery runner (`hpfx_gallery`) compiling and executing all native showcases in sequence with timing metrics and automated PPM/PNG conversion.
   - Calculate deterministic IEEE 802.3 CRC32 checksums for every rendered frame to ensure 100% bitwise repeatability across driver refactorings.
