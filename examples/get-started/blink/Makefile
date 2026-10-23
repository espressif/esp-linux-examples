# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: CC0-1.0

TARGET := build/blink
SOURCE := src/blink.c
CROSS_COMPILE ?= riscv64-esp-linux-gnu-
PKG_CONFIG ?= pkg-config

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

LIBGPIOD_CFLAGS = $(shell $(PKG_CONFIG) --cflags libgpiod)
LIBGPIOD_LIBS = $(shell $(PKG_CONFIG) --libs libgpiod)

.PHONY: all install clean

all: $(TARGET)

$(TARGET): $(SOURCE) | build
	$(CC) $(CPPFLAGS) $(LIBGPIOD_CFLAGS) $(CFLAGS) $< \
		$(LDFLAGS) $(LIBGPIOD_LIBS) $(LDLIBS) -o $@

build:
	mkdir -p $@

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(bindir)/blink

clean:
	rm -rf build
