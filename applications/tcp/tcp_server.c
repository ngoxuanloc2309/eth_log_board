#include "tcp_server.h"

#include "tcp_client_internal.h"

#include <errno.h>
#include <string.h>

#include "FreeRTOS.h"
#include "logger.h"
#include "lwip/errno.h"
#include "lwip/sockets.h"
#include "semphr.h"
#include "task.h"

#define TCP_SERVER_LOG_TAG "TCP_SERVER"

struct tcp_server
{
    tcp_server_config_t config;
    tcp_server_event_callback_t callback;
    void *callback_context;
    int listen_socket;
    TaskHandle_t task;
    SemaphoreHandle_t started;
    SemaphoreHandle_t stopped;
    StaticTask_t task_control;
    StackType_t task_stack[TCP_SERVER_TASK_STACK_WORDS];
    StaticSemaphore_t started_storage;
    StaticSemaphore_t stopped_storage;
    bool startup_succeeded;
    uint16_t active_clients;
    tcp_client_t clients[TCP_SERVER_MAX_CLIENTS];
};

static tcp_server_t tcp_server_storage;
static bool tcp_server_storage_in_use;

static void tcp_server_notify(tcp_server_t *server,
                              tcp_client_t *client,
                              tcp_server_event_t event)
{
    if (server->callback != NULL) {
        server->callback(server, client, event, server->callback_context);
    }
}

static bool tcp_server_set_nonblocking(int socket_fd)
{
    int enabled = 1;

    return lwip_ioctl(socket_fd, FIONBIO, &enabled) == 0;
}

static void tcp_server_close_fd(int socket_fd, const char *description)
{
    if (socket_fd >= 0 && lwip_close(socket_fd) != 0) {
        int error = errno;
        LOGE(TCP_SERVER_LOG_TAG, "Could not close %s socket: %d",
             description, error);
    }
}

static void tcp_server_close_socket(int *socket_fd, const char *description)
{
    if (*socket_fd >= 0) {
        tcp_server_close_fd(*socket_fd, description);
        *socket_fd = -1;
    }
}

static int tcp_server_create_listener(tcp_server_t *server)
{
    struct sockaddr_in address;
    int socket_fd;
    int error;

    socket_fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_fd < 0) {
        error = errno;
        LOGE(TCP_SERVER_LOG_TAG, "lwip_socket failed: %d", error);
        return -1;
    }
    if (socket_fd >= FD_SETSIZE) {
        LOGE(TCP_SERVER_LOG_TAG,
             "Listener descriptor %d exceeds FD_SETSIZE %d",
             socket_fd, FD_SETSIZE);
        tcp_server_close_fd(socket_fd, "unusable listener");
        return -1;
    }
    if (!tcp_server_set_nonblocking(socket_fd)) {
        error = errno;
        LOGE(TCP_SERVER_LOG_TAG, "Could not make listener non-blocking: %d",
             error);
        tcp_server_close_fd(socket_fd, "listener");
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = lwip_htons(server->config.port);
    address.sin_addr.s_addr = server->config.listen_address;

    if (lwip_bind(socket_fd, (const struct sockaddr *)&address,
                  sizeof(address)) != 0) {
        error = errno;
        LOGE(TCP_SERVER_LOG_TAG, "lwip_bind failed: %d", error);
        tcp_server_close_fd(socket_fd, "listener");
        return -1;
    }
    if (lwip_listen(socket_fd, server->config.listen_backlog) != 0) {
        error = errno;
        LOGE(TCP_SERVER_LOG_TAG, "lwip_listen failed: %d", error);
        tcp_server_close_fd(socket_fd, "listener");
        return -1;
    }

    LOGI(TCP_SERVER_LOG_TAG, "Listening on port %u",
         (unsigned int)server->config.port);
    return socket_fd;
}

static void tcp_server_deactivate_client(tcp_server_t *server,
                                         tcp_client_t *client)
{
    int socket_fd = client->socket_fd;

    tcp_client_deactivate_internal(client);
    tcp_server_close_fd(socket_fd, "client");

    taskENTER_CRITICAL();
    if (server->active_clients > 0U) {
        server->active_clients--;
    }
    taskEXIT_CRITICAL();
    tcp_server_notify(server, client, TCP_SERVER_EVENT_CLIENT_DISCONNECTED);
    tcp_client_clear_buffers_internal(client);
}

static tcp_client_t *tcp_server_find_free_client(tcp_server_t *server)
{
    uint16_t index;

    for (index = 0U; index < server->config.max_clients; index++) {
        if (!tcp_client_is_active_internal(&server->clients[index])) {
            return &server->clients[index];
        }
    }
    return NULL;
}

static void tcp_server_accept_pending(tcp_server_t *server)
{
    for (;;) {
        struct sockaddr_in remote_address;
        socklen_t address_length = sizeof(remote_address);
        tcp_client_t *client;
        int client_socket;

        memset(&remote_address, 0, sizeof(remote_address));
        client_socket = lwip_accept(server->listen_socket,
                                    (struct sockaddr *)&remote_address,
                                    &address_length);
        if (client_socket < 0) {
            int error = errno;

            if (error == EAGAIN || error == EWOULDBLOCK) {
                return;
            }
            if (error == EINTR) {
                continue;
            }
            LOGE(TCP_SERVER_LOG_TAG, "lwip_accept failed: %d", error);
            return;
        }

        if (client_socket >= FD_SETSIZE) {
            LOGW(TCP_SERVER_LOG_TAG,
                 "Rejecting descriptor %d outside FD_SETSIZE %d",
                 client_socket, FD_SETSIZE);
            tcp_server_close_fd(client_socket, "rejected client");
            continue;
        }
        if (!tcp_server_set_nonblocking(client_socket)) {
            int error = errno;
            LOGE(TCP_SERVER_LOG_TAG,
                 "Could not make accepted socket non-blocking: %d", error);
            tcp_server_close_fd(client_socket, "unusable client");
            continue;
        }

        client = tcp_server_find_free_client(server);
        if (client == NULL) {
            LOGW(TCP_SERVER_LOG_TAG,
                 "Rejecting client: maximum of %u active connections reached",
                 (unsigned int)server->config.max_clients);
            tcp_server_close_fd(client_socket, "rejected client");
            continue;
        }

        tcp_client_init_internal(client, client_socket, &remote_address);
        taskENTER_CRITICAL();
        server->active_clients++;
        taskEXIT_CRITICAL();
        tcp_server_notify(server, client, TCP_SERVER_EVENT_CLIENT_CONNECTED);
    }
}

static bool tcp_server_receive_client(tcp_server_t *server,
                                      tcp_client_t *client)
{
    size_t byte_budget = TCP_CLIENT_BUFFER_SIZE;

    while (byte_budget > 0U) {
        size_t free_length;
        uint8_t *buffer = tcp_client_rx_free_span_internal(client,
                                                           &free_length);
        ssize_t received;

        if (free_length == 0U || buffer == NULL) {
            return true;
        }
        if (free_length > byte_budget) {
            free_length = byte_budget;
        }

        received = lwip_recv(client->socket_fd, buffer, free_length, 0);
        if (received > 0) {
            if (!tcp_client_rx_commit_internal(client, (size_t)received)) {
                LOGE(TCP_SERVER_LOG_TAG,
                     "Could not commit received client data");
                tcp_server_deactivate_client(server, client);
                return false;
            }
            byte_budget -= (size_t)received;
            tcp_server_notify(server, client,
                              TCP_SERVER_EVENT_CLIENT_DATA_AVAILABLE);
            continue;
        }
        if (received == 0) {
            tcp_server_deactivate_client(server, client);
            return false;
        }

        {
            int error = errno;

            if (error == EAGAIN || error == EWOULDBLOCK) {
                return true;
            }
            if (error == EINTR) {
                continue;
            }
            LOGE(TCP_SERVER_LOG_TAG, "lwip_recv failed: %d", error);
            tcp_server_deactivate_client(server, client);
            return false;
        }
    }
    return true;
}

static bool tcp_server_send_client(tcp_server_t *server,
                                   tcp_client_t *client)
{
    size_t data_length;
    const uint8_t *data = tcp_client_tx_data_internal(client, &data_length);
    ssize_t sent;

    if (data_length == 0U || data == NULL) {
        return true;
    }

    sent = lwip_send(client->socket_fd, data, data_length, 0);
    if (sent > 0) {
        if (!tcp_client_tx_consume_internal(client, (size_t)sent)) {
            LOGE(TCP_SERVER_LOG_TAG, "Could not consume sent client data");
            tcp_server_deactivate_client(server, client);
            return false;
        }
        tcp_server_notify(server, client,
                          TCP_SERVER_EVENT_CLIENT_SEND_SPACE);
        return true;
    }
    if (sent == 0) {
        tcp_server_deactivate_client(server, client);
        return false;
    }

    {
        int error = errno;

        if (error == EAGAIN || error == EWOULDBLOCK || error == EINTR) {
            return true;
        }
        LOGE(TCP_SERVER_LOG_TAG, "lwip_send failed: %d", error);
        tcp_server_deactivate_client(server, client);
        return false;
    }
}

static void tcp_server_close_all_clients(tcp_server_t *server)
{
    uint16_t index;

    for (index = 0U; index < server->config.max_clients; index++) {
        tcp_client_t *client = &server->clients[index];
        if (tcp_client_is_active_internal(client)) {
            tcp_server_deactivate_client(server, client);
        }
    }
}

static void tcp_server_task(void *argument)
{
    tcp_server_t *server = argument;
    uint16_t index;

    for (index = 0U; index < TCP_SERVER_MAX_CLIENTS; index++) {
        server->clients[index].socket_fd = -1;
    }

    server->listen_socket = tcp_server_create_listener(server);
    if (server->listen_socket < 0) {
        server->startup_succeeded = false;
        (void)xSemaphoreGive(server->started);
        vTaskSuspend(NULL);
    }

    server->startup_succeeded = true;
    (void)xSemaphoreGive(server->started);

    for (;;) {
        fd_set read_set;
        fd_set write_set;
        struct timeval timeout;
        int max_socket = server->listen_socket;
        int select_result;

        if (ulTaskNotifyTake(pdTRUE, 0U) > 0U) {
            break;
        }

        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(server->listen_socket, &read_set);

        for (index = 0U; index < server->config.max_clients; index++) {
            tcp_client_t *client = &server->clients[index];
            size_t rx_free;

            if (!tcp_client_is_active_internal(client)) {
                continue;
            }
            if (tcp_client_rx_free_span_internal(client, &rx_free) != NULL &&
                rx_free > 0U) {
                FD_SET(client->socket_fd, &read_set);
            }
            if (tcp_client_tx_queued_internal(client) > 0U) {
                FD_SET(client->socket_fd, &write_set);
            }
            if (client->socket_fd > max_socket) {
                max_socket = client->socket_fd;
            }
        }

        timeout.tv_sec =
            (long)(server->config.select_timeout_ms / 1000U);
        timeout.tv_usec =
            (long)((server->config.select_timeout_ms % 1000U) * 1000U);

        select_result = lwip_select(max_socket + 1, &read_set, &write_set,
                                    NULL, &timeout);
        if (select_result < 0) {
            int error = errno;

            if (error == EINTR) {
                continue;
            }
            LOGE(TCP_SERVER_LOG_TAG, "lwip_select failed: %d", error);
            vTaskDelay(pdMS_TO_TICKS(50U));
            continue;
        }
        if (ulTaskNotifyTake(pdTRUE, 0U) > 0U) {
            break;
        }
        if (select_result == 0) {
            continue;
        }

        if (FD_ISSET(server->listen_socket, &read_set)) {
            tcp_server_accept_pending(server);
        }

        for (index = 0U; index < server->config.max_clients; index++) {
            tcp_client_t *client = &server->clients[index];
            int socket_fd;

            if (!tcp_client_is_active_internal(client)) {
                continue;
            }
            socket_fd = client->socket_fd;
            if (FD_ISSET(socket_fd, &read_set) &&
                !tcp_server_receive_client(server, client)) {
                continue;
            }
            if (tcp_client_is_active_internal(client) &&
                FD_ISSET(socket_fd, &write_set)) {
                (void)tcp_server_send_client(server, client);
            }
        }
    }

    tcp_server_close_all_clients(server);
    tcp_server_close_socket(&server->listen_socket, "listener");
    (void)xSemaphoreGive(server->stopped);
    vTaskSuspend(NULL);
}

static void tcp_server_release(void)
{
    taskENTER_CRITICAL();
    tcp_server_storage_in_use = false;
    taskEXIT_CRITICAL();
}

bool tcp_server_start(const tcp_server_config_t *config,
                      tcp_server_event_callback_t callback,
                      void *callback_context,
                      tcp_server_t **server_out)
{
    tcp_server_t *server;

    if (server_out == NULL) {
        return false;
    }
    *server_out = NULL;
    if (config == NULL || config->port == 0U || config->max_clients == 0U ||
        config->max_clients > TCP_SERVER_MAX_CLIENTS ||
        config->select_timeout_ms > TCP_SERVER_MAX_SELECT_TIMEOUT_MS ||
        config->task_priority >= configMAX_PRIORITIES ||
        TCP_SERVER_TASK_STACK_WORDS < configMINIMAL_STACK_SIZE) {
        LOGE(TCP_SERVER_LOG_TAG, "Invalid TCP server configuration");
        return false;
    }

    taskENTER_CRITICAL();
    if (tcp_server_storage_in_use) {
        taskEXIT_CRITICAL();
        LOGE(TCP_SERVER_LOG_TAG, "A TCP server instance is already active");
        return false;
    }
    tcp_server_storage_in_use = true;
    taskEXIT_CRITICAL();

    server = &tcp_server_storage;
    memset(server, 0, sizeof(*server));
    server->config = *config;
    server->callback = callback;
    server->callback_context = callback_context;
    server->listen_socket = -1;
    if (server->config.listen_backlog == 0U) {
        server->config.listen_backlog = server->config.max_clients;
    }
    if (server->config.select_timeout_ms == 0U) {
        server->config.select_timeout_ms =
            TCP_SERVER_DEFAULT_SELECT_TIMEOUT_MS;
    }

    server->started = xSemaphoreCreateBinaryStatic(&server->started_storage);
    server->stopped = xSemaphoreCreateBinaryStatic(&server->stopped_storage);

    server->task = xTaskCreateStatic(
        tcp_server_task, "tcp-server", TCP_SERVER_TASK_STACK_WORDS, server,
        (UBaseType_t)server->config.task_priority, server->task_stack,
        &server->task_control);
    if (server->task == NULL) {
        LOGE(TCP_SERVER_LOG_TAG, "Could not create TCP server task");
        tcp_server_release();
        return false;
    }

    if (xSemaphoreTake(server->started, portMAX_DELAY) != pdTRUE) {
        LOGE(TCP_SERVER_LOG_TAG, "Could not wait for server startup");
        vTaskDelete(server->task);
        tcp_server_release();
        return false;
    }
    if (!server->startup_succeeded) {
        vTaskDelete(server->task);
        tcp_server_release();
        return false;
    }

    *server_out = server;
    return true;
}

bool tcp_server_stop(tcp_server_t *server)
{
    if (server == NULL || xTaskGetCurrentTaskHandle() == server->task) {
        return false;
    }

    (void)xTaskNotifyGive(server->task);
    if (xSemaphoreTake(server->stopped, portMAX_DELAY) != pdTRUE) {
        LOGE(TCP_SERVER_LOG_TAG, "Could not wait for server shutdown");
        return false;
    }
    vTaskDelete(server->task);
    tcp_server_release();
    return true;
}

uint16_t tcp_server_get_client_count(const tcp_server_t *server)
{
    uint16_t active_clients;

    if (server == NULL) {
        return 0U;
    }

    taskENTER_CRITICAL();
    active_clients = server->active_clients;
    taskEXIT_CRITICAL();
    return active_clients;
}
