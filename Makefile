# Makefile for HP Visualize FX Linux Drivers & 3D Acceleration Suite

# Kernel module configuration
obj-m += hpfx_fb.o
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

.PHONY: all modules userspace clean install load unload test test-demo

all: modules userspace test_hpfx_suite

modules:
	@if [ -d "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) modules; \
	else \
		echo "Kernel source tree not found at $(KDIR). Skipping kernel module build."; \
	fi

userspace: libhpfx3d.a hpfx_3d_demo test_hpfx_suite

hpfx3d.o: hpfx3d.c hpfx3d.h hpfx_regs.h
	$(CC) $(CFLAGS) -c hpfx3d.c -o hpfx3d.o

libhpfx3d.a: hpfx3d.o
	$(AR) rcs libhpfx3d.a hpfx3d.o

hpfx_3d_demo: hpfx_3d_demo.c libhpfx3d.a
	$(CC) $(CFLAGS) hpfx_3d_demo.c -L. -lhpfx3d $(LDFLAGS) -o hpfx_3d_demo

test_hpfx_suite: test_hpfx_suite.c libhpfx3d.a
	$(CC) $(CFLAGS) test_hpfx_suite.c -L. -lhpfx3d $(LDFLAGS) -o test_hpfx_suite

test: test_hpfx_suite
	./test_hpfx_suite

test-demo: hpfx_3d_demo
	./hpfx_3d_demo -f 30 -o hpfx_test_render.ppm

clean:
	@if [ -d "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) clean 2>/dev/null || true; \
	fi
	rm -f *.o *.a hpfx_3d_demo test_hpfx_suite *.ppm *.png

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

load:
	sudo insmod hpfx_fb.ko

unload:
	sudo rmmod hpfx_fb
