/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <microhttpd.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

struct http_server {
    struct MHD_Daemon *daemon;
    struct sockaddr_in bind_address;
    char *index_html;
    size_t index_html_size;
};

int http_server_start(struct http_server *server, const char *bind_address,
                      uint16_t port, const char *web_root);
void http_server_stop(struct http_server *server);

#endif
