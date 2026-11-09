#ifndef TCP_CLIENT_INTERNAL_H
#define TCP_CLIENT_INTERNAL_H

#include "tcp_client.h"

#include "lwip/sockets.h"

struct tcp_client
{
    int socket_fd;
    bool active;
    struct sockaddr_in remote_address;
    size_t rx_head;
    size_t rx_count;
    size_t tx_head;
    size_t tx_count;
    uint8_t rx_buffer[TCP_CLIENT_BUFFER_SIZE];
    uint8_t tx_buffer[TCP_CLIENT_BUFFER_SIZE];
};

void tcp_client_init_internal(tcp_client_t *client,
                              int socket_fd,
                              const struct sockaddr_in *remote_address);
void tcp_client_deactivate_internal(tcp_client_t *client);
void tcp_client_clear_buffers_internal(tcp_client_t *client);
bool tcp_client_is_active_internal(const tcp_client_t *client);
uint8_t *tcp_client_rx_free_span_internal(tcp_client_t *client,
                                          size_t *length);
bool tcp_client_rx_commit_internal(tcp_client_t *client, size_t length);
const uint8_t *tcp_client_tx_data_internal(tcp_client_t *client,
                                           size_t *length);
bool tcp_client_tx_consume_internal(tcp_client_t *client, size_t length);
size_t tcp_client_tx_queued_internal(tcp_client_t *client);

#endif
