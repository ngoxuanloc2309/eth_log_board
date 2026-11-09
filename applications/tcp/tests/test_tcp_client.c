#include "tcp_client_internal.h"
#include "tcp_server.h"

#include <assert.h>
#include <string.h>

static void fill_span(uint8_t *span, size_t length, uint8_t value)
{
    memset(span, value, length);
}

static void test_receive_ring_wrap_and_bounds(void)
{
    tcp_client_t client = {0};
    struct sockaddr_in remote = {0};
    size_t length;
    uint8_t *free_span;
    const uint8_t *read_span;

    tcp_client_init_internal(&client, 1, &remote);
    read_span = tcp_client_read_ptr(&client, &length);
    assert(read_span == NULL);
    assert(length == 0U);

    free_span = tcp_client_rx_free_span_internal(&client, &length);
    assert(free_span != NULL && length == TCP_CLIENT_BUFFER_SIZE);
    fill_span(free_span, 400U, 0x11U);
    assert(tcp_client_rx_commit_internal(&client, 400U));

    assert(tcp_client_read_consume(&client, 300U));
    free_span = tcp_client_rx_free_span_internal(&client, &length);
    assert(free_span != NULL && length == 112U);
    fill_span(free_span, length, 0x22U);
    assert(tcp_client_rx_commit_internal(&client, length));

    read_span = tcp_client_read_ptr(&client, &length);
    assert(read_span != NULL && length == 212U);
    assert(read_span[0] == 0x11U);
    assert(read_span[99] == 0x11U);
    assert(read_span[100] == 0x22U);
    assert(read_span[211] == 0x22U);
    assert(!tcp_client_read_consume(&client, 213U));
    assert(tcp_client_read_consume(&client, 212U));

    free_span = tcp_client_rx_free_span_internal(&client, &length);
    assert(free_span != NULL && length == TCP_CLIENT_BUFFER_SIZE);
    fill_span(free_span, length, 0x33U);
    assert(tcp_client_rx_commit_internal(&client, length));
    assert(tcp_client_rx_free_span_internal(&client, &length) == NULL);
    assert(length == 0U);

    read_span = tcp_client_read_ptr(&client, &length);
    assert(read_span != NULL && length == TCP_CLIENT_BUFFER_SIZE);
    assert(read_span[0] == 0x33U && read_span[length - 1U] == 0x33U);
    assert(tcp_client_read_consume(&client, length));
    assert(tcp_client_read_ptr(&client, &length) == NULL);
    assert(length == 0U);
}

static void test_transmit_ring_wrap_and_bounds(void)
{
    tcp_client_t client = {0};
    struct sockaddr_in remote = {0};
    size_t length;
    uint8_t *send_span;
    const uint8_t *queued_span;

    tcp_client_init_internal(&client, 1, &remote);
    send_span = tcp_client_send_ptr(&client, &length);
    assert(send_span != NULL && length == TCP_CLIENT_BUFFER_SIZE);
    fill_span(send_span, 400U, 0x41U);
    assert(tcp_client_send_commit(&client, 400U));
    assert(!tcp_client_send_commit(&client, 113U));

    assert(tcp_client_tx_consume_internal(&client, 300U));
    send_span = tcp_client_send_ptr(&client, &length);
    assert(send_span != NULL && length == 112U);
    fill_span(send_span, length, 0x42U);
    assert(tcp_client_send_commit(&client, length));

    queued_span = tcp_client_tx_data_internal(&client, &length);
    assert(queued_span != NULL && length == 212U);
    assert(queued_span[0] == 0x41U);
    assert(queued_span[99] == 0x41U);
    assert(queued_span[100] == 0x42U);
    assert(queued_span[211] == 0x42U);
    assert(tcp_client_tx_consume_internal(&client, length));

    send_span = tcp_client_send_ptr(&client, &length);
    assert(send_span != NULL && length == TCP_CLIENT_BUFFER_SIZE);
    fill_span(send_span, length, 0x55U);
    assert(tcp_client_send_commit(&client, length));
    assert(tcp_client_send_ptr(&client, &length) == NULL);
    assert(length == 0U);
    assert(tcp_client_tx_queued_internal(&client) == TCP_CLIENT_BUFFER_SIZE);

    assert(tcp_client_tx_consume_internal(&client, 300U));
    send_span = tcp_client_send_ptr(&client, &length);
    assert(send_span != NULL && length == 300U);
    fill_span(send_span, length, 0x66U);
    assert(tcp_client_send_commit(&client, length));

    queued_span = tcp_client_tx_data_internal(&client, &length);
    assert(queued_span != NULL && length == 212U);
    assert(tcp_client_tx_consume_internal(&client, length));
    queued_span = tcp_client_tx_data_internal(&client, &length);
    assert(queued_span != NULL && length == 300U);
    assert(queued_span[0] == 0x66U && queued_span[length - 1U] == 0x66U);
    assert(tcp_client_tx_consume_internal(&client, length));
    assert(tcp_client_tx_data_internal(&client, &length) == NULL);
    assert(length == 0U);
}

static void test_unread_receive_data_survives_disconnect_callback(void)
{
    tcp_client_t client = {0};
    struct sockaddr_in remote = {0};
    size_t length;
    uint8_t *free_span;
    const uint8_t *read_span;

    tcp_client_init_internal(&client, 1, &remote);
    free_span = tcp_client_rx_free_span_internal(&client, &length);
    assert(free_span != NULL && length > 3U);
    memcpy(free_span, "tcp", 3U);
    assert(tcp_client_rx_commit_internal(&client, 3U));

    tcp_client_deactivate_internal(&client);
    read_span = tcp_client_read_ptr(&client, &length);
    assert(read_span != NULL && length == 3U);
    assert(memcmp(read_span, "tcp", 3U) == 0);
    assert(tcp_client_send_ptr(&client, &length) == NULL);
    assert(length == 0U);
    assert(tcp_client_read_consume(&client, 3U));
    tcp_client_clear_buffers_internal(&client);
    assert(tcp_client_read_ptr(&client, &length) == NULL);
    assert(length == 0U);
}

int main(void)
{
    assert(TCP_SERVER_MAX_CLIENTS == 4U);
    test_receive_ring_wrap_and_bounds();
    test_transmit_ring_wrap_and_bounds();
    test_unread_receive_data_survives_disconnect_callback();
    return 0;
}
