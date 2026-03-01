#ifndef UMDNS_CONFIG_H
#define UMDNS_CONFIG_H

#include <stddef.h>

#include "umdns/common.h"
#include "umdns/mdns.h"

typedef struct umdns_server_config {
    char hostname[UMDNS_MAX_NAME];
    char ipv4[UMDNS_MAX_NAME];
    char ipv6[UMDNS_MAX_NAME];
    char host_txt[UMDNS_MAX_TXT];
    umdns_service_t services[UMDNS_MAX_SERVICES];
    size_t service_count;
} umdns_server_config_t;

void umdns_server_config_init(umdns_server_config_t *config);
int umdns_server_config_load(const char *path, umdns_server_config_t *config);

#endif
