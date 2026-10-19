/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "system_metrics.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int make_path(char *path, size_t size, const char *root,
                     const char *name)
{
    int length = snprintf(path, size, "%s/%s", root, name);

    return length >= 0 && (size_t)length < size ? 0 : -1;
}

static int read_cpu(const char *proc_root, struct cpu_metrics *cpu)
{
    char path[256];
    char line[512];
    FILE *file;

    if (make_path(path, sizeof(path), proc_root, "stat") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;

    if (fgets(line, sizeof(line), file) == NULL ||
        sscanf(line,
               "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
               " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
               &cpu->user, &cpu->nice, &cpu->system, &cpu->idle,
               &cpu->iowait, &cpu->irq, &cpu->softirq, &cpu->steal) != 8) {
        fclose(file);
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        unsigned int index;

        if (sscanf(line, "cpu%u ", &index) == 1)
            cpu->count++;
    }
    fclose(file);
    return cpu->count > 0 ? 0 : -1;
}

static int read_memory(const char *proc_root, struct memory_metrics *memory)
{
    char path[256];
    char line[256];
    FILE *file;
    uint64_t free_kib = 0;
    uint64_t buffers_kib = 0;
    uint64_t cached_kib = 0;
    int have_total = 0;
    int have_available = 0;

    if (make_path(path, sizeof(path), proc_root, "meminfo") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;

    while (fgets(line, sizeof(line), file) != NULL) {
        uint64_t value;

        if (sscanf(line, "MemTotal: %" SCNu64 " kB", &value) == 1) {
            memory->total_bytes = value * 1024;
            have_total = 1;
        } else if (sscanf(line, "MemAvailable: %" SCNu64 " kB",
                          &value) == 1) {
            memory->available_bytes = value * 1024;
            have_available = 1;
        } else if (sscanf(line, "MemFree: %" SCNu64 " kB", &value) == 1) {
            free_kib = value;
        } else if (sscanf(line, "Buffers: %" SCNu64 " kB", &value) == 1) {
            buffers_kib = value;
        } else if (sscanf(line, "Cached: %" SCNu64 " kB", &value) == 1) {
            cached_kib = value;
        }
    }
    fclose(file);

    if (!have_available)
        memory->available_bytes =
            (free_kib + buffers_kib + cached_kib) * 1024;
    return have_total ? 0 : -1;
}

static int read_uptime(const char *proc_root, double *uptime)
{
    char path[256];
    FILE *file;
    int result;

    if (make_path(path, sizeof(path), proc_root, "uptime") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;
    result = fscanf(file, "%lf", uptime);
    fclose(file);
    return result == 1 && *uptime >= 0 ? 0 : -1;
}

static int read_load(const char *proc_root, struct system_metrics *metrics)
{
    char path[256];
    FILE *file;
    int result;

    if (make_path(path, sizeof(path), proc_root, "loadavg") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;
    result = fscanf(file, "%lf %lf %lf", &metrics->load_1, &metrics->load_5,
                    &metrics->load_15);
    fclose(file);
    return result == 3 ? 0 : -1;
}

int system_metrics_collect(const char *proc_root, struct system_metrics *metrics)
{
    if (proc_root == NULL || metrics == NULL) {
        errno = EINVAL;
        return -1;
    }

    memset(metrics, 0, sizeof(*metrics));
    if (read_cpu(proc_root, &metrics->cpu) != 0 ||
        read_memory(proc_root, &metrics->memory) != 0 ||
        read_uptime(proc_root, &metrics->uptime_seconds) != 0 ||
        read_load(proc_root, metrics) != 0)
        return -1;
    return 0;
}
