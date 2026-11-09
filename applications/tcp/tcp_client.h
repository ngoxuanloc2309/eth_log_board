#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_CLIENT_BUFFER_SIZE 512U

typedef struct tcp_client tcp_client_t;

/*
 * These pointers borrow client ring-buffer memory. Use the returned span
 * before consuming/committing it, and serialize access from application tasks.
 * Use one producer for send and one consumer for received bytes. A data or
 * disconnect callback must consume or copy borrowed RX data before returning.
 * Client handles are valid only until the disconnect callback returns.
 */
const uint8_t *tcp_client_read_ptr(tcp_client_t *client,
                                   size_t *contiguous_length);
bool tcp_client_read_consume(tcp_client_t *client, size_t length);

uint8_t *tcp_client_send_ptr(tcp_client_t *client,
                             size_t *contiguous_capacity);
bool tcp_client_send_commit(tcp_client_t *client, size_t length);

/* IPv4 is returned in network byte order; port is returned in host byte order. */
bool tcp_client_get_remote_address(const tcp_client_t *client,
                                   uint32_t *ipv4_network_order,
                                   uint16_t *port_host_order);

#ifdef __cplusplus
}
#endif

#endif
