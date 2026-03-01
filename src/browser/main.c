#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "umdns/common.h"
#include "umdns/log.h"
#include "umdns/mdns.h"
#include "umdns/signal.h"
#include "umdns/socket.h"
#include "umdns/table.h"

typedef struct browser_options {
    char interface_name[64];
    int timeout_seconds;
    int interval_seconds;
    char log_file[256];
    umdns_log_level_t log_level;
} browser_options_t;

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) {
        return;
    }
    snprintf(dst, dst_len, "%s", src == NULL ? "" : src);
}

static void print_help(void) {
    printf("umdns_browser - mDNS network browser\n\n");
    printf("Usage:\n");
    printf("  umdns_browser [options]\n\n");
    printf("Options:\n");
    printf("  -i <iface>          Bind sockets to interface (best effort)\n");
    printf("  -t <seconds>        Total browse timeout (0 = infinite, default: 10)\n");
    printf("  -n <seconds>        Interval between network-wide queries (default: 3)\n");
    printf("  --log-level <lvl>   debug|info|warn|error (default: info)\n");
    printf("  --log-file <path>   Log destination file (default: stderr)\n");
    printf("  -h, --help          Show this help\n\n");
    printf("Example:\n");
    printf("  umdns_browser -i eth0 -t 0 -n 5\n");
}

static int parse_int(const char *text, int min_value, int max_value, int *value) {
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < min_value || parsed > max_value) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

static int parse_args(int argc, char **argv, browser_options_t *options) {
    int idx = 1;

    memset(options, 0, sizeof(*options));
    options->timeout_seconds = 10;
    options->interval_seconds = 3;
    options->log_level = UMDNS_LOG_INFO;

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
            if (parse_int(argv[idx + 1], 0, 86400, &options->timeout_seconds) != 0) {
                fprintf(stderr, "invalid timeout: %s\\n", argv[idx + 1]);
                return -1;
            }
            idx += 2;
            continue;
        }

        if (strcmp(argv[idx], "-n") == 0 && idx + 1 < argc) {
            if (parse_int(argv[idx + 1], 1, 3600, &options->interval_seconds) != 0) {
                fprintf(stderr, "invalid interval: %s\\n", argv[idx + 1]);
                return -1;
            }
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

    return 0;
}

static size_t append_results_unique(umdns_result_t *dst, size_t dst_count, size_t dst_max, const umdns_result_t *src, size_t src_count) {
    size_t src_idx;
    for (src_idx = 0; src_idx < src_count && dst_count < dst_max; ++src_idx) {
        size_t dst_idx;
        int duplicate = 0;
        for (dst_idx = 0; dst_idx < dst_count; ++dst_idx) {
            if (strcmp(dst[dst_idx].instance, src[src_idx].instance) == 0 &&
                strcmp(dst[dst_idx].hostname, src[src_idx].hostname) == 0 &&
                strcmp(dst[dst_idx].service_type, src[src_idx].service_type) == 0 &&
                strcmp(dst[dst_idx].address, src[src_idx].address) == 0 &&
                dst[dst_idx].port == src[src_idx].port &&
                strcmp(dst[dst_idx].txt, src[src_idx].txt) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            dst[dst_count++] = src[src_idx];
        }
    }
    return dst_count;
}

static int receive_responses(int fd4, int fd6, umdns_result_t *results, size_t *result_count, int timeout_ms) {
    int remaining = timeout_ms;

    while (remaining > 0) {
        int slice = remaining > 250 ? 250 : remaining;
        uint8_t packet[1500];
        struct sockaddr_storage src;
        socklen_t src_len = sizeof(src);
        int received;
        umdns_result_t cycle_results[64];
        size_t cycle_count;

        if (fd4 >= 0) {
            src_len = sizeof(src);
            received = umdns_socket_recv_with_timeout(fd4, packet, sizeof(packet), slice, &src, &src_len);
            if (received > 0) {
                cycle_count = umdns_mdns_parse_answers(packet, (size_t)received, cycle_results, 64);
                *result_count = append_results_unique(results, *result_count, UMDNS_MAX_RESULTS, cycle_results, cycle_count);
            }
        }

        if (fd6 >= 0) {
            src_len = sizeof(src);
            received = umdns_socket_recv_with_timeout(fd6, packet, sizeof(packet), slice, &src, &src_len);
            if (received > 0) {
                cycle_count = umdns_mdns_parse_answers(packet, (size_t)received, cycle_results, 64);
                *result_count = append_results_unique(results, *result_count, UMDNS_MAX_RESULTS, cycle_results, cycle_count);
            }
        }

        remaining -= slice;
        if (umdns_signal_should_terminate()) {
            break;
        }
    }

    return 0;
}

static void extract_service_types(const umdns_result_t *results, size_t result_count, char service_types[][UMDNS_MAX_NAME], size_t *type_count, size_t max_types) {
    size_t idx;
    size_t out_idx = 0;

    for (idx = 0; idx < result_count && out_idx < max_types; ++idx) {
        if (results[idx].rrtype == UMDNS_RR_PTR && 
            strcmp(results[idx].service_type, "_services._dns-sd._udp.local") == 0 &&
            results[idx].instance[0] != '\0') {
            size_t check;
            int duplicate = 0;
            size_t copy_len;

            for (check = 0; check < out_idx; ++check) {
                if (strcmp(service_types[check], results[idx].instance) == 0) {
                    duplicate = 1;
                    break;
                }
            }

            if (!duplicate) {
                copy_len = strlen(results[idx].instance);
                if (copy_len >= UMDNS_MAX_NAME) {
                    copy_len = UMDNS_MAX_NAME - 1;
                }
                memcpy(service_types[out_idx], results[idx].instance, copy_len);
                service_types[out_idx][copy_len] = '\0';
                out_idx += 1;
            }
        }
    }

    *type_count = out_idx;
}

static int run_cycle(int fd4, int fd6, umdns_result_t *results, size_t *result_count, int cycle_ms) {
    uint8_t query[1500];
    size_t query_len;
    char service_types[32][UMDNS_MAX_NAME];
    size_t service_type_count = 0;
    size_t idx;

    query_len = umdns_mdns_build_query(UMDNS_RR_PTR, "_services._dns-sd._udp.local", query, sizeof(query));
    if (query_len == 0) {
        return -1;
    }

    if (fd4 >= 0 && umdns_socket_send_query_ipv4(fd4, query, query_len) != 0) {
        umdns_log_warn("failed IPv4 browser query send: %s", strerror(errno));
    }
    if (fd6 >= 0 && umdns_socket_send_query_ipv6(fd6, query, query_len) != 0) {
        umdns_log_warn("failed IPv6 browser query send: %s", strerror(errno));
    }

    if (receive_responses(fd4, fd6, results, result_count, cycle_ms) != 0) {
        return -1;
    }

    extract_service_types(results, *result_count, service_types, &service_type_count, 32);

    for (idx = 0; idx < service_type_count; ++idx) {
        umdns_log_debug("querying service type: %s", service_types[idx]);

        query_len = umdns_mdns_build_query(UMDNS_RR_PTR, service_types[idx], query, sizeof(query));
        if (query_len == 0) {
            continue;
        }

        if (fd4 >= 0 && umdns_socket_send_query_ipv4(fd4, query, query_len) != 0) {
            umdns_log_warn("failed IPv4 service query send: %s", strerror(errno));
        }
        if (fd6 >= 0 && umdns_socket_send_query_ipv6(fd6, query, query_len) != 0) {
            umdns_log_warn("failed IPv6 service query send: %s", strerror(errno));
        }

        if (receive_responses(fd4, fd6, results, result_count, cycle_ms / 2) != 0) {
            return -1;
        }
    }

    return 0;
}

static size_t merge_service_results(umdns_result_t *results, size_t result_count) {
    umdns_result_t merged[UMDNS_MAX_RESULTS];
    size_t merged_count = 0;
    size_t idx;

    for (idx = 0; idx < result_count; ++idx) {
        size_t merge_idx;
        int found = 0;

        if (results[idx].instance[0] == '\0') {
            continue;
        }

        if (strcmp(results[idx].service_type, "_services._dns-sd._udp.local") == 0) {
            continue;
        }

        for (merge_idx = 0; merge_idx < merged_count; ++merge_idx) {
            if (strcmp(merged[merge_idx].instance, results[idx].instance) == 0) {
                found = 1;

                if (results[idx].hostname[0] != '\0' && merged[merge_idx].hostname[0] == '\0') {
                    memcpy(merged[merge_idx].hostname, results[idx].hostname, sizeof(merged[merge_idx].hostname));
                }
                if (results[idx].service_type[0] != '\0' && merged[merge_idx].service_type[0] == '\0') {
                    memcpy(merged[merge_idx].service_type, results[idx].service_type, sizeof(merged[merge_idx].service_type));
                }
                if (results[idx].address[0] != '\0' && merged[merge_idx].address[0] == '\0') {
                    memcpy(merged[merge_idx].address, results[idx].address, sizeof(merged[merge_idx].address));
                }
                if (results[idx].port != 0 && merged[merge_idx].port == 0) {
                    merged[merge_idx].port = results[idx].port;
                }
                if (results[idx].txt[0] != '\0' && merged[merge_idx].txt[0] == '\0') {
                    memcpy(merged[merge_idx].txt, results[idx].txt, sizeof(merged[merge_idx].txt));
                }
                break;
            }
        }

        if (!found && merged_count < UMDNS_MAX_RESULTS) {
            merged[merged_count++] = results[idx];
        }
    }

    if (merged_count > 0 && merged_count <= result_count) {
        memcpy(results, merged, merged_count * sizeof(umdns_result_t));
    }

    return merged_count;
}

int main(int argc, char **argv) {
    browser_options_t options;
    int parse_result;
    int fd4 = -1;
    int fd6 = -1;
    umdns_result_t results[UMDNS_MAX_RESULTS];
    size_t result_count = 0;
    int elapsed = 0;

    parse_result = parse_args(argc, argv, &options);
    if (parse_result != 0) {
        return parse_result > 0 ? 0 : 1;
    }

    if (umdns_log_init(options.log_level, options.log_file[0] != '\0' ? options.log_file : NULL) != 0) {
        fprintf(stderr, "failed to initialize logging\\n");
        return 1;
    }

    if (umdns_signal_install_handlers() != 0) {
        umdns_log_error("failed to install signal handlers");
        umdns_log_close();
        return 1;
    }

    fd4 = umdns_socket_create_ipv4_listener(options.interface_name);
    fd6 = umdns_socket_create_ipv6_listener(options.interface_name);
    if (fd4 < 0 && fd6 < 0) {
        umdns_log_error("failed to create IPv4/IPv6 listeners");
        umdns_log_close();
        return 1;
    }

    while (!umdns_signal_should_terminate()) {
        if (run_cycle(fd4, fd6, results, &result_count, options.interval_seconds * 1000) != 0) {
            umdns_log_error("browser cycle failed");
            break;
        }

        if (options.timeout_seconds > 0) {
            elapsed += options.interval_seconds;
            if (elapsed >= options.timeout_seconds) {
                break;
            }
        }
    }

    if (result_count == 0) {
        printf("No mDNS results discovered\\n");
    } else {
        result_count = merge_service_results(results, result_count);
        umdns_table_print_results(results, result_count);
    }

    if (fd4 >= 0) {
        close(fd4);
    }
    if (fd6 >= 0) {
        close(fd6);
    }
    umdns_log_close();

    return 0;
}
