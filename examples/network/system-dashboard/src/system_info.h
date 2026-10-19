/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <stddef.h>
#include <stdio.h>

struct os_identity {
    char pretty_name[128];
    char id[64];
    char id_like[128];
    char version_id[64];
    char family[32];
};

struct system_info {
    struct os_identity os;
    char hostname[128];
    char model[256];
    char kernel_release[128];
    char architecture[128];
    char init[64];
    char root_source[256];
    char root_type[64];
};

int os_release_parse(FILE *file, struct os_identity *identity);
int system_info_collect(const char *proc_root, const char *sys_root,
                        const char *etc_root, const char *usr_lib_root,
                        struct system_info *info);

#endif
