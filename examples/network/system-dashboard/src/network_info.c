/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "network_info.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static int make_path(char *path, size_t size, const char *root,
                     const char *name)
{
    int length = snprintf(path, size, "%s/%s", root, name);

    return length >= 0 && (size_t)length < size ? 0 : -1;
}

static int read_line(const char *path, char *value, size_t size)
{
    FILE *file = fopen(path, "r");
    size_t length;

    if (file == NULL)
        return -1;
    if (fgets(value, size, file) == NULL) {
        fclose(file);
        return -1;
    }
    fclose(file);
    length = strcspn(value, "\r\n");
    value[length] = '\0';
    return 0;
}

static int read_uint64(const char *path, uint64_t *value)
{
    FILE *file = fopen(path, "r");
    int result;

    if (file == NULL)
        return -1;
    result = fscanf(file, "%" SCNu64, value);
    fclose(file);
    return result == 1 ? 0 : -1;
}

static struct network_interface *find_interface(struct network_info *info,
                                                const char *name)
{
    size_t index;

    for (index = 0; index < info->interface_count; index++) {
        if (strcmp(info->interfaces[index].name, name) == 0)
            return &info->interfaces[index];
    }
    return NULL;
}

static struct network_interface *add_interface(struct network_info *info,
                                               const char *name)
{
    struct network_interface *interface;

    if (info->interface_count >= NETWORK_MAX_INTERFACES)
        return NULL;
    interface = &info->interfaces[info->interface_count++];
    memset(interface, 0, sizeof(*interface));
    interface->carrier = -1;
    snprintf(interface->name, sizeof(interface->name), "%s", name);
    return interface;
}

static void read_interface_value(const char *sys_root, const char *name,
                                 const char *entry, char *value, size_t size)
{
    char relative[256];
    char path[512];

    if (snprintf(relative, sizeof(relative), "class/net/%s/%s", name, entry) <
            0 ||
        make_path(path, sizeof(path), sys_root, relative) != 0 ||
        read_line(path, value, size) != 0)
        value[0] = '\0';
}

static uint64_t read_interface_stat(const char *sys_root, const char *name,
                                    const char *entry)
{
    char relative[256];
    char path[512];
    uint64_t value = 0;

    if (snprintf(relative, sizeof(relative), "class/net/%s/statistics/%s",
                 name, entry) < 0 ||
        make_path(path, sizeof(path), sys_root, relative) != 0)
        return 0;
    read_uint64(path, &value);
    return value;
}

static void fill_interface(const char *sys_root,
                           struct network_interface *interface)
{
    char value[64];
    char *end;
    unsigned long number;

    read_interface_value(sys_root, interface->name, "operstate",
                         interface->operational_state,
                         sizeof(interface->operational_state));
    read_interface_value(sys_root, interface->name, "address",
                         interface->mac_address,
                         sizeof(interface->mac_address));
    read_interface_value(sys_root, interface->name, "mtu", value,
                         sizeof(value));
    errno = 0;
    number = strtoul(value, &end, 10);
    if (errno == 0 && end != value && *end == '\0' && number <= UINT32_MAX)
        interface->mtu = (unsigned int)number;
    read_interface_value(sys_root, interface->name, "carrier", value,
                         sizeof(value));
    if (strcmp(value, "0") == 0 || strcmp(value, "1") == 0)
        interface->carrier = value[0] - '0';

    interface->rx_bytes =
        read_interface_stat(sys_root, interface->name, "rx_bytes");
    interface->tx_bytes =
        read_interface_stat(sys_root, interface->name, "tx_bytes");
    interface->rx_errors =
        read_interface_stat(sys_root, interface->name, "rx_errors");
    interface->tx_errors =
        read_interface_stat(sys_root, interface->name, "tx_errors");
    interface->rx_dropped =
        read_interface_stat(sys_root, interface->name, "rx_dropped");
    interface->tx_dropped =
        read_interface_stat(sys_root, interface->name, "tx_dropped");
}

int network_default_route_parse(FILE *file, char *interface_name,
                                size_t interface_size, char *gateway,
                                size_t gateway_size)
{
    char line[512];

    if (file == NULL || interface_name == NULL || interface_size == 0 ||
        gateway == NULL || gateway_size == 0) {
        errno = EINVAL;
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char name[IFNAMSIZ];
        unsigned long destination;
        unsigned long route_gateway;
        unsigned int flags;

        if (sscanf(line, "%15s %lx %lx %x", name, &destination,
                   &route_gateway, &flags) != 4)
            continue;
        if (destination != 0 || (flags & 0x1U) == 0)
            continue;
        snprintf(interface_name, interface_size, "%s", name);
        if ((flags & 0x2U) != 0)
            snprintf(gateway, gateway_size, "%lu.%lu.%lu.%lu",
                     route_gateway & 0xffUL,
                     (route_gateway >> 8) & 0xffUL,
                     (route_gateway >> 16) & 0xffUL,
                     (route_gateway >> 24) & 0xffUL);
        else
            gateway[0] = '\0';
        return 0;
    }
    return -1;
}

static void read_default_route(const char *proc_root, struct network_info *info)
{
    char path[256];
    FILE *file;

    if (make_path(path, sizeof(path), proc_root, "net/route") != 0)
        return;
    file = fopen(path, "r");
    if (file == NULL)
        return;
    network_default_route_parse(file, info->default_interface,
                                sizeof(info->default_interface),
                                info->default_gateway,
                                sizeof(info->default_gateway));
    fclose(file);
}

static int enumerate_interfaces(const char *sys_root, struct network_info *info)
{
    char path[256];
    struct dirent *entry;
    DIR *directory;

    if (make_path(path, sizeof(path), sys_root, "class/net") != 0)
        return -1;
    directory = opendir(path);
    if (directory == NULL)
        return -1;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "lo") == 0)
            continue;
        if (find_interface(info, entry->d_name) == NULL &&
            add_interface(info, entry->d_name) == NULL)
            break;
    }
    closedir(directory);
    return 0;
}

int network_info_collect(const char *proc_root, const char *sys_root,
                         struct network_info *info)
{
    struct ifaddrs *addresses;
    struct ifaddrs *entry;

    if (proc_root == NULL || sys_root == NULL || info == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(info, 0, sizeof(*info));
    if (enumerate_interfaces(sys_root, info) != 0)
        return -1;
    if (getifaddrs(&addresses) != 0)
        return -1;

    for (entry = addresses; entry != NULL; entry = entry->ifa_next) {
        struct network_interface *interface;
        void *source;
        char address[NETWORK_ADDRESS_LENGTH];

        if (entry->ifa_name == NULL || entry->ifa_addr == NULL ||
            (entry->ifa_flags & IFF_LOOPBACK) != 0)
            continue;
        interface = find_interface(info, entry->ifa_name);
        if (interface == NULL)
            interface = add_interface(info, entry->ifa_name);
        if (interface == NULL ||
            interface->address_count >= NETWORK_MAX_ADDRESSES)
            continue;

        if (entry->ifa_addr->sa_family == AF_INET)
            source = &((struct sockaddr_in *)entry->ifa_addr)->sin_addr;
        else if (entry->ifa_addr->sa_family == AF_INET6)
            source = &((struct sockaddr_in6 *)entry->ifa_addr)->sin6_addr;
        else
            continue;
        if (inet_ntop(entry->ifa_addr->sa_family, source, address,
                      sizeof(address)) == NULL)
            continue;
        snprintf(interface->addresses[interface->address_count++],
                 NETWORK_ADDRESS_LENGTH, "%s", address);
    }
    freeifaddrs(addresses);

    for (size_t index = 0; index < info->interface_count; index++)
        fill_interface(sys_root, &info->interfaces[index]);
    read_default_route(proc_root, info);
    return 0;
}
