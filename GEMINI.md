# HP Visualize FX5 & FX10 Linux Driver Suite

This project contains the reverse-engineered Linux kernel driver, 2D/3D hardware acceleration library, diagnostics test suite, OpenGL 1.1 runtime, modern DRM/KMS module, and GPGPU compute tools for the **HP Visualize FX5 and FX10** ("Lego" Architecture) AGP 4x / PCI 66MHz workstation graphics accelerators.

## Project Structure
* `hpfx_regs.h`: Hardware register map, MMIO BAR allocations, video timings, and IOCTL interfaces.
* `hpfx_fb.c`: Linux framebuffer (`fbdev`) kernel module with PCI probing, mode setting, DPMS sync, and 3D/texture IOCTL dispatch.
* `hpfx_drm.h` / `hpfx_drm.c`: Modern Linux DRM/KMS kernel driver with Atomic Modesetting, GEM dumb buffers, and DRI render nodes.
* `hpfx3d.h` / `hpfx3d.c`: Userspace 3D acceleration library (`libhpfx3d.a`) with 4x4 matrix math, batching, textures, fog, scissor boxes, and procedural meshes (Cube, Torus, Sphere, shoe4R CAD).
* `hpfx_diag.h` / `hpfx_diag.c`: HP Diagnostics suite runner (FX5DIAG / FX5CRC golden emulation).
* `hpfx_compute.h` / `hpfx_compute.c`: Pre-Shader GPGPU numerical compute framework (2D PDEs, Sobel convolutions, Voronoi via Z-buffer, Conway Life ROP logic, and FP14 GEMM matrix multiplication).
* `hpfx_gl.h` / `hpfx_gl.c`: OpenGL 1.1 / TinyGL hardware backend mapping standard OpenGL immediate-mode calls to `libhpfx3d.a`.
* `hpfx_cad_studio.c`: HP industrial CAD workstation studio showcasing the `shoe4R` CAD model, golden Torus, sapphire Sphere, calibration Cube, engineering grid, and depth fog.
* `hpfx_gears.c`: Classic OpenGL gears benchmark rendering animated interlocking gears on `hpfx_gl`.
* `hpfx_3d_demo.c`: Standalone 3D demo rendering multi-mesh animated scenes with real-time FPS profiling and PPM exports.
* `hpfx_texture_demo.c`: Procedural material synthesis gallery (Mahogany wood grain, Carrara marble, checkerboard, and HP Corporate Medallion).
* `hpfx_voronoi_3d.c`: Hardware 3D Z-buffer computational geometry demo solving Voronoi diagrams in silicon (Hoff et al. SIGGRAPH 1999).
* `hpfx_2d_demo.c`: Hardware 2D BitBLT & ROP compositing demo exercising the Lego 128-bit 2D acceleration engine (windowing, XOR rubber-banding, screen blits).
* `hpfx_gallery.c`: Unified visual showcase master generator rendering all 6 native GPU showcases and exporting high-res PPM and PNG images.
* `test_hpfx_suite.c`: Automated regression test suite containing 377 assertions across 20 domains (100% pass rate).
* `10-hpfx.conf`: X.Org configuration snippet for running desktop environments over `/dev/fb0`.
* `.agents/skills/legacy-driver-re-to-linux/SKILL.md`: Workspace skill capturing the 8-phase reverse engineering and Linux porting runbook.

## Build & Test Commands
* `make`: Compile userspace 3D library and demos (`libhpfx3d.a`, `hpfx_3d_demo`, `hpfx_gears`, `hpfx_cad_studio`, `hpfx_texture_demo`, `hpfx_voronoi_3d`, `hpfx_2d_demo`, `hpfx_gallery`, `test_hpfx_suite`).
* `make modules`: Compile out-of-tree Linux kernel driver modules (`hpfx_fb.ko`, `hpfx_drm.ko`).
* `make test`: Run the 377-assertion hardware regression test suite across 20 domains.
* `make gallery`: Run the unified visual suite generating high-resolution renderings of all 6 native GPU showcases.
* `make test-demo`: Run the 3D animation demo and export a sample frame screenshot (`hpfx_test_render.ppm`).
* `make test-gears`: Run the OpenGL Gears demo and export a sample frame screenshot (`hpfx_gears_render.ppm`).
* `make clean`: Clean build artifacts.

## Guidelines
* Always run `make test` after code modifications to ensure 100% assertion pass rates.
* Maintain clean builds with zero compiler warnings (`-Wall -Wextra`).
* Preserve register cross-references with official HP documentation (Data Sheet `5980-1411E`, Config Guide `A5021-90015`).
