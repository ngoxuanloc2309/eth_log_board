#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "tcp_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_SERVER_MAX_CLIENTS 4U
#define TCP_SERVER_DEFAULT_SELECT_TIMEOUT_MS 100U
#define TCP_SERVER_MAX_SELECT_TIMEOUT_MS 1000U
#define TCP_SERVER_TASK_STACK_WORDS 768U

typedef struct tcp_server tcp_server_t;

typedef enum
{
    TCP_SERVER_EVENT_CLIENT_CONNECTED,
    TCP_SERVER_EVENT_CLIENT_DATA_AVAILABLE,
    TCP_SERVER_EVENT_CLIENT_SEND_SPACE,
    TCP_SERVER_EVENT_CLIENT_DISCONNECTED
} tcp_server_event_t;

typedef struct
{
    /* IPv4 address in network byte order; zero selects INADDR_ANY. */
    uint32_t listen_address;
    uint16_t port;
    uint16_t max_clients;
    uint16_t listen_backlog;
    uint32_t select_timeout_ms;
    uint8_t task_priority;
} tcp_server_config_t;

typedef void (*tcp_server_event_callback_t)(tcp_server_t *server,
                                             tcp_client_t *client,
                                             tcp_server_event_t event,
                                             void *context);

/*
 * Starts the single statically allocated server and waits until its listening
 * socket is ready. Only one server instance may be active at a time.
 * The callback runs in the server task and must not block. Copy or consume
 * borrowed RX data before returning if it must outlive the callback.
 */
bool tcp_server_start(const tcp_server_config_t *config,
                      tcp_server_event_callback_t callback,
                      void *callback_context,
                      tcp_server_t **server_out);

/*
 * Stops the server, closes its sockets in the server task, and releases it.
 * Must be called from a task other than the server's callback/task.
 */
bool tcp_server_stop(tcp_server_t *server);

/* Returns the current number of active client connections. */
uint16_t tcp_server_get_client_count(const tcp_server_t *server);

#ifdef __cplusplus
}
#endif

#endif
