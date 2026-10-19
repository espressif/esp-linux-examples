# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: CC0-1.0

PROGRAM := esp-system-dashboard
TARGET := build/$(PROGRAM)
SOURCES := src/main.c src/http_server.c src/system_info.c \
	src/system_metrics.c src/network_info.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)
TEST_TARGET := build/test-parsers
TEST_SOURCES := tests/test_parsers.c src/system_info.c \
	src/system_metrics.c src/network_info.c

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
MHD_CFLAGS = $(shell $(PKG_CONFIG) --cflags libmicrohttpd)
MHD_LIBS = $(shell $(PKG_CONFIG) --libs libmicrohttpd)

ifeq ($(SANITIZE),1)
override CFLAGS += -O1 -g -fno-omit-frame-pointer \
	-fsanitize=address,undefined
override LDFLAGS += -fsanitize=address,undefined
endif

prefix ?= /usr
bindir ?= $(prefix)/bin
datadir ?= $(prefix)/share
dashboard_datadir ?= $(datadir)/$(PROGRAM)
DESTDIR ?=
INSTALL ?= install
PYTHON ?= python3

.PHONY: all check-dependency install test clean

all: check-dependency $(TARGET)

check-dependency:
	@$(PKG_CONFIG) --atleast-version=1.0.2 libmicrohttpd || { \
		echo "libmicrohttpd >= 1.0.2 is required" >&2; exit 1; }

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS) $(MHD_LIBS)

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MHD_CFLAGS) \
		-DDASHBOARD_WEB_ROOT=\"$(dashboard_datadir)\" -c $< -o $@

build:
	mkdir -p $@

$(TEST_TARGET): $(TEST_SOURCES) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $(TEST_SOURCES) $(LDFLAGS) -o $@

test: all $(TEST_TARGET)
	ASAN_OPTIONS=detect_leaks=1 $(TEST_TARGET) tests/fixtures
	$(PYTHON) tests/test_http.py $(TARGET) www

install: all
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(bindir)/$(PROGRAM)
	$(INSTALL) -d $(DESTDIR)$(dashboard_datadir)
	$(INSTALL) -m 0644 www/index.html \
		$(DESTDIR)$(dashboard_datadir)/index.html

clean:
	rm -rf build
