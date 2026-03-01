#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "umdns/config.h"
#include "umdns/log.h"
#include "umdns/mdns.h"
#include "umdns/signal.h"
#include "umdns/socket.h"

typedef struct server_options {
    char interface_name[64];
    char config_path[256];
    char log_file[256];
    umdns_log_level_t log_level;
} server_options_t;

static void copy_text(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) {
        return;
    }
    snprintf(dst, dst_len, "%s", src == NULL ? "" : src);
}

static void print_help(void) {
    printf("umdns_server - mDNS responder\\n\\n");
    printf("Usage:\\n");
    printf("  umdns_server [options]\\n\\n");
    printf("Options:\\n");
    printf("  -i <iface>          Bind sockets to interface (best effort)\\n");
    printf("  -c <config.ini>     INI file with hostname and services\\n");
    printf("  --log-level <lvl>   debug|info|warn|error (default: info)\\n");
    printf("  --log-file <path>   Log destination file (default: stderr)\\n");
    printf("  -h, --help          Show this help\\n\\n");
    printf("Example:\\n");
    printf("  umdns_server -i eth0 -c ./config/umdns_server.ini --log-level debug\\n");
}

static void normalize_local_name(const char *input, char *output, size_t output_len) {
    size_t len;
    if (input == NULL || input[0] == '\0') {
        snprintf(output, output_len, "localhost.local");
        return;
    }
    len = strlen(input);
    if (len >= 6 && strcasecmp(input + len - 6, ".local") == 0) {
        snprintf(output, output_len, "%s", input);
        return;
    }
    snprintf(output, output_len, "%s.local", input);
}

static int parse_args(int argc, char **argv, server_options_t *options) {
    int idx = 1;

    memset(options, 0, sizeof(*options));
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

        if (strcmp(argv[idx], "-c") == 0 && idx + 1 < argc) {
            copy_text(options->config_path, sizeof(options->config_path), argv[idx + 1]);
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

static int name_matches(const char *left, const char *right) {
    return strcasecmp(left, right) == 0;
}

static int handle_question(int fd,
                           const struct sockaddr *src_addr,
                           socklen_t src_len,
                           const umdns_server_config_t *config,
                           const umdns_question_t *question) {
    uint8_t response[1500];
    size_t response_len = 0;
    char host_fqdn[UMDNS_MAX_NAME];
    size_t idx;

    normalize_local_name(config->hostname, host_fqdn, sizeof(host_fqdn));

    if (question->qtype == UMDNS_RR_A || question->qtype == UMDNS_RR_AAAA || question->qtype == UMDNS_RR_TXT || question->qtype == 255) {
        if (name_matches(question->name, host_fqdn)) {
            response_len = umdns_mdns_build_response_hostname(config->hostname, config->ipv4, config->ipv6, config->host_txt, response, sizeof(response));
        }
    }

    if (question->qtype == UMDNS_RR_PTR && name_matches(question->name, "_services._dns-sd._udp.local")) {
        response_len = umdns_mdns_build_response_service_enum(config->services, config->service_count, response, sizeof(response));
    }

    for (idx = 0; idx < config->service_count; ++idx) {
        char type_fqdn[UMDNS_MAX_NAME];
        char instance_fqdn[UMDNS_MAX_NAME];
        int written;

        if (config->services[idx].type[0] == '\0') {
            continue;
        }

        normalize_local_name(config->services[idx].type, type_fqdn, sizeof(type_fqdn));
        written = snprintf(instance_fqdn, sizeof(instance_fqdn), "%s.%s", config->services[idx].instance, type_fqdn);
        if (written < 0 || (size_t)written >= sizeof(instance_fqdn)) {
            continue;
        }

        if (question->qtype == UMDNS_RR_PTR && name_matches(question->name, type_fqdn)) {
            response_len = umdns_mdns_build_response_service(&config->services[idx], response, sizeof(response));
        }

        if ((question->qtype == UMDNS_RR_SRV || question->qtype == UMDNS_RR_TXT || question->qtype == 255) && name_matches(question->name, instance_fqdn)) {
            response_len = umdns_mdns_build_response_service(&config->services[idx], response, sizeof(response));
        }
    }

    if (response_len > 0) {
        if (umdns_socket_send_response(fd, src_addr, src_len, response, response_len) != 0) {
            umdns_log_warn("failed to send response: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    server_options_t options;
    umdns_server_config_t config;
    int parse_result;
    int fd4 = -1;
    int fd6 = -1;

    parse_result = parse_args(argc, argv, &options);
    if (parse_result != 0) {
        return parse_result > 0 ? 0 : 1;
    }

    if (umdns_log_init(options.log_level, options.log_file[0] != '\0' ? options.log_file : NULL) != 0) {
        fprintf(stderr, "failed to initialize logging\\n");
        return 1;
    }

    umdns_server_config_init(&config);
    if (gethostname(config.hostname, sizeof(config.hostname) - 1) != 0) {
        copy_text(config.hostname, sizeof(config.hostname), "umdns");
    }

    if (options.config_path[0] != '\0') {
        if (umdns_server_config_load(options.config_path, &config) != 0) {
            umdns_log_error("failed to load config file: %s", options.config_path);
            umdns_log_close();
            return 1;
        }
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

    umdns_log_info("server ready for hostname '%s' with %zu services", config.hostname, config.service_count);

    while (!umdns_signal_should_terminate()) {
        uint8_t buffer[1500];
        struct sockaddr_storage src;
        socklen_t src_len = sizeof(src);
        int received;
        umdns_question_t question;

        if (fd4 >= 0) {
            src_len = sizeof(src);
            received = umdns_socket_recv_with_timeout(fd4, buffer, sizeof(buffer), 250, &src, &src_len);
            if (received > 0 && umdns_mdns_parse_question(buffer, (size_t)received, &question) == 0) {
                umdns_log_debug("IPv4 question: %s type=%u", question.name, question.qtype);
                (void)handle_question(fd4, (const struct sockaddr *)&src, src_len, &config, &question);
            }
        }

        if (fd6 >= 0) {
            src_len = sizeof(src);
            received = umdns_socket_recv_with_timeout(fd6, buffer, sizeof(buffer), 250, &src, &src_len);
            if (received > 0 && umdns_mdns_parse_question(buffer, (size_t)received, &question) == 0) {
                umdns_log_debug("IPv6 question: %s type=%u", question.name, question.qtype);
                (void)handle_question(fd6, (const struct sockaddr *)&src, src_len, &config, &question);
            }
        }
    }

    umdns_log_info("shutdown requested, exiting");
    if (fd4 >= 0) {
        close(fd4);
    }
    if (fd6 >= 0) {
        close(fd6);
    }
    umdns_log_close();
    return 0;
}
