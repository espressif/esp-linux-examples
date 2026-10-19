/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "http_server.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DASHBOARD_WEB_ROOT
#define DASHBOARD_WEB_ROOT "/usr/share/esp-system-dashboard"
#endif

static void usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage: %s [--bind ADDRESS] [--port PORT] [--web-root PATH]\n"
            "\n"
            "  --bind ADDRESS  IPv4 address to listen on (default: 0.0.0.0)\n"
            "  --port PORT     TCP port to listen on (default: 8080)\n"
            "  --web-root PATH directory containing index.html\n"
            "  --help          show this help\n",
            program);
}

static int parse_port(const char *value, uint16_t *port)
{
    char *end;
    unsigned long number;

    errno = 0;
    number = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || number == 0 ||
        number > UINT16_MAX)
        return -1;
    *port = (uint16_t)number;
    return 0;
}

int main(int argc, char **argv)
{
    const char *bind_address = "0.0.0.0";
    const char *web_root = DASHBOARD_WEB_ROOT;
    uint16_t port = 8080;
    struct http_server server;
    sigset_t signals;
    int signal_number;
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--help") == 0) {
            usage(stdout, argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[index], "--bind") == 0 && index + 1 < argc) {
            bind_address = argv[++index];
        } else if (strcmp(argv[index], "--port") == 0 && index + 1 < argc) {
            if (parse_port(argv[++index], &port) != 0) {
                fprintf(stderr, "Invalid port: %s\n", argv[index]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[index], "--web-root") == 0 &&
                   index + 1 < argc) {
            web_root = argv[++index];
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[index]);
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, NULL) != 0) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }
    if (http_server_start(&server, bind_address, port, web_root) != 0) {
        fprintf(stderr, "Failed to start server on %s:%u: %s\n",
                bind_address, port, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("ESP system dashboard listening on http://%s:%u/\n", bind_address,
           port);
    fflush(stdout);
    if (sigwait(&signals, &signal_number) != 0) {
        fprintf(stderr, "Failed while waiting for termination signal\n");
        http_server_stop(&server);
        return EXIT_FAILURE;
    }
    http_server_stop(&server);
    return EXIT_SUCCESS;
}
