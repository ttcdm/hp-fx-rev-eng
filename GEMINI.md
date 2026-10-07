# HP Visualize FX5 & FX10 Linux Driver Suite

This project contains the reverse-engineered Linux kernel driver, 2D/3D hardware acceleration library, diagnostics test suite, and GPGPU compute tools for the **HP Visualize FX5 and FX10** ("Lego" Architecture) AGP 4x / PCI 66MHz workstation graphics accelerators.

## Project Structure
* `hpfx_regs.h`: Hardware register map, MMIO BAR allocations, video timings, and IOCTL interfaces.
* `hpfx_fb.c`: Linux framebuffer (`fbdev`) kernel module with PCI probing, mode setting, DPMS sync, and 3D/texture IOCTL dispatch.
* `hpfx3d.h` / `hpfx3d.c`: Userspace 3D acceleration library (`libhpfx3d.a`) with 4x4 matrix math, batching, textures, fog, and scissor boxes.
* `hpfx_3d_demo.c`: Standalone 3D demo rendering multi-mesh animated scenes with real-time FPS profiling and PPM exports.
* `test_hpfx_suite.c`: Automated regression test suite containing 303 assertions across 16 domains (100% pass rate).
* `10-hpfx.conf`: X.Org configuration snippet for running desktop environments over `/dev/fb0`.
* `.agents/skills/legacy-driver-re-to-linux/SKILL.md`: Workspace skill capturing the 8-phase reverse engineering and Linux porting runbook.

## Build & Test Commands
* `make`: Compile userspace 3D library and demo (`libhpfx3d.a`, `hpfx_3d_demo`, `test_hpfx_suite`).
* `make modules`: Compile out-of-tree Linux kernel driver module (`hpfx_fb.ko`).
* `make test`: Run the 303-assertion hardware regression test suite.
* `make test-demo`: Run the 3D animation demo and export a sample frame screenshot (`hpfx_test_render.ppm`).
* `make clean`: Clean build artifacts.

## Guidelines
* Always run `make test` after code modifications to ensure 100% assertion pass rates.
* Maintain clean builds with zero compiler warnings (`-Wall -Wextra`).
* Preserve register cross-references with official HP documentation (Data Sheet `5980-1411E`, Config Guide `A5021-90015`).
