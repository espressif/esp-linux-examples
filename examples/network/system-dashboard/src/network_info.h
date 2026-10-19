/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef NETWORK_INFO_H
#define NETWORK_INFO_H

#include <net/if.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define NETWORK_MAX_INTERFACES 16
#define NETWORK_MAX_ADDRESSES 8
#define NETWORK_ADDRESS_LENGTH 64

struct network_interface {
    char name[IFNAMSIZ];
    char operational_state[32];
    char mac_address[32];
    unsigned int mtu;
    int carrier;
    char addresses[NETWORK_MAX_ADDRESSES][NETWORK_ADDRESS_LENGTH];
    size_t address_count;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
};

struct network_info {
    struct network_interface interfaces[NETWORK_MAX_INTERFACES];
    size_t interface_count;
    char default_interface[IFNAMSIZ];
    char default_gateway[NETWORK_ADDRESS_LENGTH];
};

int network_default_route_parse(FILE *file, char *interface_name,
                                size_t interface_size, char *gateway,
                                size_t gateway_size);
int network_info_collect(const char *proc_root, const char *sys_root,
                         struct network_info *info);

#endif
