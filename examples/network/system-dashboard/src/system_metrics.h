/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef SYSTEM_METRICS_H
#define SYSTEM_METRICS_H

#include <stdint.h>

struct cpu_metrics {
    unsigned int count;
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
};

struct memory_metrics {
    uint64_t total_bytes;
    uint64_t available_bytes;
};

struct system_metrics {
    struct cpu_metrics cpu;
    struct memory_metrics memory;
    double uptime_seconds;
    double load_1;
    double load_5;
    double load_15;
};

int system_metrics_collect(const char *proc_root, struct system_metrics *metrics);

#endif
