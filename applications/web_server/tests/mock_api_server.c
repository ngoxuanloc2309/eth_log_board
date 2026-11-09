#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define WEB_TEST_DEFAULT_PORT (8080U)
#define WEB_TEST_BACKLOG (2)
#define WEB_TEST_HEADER_MAX (1024U)
#define WEB_TEST_BODY_MAX (512U)
#define WEB_TEST_REQUEST_MAX (WEB_TEST_HEADER_MAX + WEB_TEST_BODY_MAX + 1U)
#define WEB_TEST_RESPONSE_MAX (1024U)
#define WEB_TEST_IO_CHUNK (1024U)
#define WEB_TEST_OPERATION_MS (1200U)
#define WEB_TEST_CONFIG_FIELD_COUNT (10U)

typedef struct
{
    uint16_t pan_id;
    uint8_t channel;
    uint16_t user_address;
    uint8_t point_type;
    uint8_t transfer_model;
    uint8_t baud_rate;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t antenna_select;
} mock_radio_config_t;

typedef enum
{
    MOCK_OPERATION_IDLE,
    MOCK_OPERATION_PENDING,
    MOCK_OPERATION_SUCCESS,
    MOCK_OPERATION_FAILURE
} mock_operation_state_t;

typedef struct
{
    mock_operation_state_t state;
    const char *kind;
    uint64_t started_ms;
    mock_radio_config_t pending_config;
    bool has_pending_config;
} mock_operation_t;

typedef struct
{
    char method[8];
    char path[128];
    char version[16];
    const char *body;
    size_t body_length;
} mock_http_request_t;

static mock_radio_config_t mock_radio_config = {
    .pan_id = 0x2B02U,
    .channel = 22U,
    .user_address = 0x6677U,
    .point_type = 1U,
    .transfer_model = 1U,
    .baud_rate = 8U,
    .data_bits = 8U,
    .stop_bits = 1U,
    .parity = 1U,
    .antenna_select = 1U,
};

static mock_operation_t mock_operation = {
    .state = MOCK_OPERATION_IDLE,
    .kind = "none",
    .started_ms = 0U,
    .has_pending_config = false,
};

static const char *mock_settings_save_state = "idle";

static const char *const mock_config_field_names[WEB_TEST_CONFIG_FIELD_COUNT] = {
    "panId",
    "channel",
    "userAddress",
    "pointType",
    "transferModel",
    "baudRate",
    "dataBits",
    "stopBits",
    "parity",
    "antennaSelect",
};

static uint64_t mock_now_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }

    return ((uint64_t)now.tv_sec * UINT64_C(1000)) +
           ((uint64_t)now.tv_nsec / UINT64_C(1000000));
}

static const char *mock_operation_state_name(mock_operation_state_t state)
{
    switch (state) {
    case MOCK_OPERATION_PENDING:
        return "pending";
    case MOCK_OPERATION_SUCCESS:
        return "success";
    case MOCK_OPERATION_FAILURE:
        return "failure";
    case MOCK_OPERATION_IDLE:
    default:
        return "idle";
    }
}

static void mock_operation_advance(void)
{
    if (mock_operation.state == MOCK_OPERATION_PENDING &&
        (mock_now_ms() - mock_operation.started_ms) >= WEB_TEST_OPERATION_MS) {
        if (mock_operation.has_pending_config) {
            mock_radio_config = mock_operation.pending_config;
            mock_operation.has_pending_config = false;
        }
        mock_operation.state = MOCK_OPERATION_SUCCESS;
    }
}

static bool mock_operation_start(const char *kind,
                                 const mock_radio_config_t *config)
{
    if (mock_operation.state == MOCK_OPERATION_PENDING) {
        return false;
    }

    mock_operation.state = MOCK_OPERATION_PENDING;
    mock_operation.kind = kind;
    mock_operation.started_ms = mock_now_ms();
    mock_operation.has_pending_config = config != NULL;
    if (config != NULL) {
        mock_operation.pending_config = *config;
    }
    return true;
}

static bool mock_send_all(int32_t socket_fd,
                          const void *data,
                          size_t length)
{
    const uint8_t *bytes = data;
    size_t sent_total = 0U;

    while (sent_total < length) {
        ssize_t sent = send(socket_fd, &bytes[sent_total],
                            length - sent_total, 0);
        if (sent <= 0) {
            return false;
        }
        sent_total += (size_t)sent;
    }
    return true;
}

static bool mock_send_response(int32_t socket_fd,
                               uint16_t status_code,
                               const char *reason,
                               const char *content_type,
                               const void *body,
                               size_t body_length)
{
    char header[WEB_TEST_RESPONSE_MAX];
    int32_t header_length;

    header_length = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %u %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'self'; style-src 'self'; "
        "script-src 'self'; img-src 'self' data:; connect-src 'self'\r\n"
        "\r\n",
        (unsigned int)status_code,
        reason,
        content_type,
        body_length);
    if (header_length < 0 || (size_t)header_length >= sizeof(header)) {
        return false;
    }
    if (!mock_send_all(socket_fd, header, (size_t)header_length)) {
        return false;
    }
    return body_length == 0U || mock_send_all(socket_fd, body, body_length);
}

static bool mock_send_json(int32_t socket_fd,
                           uint16_t status_code,
                           const char *reason,
                           const char *json)
{
    return mock_send_response(socket_fd,
                              status_code,
                              reason,
                              "application/json; charset=utf-8",
                              json,
                              strlen(json));
}

static bool mock_send_error(int32_t socket_fd,
                            uint16_t status_code,
                            const char *reason,
                            const char *message)
{
    char json[WEB_TEST_RESPONSE_MAX];
    int32_t json_length = snprintf(json, sizeof(json),
                                   "{\"error\":\"%s\"}", message);

    if (json_length < 0 || (size_t)json_length >= sizeof(json)) {
        return false;
    }
    return mock_send_json(socket_fd, status_code, reason, json);
}

static char *mock_find_header_end(char *request, size_t length)
{
    size_t index;

    if (length < 4U) {
        return NULL;
    }
    for (index = 0U; index <= length - 4U; index++) {
        if (memcmp(&request[index], "\r\n\r\n", 4U) == 0) {
            return &request[index];
        }
    }
    return NULL;
}

static bool mock_parse_content_length(char *headers,
                                     size_t header_length,
                                     size_t *content_length)
{
    char *line = strstr(headers, "\r\n");

    if (line == NULL) {
        return false;
    }
    line += 2;
    while ((size_t)(line - headers) < header_length) {
        char *line_end = strstr(line, "\r\n");

        if (line_end == NULL ||
            (size_t)(line_end - headers) > header_length) {
            return false;
        }
        if ((size_t)(line_end - line) >= 15U &&
            strncasecmp(line, "Content-Length:", 15U) == 0) {
            char *value = line + 15;
            char *parse_end;
            uintmax_t parsed;

            while (*value == ' ' || *value == '\t') {
                value++;
            }
            errno = 0;
            parsed = strtoumax(value, &parse_end, 10);
            if (errno != 0 || parse_end == value ||
                parse_end != line_end || parsed > WEB_TEST_BODY_MAX) {
                return false;
            }
            *content_length = (size_t)parsed;
            return true;
        }
        line = line_end + 2;
    }

    *content_length = 0U;
    return true;
}

static bool mock_receive_request(int32_t socket_fd,
                                char *buffer,
                                size_t buffer_capacity,
                                mock_http_request_t *request,
                                size_t *request_length)
{
    size_t received_total = 0U;
    size_t header_length = 0U;
    size_t content_length = 0U;
    char *header_end = NULL;

    while (received_total < buffer_capacity - 1U) {
        ssize_t received = recv(socket_fd,
                                &buffer[received_total],
                                buffer_capacity - 1U - received_total,
                                0);
        if (received <= 0) {
            return false;
        }
        received_total += (size_t)received;
        buffer[received_total] = '\0';
        header_end = mock_find_header_end(buffer, received_total);
        if (header_end != NULL) {
            header_length = (size_t)(header_end - buffer) + 2U;
            if (header_length > WEB_TEST_HEADER_MAX ||
                !mock_parse_content_length(buffer, header_length,
                                           &content_length)) {
                return false;
            }
            if (received_total >= header_length + 2U + content_length) {
                break;
            }
        } else if (received_total >= WEB_TEST_HEADER_MAX) {
            return false;
        }
    }

    if (header_end == NULL ||
        received_total < header_length + 2U + content_length) {
        return false;
    }
    *header_end = '\0';

    if (sscanf(buffer, "%7s %127s %15s",
               request->method,
               request->path,
               request->version) != 3) {
        return false;
    }
    if (strcmp(request->version, "HTTP/1.1") != 0 &&
        strcmp(request->version, "HTTP/1.0") != 0) {
        return false;
    }
    request->body = header_end + 4;
    request->body_length = content_length;
    *request_length = header_length + 2U + content_length;
    return true;
}

static void mock_skip_whitespace(const char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t' ||
           **cursor == '\r' || **cursor == '\n') {
        (*cursor)++;
    }
}

static bool mock_parse_json_key(const char **cursor,
                                char *key,
                                size_t key_capacity)
{
    size_t key_length = 0U;

    mock_skip_whitespace(cursor);
    if (**cursor != '"') {
        return false;
    }
    (*cursor)++;
    while (**cursor != '\0' && **cursor != '"') {
        if (**cursor == '\\' || key_length >= key_capacity - 1U) {
            return false;
        }
        key[key_length] = **cursor;
        key_length++;
        (*cursor)++;
    }
    if (**cursor != '"') {
        return false;
    }
    key[key_length] = '\0';
    (*cursor)++;
    return true;
}

static bool mock_parse_json_value(const char **cursor, uint32_t *value)
{
    uint32_t parsed = 0U;
    bool has_digit = false;

    mock_skip_whitespace(cursor);
    while (**cursor >= '0' && **cursor <= '9') {
        uint32_t digit = (uint32_t)(**cursor - '0');

        if (parsed > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        parsed = (parsed * 10U) + digit;
        has_digit = true;
        (*cursor)++;
    }
    if (!has_digit) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool mock_config_set_value(mock_radio_config_t *config,
                                  uint32_t field,
                                  uint32_t value)
{
    switch (field) {
    case 0U:
        if (value < 1U || value > 0xFF00U) {
            return false;
        }
        config->pan_id = (uint16_t)value;
        return true;
    case 1U:
        if (value < 11U || value > 26U) {
            return false;
        }
        config->channel = (uint8_t)value;
        return true;
    case 2U:
        if (value < 1U || value > 0xFF00U) {
            return false;
        }
        config->user_address = (uint16_t)value;
        return true;
    case 3U:
        if (value < 1U || value > 3U) {
            return false;
        }
        config->point_type = (uint8_t)value;
        return true;
    case 4U:
        if (value < 1U || value > 5U) {
            return false;
        }
        config->transfer_model = (uint8_t)value;
        return true;
    case 5U:
        if (value < 1U || value > 8U) {
            return false;
        }
        config->baud_rate = (uint8_t)value;
        return true;
    case 6U:
        if (value != 8U) {
            return false;
        }
        config->data_bits = (uint8_t)value;
        return true;
    case 7U:
        if (value != 1U) {
            return false;
        }
        config->stop_bits = (uint8_t)value;
        return true;
    case 8U:
        if (value < 1U || value > 3U) {
            return false;
        }
        config->parity = (uint8_t)value;
        return true;
    case 9U:
        if (value > 1U) {
            return false;
        }
        config->antenna_select = (uint8_t)value;
        return true;
    default:
        return false;
    }
}

static bool mock_parse_config(const char *body,
                              size_t body_length,
                              mock_radio_config_t *config)
{
    const char *cursor = body;
    uint16_t seen_fields = 0U;
    uint32_t field_count = 0U;
    size_t index;

    if (body_length == 0U || config == NULL) {
        return false;
    }
    *config = mock_radio_config;
    mock_skip_whitespace(&cursor);
    if (*cursor != '{') {
        return false;
    }
    cursor++;

    for (;;) {
        char key[24];
        uint32_t value;
        bool found = false;

        mock_skip_whitespace(&cursor);
        if (*cursor == '}') {
            cursor++;
            break;
        }
        if (!mock_parse_json_key(&cursor, key, sizeof(key))) {
            return false;
        }
        mock_skip_whitespace(&cursor);
        if (*cursor != ':') {
            return false;
        }
        cursor++;
        if (!mock_parse_json_value(&cursor, &value)) {
            return false;
        }

        for (index = 0U; index < WEB_TEST_CONFIG_FIELD_COUNT; index++) {
            if (strcmp(key, mock_config_field_names[index]) == 0) {
                uint16_t field_mask = (uint16_t)(1U << index);

                if ((seen_fields & field_mask) != 0U ||
                    !mock_config_set_value(config, (uint32_t)index, value)) {
                    return false;
                }
                seen_fields |= field_mask;
                field_count++;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }

        mock_skip_whitespace(&cursor);
        if (*cursor == ',') {
            cursor++;
            continue;
        }
        if (*cursor == '}') {
            cursor++;
            break;
        }
        return false;
    }

    mock_skip_whitespace(&cursor);
    return *cursor == '\0' &&
           field_count == WEB_TEST_CONFIG_FIELD_COUNT;
}

static bool mock_body_is_empty_object(const char *body, size_t body_length)
{
    const char *cursor = body;
    const char *end = body + body_length;

    while (cursor < end &&
           (*cursor == ' ' || *cursor == '\t' ||
            *cursor == '\r' || *cursor == '\n')) {
        cursor++;
    }
    if (cursor >= end || *cursor != '{') {
        return false;
    }
    cursor++;
    while (cursor < end &&
           (*cursor == ' ' || *cursor == '\t' ||
            *cursor == '\r' || *cursor == '\n')) {
        cursor++;
    }
    if (cursor >= end || *cursor != '}') {
        return false;
    }
    cursor++;
    while (cursor < end &&
           (*cursor == ' ' || *cursor == '\t' ||
            *cursor == '\r' || *cursor == '\n')) {
        cursor++;
    }
    return cursor == end;
}

static bool mock_body_is_object(const char *body, size_t body_length)
{
    const char *cursor = body;
    const char *end = body + body_length;

    while (cursor < end &&
           (*cursor == ' ' || *cursor == '\t' ||
            *cursor == '\r' || *cursor == '\n')) {
        cursor++;
    }
    if (cursor >= end || *cursor++ != '{') {
        return false;
    }
    while (end > cursor &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    return end > cursor && end[-1] == '}';
}

static bool mock_format_operation_json(char *buffer, size_t capacity)
{
    int32_t length;

    mock_operation_advance();
    length = snprintf(buffer,
                      capacity,
                      "{\"kind\":\"%s\",\"state\":\"%s\",\"error\":null}",
                      mock_operation.kind,
                      mock_operation_state_name(mock_operation.state));
    return length >= 0 && (size_t)length < capacity;
}

static bool mock_send_operation_accepted(int32_t socket_fd)
{
    char operation_json[160];
    char response_json[WEB_TEST_RESPONSE_MAX];
    int32_t response_length;

    if (!mock_format_operation_json(operation_json, sizeof(operation_json))) {
        return mock_send_error(socket_fd, 500U, "Internal Server Error",
                               "response_error");
    }
    response_length = snprintf(response_json,
                               sizeof(response_json),
                               "{\"operation\":%s}",
                               operation_json);
    if (response_length < 0 ||
        (size_t)response_length >= sizeof(response_json)) {
        return mock_send_error(socket_fd, 500U, "Internal Server Error",
                               "response_error");
    }
    return mock_send_json(socket_fd, 202U, "Accepted", response_json);
}

static bool mock_handle_api(int32_t socket_fd,
                            const mock_http_request_t *request)
{
    char json[WEB_TEST_RESPONSE_MAX];

    mock_operation_advance();
    if (strcmp(request->path, "/api/v1/status") == 0 &&
        strcmp(request->method, "GET") == 0) {
        char operation_json[160];
        int32_t length;

        if (!mock_format_operation_json(operation_json,
                                        sizeof(operation_json))) {
            return mock_send_error(socket_fd, 500U, "Internal Server Error",
                                   "response_error");
        }
        length = snprintf(
            json,
            sizeof(json),
            "{\"network\":{\"linkUp\":true,\"interfaceUp\":true,"
            "\"dhcp\":true,\"ip\":\"192.0.2.10\","
            "\"netmask\":\"255.255.255.0\","
            "\"gateway\":\"192.0.2.1\"},"
            "\"storage\":{\"mounted\":true},"
            "\"system\":{\"heapTotalBytes\":65536,"
            "\"heapFreeBytes\":42100,\"heapMinimumFreeBytes\":38000,"
            "\"cpuAvailable\":true,\"cpuTenthsPercent\":123,"
            "\"taskCount\":1,\"taskReported\":1,"
            "\"tasksTruncated\":false,\"tasks\":[{"
            "\"name\":\"mock_task\",\"state\":\"running\","
            "\"priority\":3,\"stackHighWaterBytes\":1024,"
            "\"cpuTenthsPercent\":100}]},"
            "\"operation\":%s}",
            operation_json);
        if (length < 0 || (size_t)length >= sizeof(json)) {
            return mock_send_error(socket_fd, 500U, "Internal Server Error",
                                   "response_error");
        }
        return mock_send_json(socket_fd, 200U, "OK", json);
    }

    if (strcmp(request->path, "/api/v1/settings") == 0 &&
        strcmp(request->method, "GET") == 0) {
        int32_t length = snprintf(
            json, sizeof(json),
            "{\"saveState\":\"%s\",\"apply\":\"reboot\","
            "\"mqtt\":{\"enabled\":true,\"tlsEnabled\":false,"
            "\"host\":\"broker.emqx.io\",\"port\":1883,"
            "\"clientId\":\"synaptix-dtu-h563\",\"keepAliveSeconds\":60},"
            "\"ethernet\":{\"dhcpEnabled\":true,"
            "\"address\":\"192.168.1.50\","
            "\"netmask\":\"255.255.255.0\","
            "\"gateway\":\"192.168.1.1\"},"
            "\"uart\":{\"uart7\":{\"baudRate\":115200,\"dataBits\":8,"
            "\"parity\":0,\"stopBits\":1},"
            "\"usart3\":{\"baudRate\":115200,\"dataBits\":8,"
            "\"parity\":0,\"stopBits\":1},"
            "\"usart10\":{\"baudRate\":115200,\"dataBits\":8,"
            "\"parity\":0,\"stopBits\":1},"
            "\"uart9ManagedBy\":\"radio-settings\"},"
            "\"productVariant\":null}",
            mock_settings_save_state);
        if (length < 0 || (size_t)length >= sizeof(json)) {
            return mock_send_error(socket_fd, 500U, "Internal Server Error",
                                   "response_error");
        }
        return mock_send_json(socket_fd, 200U, "OK", json);
    }

    if ((strcmp(request->path, "/api/v1/settings/mqtt") == 0 ||
         strcmp(request->path, "/api/v1/settings/ethernet") == 0 ||
         strcmp(request->path, "/api/v1/settings/uart7") == 0 ||
         strcmp(request->path, "/api/v1/settings/usart3") == 0 ||
         strcmp(request->path, "/api/v1/settings/usart10") == 0) &&
        strcmp(request->method, "PUT") == 0) {
        if (!mock_body_is_object(request->body, request->body_length)) {
            return mock_send_error(socket_fd, 400U, "Bad Request",
                                   "invalid_settings");
        }
        mock_settings_save_state = "saved";
        return mock_send_json(socket_fd, 202U, "Accepted",
                              "{\"status\":\"pending\",\"apply\":\"reboot\"}");
    }

    if (strcmp(request->path,
               "/api/v1/settings/network/confirm") == 0 &&
        strcmp(request->method, "POST") == 0) {
        if (!mock_body_is_empty_object(request->body,
                                       request->body_length)) {
            return mock_send_error(socket_fd, 400U, "Bad Request",
                                   "invalid_request");
        }
        return mock_send_json(socket_fd, 200U, "OK",
                              "{\"status\":\"confirmed\"}");
    }

    if (strcmp(request->path, "/api/v1/radio/config") == 0 &&
        strcmp(request->method, "GET") == 0) {
        int32_t length = snprintf(
            json,
            sizeof(json),
            "{\"initialized\":true,\"busy\":%s,"
            "\"configAvailable\":true,\"config\":{"
            "\"panId\":%u,\"channel\":%u,\"userAddress\":%u,"
            "\"pointType\":%u,\"transferModel\":%u,\"baudRate\":%u,"
            "\"dataBits\":%u,\"stopBits\":%u,\"parity\":%u,"
            "\"antennaSelect\":%u}}",
            mock_operation.state == MOCK_OPERATION_PENDING ? "true" : "false",
            (unsigned int)mock_radio_config.pan_id,
            (unsigned int)mock_radio_config.channel,
            (unsigned int)mock_radio_config.user_address,
            (unsigned int)mock_radio_config.point_type,
            (unsigned int)mock_radio_config.transfer_model,
            (unsigned int)mock_radio_config.baud_rate,
            (unsigned int)mock_radio_config.data_bits,
            (unsigned int)mock_radio_config.stop_bits,
            (unsigned int)mock_radio_config.parity,
            (unsigned int)mock_radio_config.antenna_select);
        if (length < 0 || (size_t)length >= sizeof(json)) {
            return mock_send_error(socket_fd, 500U, "Internal Server Error",
                                   "response_error");
        }
        return mock_send_json(socket_fd, 200U, "OK", json);
    }

    if (strcmp(request->path, "/api/v1/radio/read") == 0 &&
        strcmp(request->method, "POST") == 0) {
        if (!mock_body_is_empty_object(request->body,
                                       request->body_length)) {
            return mock_send_error(socket_fd, 400U, "Bad Request",
                                   "invalid_request_body");
        }
        if (!mock_operation_start("radio-read", NULL)) {
            return mock_send_error(socket_fd, 409U, "Conflict",
                                   "operation_in_progress");
        }
        return mock_send_operation_accepted(socket_fd);
    }

    if (strcmp(request->path, "/api/v1/radio/config") == 0 &&
        strcmp(request->method, "PUT") == 0) {
        mock_radio_config_t config;

        if (!mock_parse_config(request->body,
                               request->body_length,
                               &config)) {
            return mock_send_error(socket_fd, 400U, "Bad Request",
                                   "invalid_configuration");
        }
        if (!mock_operation_start("radio-update", &config)) {
            return mock_send_error(socket_fd, 409U, "Conflict",
                                   "operation_in_progress");
        }
        return mock_send_operation_accepted(socket_fd);
    }

    if (strncmp(request->path, "/api/", 5U) == 0) {
        return mock_send_error(socket_fd, 404U, "Not Found",
                               "route_not_found");
    }
    return mock_send_error(socket_fd, 405U, "Method Not Allowed",
                           "method_not_allowed");
}

static const char *mock_asset_name(const char *path, const char **content_type)
{
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        *content_type = "text/html; charset=utf-8";
        return "index.html";
    }
    if (strcmp(path, "/styles.css") == 0) {
        *content_type = "text/css; charset=utf-8";
        return "styles.css";
    }
    if (strcmp(path, "/app.js") == 0) {
        *content_type = "application/javascript; charset=utf-8";
        return "app.js";
    }
    if (strcmp(path, "/X-favicon.png") == 0) {
        *content_type = "image/png";
        return "X-favicon.png";
    }
    return NULL;
}

static bool mock_serve_asset(int32_t socket_fd,
                             const char *asset_name,
                             const char *content_type)
{
    char path[512];
    uint8_t chunk[WEB_TEST_IO_CHUNK];
    FILE *asset;
    long file_length;
    int32_t path_length;
    int32_t header_length;
    char header[WEB_TEST_RESPONSE_MAX];

    path_length = snprintf(path, sizeof(path), "%s/%s",
                           WEB_TEST_ASSET_DIR, asset_name);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        return mock_send_error(socket_fd, 500U, "Internal Server Error",
                               "asset_path_error");
    }
    asset = fopen(path, "rb");
    if (asset == NULL) {
        return mock_send_error(socket_fd, 404U, "Not Found",
                               "asset_not_found");
    }
    if (fseek(asset, 0L, SEEK_END) != 0 ||
        (file_length = ftell(asset)) < 0L ||
        fseek(asset, 0L, SEEK_SET) != 0) {
        (void)fclose(asset);
        return mock_send_error(socket_fd, 500U, "Internal Server Error",
                               "asset_read_error");
    }

    header_length = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ju\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Content-Security-Policy: default-src 'self'; style-src 'self'; "
        "script-src 'self'; img-src 'self' data:; connect-src 'self'\r\n"
        "\r\n",
        content_type,
        (uintmax_t)file_length);
    if (header_length < 0 || (size_t)header_length >= sizeof(header) ||
        !mock_send_all(socket_fd, header, (size_t)header_length)) {
        (void)fclose(asset);
        return false;
    }

    for (;;) {
        size_t bytes_read = fread(chunk, 1U, sizeof(chunk), asset);

        if (bytes_read > 0U && !mock_send_all(socket_fd, chunk, bytes_read)) {
            (void)fclose(asset);
            return false;
        }
        if (bytes_read < sizeof(chunk)) {
            if (ferror(asset) != 0) {
                (void)fclose(asset);
                return false;
            }
            break;
        }
    }
    return fclose(asset) == 0;
}

static bool mock_serve_request(int32_t socket_fd,
                               const mock_http_request_t *request)
{
    const char *content_type = NULL;
    const char *asset_name;

    if (strcmp(request->method, "GET") == 0) {
        asset_name = mock_asset_name(request->path, &content_type);
        if (asset_name != NULL) {
            return mock_serve_asset(socket_fd, asset_name, content_type);
        }
    }
    return mock_handle_api(socket_fd, request);
}

static bool mock_get_port(int32_t argc,
                          char *const argv[],
                          uint16_t *port)
{
    char *parse_end;
    uintmax_t parsed;

    if (port == NULL || argc > 2) {
        return false;
    }
    if (argc == 1) {
        *port = (uint16_t)WEB_TEST_DEFAULT_PORT;
        return true;
    }

    errno = 0;
    parsed = strtoumax(argv[1], &parse_end, 10);
    if (errno != 0 || parse_end == argv[1] ||
        *parse_end != '\0' || parsed == 0U || parsed > UINT16_MAX) {
        return false;
    }
    *port = (uint16_t)parsed;
    return true;
}

int main(int argc, char **argv)
{
    struct sockaddr_in address;
    uint16_t port;
    int32_t server_fd;

    if (!mock_get_port(argc, argv, &port)) {
        (void)fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    server_fd = (int32_t)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    {
        int32_t reuse_address = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                       sizeof(reuse_address)) != 0) {
            perror("setsockopt");
            (void)close(server_fd);
            return EXIT_FAILURE;
        }
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server_fd, (const struct sockaddr *)&address,
             sizeof(address)) != 0) {
        perror("bind");
        (void)close(server_fd);
        return EXIT_FAILURE;
    }
    if (listen(server_fd, WEB_TEST_BACKLOG) != 0) {
        perror("listen");
        (void)close(server_fd);
        return EXIT_FAILURE;
    }

    (void)printf("Host mock API listening on http://127.0.0.1:%u\n",
                 (unsigned int)port);
    (void)fflush(stdout);

    for (;;) {
        struct sockaddr_in client_address;
        socklen_t address_length = sizeof(client_address);
        int32_t client_fd;
        char buffer[WEB_TEST_REQUEST_MAX];
        mock_http_request_t request;
        size_t request_length = 0U;

        client_fd = (int32_t)accept(server_fd,
                                    (struct sockaddr *)&client_address,
                                    &address_length);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        memset(&request, 0, sizeof(request));
        if (!mock_receive_request(client_fd,
                                  buffer,
                                  sizeof(buffer),
                                  &request,
                                  &request_length)) {
            (void)mock_send_error(client_fd, 400U, "Bad Request",
                                  "invalid_or_oversized_request");
        } else {
            (void)request_length;
            if (!mock_serve_request(client_fd, &request)) {
                (void)fprintf(stderr, "Could not serve HTTP request\n");
            }
        }
        (void)close(client_fd);
    }
}
