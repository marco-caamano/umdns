#ifndef UMDNS_MDNS_H
#define UMDNS_MDNS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "umdns/common.h"

typedef struct umdns_question {
    char name[UMDNS_MAX_NAME];
    uint16_t qtype;
} umdns_question_t;

typedef struct umdns_service {
    char instance[UMDNS_MAX_NAME];
    char type[UMDNS_MAX_NAME];
    char host[UMDNS_MAX_NAME];
    uint16_t port;
    char txt[UMDNS_MAX_TXT];
} umdns_service_t;

size_t umdns_mdns_build_query(uint16_t qtype, const char *name, uint8_t *out, size_t out_len);
size_t umdns_mdns_build_response_hostname(const char *hostname, const char *ipv4, const char *ipv6, const char *txt, uint8_t *out, size_t out_len);
size_t umdns_mdns_build_response_service(const umdns_service_t *service, uint8_t *out, size_t out_len);
size_t umdns_mdns_build_response_service_enum(const umdns_service_t *services, size_t service_count, uint8_t *out, size_t out_len);

int umdns_mdns_parse_question(const uint8_t *packet, size_t packet_len, umdns_question_t *question);
size_t umdns_mdns_parse_answers(const uint8_t *packet, size_t packet_len, umdns_result_t *results, size_t max_results);

#endif
