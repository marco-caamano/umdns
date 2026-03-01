#include "umdns/socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "umdns/common.h"
#include "umdns/log.h"

static int umdns_socket_set_common_options(int fd) {
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        return -1;
    }
    return 0;
}

static void umdns_socket_try_bind_device(int fd, const char *interface_name) {
#ifdef SO_BINDTODEVICE
    if (interface_name != NULL && interface_name[0] != '\0') {
        if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface_name, strlen(interface_name)) != 0) {
            umdns_log_warn("SO_BINDTODEVICE failed on %s: %s", interface_name, strerror(errno));
        }
    }
#else
    (void)fd;
    (void)interface_name;
#endif
}

int umdns_socket_create_ipv4_listener(const char *interface_name) {
    int fd;
    struct sockaddr_in bind_addr;
    struct ip_mreqn mreq;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (umdns_socket_set_common_options(fd) != 0) {
        close(fd);
        return -1;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(UMDNS_MDNS_PORT);

    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        close(fd);
        return -1;
    }

    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, UMDNS_MCAST_IPV4, &mreq.imr_multiaddr) != 1) {
        close(fd);
        return -1;
    }
    mreq.imr_address.s_addr = htonl(INADDR_ANY);
    if (interface_name != NULL && interface_name[0] != '\0') {
        mreq.imr_ifindex = if_nametoindex(interface_name);
    }

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
        close(fd);
        return -1;
    }

    umdns_socket_try_bind_device(fd, interface_name);
    return fd;
}

int umdns_socket_create_ipv6_listener(const char *interface_name) {
    int fd;
    int on = 1;
    struct sockaddr_in6 bind_addr;
    struct ipv6_mreq mreq;

    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) != 0) {
        close(fd);
        return -1;
    }

    if (umdns_socket_set_common_options(fd) != 0) {
        close(fd);
        return -1;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_addr = in6addr_any;
    bind_addr.sin6_port = htons(UMDNS_MDNS_PORT);

    if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        close(fd);
        return -1;
    }

    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET6, UMDNS_MCAST_IPV6, &mreq.ipv6mr_multiaddr) != 1) {
        close(fd);
        return -1;
    }
    if (interface_name != NULL && interface_name[0] != '\0') {
        mreq.ipv6mr_interface = if_nametoindex(interface_name);
    }

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) != 0) {
        close(fd);
        return -1;
    }

    umdns_socket_try_bind_device(fd, interface_name);
    return fd;
}

int umdns_socket_send_query_ipv4(int fd, const uint8_t *buffer, size_t length) {
    struct sockaddr_in target;

    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(UMDNS_MDNS_PORT);
    if (inet_pton(AF_INET, UMDNS_MCAST_IPV4, &target.sin_addr) != 1) {
        return -1;
    }

    if (sendto(fd, buffer, length, 0, (struct sockaddr *)&target, sizeof(target)) < 0) {
        return -1;
    }
    return 0;
}

int umdns_socket_send_query_ipv6(int fd, const uint8_t *buffer, size_t length) {
    struct sockaddr_in6 target;

    memset(&target, 0, sizeof(target));
    target.sin6_family = AF_INET6;
    target.sin6_port = htons(UMDNS_MDNS_PORT);
    if (inet_pton(AF_INET6, UMDNS_MCAST_IPV6, &target.sin6_addr) != 1) {
        return -1;
    }

    if (sendto(fd, buffer, length, 0, (struct sockaddr *)&target, sizeof(target)) < 0) {
        return -1;
    }
    return 0;
}

int umdns_socket_send_response(int fd, const struct sockaddr *target, socklen_t target_len, const uint8_t *buffer, size_t length) {
    if (sendto(fd, buffer, length, 0, target, target_len) < 0) {
        return -1;
    }
    return 0;
}

int umdns_socket_recv_with_timeout(int fd, uint8_t *buffer, size_t buffer_len, int timeout_ms, struct sockaddr_storage *src, socklen_t *src_len) {
    struct pollfd pfd;
    int poll_result;

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    poll_result = poll(&pfd, 1, timeout_ms);
    if (poll_result <= 0) {
        return poll_result;
    }

    if ((pfd.revents & POLLIN) == 0) {
        return 0;
    }

    return (int)recvfrom(fd, buffer, buffer_len, 0, (struct sockaddr *)src, src_len);
}
