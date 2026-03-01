#ifndef UMDNS_COMMON_H
#define UMDNS_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UMDNS_MAX_NAME 256
#define UMDNS_MAX_TXT 512
#define UMDNS_MAX_SERVICES 32
#define UMDNS_MAX_RESULTS 256

#define UMDNS_MDNS_PORT 5353
#define UMDNS_MCAST_IPV4 "224.0.0.251"
#define UMDNS_MCAST_IPV6 "ff02::fb"

#define UMDNS_RR_A 1
#define UMDNS_RR_PTR 12
#define UMDNS_RR_TXT 16
#define UMDNS_RR_AAAA 28
#define UMDNS_RR_SRV 33

typedef enum umdns_query_mode {
    UMDNS_QUERY_HOSTNAME = 0,
    UMDNS_QUERY_SERVICE = 1
} umdns_query_mode_t;

typedef struct umdns_result {
    char instance[UMDNS_MAX_NAME];
    char hostname[UMDNS_MAX_NAME];
    char service_type[UMDNS_MAX_NAME];
    char address[UMDNS_MAX_NAME];
    uint16_t port;
    char txt[UMDNS_MAX_TXT];
    uint16_t rrtype;
} umdns_result_t;

#endif
