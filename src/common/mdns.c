#include "umdns/mdns.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct umdns_dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} umdns_dns_header_t;

static uint16_t umdns_read_u16(const uint8_t *buffer, size_t offset) {
    uint16_t value;
    memcpy(&value, buffer + offset, sizeof(value));
    return ntohs(value);
}

static uint32_t umdns_read_u32(const uint8_t *buffer, size_t offset) {
    uint32_t value;
    memcpy(&value, buffer + offset, sizeof(value));
    return ntohl(value);
}

static void umdns_write_u16(uint8_t *buffer, size_t offset, uint16_t value) {
    uint16_t network = htons(value);
    memcpy(buffer + offset, &network, sizeof(network));
}

static void umdns_write_u32(uint8_t *buffer, size_t offset, uint32_t value) {
    uint32_t network = htonl(value);
    memcpy(buffer + offset, &network, sizeof(network));
}

static void umdns_copy_string(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_len, "%s", src);
}

static int umdns_append_name(uint8_t *out, size_t out_len, size_t *offset, const char *name) {
    char copy[UMDNS_MAX_NAME];
    char *token;
    char *save = NULL;

    snprintf(copy, sizeof(copy), "%s", name);

    token = strtok_r(copy, ".", &save);
    while (token != NULL) {
        size_t token_len = strlen(token);
        if (token_len > 63 || *offset + token_len + 2 > out_len) {
            return -1;
        }
        out[*offset] = (uint8_t)token_len;
        *offset += 1;
        memcpy(out + *offset, token, token_len);
        *offset += token_len;
        token = strtok_r(NULL, ".", &save);
    }

    if (*offset + 1 > out_len) {
        return -1;
    }
    out[*offset] = 0;
    *offset += 1;
    return 0;
}

static int umdns_decode_name_internal(const uint8_t *packet,
                                      size_t packet_len,
                                      size_t *offset,
                                      char *out,
                                      size_t out_len,
                                      int depth) {
    size_t pos = *offset;
    size_t out_pos = 0;
    int jumped = 0;

    if (depth > 10) {
        return -1;
    }

    while (pos < packet_len) {
        uint8_t len = packet[pos];

        if (len == 0) {
            if (!jumped) {
                *offset = pos + 1;
            }
            break;
        }

        if ((len & 0xC0) == 0xC0) {
            size_t pointer;
            if (pos + 1 >= packet_len) {
                return -1;
            }
            pointer = (size_t)(((len & 0x3F) << 8) | packet[pos + 1]);
            if (pointer >= packet_len) {
                return -1;
            }
            if (!jumped) {
                *offset = pos + 2;
                jumped = 1;
            }
            pos = pointer;
            depth += 1;
            if (depth > 10) {
                return -1;
            }
            continue;
        }

        pos += 1;
        if (pos + len > packet_len || out_pos + len + 2 > out_len) {
            return -1;
        }

        memcpy(out + out_pos, packet + pos, len);
        out_pos += len;
        pos += len;

        if (pos < packet_len && packet[pos] != 0) {
            out[out_pos++] = '.';
        }
    }

    if (out_pos >= out_len) {
        return -1;
    }
    out[out_pos] = '\0';
    return 0;
}

static int umdns_decode_name(const uint8_t *packet, size_t packet_len, size_t *offset, char *out, size_t out_len) {
    return umdns_decode_name_internal(packet, packet_len, offset, out, out_len, 0);
}

static void umdns_normalize_local_name(const char *input, char *out, size_t out_len) {
    size_t input_len;

    if (input == NULL || input[0] == '\0') {
        umdns_copy_string(out, out_len, "localhost.local");
        return;
    }

    input_len = strlen(input);
    if (input_len >= 6 && strcasecmp(input + input_len - 6, ".local") == 0) {
        umdns_copy_string(out, out_len, input);
        return;
    }

    snprintf(out, out_len, "%s.local", input);
}

size_t umdns_mdns_build_query(uint16_t qtype, const char *name, uint8_t *out, size_t out_len) {
    char fqdn[UMDNS_MAX_NAME];
    size_t offset = sizeof(umdns_dns_header_t);

    if (out_len < sizeof(umdns_dns_header_t)) {
        return 0;
    }

    memset(out, 0, out_len);
    umdns_normalize_local_name(name, fqdn, sizeof(fqdn));

    if (umdns_append_name(out, out_len, &offset, fqdn) != 0) {
        return 0;
    }
    if (offset + 4 > out_len) {
        return 0;
    }

    umdns_write_u16(out, offset, qtype);
    offset += 2;
    umdns_write_u16(out, offset, 0x0001);
    offset += 2;

    umdns_write_u16(out, 4, 1);
    return offset;
}

static int umdns_append_rr_header(uint8_t *out,
                                  size_t out_len,
                                  size_t *offset,
                                  const char *name,
                                  uint16_t rrtype,
                                  uint16_t rrclass,
                                  uint32_t ttl,
                                  uint16_t rdlength) {
    if (umdns_append_name(out, out_len, offset, name) != 0) {
        return -1;
    }
    if (*offset + 10 > out_len) {
        return -1;
    }
    umdns_write_u16(out, *offset, rrtype);
    *offset += 2;
    umdns_write_u16(out, *offset, rrclass);
    *offset += 2;
    umdns_write_u32(out, *offset, ttl);
    *offset += 4;
    umdns_write_u16(out, *offset, rdlength);
    *offset += 2;
    return 0;
}

size_t umdns_mdns_build_response_hostname(const char *hostname,
                                          const char *ipv4,
                                          const char *ipv6,
                                          const char *txt,
                                          uint8_t *out,
                                          size_t out_len) {
    char host_fqdn[UMDNS_MAX_NAME];
    size_t offset = sizeof(umdns_dns_header_t);
    uint16_t ancount = 0;
    struct in_addr addr4;
    struct in6_addr addr6;

    if (out_len < sizeof(umdns_dns_header_t)) {
        return 0;
    }

    memset(out, 0, out_len);
    umdns_normalize_local_name(hostname, host_fqdn, sizeof(host_fqdn));

    if (ipv4 != NULL && inet_pton(AF_INET, ipv4, &addr4) == 1) {
        if (umdns_append_rr_header(out, out_len, &offset, host_fqdn, UMDNS_RR_A, 0x8001, 120, 4) != 0) {
            return 0;
        }
        if (offset + 4 > out_len) {
            return 0;
        }
        memcpy(out + offset, &addr4, 4);
        offset += 4;
        ancount += 1;
    }

    if (ipv6 != NULL && inet_pton(AF_INET6, ipv6, &addr6) == 1) {
        if (umdns_append_rr_header(out, out_len, &offset, host_fqdn, UMDNS_RR_AAAA, 0x8001, 120, 16) != 0) {
            return 0;
        }
        if (offset + 16 > out_len) {
            return 0;
        }
        memcpy(out + offset, &addr6, 16);
        offset += 16;
        ancount += 1;
    }

    if (txt != NULL && txt[0] != '\0') {
        size_t txt_len = strlen(txt);
        if (txt_len > 255) {
            txt_len = 255;
        }
        if (umdns_append_rr_header(out, out_len, &offset, host_fqdn, UMDNS_RR_TXT, 0x8001, 120, (uint16_t)(txt_len + 1)) != 0) {
            return 0;
        }
        if (offset + txt_len + 1 > out_len) {
            return 0;
        }
        out[offset++] = (uint8_t)txt_len;
        memcpy(out + offset, txt, txt_len);
        offset += txt_len;
        ancount += 1;
    }

    umdns_write_u16(out, 2, 0x8400);
    umdns_write_u16(out, 6, ancount);
    return offset;
}

size_t umdns_mdns_build_response_service(const umdns_service_t *service, uint8_t *out, size_t out_len) {
    char service_type_fqdn[UMDNS_MAX_NAME];
    char instance_fqdn[UMDNS_MAX_NAME];
    char host_fqdn[UMDNS_MAX_NAME];
    size_t offset = sizeof(umdns_dns_header_t);
    size_t rdata_len_pos;
    size_t rdata_start;
    uint16_t ancount = 0;

    if (service == NULL || out_len < sizeof(umdns_dns_header_t)) {
        return 0;
    }

    memset(out, 0, out_len);

    umdns_normalize_local_name(service->type, service_type_fqdn, sizeof(service_type_fqdn));
    {
        int written = snprintf(instance_fqdn, sizeof(instance_fqdn), "%s.%s", service->instance, service_type_fqdn);
        if (written < 0 || (size_t)written >= sizeof(instance_fqdn)) {
            return 0;
        }
    }
    umdns_normalize_local_name(service->host[0] == '\0' ? "localhost" : service->host, host_fqdn, sizeof(host_fqdn));

    if (umdns_append_rr_header(out, out_len, &offset, service_type_fqdn, UMDNS_RR_PTR, 0x8001, 120, 0) != 0) {
        return 0;
    }
    rdata_len_pos = offset - 2;
    rdata_start = offset;
    if (umdns_append_name(out, out_len, &offset, instance_fqdn) != 0) {
        return 0;
    }
    umdns_write_u16(out, rdata_len_pos, (uint16_t)(offset - rdata_start));
    ancount += 1;

    if (umdns_append_rr_header(out, out_len, &offset, instance_fqdn, UMDNS_RR_SRV, 0x8001, 120, 0) != 0) {
        return 0;
    }
    rdata_len_pos = offset - 2;
    rdata_start = offset;

    if (offset + 6 > out_len) {
        return 0;
    }
    umdns_write_u16(out, offset, 0);
    offset += 2;
    umdns_write_u16(out, offset, 0);
    offset += 2;
    umdns_write_u16(out, offset, service->port);
    offset += 2;
    if (umdns_append_name(out, out_len, &offset, host_fqdn) != 0) {
        return 0;
    }
    umdns_write_u16(out, rdata_len_pos, (uint16_t)(offset - rdata_start));
    ancount += 1;

    if (service->txt[0] != '\0') {
        size_t txt_len = strlen(service->txt);
        if (txt_len > 255) {
            txt_len = 255;
        }
        if (umdns_append_rr_header(out, out_len, &offset, instance_fqdn, UMDNS_RR_TXT, 0x8001, 120, (uint16_t)(txt_len + 1)) != 0) {
            return 0;
        }
        if (offset + txt_len + 1 > out_len) {
            return 0;
        }
        out[offset++] = (uint8_t)txt_len;
        memcpy(out + offset, service->txt, txt_len);
        offset += txt_len;
        ancount += 1;
    }

    umdns_write_u16(out, 2, 0x8400);
    umdns_write_u16(out, 6, ancount);
    return offset;
}

size_t umdns_mdns_build_response_service_enum(const umdns_service_t *services, size_t service_count, uint8_t *out, size_t out_len) {
    size_t offset = sizeof(umdns_dns_header_t);
    size_t idx;
    uint16_t ancount = 0;

    if (out_len < sizeof(umdns_dns_header_t)) {
        return 0;
    }

    memset(out, 0, out_len);

    for (idx = 0; idx < service_count; ++idx) {
        char service_type_fqdn[UMDNS_MAX_NAME];
        size_t rdata_len_pos;
        size_t rdata_start;

        if (services[idx].type[0] == '\0') {
            continue;
        }

        umdns_normalize_local_name(services[idx].type, service_type_fqdn, sizeof(service_type_fqdn));
        if (umdns_append_rr_header(out,
                                   out_len,
                                   &offset,
                                   "_services._dns-sd._udp.local",
                                   UMDNS_RR_PTR,
                                   0x8001,
                                   120,
                                   0) != 0) {
            return 0;
        }

        rdata_len_pos = offset - 2;
        rdata_start = offset;
        if (umdns_append_name(out, out_len, &offset, service_type_fqdn) != 0) {
            return 0;
        }
        umdns_write_u16(out, rdata_len_pos, (uint16_t)(offset - rdata_start));
        ancount += 1;
    }

    umdns_write_u16(out, 2, 0x8400);
    umdns_write_u16(out, 6, ancount);
    return offset;
}

int umdns_mdns_parse_question(const uint8_t *packet, size_t packet_len, umdns_question_t *question) {
    size_t offset = sizeof(umdns_dns_header_t);
    uint16_t qdcount;

    if (packet_len < sizeof(umdns_dns_header_t) || question == NULL) {
        return -1;
    }

    qdcount = umdns_read_u16(packet, 4);
    if (qdcount == 0) {
        return -1;
    }

    if (umdns_decode_name(packet, packet_len, &offset, question->name, sizeof(question->name)) != 0) {
        return -1;
    }

    if (offset + 4 > packet_len) {
        return -1;
    }

    question->qtype = umdns_read_u16(packet, offset);
    return 0;
}

static void umdns_result_init(umdns_result_t *result) {
    memset(result, 0, sizeof(*result));
}

size_t umdns_mdns_parse_answers(const uint8_t *packet, size_t packet_len, umdns_result_t *results, size_t max_results) {
    size_t offset = sizeof(umdns_dns_header_t);
    uint16_t qdcount;
    uint16_t ancount;
    size_t idx;
    size_t result_count = 0;

    if (packet_len < sizeof(umdns_dns_header_t) || results == NULL || max_results == 0) {
        return 0;
    }

    qdcount = umdns_read_u16(packet, 4);
    ancount = umdns_read_u16(packet, 6);

    for (idx = 0; idx < qdcount; ++idx) {
        char skip_name[UMDNS_MAX_NAME];
        if (umdns_decode_name(packet, packet_len, &offset, skip_name, sizeof(skip_name)) != 0 || offset + 4 > packet_len) {
            return result_count;
        }
        offset += 4;
    }

    for (idx = 0; idx < ancount && result_count < max_results; ++idx) {
        char rr_name[UMDNS_MAX_NAME];
        uint16_t rrtype;
        uint16_t rdlength;
        size_t rdata_offset;

        if (umdns_decode_name(packet, packet_len, &offset, rr_name, sizeof(rr_name)) != 0) {
            break;
        }

        if (offset + 10 > packet_len) {
            break;
        }

        rrtype = umdns_read_u16(packet, offset);
        offset += 2;
        (void)umdns_read_u16(packet, offset);
        offset += 2;
        (void)umdns_read_u32(packet, offset);
        offset += 4;
        rdlength = umdns_read_u16(packet, offset);
        offset += 2;

        rdata_offset = offset;
        if (offset + rdlength > packet_len) {
            break;
        }

        umdns_result_init(&results[result_count]);
        umdns_copy_string(results[result_count].hostname, sizeof(results[result_count].hostname), rr_name);
        results[result_count].rrtype = rrtype;

        if (rrtype == UMDNS_RR_A && rdlength == 4) {
            inet_ntop(AF_INET, packet + rdata_offset, results[result_count].address, sizeof(results[result_count].address));
            result_count += 1;
        } else if (rrtype == UMDNS_RR_AAAA && rdlength == 16) {
            inet_ntop(AF_INET6, packet + rdata_offset, results[result_count].address, sizeof(results[result_count].address));
            result_count += 1;
        } else if (rrtype == UMDNS_RR_PTR) {
            size_t ptr_offset = rdata_offset;
            if (umdns_decode_name(packet, packet_len, &ptr_offset, results[result_count].instance, sizeof(results[result_count].instance)) == 0) {
                umdns_copy_string(results[result_count].service_type, sizeof(results[result_count].service_type), rr_name);
                result_count += 1;
            }
        } else if (rrtype == UMDNS_RR_SRV && rdlength >= 6) {
            size_t srv_offset = rdata_offset;
            srv_offset += 4;
            results[result_count].port = umdns_read_u16(packet, srv_offset);
            srv_offset += 2;
            if (umdns_decode_name(packet, packet_len, &srv_offset, results[result_count].hostname, sizeof(results[result_count].hostname)) == 0) {
                umdns_copy_string(results[result_count].instance, sizeof(results[result_count].instance), rr_name);
                result_count += 1;
            }
        } else if (rrtype == UMDNS_RR_TXT && rdlength >= 1) {
            uint8_t txt_len = packet[rdata_offset];
            size_t copy_len = txt_len;
            if (copy_len > (size_t)(rdlength - 1U)) {
                copy_len = (size_t)(rdlength - 1U);
            }
            if (copy_len >= sizeof(results[result_count].txt)) {
                copy_len = sizeof(results[result_count].txt) - 1;
            }
            memcpy(results[result_count].txt, packet + rdata_offset + 1, copy_len);
            results[result_count].txt[copy_len] = '\0';
            umdns_copy_string(results[result_count].instance, sizeof(results[result_count].instance), rr_name);
            result_count += 1;
        }

        offset = rdata_offset + rdlength;
    }

    return result_count;
}
