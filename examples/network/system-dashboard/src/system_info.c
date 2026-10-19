/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */

#include "system_info.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/utsname.h>
#include <unistd.h>

static int make_path(char *path, size_t size, const char *root,
                     const char *name)
{
    int length = snprintf(path, size, "%s/%s", root, name);

    return length >= 0 && (size_t)length < size ? 0 : -1;
}

static void trim_line(char *value)
{
    size_t length = strlen(value);

    while (length > 0 &&
           (value[length - 1] == '\n' || value[length - 1] == '\r' ||
            isspace((unsigned char)value[length - 1])))
        value[--length] = '\0';
}

static int read_line(const char *path, char *value, size_t size)
{
    FILE *file = fopen(path, "r");

    if (file == NULL)
        return -1;
    if (fgets(value, size, file) == NULL) {
        fclose(file);
        return -1;
    }
    fclose(file);
    trim_line(value);
    return 0;
}

static int parse_value(const char *input, char *output, size_t size)
{
    const char *cursor = input;
    size_t used = 0;
    char quote = '\0';

    while (isspace((unsigned char)*cursor))
        cursor++;
    if (*cursor == '\'' || *cursor == '"')
        quote = *cursor++;

    while (*cursor != '\0' && *cursor != '\n' && *cursor != '\r') {
        char value = *cursor++;

        if (quote != '\0' && value == quote)
            break;
        if (value == '\\' && *cursor != '\0' &&
            (quote == '"' || *cursor == '\\' || *cursor == quote))
            value = *cursor++;
        if (used + 1 >= size)
            return -1;
        output[used++] = value;
    }
    output[used] = '\0';
    if (quote == '\0')
        trim_line(output);
    return 0;
}

int os_release_parse(FILE *file, struct os_identity *identity)
{
    char line[512];
    int found = 0;

    if (file == NULL || identity == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(identity, 0, sizeof(*identity));

    while (fgets(line, sizeof(line), file) != NULL) {
        char *separator;
        char *key = line;
        char *value;
        char *destination = NULL;
        size_t size = 0;

        while (isspace((unsigned char)*key))
            key++;
        if (*key == '#' || *key == '\0')
            continue;
        separator = strchr(key, '=');
        if (separator == NULL)
            continue;
        *separator = '\0';
        trim_line(key);
        value = separator + 1;

        if (strcmp(key, "PRETTY_NAME") == 0) {
            destination = identity->pretty_name;
            size = sizeof(identity->pretty_name);
        } else if (strcmp(key, "ID") == 0) {
            destination = identity->id;
            size = sizeof(identity->id);
        } else if (strcmp(key, "ID_LIKE") == 0) {
            destination = identity->id_like;
            size = sizeof(identity->id_like);
        } else if (strcmp(key, "VERSION_ID") == 0) {
            destination = identity->version_id;
            size = sizeof(identity->version_id);
        }
        if (destination != NULL && parse_value(value, destination, size) == 0)
            found++;
    }
    return found > 0 ? 0 : -1;
}

static int read_os_release(const char *etc_root, const char *usr_lib_root,
                           struct os_identity *identity)
{
    char path[256];
    FILE *file;
    int result;

    if (make_path(path, sizeof(path), etc_root, "os-release") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL) {
        if (make_path(path, sizeof(path), usr_lib_root, "os-release") != 0)
            return -1;
        file = fopen(path, "r");
    }
    if (file == NULL)
        return -1;
    result = os_release_parse(file, identity);
    fclose(file);
    return result;
}

static int read_openwrt_release(const char *etc_root,
                                struct os_identity *identity)
{
    char path[256];
    char line[512];
    FILE *file;
    int found = 0;

    if (make_path(path, sizeof(path), etc_root, "openwrt_release") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *separator = strchr(line, '=');
        char *destination = NULL;
        size_t size = 0;

        if (separator == NULL)
            continue;
        *separator = '\0';
        trim_line(line);
        if (strcmp(line, "DISTRIB_DESCRIPTION") == 0) {
            destination = identity->pretty_name;
            size = sizeof(identity->pretty_name);
        } else if (strcmp(line, "DISTRIB_ID") == 0) {
            destination = identity->id;
            size = sizeof(identity->id);
        } else if (strcmp(line, "DISTRIB_RELEASE") == 0) {
            destination = identity->version_id;
            size = sizeof(identity->version_id);
        }
        if (destination != NULL &&
            parse_value(separator + 1, destination, size) == 0)
            found++;
    }
    fclose(file);
    return found > 0 ? 0 : -1;
}

static int read_buildroot_release(const char *etc_root,
                                  struct os_identity *identity)
{
    char path[256];
    char release[sizeof(identity->version_id)];

    if (make_path(path, sizeof(path), etc_root, "buildroot-release") != 0 ||
        read_line(path, release, sizeof(release)) != 0)
        return -1;
    if (strncasecmp(release, "Buildroot", 9) == 0)
        snprintf(identity->pretty_name, sizeof(identity->pretty_name), "%s",
                 release);
    else
        snprintf(identity->pretty_name, sizeof(identity->pretty_name),
                 "Buildroot %s", release);
    snprintf(identity->id, sizeof(identity->id), "buildroot");
    snprintf(identity->version_id, sizeof(identity->version_id), "%s",
             release);
    return 0;
}

static void read_issue_fallback(const char *etc_root,
                                struct os_identity *identity)
{
    char path[256];

    if (make_path(path, sizeof(path), etc_root, "issue") != 0 ||
        read_line(path, identity->pretty_name,
                  sizeof(identity->pretty_name)) != 0)
        snprintf(identity->pretty_name, sizeof(identity->pretty_name),
                 "Unknown Linux");
}

static int contains_case_insensitive(const char *value, const char *needle)
{
    size_t needle_length = strlen(needle);

    while (*value != '\0') {
        if (strncasecmp(value, needle, needle_length) == 0)
            return 1;
        value++;
    }
    return 0;
}

static int id_like_contains(const char *id_like, const char *id)
{
    size_t id_length = strlen(id);

    while (*id_like != '\0') {
        const char *token;
        size_t token_length;

        while (isspace((unsigned char)*id_like))
            id_like++;
        token = id_like;
        while (*id_like != '\0' && !isspace((unsigned char)*id_like))
            id_like++;
        token_length = (size_t)(id_like - token);
        if (token_length == id_length &&
            strncasecmp(token, id, id_length) == 0)
            return 1;
    }
    return 0;
}

static void identify_os_family(struct os_identity *identity)
{
    const char *family = "Linux";

    if (strcasecmp(identity->id, "openwrt") == 0 ||
        contains_case_insensitive(identity->pretty_name, "openwrt"))
        family = "OpenWrt";
    else if (strcasecmp(identity->id, "buildroot") == 0 ||
             contains_case_insensitive(identity->pretty_name, "buildroot"))
        family = "Buildroot";
    else if (strcasecmp(identity->id, "poky") == 0 ||
             id_like_contains(identity->id_like, "poky") ||
             id_like_contains(identity->id_like, "openembedded") ||
             id_like_contains(identity->id_like, "yocto"))
        family = "Yocto";
    snprintf(identity->family, sizeof(identity->family), "%s", family);
}

static int read_root_mount(const char *proc_root, struct system_info *info)
{
    char path[256];
    char line[1024];
    FILE *file;

    if (make_path(path, sizeof(path), proc_root, "mounts") != 0)
        return -1;
    file = fopen(path, "r");
    if (file == NULL)
        return -1;
    while (fgets(line, sizeof(line), file) != NULL) {
        char source[256];
        char mountpoint[256];
        char type[64];

        if (sscanf(line, "%255s %255s %63s", source, mountpoint, type) == 3 &&
            strcmp(mountpoint, "/") == 0) {
            snprintf(info->root_source, sizeof(info->root_source), "%s",
                     source);
            snprintf(info->root_type, sizeof(info->root_type), "%s", type);
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return -1;
}

int system_info_collect(const char *proc_root, const char *sys_root,
                        const char *etc_root, const char *usr_lib_root,
                        struct system_info *info)
{
    char path[256];
    struct utsname uts;

    if (proc_root == NULL || sys_root == NULL || etc_root == NULL ||
        usr_lib_root == NULL || info == NULL) {
        errno = EINVAL;
        return -1;
    }
    memset(info, 0, sizeof(*info));

    if (read_os_release(etc_root, usr_lib_root, &info->os) != 0 &&
        read_openwrt_release(etc_root, &info->os) != 0 &&
        read_buildroot_release(etc_root, &info->os) != 0)
        read_issue_fallback(etc_root, &info->os);
    identify_os_family(&info->os);
    if (gethostname(info->hostname, sizeof(info->hostname) - 1) != 0)
        snprintf(info->hostname, sizeof(info->hostname), "unknown");
    if (uname(&uts) == 0) {
        snprintf(info->kernel_release, sizeof(info->kernel_release), "%s",
                 uts.release);
        snprintf(info->architecture, sizeof(info->architecture), "%s",
                 uts.machine);
    }

    if (make_path(path, sizeof(path), sys_root, "firmware/devicetree/base/model")
            != 0 ||
        read_line(path, info->model, sizeof(info->model)) != 0)
        snprintf(info->model, sizeof(info->model), "Unknown device");
    if (make_path(path, sizeof(path), proc_root, "1/comm") != 0 ||
        read_line(path, info->init, sizeof(info->init)) != 0)
        snprintf(info->init, sizeof(info->init), "unknown");
    if (read_root_mount(proc_root, info) != 0) {
        snprintf(info->root_source, sizeof(info->root_source), "unknown");
        snprintf(info->root_type, sizeof(info->root_type), "unknown");
    }
    return 0;
}
