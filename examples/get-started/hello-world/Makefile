# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: CC0-1.0

TARGET := build/hello-world
SOURCE := src/hello_world.c
CROSS_COMPILE ?= riscv64-esp-linux-gnu-

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(strip $(CROSS_COMPILE)),)
$(error CROSS_COMPILE is not set; specify the RISC-V toolchain prefix)
endif
endif

CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -O2 -Wall -Wextra

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@

build:
	mkdir -p $@

clean:
	rm -rf build
