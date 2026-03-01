#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "umdns/common.h"
#include "umdns/log.h"
#include "umdns/mdns.h"
#include "umdns/socket.h"
#include "umdns/table.h"

typedef struct client_options {
    char interface_name[64];
    int timeout_seconds;
    char target[UMDNS_MAX_NAME];
    umdns_query_mode_t mode;
    char log_file[256];
    umdns_log_level_t log_level;
} client_options_t;

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) {
        return;
    }
    snprintf(dst, dst_len, "%s", src == NULL ? "" : src);
}

static void print_help(void) {
    printf("umdns_client - mDNS query tool\\n\\n");
    printf("Usage:\\n");
    printf("  umdns_client [options]\\n\\n");
    printf("Modes (mutually exclusive):\\n");
    printf("  -H <hostname>       Resolve hostname (A/AAAA/TXT)\\n");
    printf("  -s <service_type>   Resolve service (PTR/SRV/TXT)\\n\\n");
    printf("Options:\\n");
    printf("  -i <iface>          Bind sockets to interface (best effort)\\n");
    printf("  -t <seconds>        Timeout waiting for replies (default: 3)\\n");
    printf("  --log-level <lvl>   debug|info|warn|error (default: info)\\n");
    printf("  --log-file <path>   Log destination file (default: stderr)\\n");
    printf("  -h, --help          Show this help\\n\\n");
    printf("Examples:\\n");
    printf("  umdns_client -H myhost -i eth0 -t 3\\n");
    printf("  umdns_client -s _http._tcp -i eth0 -t 5\\n");
}

static int parse_int(const char *text, int *value) {
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 3600) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

static int parse_args(int argc, char **argv, client_options_t *options) {
    int idx = 1;
    int mode_count = 0;
    char host[UMDNS_MAX_NAME];

    memset(options, 0, sizeof(*options));
    options->timeout_seconds = 3;
    options->mode = UMDNS_QUERY_HOSTNAME;
    options->log_level = UMDNS_LOG_INFO;

    if (gethostname(host, sizeof(host) - 1) == 0) {
        copy_text(options->target, sizeof(options->target), host);
    } else {
        copy_text(options->target, sizeof(options->target), "localhost");
    }

    while (idx < argc) {
        if ((strcmp(argv[idx], "-h") == 0) || (strcmp(argv[idx], "--help") == 0)) {
            print_help();
            return 1;
        }

        if (strcmp(argv[idx], "-i") == 0 && idx + 1 < argc) {
            copy_text(options->interface_name, sizeof(options->interface_name), argv[idx + 1]);
            idx += 2;
            continue;
        }

        if (strcmp(argv[idx], "-t") == 0 && idx + 1 < argc) {
            if (parse_int(argv[idx + 1], &options->timeout_seconds) != 0) {
                fprintf(stderr, "invalid timeout: %s\\n", argv[idx + 1]);
                return -1;
            }
            idx += 2;
            continue;
        }

        if (strcmp(argv[idx], "-H") == 0 && idx + 1 < argc) {
            options->mode = UMDNS_QUERY_HOSTNAME;
            copy_text(options->target, sizeof(options->target), argv[idx + 1]);
            mode_count += 1;
            idx += 2;
            continue;
        }

        if (strcmp(argv[idx], "-s") == 0 && idx + 1 < argc) {
            options->mode = UMDNS_QUERY_SERVICE;
            copy_text(options->target, sizeof(options->target), argv[idx + 1]);
            mode_count += 1;
            idx += 2;
            continue;
        }

        if (strcmp(argv[idx], "--log-file") == 0 && idx + 1 < argc) {
            copy_text(options->log_file, sizeof(options->log_file), argv[idx + 1]);
            idx += 2;
            continue;
        }

        if (strcmp(argv[idx], "--log-level") == 0 && idx + 1 < argc) {
            bool ok = false;
            options->log_level = umdns_log_level_from_string(argv[idx + 1], &ok);
            if (!ok) {
                fprintf(stderr, "Invalid log level: %s\\n", argv[idx + 1]);
                return -1;
            }
            idx += 2;
            continue;
        }

        fprintf(stderr, "Unknown argument: %s\\n", argv[idx]);
        return -1;
    }

    if (mode_count > 1) {
        fprintf(stderr, "choose either hostname (-H) or service (-s), not both\\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    client_options_t options;
    int parse_result;
    int fd4 = -1;
    int fd6 = -1;
    uint8_t query[1500];
    size_t query_len;
    umdns_result_t results[UMDNS_MAX_RESULTS];
    size_t result_count = 0;
    int timeout_ms;

    parse_result = parse_args(argc, argv, &options);
    if (parse_result != 0) {
        return parse_result > 0 ? 0 : 1;
    }

    if (umdns_log_init(options.log_level, options.log_file[0] != '\0' ? options.log_file : NULL) != 0) {
        fprintf(stderr, "failed to initialize logging\\n");
        return 1;
    }

    fd4 = umdns_socket_create_ipv4_listener(options.interface_name);
    fd6 = umdns_socket_create_ipv6_listener(options.interface_name);
    if (fd4 < 0 && fd6 < 0) {
        umdns_log_error("failed to create IPv4/IPv6 listeners");
        umdns_log_close();
        return 1;
    }

    query_len = umdns_mdns_build_query(options.mode == UMDNS_QUERY_HOSTNAME ? UMDNS_RR_A : UMDNS_RR_PTR,
                                       options.target,
                                       query,
                                       sizeof(query));
    if (query_len == 0) {
        umdns_log_error("failed to build query packet");
        if (fd4 >= 0) {
            close(fd4);
        }
        if (fd6 >= 0) {
            close(fd6);
        }
        umdns_log_close();
        return 1;
    }

    if (fd4 >= 0 && umdns_socket_send_query_ipv4(fd4, query, query_len) != 0) {
        umdns_log_warn("failed IPv4 query send: %s", strerror(errno));
    }
    if (fd6 >= 0 && umdns_socket_send_query_ipv6(fd6, query, query_len) != 0) {
        umdns_log_warn("failed IPv6 query send: %s", strerror(errno));
    }

    timeout_ms = options.timeout_seconds * 1000;
    while (timeout_ms > 0 && result_count < UMDNS_MAX_RESULTS) {
        int slice = timeout_ms > 200 ? 200 : timeout_ms;
        uint8_t packet[1500];
        struct sockaddr_storage src;
        socklen_t src_len = sizeof(src);
        int received;

        if (fd4 >= 0) {
            src_len = sizeof(src);
            received = umdns_socket_recv_with_timeout(fd4, packet, sizeof(packet), slice, &src, &src_len);
            if (received > 0) {
                result_count += umdns_mdns_parse_answers(packet,
                                                         (size_t)received,
                                                         results + result_count,
                                                         UMDNS_MAX_RESULTS - result_count);
            }
        }

        if (fd6 >= 0 && result_count < UMDNS_MAX_RESULTS) {
            src_len = sizeof(src);
            received = umdns_socket_recv_with_timeout(fd6, packet, sizeof(packet), slice, &src, &src_len);
            if (received > 0) {
                result_count += umdns_mdns_parse_answers(packet,
                                                         (size_t)received,
                                                         results + result_count,
                                                         UMDNS_MAX_RESULTS - result_count);
            }
        }

        timeout_ms -= slice;
    }

    if (result_count == 0) {
        umdns_log_info("no results received");
        printf("No mDNS results for target '%s'\\n", options.target);
    } else {
        umdns_table_print_results(results, result_count);
    }

    if (fd4 >= 0) {
        close(fd4);
    }
    if (fd6 >= 0) {
        close(fd6);
    }
    umdns_log_close();

    return result_count > 0 ? 0 : 1;
}
