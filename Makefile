# Makefile for HP Visualize FX Linux Drivers & 3D Acceleration Suite

# Kernel module configuration
obj-m += hpfx_fb.o hpfx_drm.o
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# Userspace compiler configuration
CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?= -lm
AR ?= ar
ifeq ($(shell uname -s),Darwin)
AR := /usr/bin/ar
endif

DEMO_BINS = hpfx_3d_demo hpfx_gears hpfx_cad_studio hpfx_texture_demo hpfx_voronoi_3d hpfx_2d_demo hpfx_gallery

.PHONY: all modules userspace clean install load unload test test-demo test-gears gallery

all: modules userspace test_hpfx_suite

modules:
	@if [ -d "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) modules; \
	else \
		echo "Kernel source tree not found at $(KDIR). Skipping kernel module build."; \
	fi

userspace: libhpfx3d.a $(DEMO_BINS) test_hpfx_suite

hpfx3d.o: hpfx3d.c hpfx3d.h hpfx_regs.h
	$(CC) $(CFLAGS) -c hpfx3d.c -o hpfx3d.o

hpfx_diag.o: hpfx_diag.c hpfx_diag.h hpfx3d.h hpfx_regs.h
	$(CC) $(CFLAGS) -c hpfx_diag.c -o hpfx_diag.o

hpfx_compute.o: hpfx_compute.c hpfx_compute.h hpfx3d.h hpfx_regs.h
	$(CC) $(CFLAGS) -c hpfx_compute.c -o hpfx_compute.o

hpfx_gl.o: hpfx_gl.c hpfx_gl.h hpfx3d.h hpfx_regs.h
	$(CC) $(CFLAGS) -c hpfx_gl.c -o hpfx_gl.o

libhpfx3d.a: hpfx3d.o hpfx_diag.o hpfx_compute.o hpfx_gl.o
	$(AR) rcs libhpfx3d.a hpfx3d.o hpfx_diag.o hpfx_compute.o hpfx_gl.o

hpfx_3d_demo: hpfx_3d_demo.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_3d_demo.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_3d_demo

hpfx_gears: hpfx_gears.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_gears.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_gears

hpfx_cad_studio: hpfx_cad_studio.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_cad_studio.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_cad_studio

hpfx_texture_demo: hpfx_texture_demo.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_texture_demo.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_texture_demo

hpfx_voronoi_3d: hpfx_voronoi_3d.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_voronoi_3d.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_voronoi_3d

hpfx_2d_demo: hpfx_2d_demo.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_2d_demo.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_2d_demo

hpfx_gallery: hpfx_gallery.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_gallery.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_gallery

test_hpfx_suite: test_hpfx_suite.c libhpfx3d.a
	$(CC) $(CFLAGS) test_hpfx_suite.c -L. -lhpfx3d $(LDFLAGS) -o test_hpfx_suite

test: test_hpfx_suite
	./test_hpfx_suite

test-demo: hpfx_3d_demo
	./hpfx_3d_demo -f 30 -o hpfx_test_render.ppm

test-gears: hpfx_gears
	./hpfx_gears -f 30 -o hpfx_gears_render.ppm

gallery: userspace
	./hpfx_gallery

clean:
	@if [ -d "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) clean 2>/dev/null || true; \
	fi
	rm -f *.o *.a $(DEMO_BINS) test_hpfx_suite *.ppm *.png

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

load:
	sudo insmod hpfx_fb.ko

unload:
	sudo rmmod hpfx_fb
