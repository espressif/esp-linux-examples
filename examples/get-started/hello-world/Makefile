# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: CC0-1.0

TARGET := build/hello-world
SOURCE := src/hello_world.c
CROSS_COMPILE ?= riscv64-esp-linux-gnu-

ifeq ($(origin CC), default)
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(strip $(CROSS_COMPILE)),)
$(error CROSS_COMPILE is not set; specify the RISC-V toolchain prefix)
endif
endif
CC := $(CROSS_COMPILE)gcc
endif

CFLAGS ?= -O2 -Wall -Wextra

prefix ?= /usr
bindir ?= $(prefix)/bin
DESTDIR ?=
INSTALL ?= install

.PHONY: all install clean

all: $(TARGET)

$(TARGET): $(SOURCE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

build:
	mkdir -p $@

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(bindir)/hello-world

clean:
	rm -rf build
