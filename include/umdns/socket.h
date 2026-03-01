#ifndef UMDNS_SOCKET_H
#define UMDNS_SOCKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/socket.h>

int umdns_socket_create_ipv4_listener(const char *interface_name);
int umdns_socket_create_ipv6_listener(const char *interface_name);
int umdns_socket_send_query_ipv4(int fd, const uint8_t *buffer, size_t length);
int umdns_socket_send_query_ipv6(int fd, const uint8_t *buffer, size_t length);
int umdns_socket_send_response(int fd, const struct sockaddr *target, socklen_t target_len, const uint8_t *buffer, size_t length);
int umdns_socket_recv_with_timeout(int fd, uint8_t *buffer, size_t buffer_len, int timeout_ms, struct sockaddr_storage *src, socklen_t *src_len);

#endif
