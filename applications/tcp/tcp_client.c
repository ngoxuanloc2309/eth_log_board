#include "tcp_client_internal.h"

#include "FreeRTOS.h"
#include "task.h"

static size_t min_size(size_t left, size_t right)
{
    return left < right ? left : right;
}

void tcp_client_init_internal(tcp_client_t *client,
                              int socket_fd,
                              const struct sockaddr_in *remote_address)
{
    taskENTER_CRITICAL();
    client->socket_fd = socket_fd;
    client->active = true;
    client->remote_address = *remote_address;
    client->rx_head = 0U;
    client->rx_count = 0U;
    client->tx_head = 0U;
    client->tx_count = 0U;
    taskEXIT_CRITICAL();
}

void tcp_client_deactivate_internal(tcp_client_t *client)
{
    taskENTER_CRITICAL();
    client->active = false;
    client->socket_fd = -1;
    taskEXIT_CRITICAL();
}

void tcp_client_clear_buffers_internal(tcp_client_t *client)
{
    taskENTER_CRITICAL();
    client->rx_head = 0U;
    client->rx_count = 0U;
    client->tx_head = 0U;
    client->tx_count = 0U;
    taskEXIT_CRITICAL();
}

bool tcp_client_is_active_internal(const tcp_client_t *client)
{
    bool active;

    if (client == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    active = client->active;
    taskEXIT_CRITICAL();
    return active;
}

const uint8_t *tcp_client_read_ptr(tcp_client_t *client,
                                   size_t *contiguous_length)
{
    size_t length = 0U;
    const uint8_t *data = NULL;

    if (contiguous_length == NULL) {
        return NULL;
    }
    *contiguous_length = 0U;
    if (client == NULL) {
        return NULL;
    }

    taskENTER_CRITICAL();
    if (client->rx_count > 0U) {
        length = min_size(client->rx_count,
                          TCP_CLIENT_BUFFER_SIZE - client->rx_head);
        data = &client->rx_buffer[client->rx_head];
    }
    taskEXIT_CRITICAL();
    *contiguous_length = length;
    return data;
}

bool tcp_client_read_consume(tcp_client_t *client, size_t length)
{
    bool consumed = false;

    if (client == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    if (length <= client->rx_count) {
        client->rx_head = (client->rx_head + length) %
                          TCP_CLIENT_BUFFER_SIZE;
        client->rx_count -= length;
        consumed = true;
    }
    taskEXIT_CRITICAL();
    return consumed;
}

uint8_t *tcp_client_send_ptr(tcp_client_t *client,
                             size_t *contiguous_capacity)
{
    size_t length = 0U;
    size_t tail;
    uint8_t *data = NULL;

    if (contiguous_capacity == NULL) {
        return NULL;
    }
    *contiguous_capacity = 0U;
    if (client == NULL) {
        return NULL;
    }

    taskENTER_CRITICAL();
    if (client->active && client->tx_count < TCP_CLIENT_BUFFER_SIZE) {
        tail = (client->tx_head + client->tx_count) %
               TCP_CLIENT_BUFFER_SIZE;
        length = min_size(TCP_CLIENT_BUFFER_SIZE - client->tx_count,
                          TCP_CLIENT_BUFFER_SIZE - tail);
        data = &client->tx_buffer[tail];
    }
    taskEXIT_CRITICAL();
    *contiguous_capacity = length;
    return data;
}

bool tcp_client_send_commit(tcp_client_t *client, size_t length)
{
    bool committed = false;
    size_t tail;
    size_t contiguous_capacity;

    if (client == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    if (client->active && client->tx_count < TCP_CLIENT_BUFFER_SIZE) {
        tail = (client->tx_head + client->tx_count) %
               TCP_CLIENT_BUFFER_SIZE;
        contiguous_capacity = min_size(
            TCP_CLIENT_BUFFER_SIZE - client->tx_count,
            TCP_CLIENT_BUFFER_SIZE - tail);
        if (length <= contiguous_capacity) {
            client->tx_count += length;
            committed = true;
        }
    } else if (client->active && length == 0U) {
        committed = true;
    }
    taskEXIT_CRITICAL();
    return committed;
}

bool tcp_client_get_remote_address(const tcp_client_t *client,
                                   uint32_t *ipv4_network_order,
                                   uint16_t *port_host_order)
{
    struct sockaddr_in remote_address;

    if (client == NULL || ipv4_network_order == NULL ||
        port_host_order == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    remote_address = client->remote_address;
    taskEXIT_CRITICAL();

    *ipv4_network_order = remote_address.sin_addr.s_addr;
    *port_host_order = lwip_ntohs(remote_address.sin_port);
    return true;
}

uint8_t *tcp_client_rx_free_span_internal(tcp_client_t *client,
                                          size_t *length)
{
    size_t tail;
    size_t free_length;
    uint8_t *data = NULL;

    if (length == NULL) {
        return NULL;
    }
    *length = 0U;
    if (client == NULL) {
        return NULL;
    }

    taskENTER_CRITICAL();
    free_length = TCP_CLIENT_BUFFER_SIZE - client->rx_count;
    if (client->active && free_length > 0U) {
        tail = (client->rx_head + client->rx_count) %
               TCP_CLIENT_BUFFER_SIZE;
        *length = min_size(free_length, TCP_CLIENT_BUFFER_SIZE - tail);
        data = &client->rx_buffer[tail];
    }
    taskEXIT_CRITICAL();
    return data;
}

bool tcp_client_rx_commit_internal(tcp_client_t *client, size_t length)
{
    bool committed = false;

    if (client == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    if (client->active &&
        length <= TCP_CLIENT_BUFFER_SIZE - client->rx_count) {
        client->rx_count += length;
        committed = true;
    }
    taskEXIT_CRITICAL();
    return committed;
}

const uint8_t *tcp_client_tx_data_internal(tcp_client_t *client,
                                           size_t *length)
{
    size_t contiguous_length = 0U;
    const uint8_t *data = NULL;

    if (length == NULL) {
        return NULL;
    }
    *length = 0U;
    if (client == NULL) {
        return NULL;
    }

    taskENTER_CRITICAL();
    if (client->active && client->tx_count > 0U) {
        contiguous_length = min_size(client->tx_count,
                                      TCP_CLIENT_BUFFER_SIZE -
                                          client->tx_head);
        data = &client->tx_buffer[client->tx_head];
    }
    taskEXIT_CRITICAL();
    *length = contiguous_length;
    return data;
}

bool tcp_client_tx_consume_internal(tcp_client_t *client, size_t length)
{
    bool consumed = false;

    if (client == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    if (client->active && length <= client->tx_count) {
        client->tx_head = (client->tx_head + length) %
                          TCP_CLIENT_BUFFER_SIZE;
        client->tx_count -= length;
        consumed = true;
    }
    taskEXIT_CRITICAL();
    return consumed;
}

size_t tcp_client_tx_queued_internal(tcp_client_t *client)
{
    size_t queued = 0U;

    if (client != NULL) {
        taskENTER_CRITICAL();
        if (client->active) {
            queued = client->tx_count;
        }
        taskEXIT_CRITICAL();
    }
    return queued;
}
