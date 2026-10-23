/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "network_info.h"
#include "system_info.h"
#include "system_metrics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void path_join(char *path, size_t size, const char *root,
                      const char *suffix)
{
    int length = snprintf(path, size, "%s/%s", root, suffix);

    assert(length >= 0 && (size_t)length < size);
}

static void test_yocto(const char *fixtures)
{
    struct system_metrics metrics;
    struct system_info info;
    char root[512];
    char proc[512];
    char sys[512];
    char etc[512];
    char usr_lib[512];

    path_join(root, sizeof(root), fixtures, "yocto");
    path_join(proc, sizeof(proc), root, "proc");
    path_join(sys, sizeof(sys), root, "sys");
    path_join(etc, sizeof(etc), root, "etc");
    path_join(usr_lib, sizeof(usr_lib), root, "usr/lib");

    assert(system_metrics_collect(proc, &metrics) == 0);
    assert(metrics.cpu.count == 2);
    assert(metrics.cpu.user == 100);
    assert(metrics.cpu.idle == 700);
    assert(metrics.memory.total_bytes == 262144ULL * 1024);
    assert(metrics.memory.available_bytes == 196608ULL * 1024);
    assert(metrics.uptime_seconds == 1234.5);
    assert(metrics.load_1 == 0.25);

    assert(system_info_collect(proc, sys, etc, usr_lib, &info) == 0);
    assert(strcmp(info.os.pretty_name, "ESP Linux (Yocto)") == 0);
    assert(strcmp(info.os.id, "esplinux") == 0);
    assert(strcmp(info.os.id_like, "poky") == 0);
    assert(strcmp(info.os.version_id, "1.0") == 0);
    assert(strcmp(info.os.family, "Yocto") == 0);
    assert(strcmp(info.model, "Espressif Test DevKit") == 0);
    assert(strcmp(info.init, "systemd") == 0);
    assert(strcmp(info.root_source, "192.168.7.1:/nfs/root") == 0);
    assert(strcmp(info.root_type, "nfs") == 0);
}

static void test_buildroot_fallback(const char *fixtures)
{
    struct system_info info;
    char root[512];
    char proc[512];
    char sys[512];
    char etc[512];
    char usr_lib[512];

    path_join(root, sizeof(root), fixtures, "buildroot");
    path_join(proc, sizeof(proc), root, "proc");
    path_join(sys, sizeof(sys), root, "sys");
    path_join(etc, sizeof(etc), root, "etc");
    path_join(usr_lib, sizeof(usr_lib), root, "usr/lib");

    assert(system_info_collect(proc, sys, etc, usr_lib, &info) == 0);
    assert(strcmp(info.os.pretty_name, "Buildroot 2026.08") == 0);
    assert(strcmp(info.os.id, "buildroot") == 0);
    assert(strcmp(info.os.version_id, "2026.08") == 0);
    assert(strcmp(info.os.family, "Buildroot") == 0);
    assert(strcmp(info.init, "init") == 0);
    assert(strcmp(info.root_source, "/dev/mtdblock3") == 0);
    assert(strcmp(info.root_type, "cramfs") == 0);
}

static void test_malformed_metrics(const char *fixtures)
{
    struct system_metrics metrics;
    char proc[512];

    path_join(proc, sizeof(proc), fixtures, "malformed/proc");
    assert(system_metrics_collect(proc, &metrics) != 0);
    path_join(proc, sizeof(proc), fixtures, "missing/proc");
    assert(system_metrics_collect(proc, &metrics) != 0);
}

static void test_issue_fallback(const char *fixtures)
{
    struct system_info info;
    char root[512];
    char proc[512];
    char sys[512];
    char etc[512];
    char usr_lib[512];

    path_join(root, sizeof(root), fixtures, "issue");
    path_join(proc, sizeof(proc), root, "proc");
    path_join(sys, sizeof(sys), root, "sys");
    path_join(etc, sizeof(etc), root, "etc");
    path_join(usr_lib, sizeof(usr_lib), root, "usr/lib");

    assert(system_info_collect(proc, sys, etc, usr_lib, &info) == 0);
    assert(strcmp(info.os.pretty_name, "Welcome to Espressif Linux") == 0);
    assert(strcmp(info.os.id, "") == 0);
    assert(strcmp(info.os.family, "Linux") == 0);
    assert(strcmp(info.root_source, "unknown") == 0);
}

static void test_release_fallbacks(const char *fixtures)
{
    struct system_info info;
    char root[512];
    char proc[512];
    char sys[512];
    char etc[512];
    char usr_lib[512];

    path_join(root, sizeof(root), fixtures, "openwrt");
    path_join(proc, sizeof(proc), root, "proc");
    path_join(sys, sizeof(sys), root, "sys");
    path_join(etc, sizeof(etc), root, "etc");
    path_join(usr_lib, sizeof(usr_lib), root, "usr/lib");
    assert(system_info_collect(proc, sys, etc, usr_lib, &info) == 0);
    assert(strcmp(info.os.pretty_name, "OpenWrt 24.10.2") == 0);
    assert(strcmp(info.os.id, "OpenWrt") == 0);
    assert(strcmp(info.os.version_id, "24.10.2") == 0);
    assert(strcmp(info.os.family, "OpenWrt") == 0);

    path_join(root, sizeof(root), fixtures, "buildroot-release");
    path_join(proc, sizeof(proc), root, "proc");
    path_join(sys, sizeof(sys), root, "sys");
    path_join(etc, sizeof(etc), root, "etc");
    path_join(usr_lib, sizeof(usr_lib), root, "usr/lib");
    assert(system_info_collect(proc, sys, etc, usr_lib, &info) == 0);
    assert(strcmp(info.os.pretty_name, "Buildroot 2026.08-custom") == 0);
    assert(strcmp(info.os.id, "buildroot") == 0);
    assert(strcmp(info.os.family, "Buildroot") == 0);
}

static void test_default_route(const char *fixtures)
{
    char path[512];
    char interface_name[IFNAMSIZ] = "";
    char gateway[64] = "";
    FILE *file;

    path_join(path, sizeof(path), fixtures, "yocto/proc/net/route");
    file = fopen(path, "r");
    assert(file != NULL);
    assert(network_default_route_parse(file, interface_name,
                                       sizeof(interface_name), gateway,
                                       sizeof(gateway)) == 0);
    fclose(file);
    assert(strcmp(interface_name, "eth0") == 0);
    assert(strcmp(gateway, "192.168.7.1") == 0);

    path_join(path, sizeof(path), fixtures,
              "yocto/proc/net/route-onlink");
    file = fopen(path, "r");
    assert(file != NULL);
    assert(network_default_route_parse(file, interface_name,
                                       sizeof(interface_name), gateway,
                                       sizeof(gateway)) == 0);
    fclose(file);
    assert(strcmp(interface_name, "eth1") == 0);
    assert(strcmp(gateway, "") == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_yocto(argv[1]);
    test_buildroot_fallback(argv[1]);
    test_malformed_metrics(argv[1]);
    test_issue_fallback(argv[1]);
    test_release_fallbacks(argv[1]);
    test_default_route(argv[1]);
    puts("parser tests passed");
    return 0;
}
