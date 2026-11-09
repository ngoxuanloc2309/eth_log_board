#include "mqtt_test_mocks.h"

#include "lwip/sockets.h"

#include <string.h>

MQTTTestConnackMode_t mqtt_test_connack_mode;
int mqtt_test_network_connect_result;
uint32_t mqtt_test_network_timeout_ms;
uint32_t mqtt_test_network_disconnect_count;
uint32_t mqtt_test_receive_count;
uint8_t mqtt_test_last_connect_packet_type;
bool mqtt_test_receive_timeout_configured;

static const uint8_t connack_success[] = { 0x20U, 0x02U, 0x00U, 0x00U };
static const uint8_t connack_refused[] = { 0x20U, 0x02U, 0x00U, 0x05U };
static size_t outgoing_bytes;
static size_t incoming_offset;

void mqtt_test_reset(void)
{
    mqtt_test_connack_mode = MQTT_TEST_CONNACK_SUCCESS;
    mqtt_test_network_connect_result = 0;
    mqtt_test_network_timeout_ms = 0U;
    mqtt_test_network_disconnect_count = 0U;
    mqtt_test_receive_count = 0U;
    mqtt_test_last_connect_packet_type = 0U;
    mqtt_test_receive_timeout_configured = false;
    outgoing_bytes = 0U;
    incoming_offset = 0U;
}

int mqtt_test_setsockopt(int socket_fd,
                         int level,
                         int option,
                         const void *value,
                         socklen_t value_length)
{
    (void)socket_fd;
    (void)level;
    (void)value;
    (void)value_length;
    if (option == SO_RCVTIMEO)
    {
        mqtt_test_receive_timeout_configured = true;
    }
    return 0;
}

int mqtt_test_shutdown(int socket_fd, int how)
{
    (void)socket_fd;
    (void)how;
    return 0;
}

int network_posix_connect_mode(NetworkContext_t *context,
                               const char *host,
                               const char *port,
                               NetworkSecurityMode_t security_mode,
                               const char *ca,
                               uint32_t ca_len,
                               uint32_t timeout_ms)
{
    (void)host;
    (void)port;
    (void)security_mode;
    (void)ca;
    (void)ca_len;

    mqtt_test_network_timeout_ms = timeout_ms;
    if (mqtt_test_network_connect_result != 0)
    {
        return mqtt_test_network_connect_result;
    }

    (void)memset(context, 0, sizeof(*context));
    context->socket_fd = 123;
    return 0;
}

int network_posix_connect(NetworkContext_t *context,
                          const char *host,
                          const char *port,
                          const char *ca,
                          uint32_t ca_len)
{
    return network_posix_connect_mode(context,
                                      host,
                                      port,
                                      NETWORK_SECURITY_TLS,
                                      ca,
                                      ca_len,
                                      10000U);
}

int32_t network_posix_send(NetworkContext_t *context,
                           const void *buffer,
                           size_t length)
{
    const uint8_t *bytes = buffer;

    (void)context;
    if (length > 0U && outgoing_bytes == 0U)
    {
        mqtt_test_last_connect_packet_type = bytes[0];
    }
    outgoing_bytes += length;
    return (int32_t)length;
}

int32_t network_posix_recv(NetworkContext_t *context,
                           void *buffer,
                           size_t length)
{
    const uint8_t *response;
    size_t response_length = sizeof(connack_success);
    size_t bytes_to_copy;

    (void)context;
    mqtt_test_receive_count++;
    if (mqtt_test_connack_mode == MQTT_TEST_CONNACK_TIMEOUT)
    {
        return 0;
    }

    response = mqtt_test_connack_mode == MQTT_TEST_CONNACK_REFUSED
                   ? connack_refused
                   : connack_success;
    if (incoming_offset >= response_length)
    {
        return 0;
    }

    bytes_to_copy = response_length - incoming_offset;
    if (bytes_to_copy > length)
    {
        bytes_to_copy = length;
    }

    (void)memcpy(buffer, &response[incoming_offset], bytes_to_copy);
    incoming_offset += bytes_to_copy;
    return (int32_t)bytes_to_copy;
}

int32_t network_posix_recv_poll(NetworkContext_t *context,
                                void *buffer,
                                size_t length)
{
    return network_posix_recv(context, buffer, length);
}

void network_posix_disconnect(NetworkContext_t *context)
{
    mqtt_test_network_disconnect_count++;
    if (context != NULL)
    {
        context->socket_fd = -1;
    }
}
