#include "umdns/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *umdns_trim(char *text) {
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return text;
}

void umdns_server_config_init(umdns_server_config_t *config) {
    memset(config, 0, sizeof(*config));
    snprintf(config->hostname, sizeof(config->hostname), "%s", "umdns");
    snprintf(config->ipv4, sizeof(config->ipv4), "%s", "0.0.0.0");
    snprintf(config->ipv6, sizeof(config->ipv6), "%s", "::");
}

static void umdns_copy_value(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) {
        return;
    }
    snprintf(dst, dst_len, "%s", src);
}

static int umdns_find_or_create_service(umdns_server_config_t *config, const char *section_name) {
    size_t idx;

    for (idx = 0; idx < config->service_count; ++idx) {
        if (strcmp(config->services[idx].instance, section_name) == 0) {
            return (int)idx;
        }
    }

    if (config->service_count >= UMDNS_MAX_SERVICES) {
        return -1;
    }

    idx = config->service_count;
    memset(&config->services[idx], 0, sizeof(config->services[idx]));
    umdns_copy_value(config->services[idx].instance, sizeof(config->services[idx].instance), section_name);
    config->service_count += 1;
    return (int)idx;
}

int umdns_server_config_load(const char *path, umdns_server_config_t *config) {
    FILE *fp;
    char line[1024];
    int service_index = -1;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *trimmed = umdns_trim(line);
        char *eq;

        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            continue;
        }

        if (*trimmed == '[') {
            char *close = strchr(trimmed, ']');
            if (close == NULL) {
                continue;
            }
            *close = '\0';
            if (strncmp(trimmed + 1, "service ", 8) == 0) {
                service_index = umdns_find_or_create_service(config, trimmed + 1 + 8);
            } else {
                service_index = -1;
            }
            continue;
        }

        eq = strchr(trimmed, '=');
        if (eq == NULL) {
            continue;
        }

        *eq = '\0';
        ++eq;

        trimmed = umdns_trim(trimmed);
        eq = umdns_trim(eq);

        if (service_index >= 0) {
            umdns_service_t *svc = &config->services[(size_t)service_index];
            if (strcmp(trimmed, "type") == 0) {
                umdns_copy_value(svc->type, sizeof(svc->type), eq);
            } else if (strcmp(trimmed, "host") == 0) {
                umdns_copy_value(svc->host, sizeof(svc->host), eq);
            } else if (strcmp(trimmed, "port") == 0) {
                long value = strtol(eq, NULL, 10);
                if (value > 0 && value <= 65535) {
                    svc->port = (uint16_t)value;
                }
            } else if (strcmp(trimmed, "txt") == 0) {
                umdns_copy_value(svc->txt, sizeof(svc->txt), eq);
            }
        }
    }

    fclose(fp);
    return 0;
}
