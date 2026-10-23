/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "http_server.h"

#include "network_info.h"
#include "system_info.h"
#include "system_metrics.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STATUS_JSON_CAPACITY (64U * 1024U)
#define STATIC_FILE_LIMIT (256U * 1024U)

struct json_buffer {
    char *data;
    size_t capacity;
    size_t length;
    int failed;
};

static void json_append(struct json_buffer *json, const char *format, ...)
{
    va_list arguments;
    int written;

    if (json->failed)
        return;
    va_start(arguments, format);
    written = vsnprintf(json->data + json->length,
                        json->capacity - json->length, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= json->capacity - json->length) {
        json->failed = 1;
        return;
    }
    json->length += (size_t)written;
}

static void json_string(struct json_buffer *json, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    json_append(json, "\"");
    while (!json->failed && *cursor != '\0') {
        switch (*cursor) {
        case '"':
            json_append(json, "\\\"");
            break;
        case '\\':
            json_append(json, "\\\\");
            break;
        case '\b':
            json_append(json, "\\b");
            break;
        case '\f':
            json_append(json, "\\f");
            break;
        case '\n':
            json_append(json, "\\n");
            break;
        case '\r':
            json_append(json, "\\r");
            break;
        case '\t':
            json_append(json, "\\t");
            break;
        default:
            if (*cursor < 0x20)
                json_append(json, "\\u%04x", *cursor);
            else
                json_append(json, "%c", *cursor);
        }
        cursor++;
    }
    json_append(json, "\"");
}

static char *build_status_json(size_t *length)
{
    struct system_info system;
    struct system_metrics metrics;
    struct network_info network;
    struct json_buffer json;
    char *data = calloc(1, STATUS_JSON_CAPACITY);
    size_t interface_index;

    if (data == NULL)
        return NULL;
    if (system_info_collect("/proc", "/sys", "/etc", "/usr/lib", &system) !=
            0 ||
        system_metrics_collect("/proc", &metrics) != 0 ||
        network_info_collect("/proc", "/sys", &network) != 0) {
        free(data);
        return NULL;
    }

    json = (struct json_buffer){
        .data = data,
        .capacity = STATUS_JSON_CAPACITY,
    };
    json_append(&json, "{\"schema_version\":\"1.0\",\"system\":{");
    json_append(&json, "\"hostname\":");
    json_string(&json, system.hostname);
    json_append(&json, ",\"model\":");
    json_string(&json, system.model);
    json_append(&json, ",\"kernel_release\":");
    json_string(&json, system.kernel_release);
    json_append(&json, ",\"architecture\":");
    json_string(&json, system.architecture);
    json_append(&json, ",\"init\":");
    json_string(&json, system.init);
    json_append(&json, ",\"root_filesystem\":{\"source\":");
    json_string(&json, system.root_source);
    json_append(&json, ",\"type\":");
    json_string(&json, system.root_type);
    json_append(&json, "},\"os\":{\"pretty_name\":");
    json_string(&json, system.os.pretty_name);
    json_append(&json, ",\"id\":");
    json_string(&json, system.os.id);
    json_append(&json, ",\"version_id\":");
    json_string(&json, system.os.version_id);
    json_append(&json, ",\"id_like\":");
    json_string(&json, system.os.id_like);
    json_append(&json, ",\"family\":");
    json_string(&json, system.os.family);

    json_append(&json, "}},\"cpu\":{\"count\":%u,"
                        "\"cumulative_jiffies\":{\"user\":%llu,"
                        "\"nice\":%llu,\"system\":%llu,\"idle\":%llu,"
                        "\"iowait\":%llu,\"irq\":%llu,\"softirq\":%llu,"
                        "\"steal\":%llu}},",
                metrics.cpu.count, (unsigned long long)metrics.cpu.user,
                (unsigned long long)metrics.cpu.nice,
                (unsigned long long)metrics.cpu.system,
                (unsigned long long)metrics.cpu.idle,
                (unsigned long long)metrics.cpu.iowait,
                (unsigned long long)metrics.cpu.irq,
                (unsigned long long)metrics.cpu.softirq,
                (unsigned long long)metrics.cpu.steal);
    json_append(&json,
                "\"runtime\":{\"uptime_seconds\":%.2f,"
                "\"load_average\":{\"one_minute\":%.2f,\"five_minutes\":%.2f,"
                "\"fifteen_minutes\":%.2f}},",
                metrics.uptime_seconds, metrics.load_1, metrics.load_5,
                metrics.load_15);
    json_append(&json,
                "\"memory\":{\"total_bytes\":%llu,\"available_bytes\":%llu},",
                (unsigned long long)metrics.memory.total_bytes,
                (unsigned long long)metrics.memory.available_bytes);
    json_append(&json, "\"network\":{\"default_route\":{\"interface\":");
    json_string(&json, network.default_interface);
    json_append(&json, ",\"gateway\":");
    json_string(&json, network.default_gateway);
    json_append(&json, "},\"interfaces\":[");

    for (interface_index = 0; interface_index < network.interface_count;
         interface_index++) {
        const struct network_interface *interface =
            &network.interfaces[interface_index];
        size_t address_index;

        json_append(&json, "%s{\"name\":",
                    interface_index == 0 ? "" : ",");
        json_string(&json, interface->name);
        json_append(&json, ",\"operational_state\":");
        json_string(&json, interface->operational_state);
        json_append(&json, ",\"carrier\":");
        if (interface->carrier < 0)
            json_append(&json, "null");
        else
            json_append(&json, "%s", interface->carrier ? "true" : "false");
        json_append(&json, ",\"mtu_bytes\":%u,\"mac_address\":",
                    interface->mtu);
        json_string(&json, interface->mac_address);
        json_append(&json, ",\"addresses\":[");
        for (address_index = 0; address_index < interface->address_count;
             address_index++) {
            json_append(&json, "%s", address_index == 0 ? "" : ",");
            json_string(&json, interface->addresses[address_index]);
        }
        json_append(
            &json,
            "],\"rx_bytes\":%llu,\"tx_bytes\":%llu,"
            "\"rx_errors\":%llu,\"tx_errors\":%llu,"
            "\"rx_dropped\":%llu,\"tx_dropped\":%llu}",
            (unsigned long long)interface->rx_bytes,
            (unsigned long long)interface->tx_bytes,
            (unsigned long long)interface->rx_errors,
            (unsigned long long)interface->tx_errors,
            (unsigned long long)interface->rx_dropped,
            (unsigned long long)interface->tx_dropped);
    }
    json_append(&json, "]}}\n");

    if (json.failed) {
        free(data);
        return NULL;
    }
    *length = json.length;
    return data;
}

static enum MHD_Result queue_response(struct MHD_Connection *connection,
                                      unsigned int status, const char *body,
                                      size_t length, const char *content_type,
                                      int no_store)
{
    struct MHD_Response *response =
        MHD_create_response_from_buffer(length, (void *)body,
                                        MHD_RESPMEM_MUST_COPY);
    enum MHD_Result result;

    if (response == NULL)
        return MHD_NO;
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
                            content_type);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION, "close");
    if (no_store)
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL,
                                "no-store");
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result queue_json_error(struct MHD_Connection *connection,
                                        unsigned int status,
                                        const char *error)
{
    char body[256];
    int length = snprintf(body, sizeof(body), "{\"error\":\"%s\"}\n", error);

    if (length < 0 || (size_t)length >= sizeof(body))
        return MHD_NO;
    return queue_response(connection, status, body, (size_t)length,
                          "application/json", 1);
}

static enum MHD_Result handle_request(void *context,
                                      struct MHD_Connection *connection,
                                      const char *url, const char *method,
                                      const char *version,
                                      const char *upload_data,
                                      size_t *upload_data_size,
                                      void **connection_context)
{
    static int request_marker;
    struct http_server *server = context;
    int is_head;

    (void)version;
    (void)upload_data;
    if (*connection_context == NULL) {
        *connection_context = &request_marker;
        return MHD_YES;
    }
    if (*upload_data_size != 0) {
        *upload_data_size = 0;
        return MHD_YES;
    }

    is_head = strcmp(method, MHD_HTTP_METHOD_HEAD) == 0;
    if (!is_head && strcmp(method, MHD_HTTP_METHOD_GET) != 0) {
        struct MHD_Response *response;
        enum MHD_Result result;
        static const char body[] = "{\"error\":\"method not allowed\"}\n";

        response = MHD_create_response_from_buffer(
            sizeof(body) - 1, (void *)body, MHD_RESPMEM_PERSISTENT);
        if (response == NULL)
            return MHD_NO;
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
                                "application/json");
        MHD_add_response_header(response, MHD_HTTP_HEADER_ALLOW, "GET, HEAD");
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION, "close");
        result =
            MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                               response);
        MHD_destroy_response(response);
        return result;
    }

    if (strcmp(url, "/healthz") == 0)
        return queue_response(connection, MHD_HTTP_NO_CONTENT, "", 0,
                              "text/plain", 1);
    if (strcmp(url, "/") == 0)
        return queue_response(connection, MHD_HTTP_OK, server->index_html,
                              server->index_html_size,
                              "text/html; charset=utf-8", 0);
    if (strcmp(url, "/api/v1/status") == 0) {
        size_t length;
        char *body = build_status_json(&length);
        enum MHD_Result result;

        if (body == NULL)
            return queue_json_error(connection,
                                    MHD_HTTP_SERVICE_UNAVAILABLE,
                                    "system data unavailable");
        result = queue_response(connection, MHD_HTTP_OK, body, length,
                                "application/json", 1);
        free(body);
        return result;
    }
    return queue_json_error(connection, MHD_HTTP_NOT_FOUND, "not found");
}

static int load_index(struct http_server *server, const char *web_root)
{
    char path[512];
    struct stat status;
    FILE *file;

    if (snprintf(path, sizeof(path), "%s/index.html", web_root) < 0 ||
        stat(path, &status) != 0 || status.st_size < 0 ||
        (uint64_t)status.st_size > STATIC_FILE_LIMIT)
        return -1;
    file = fopen(path, "rb");
    if (file == NULL)
        return -1;
    server->index_html = malloc((size_t)status.st_size);
    if (server->index_html == NULL) {
        fclose(file);
        return -1;
    }
    server->index_html_size =
        fread(server->index_html, 1, (size_t)status.st_size, file);
    fclose(file);
    if (server->index_html_size != (size_t)status.st_size) {
        free(server->index_html);
        server->index_html = NULL;
        return -1;
    }
    return 0;
}

int http_server_start(struct http_server *server, const char *bind_address,
                      uint16_t port, const char *web_root)
{
    unsigned int flags =
        MHD_USE_ERROR_LOG | MHD_USE_EPOLL_INTERNAL_THREAD;

    if (server == NULL || bind_address == NULL || web_root == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(server, 0, sizeof(*server));
    if (load_index(server, web_root) != 0)
        return -1;
    server->bind_address.sin_family = AF_INET;
    server->bind_address.sin_port = htons(port);
    if (inet_pton(AF_INET, bind_address,
                  &server->bind_address.sin_addr) != 1) {
        free(server->index_html);
        server->index_html = NULL;
        errno = EINVAL;
        return -1;
    }

    server->daemon = MHD_start_daemon(
        flags, port, NULL, NULL, handle_request, server,
        MHD_OPTION_SOCK_ADDR, (struct sockaddr *)&server->bind_address,
        MHD_OPTION_CONNECTION_LIMIT, (unsigned int)4,
        MHD_OPTION_PER_IP_CONNECTION_LIMIT, (unsigned int)4,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)5,
        MHD_OPTION_CONNECTION_MEMORY_LIMIT, (size_t)(32U * 1024U),
        MHD_OPTION_END);
    if (server->daemon == NULL) {
        free(server->index_html);
        server->index_html = NULL;
        return -1;
    }
    return 0;
}

void http_server_stop(struct http_server *server)
{
    if (server == NULL)
        return;
    if (server->daemon != NULL)
        MHD_stop_daemon(server->daemon);
    free(server->index_html);
    memset(server, 0, sizeof(*server));
}
