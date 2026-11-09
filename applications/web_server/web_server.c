#include "web_server.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "file_system.h"
#include "file_system_port.h"
#include "lwip.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "logger.h"
#include "queue.h"
#include "rf_app.h"
#include "task.h"
#include "settings.h"
#include "web_server_assets.h"
#include "zigbee.h"

#define WEB_SERVER_PORT (80U)
#define WEB_SERVER_HEADER_MAX (1024U)
#define WEB_SERVER_BODY_MAX (512U)
#define WEB_SERVER_REQUEST_MAX (WEB_SERVER_HEADER_MAX + WEB_SERVER_BODY_MAX)
#define WEB_SERVER_TX_CHUNK_MAX (1024U)
#define WEB_SERVER_RESPONSE_BODY_MAX (3072U)
#define WEB_SERVER_TX_HEADER_MAX (256U)
#define WEB_SERVER_QUEUE_LENGTH (1U)
#define WEB_SERVER_POLL_INTERVAL (2U)
#define WEB_SERVER_REQUEST_TIMEOUT_POLLS (15U)
#define WEB_SERVER_CONFIG_FIELD_COUNT (10U)
#define WEB_SERVER_CONFIG_FIELDS_MASK (0x03FFU)
#define WEB_SERVER_SETTINGS_MQTT_FIELD_COUNT (6U)
#define WEB_SERVER_SETTINGS_NETWORK_FIELD_COUNT (4U)
#define WEB_SERVER_SETTINGS_UART_FIELD_COUNT (4U)
#define WEB_SERVER_SETTINGS_MQTT_FIELDS_MASK (0x003FU)
#define WEB_SERVER_SETTINGS_NETWORK_FIELDS_MASK (0x000FU)
#define WEB_SERVER_SETTINGS_UART_FIELDS_MASK (0x000FU)
#define WEB_SERVER_MONITOR_TASK_MAX (16U)
#define WEB_SERVER_MONITOR_INTERVAL_TICKS (1000U)
#define WEB_SERVER_FILESYSTEM_QUEUE_LENGTH (1U)
#define WEB_SERVER_FILESYSTEM_PATH_MAX (96U)
#define WEB_SERVER_FILESYSTEM_PAGE_SIZE (8U)
#define WEB_SERVER_FILESYSTEM_PATH_FIELD_MASK (0x01U)
#define WEB_SERVER_FILESYSTEM_REQUEST_ID_FIELD_MASK (0x02U)
#define WEB_SERVER_FILESYSTEM_OFFSET_FIELD_MASK (0x04U)
#define WEB_SERVER_FILESYSTEM_FIELDS_MASK (0x07U)
#define WEB_SERVER_FILESYSTEM_DOWNLOAD_CHUNK_SIZE (1024U)

typedef struct
{
    char name[configMAX_TASK_NAME_LEN];
    char state[10];
    uint32_t priority;
    uint32_t stack_high_water_bytes;
    uint16_t cpu_tenths_percent;
} web_server_task_metric_t;

typedef enum
{
    WEB_SERVER_OPERATION_IDLE = 0,
    WEB_SERVER_OPERATION_PENDING,
    WEB_SERVER_OPERATION_SUCCESS,
    WEB_SERVER_OPERATION_FAILURE
} web_server_operation_state_t;

typedef enum
{
    WEB_SERVER_OPERATION_NONE = 0,
    WEB_SERVER_OPERATION_RADIO_READ,
    WEB_SERVER_OPERATION_RADIO_UPDATE
} web_server_operation_kind_t;

typedef struct
{
    uint16_t pan_id;
    uint16_t user_address;
    uint8_t channel;
    uint8_t point_type;
    uint8_t transfer_model;
    uint8_t baud_rate;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t antenna_select;
} web_server_public_config_t;

typedef struct
{
    web_server_operation_kind_t kind;
    ZigbeeParameter_t parameters;
} web_server_command_t;

typedef struct
{
    uint32_t request_id;
    uint32_t offset;
    bool download;
    char path[WEB_SERVER_FILESYSTEM_PATH_MAX + 1U];
} web_server_filesystem_command_t;

typedef struct
{
    bool radio_initialized;
    bool radio_busy;
    bool radio_config_available;
    bool storage_mounted;
    ZigbeeParameter_t radio_parameters;
    web_server_operation_state_t operation_state;
    web_server_operation_kind_t operation_kind;
    uint8_t operation_error;
    size_t heap_total_bytes;
    size_t heap_free_bytes;
    size_t heap_minimum_free_bytes;
    uint16_t cpu_tenths_percent;
    bool cpu_available;
    uint32_t task_count;
    uint8_t task_reported;
    bool tasks_truncated;
    web_server_task_metric_t tasks[WEB_SERVER_MONITOR_TASK_MAX];
} web_server_snapshot_t;

typedef struct
{
    bool pending;
    bool ready;
    bool success;
    bool download;
    uint16_t http_status;
    uint32_t request_id;
    uint32_t offset;
    uint32_t download_size;
    uint16_t download_length;
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t file_count;
    uint32_t directory_count;
    uint32_t total_entries;
    uint8_t entry_count;
    char path[WEB_SERVER_FILESYSTEM_PATH_MAX + 1U];
    file_system_dirent_t entries[WEB_SERVER_FILESYSTEM_PAGE_SIZE];
} web_server_filesystem_snapshot_t;

typedef struct
{
    struct tcp_pcb *pcb;
    bool active;
    bool headers_parsed;
    bool response_started;
    bool close_pending;
    uint8_t idle_poll_count;
    char request[WEB_SERVER_REQUEST_MAX + 1U];
    size_t request_length;
    size_t body_offset;
    size_t content_length;
    size_t expected_length;
    char method[8];
    char path[128];
    bool has_content_length;
    bool has_json_content_type;
    char tx_header[WEB_SERVER_TX_HEADER_MAX];
    size_t tx_header_length;
    size_t tx_header_offset;
    const uint8_t *tx_body;
    size_t tx_body_length;
    size_t tx_body_offset;
    char response_body[WEB_SERVER_RESPONSE_BODY_MAX];
} web_server_connection_t;

typedef struct
{
    bool has_content_length;
    bool has_content_type;
    bool has_json_content_type;
} web_server_header_parser_t;

static const char WEB_SERVER_TAG[] = "WEB";
static const char *const web_server_config_field_names[
    WEB_SERVER_CONFIG_FIELD_COUNT] = {
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
static const char *const web_server_settings_mqtt_field_names[
    WEB_SERVER_SETTINGS_MQTT_FIELD_COUNT] = {
        "enabled", "tlsEnabled", "host", "port", "clientId",
        "keepAliveSeconds",
};
static const char *const web_server_settings_network_field_names[
    WEB_SERVER_SETTINGS_NETWORK_FIELD_COUNT] = {
        "dhcpEnabled", "address", "netmask", "gateway",
};
static const char *const web_server_settings_uart_field_names[
    WEB_SERVER_SETTINGS_UART_FIELD_COUNT] = {
        "baudRate", "dataBits", "parity", "stopBits",
};

static StaticQueue_t web_server_queue_storage;
_Alignas(max_align_t) static uint8_t web_server_queue_buffer[
    WEB_SERVER_QUEUE_LENGTH * sizeof(web_server_command_t)];
static StaticQueue_t web_server_filesystem_queue_storage;
_Alignas(max_align_t) static uint8_t web_server_filesystem_queue_buffer[
    WEB_SERVER_FILESYSTEM_QUEUE_LENGTH *
    sizeof(web_server_filesystem_command_t)];
static QueueHandle_t web_server_queue;
static QueueHandle_t web_server_filesystem_queue;
static struct tcp_pcb *web_server_listener;
static web_server_connection_t web_server_connection;
static web_server_snapshot_t web_server_snapshot;
static web_server_filesystem_snapshot_t web_server_filesystem_snapshot;
static web_server_filesystem_snapshot_t
    web_server_filesystem_response_snapshot;
static web_server_filesystem_snapshot_t web_server_filesystem_work;
static uint8_t web_server_filesystem_download_buffer[
    WEB_SERVER_FILESYSTEM_DOWNLOAD_CHUNK_SIZE];
static bool web_server_startup_queued;
static TaskStatus_t web_server_task_status[WEB_SERVER_MONITOR_TASK_MAX];
static TaskStatus_t web_server_previous_task_status[WEB_SERVER_MONITOR_TASK_MAX];
static UBaseType_t web_server_previous_task_count;
static size_t web_server_previous_total_runtime;
static size_t web_server_previous_idle_runtime;
static TickType_t web_server_previous_sample_tick;
static bool web_server_runtime_baseline_available;

static void web_server_operation_complete(ZigbeeMesh_t *mesh,
                                         uint8_t result,
                                         void *arg);
static void web_server_prepare_response(web_server_connection_t *connection,
                                        uint16_t status_code,
                                        const char *reason,
                                        const char *content_type,
                                        const uint8_t *body,
                                        size_t body_length,
                                        bool gzip_encoded);
static void web_server_prepare_json_response(
    web_server_connection_t *connection,
    uint16_t status_code,
    const char *reason,
    const char *json);
static void web_server_prepare_error(web_server_connection_t *connection,
                                     uint16_t status_code,
                                     const char *reason,
                                     const char *error);
static void web_server_monitor_refresh(void);
static void web_server_filesystem_refresh(
    const web_server_filesystem_command_t *command);
static err_t web_server_accept(void *arg,
                               struct tcp_pcb *new_pcb,
                               err_t error);
static err_t web_server_receive(void *arg,
                                struct tcp_pcb *pcb,
                                struct pbuf *packet,
                                err_t error);
static err_t web_server_sent(void *arg,
                             struct tcp_pcb *pcb,
                             u16_t length);
static err_t web_server_poll(void *arg, struct tcp_pcb *pcb);
static void web_server_error(void *arg, err_t error);

static void web_server_snapshot_copy(web_server_snapshot_t *snapshot)
{
    taskENTER_CRITICAL();
    *snapshot = web_server_snapshot;
    taskEXIT_CRITICAL();
}

static void web_server_operation_set(web_server_operation_state_t state,
                                     web_server_operation_kind_t kind,
                                     uint8_t error)
{
    taskENTER_CRITICAL();
    web_server_snapshot.operation_state = state;
    web_server_snapshot.operation_kind = kind;
    web_server_snapshot.operation_error = error;
    taskEXIT_CRITICAL();
}

static const char *web_server_operation_kind_name(
    web_server_operation_kind_t kind)
{
    switch (kind) {
    case WEB_SERVER_OPERATION_RADIO_READ:
        return "radio-read";
    case WEB_SERVER_OPERATION_RADIO_UPDATE:
        return "radio-update";
    case WEB_SERVER_OPERATION_NONE:
    default:
        return "none";
    }
}

static const char *web_server_operation_state_name(
    web_server_operation_state_t state)
{
    switch (state) {
    case WEB_SERVER_OPERATION_PENDING:
        return "pending";
    case WEB_SERVER_OPERATION_SUCCESS:
        return "success";
    case WEB_SERVER_OPERATION_FAILURE:
        return "failure";
    case WEB_SERVER_OPERATION_IDLE:
    default:
        return "idle";
    }
}

static const char *web_server_task_state_name(eTaskState state)
{
    switch (state) {
    case eRunning:
        return "running";
    case eReady:
        return "ready";
    case eBlocked:
        return "blocked";
    case eSuspended:
        return "suspended";
    case eDeleted:
        return "deleted";
    case eInvalid:
    default:
        return "unknown";
    }
}

static void web_server_monitor_refresh(void)
{
    configRUN_TIME_COUNTER_TYPE total_runtime = 0U;
    const TickType_t sample_tick = xTaskGetTickCount();
    UBaseType_t task_count;
    UBaseType_t reported;
    TickType_t elapsed_ticks;
    size_t total_runtime_delta = 0U;
    size_t idle_runtime_delta = 0U;
    const size_t idle_runtime = (size_t)ulTaskGetIdleRunTimeCounter();
    bool cpu_available = false;
    uint16_t cpu_tenths_percent = 0U;
    web_server_task_metric_t task_metrics[WEB_SERVER_MONITOR_TASK_MAX] = {0};

    if (web_server_runtime_baseline_available &&
        (TickType_t)(sample_tick - web_server_previous_sample_tick) <
            WEB_SERVER_MONITOR_INTERVAL_TICKS) {
        return;
    }

    task_count = uxTaskGetNumberOfTasks();
    total_runtime = (configRUN_TIME_COUNTER_TYPE)getRunTimeCounterValue();
    reported = 0U;
    if (task_count <= WEB_SERVER_MONITOR_TASK_MAX) {
        reported = uxTaskGetSystemState(
            web_server_task_status, WEB_SERVER_MONITOR_TASK_MAX,
            &total_runtime);
    }
    task_count = uxTaskGetNumberOfTasks();
    elapsed_ticks = sample_tick - web_server_previous_sample_tick;

    if (web_server_runtime_baseline_available && elapsed_ticks > 0U) {
        total_runtime_delta =
            (size_t)total_runtime - web_server_previous_total_runtime;
        idle_runtime_delta = idle_runtime - web_server_previous_idle_runtime;
        if (total_runtime_delta > 0U) {
            if (idle_runtime_delta <= total_runtime_delta) {
                cpu_tenths_percent = (uint16_t)(
                    1000U -
                    (uint16_t)(((uint64_t)idle_runtime_delta * 1000U) /
                               total_runtime_delta));
                cpu_available = true;
            }
        }
    }

    for (UBaseType_t index = 0U; index < reported; ++index) {
        const TaskStatus_t *const task = &web_server_task_status[index];
        size_t task_runtime_delta = 0U;

        for (UBaseType_t previous_index = 0U;
             previous_index < web_server_previous_task_count;
             ++previous_index) {
            if (web_server_previous_task_status[previous_index].xTaskNumber ==
                task->xTaskNumber) {
                task_runtime_delta =
                    (size_t)task->ulRunTimeCounter -
                    (size_t)web_server_previous_task_status[previous_index]
                        .ulRunTimeCounter;
                break;
            }
        }

        for (size_t name_index = 0U;
             name_index < sizeof(task_metrics[index].name) - 1U;
             ++name_index) {
            const char character = task->pcTaskName[name_index];

            if (character == '\0') {
                break;
            }
            task_metrics[index].name[name_index] =
                ((character >= 'a' && character <= 'z') ||
                 (character >= 'A' && character <= 'Z') ||
                 (character >= '0' && character <= '9') ||
                 character == '_' || character == '-' || character == '.')
                    ? character
                    : '_';
        }
        (void)snprintf(task_metrics[index].state,
                       sizeof(task_metrics[index].state), "%s",
                       web_server_task_state_name(task->eCurrentState));
        task_metrics[index].priority = (uint32_t)task->uxCurrentPriority;
        task_metrics[index].stack_high_water_bytes =
            (uint32_t)task->usStackHighWaterMark *
            (uint32_t)sizeof(StackType_t);
        if (cpu_available && total_runtime_delta > 0U) {
            task_metrics[index].cpu_tenths_percent = (uint16_t)(
                ((uint64_t)task_runtime_delta * 1000U) /
                total_runtime_delta);
        }
    }

    memcpy(web_server_previous_task_status, web_server_task_status,
           (size_t)reported * sizeof(web_server_task_status[0]));
    web_server_previous_task_count = reported;
    web_server_previous_total_runtime = (size_t)total_runtime;
    web_server_previous_idle_runtime = idle_runtime;
    web_server_previous_sample_tick = sample_tick;
    web_server_runtime_baseline_available = true;

    const size_t heap_free_bytes = xPortGetFreeHeapSize();
    const size_t heap_minimum_free_bytes = xPortGetMinimumEverFreeHeapSize();

    taskENTER_CRITICAL();
    web_server_snapshot.heap_total_bytes = configTOTAL_HEAP_SIZE;
    web_server_snapshot.heap_free_bytes = heap_free_bytes;
    web_server_snapshot.heap_minimum_free_bytes = heap_minimum_free_bytes;
    web_server_snapshot.cpu_tenths_percent = cpu_tenths_percent;
    web_server_snapshot.cpu_available = cpu_available;
    web_server_snapshot.task_count = (uint32_t)task_count;
    web_server_snapshot.task_reported = (uint8_t)reported;
    web_server_snapshot.tasks_truncated =
        task_count > reported || (reported == 0U && task_count > 0U);
    memcpy(web_server_snapshot.tasks, task_metrics, sizeof(task_metrics));
    taskEXIT_CRITICAL();
}

static bool web_server_ascii_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) !=
            tolower((unsigned char)*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static bool web_server_set_token(char *destination,
                                 size_t destination_size,
                                 const char *source,
                                 size_t source_length)
{
    if (destination == NULL || source == NULL ||
        source_length == 0U || source_length >= destination_size) {
        return false;
    }
    memcpy(destination, source, source_length);
    destination[source_length] = '\0';
    return true;
}

static bool web_server_parse_unsigned(const char *text,
                                      size_t length,
                                      uint32_t *value)
{
    uint32_t parsed = 0U;

    if (text == NULL || value == NULL || length == 0U) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        const char digit = text[index];

        if (digit < '0' || digit > '9') {
            return false;
        }
        if (parsed > (UINT32_MAX - (uint32_t)(digit - '0')) / 10U) {
            return false;
        }
        parsed = (parsed * 10U) + (uint32_t)(digit - '0');
    }
    *value = parsed;
    return true;
}

static int32_t web_server_hex_digit(char character)
{
    if (character >= '0' && character <= '9') {
        return (int32_t)(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return (int32_t)(character - 'a') + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return (int32_t)(character - 'A') + 10;
    }
    return -1;
}

static bool web_server_filesystem_path_valid(const char *path)
{
    const char *component_start;

    if (path == NULL || path[0] != '/') {
        return false;
    }
    if (path[1] == '\0') {
        return true;
    }

    component_start = &path[1];
    for (const char *cursor = component_start; ; ++cursor) {
        if (*cursor == '\\' ||
            (*cursor != '\0' &&
             ((unsigned char)*cursor < 0x20U ||
              (unsigned char)*cursor == 0x7FU))) {
            return false;
        }
        if (*cursor == '/' || *cursor == '\0') {
            const size_t component_length =
                (size_t)(cursor - component_start);

            if (component_length == 0U ||
                (component_length == 1U && component_start[0] == '.') ||
                (component_length == 2U && component_start[0] == '.' &&
                 component_start[1] == '.')) {
                return false;
            }
            if (*cursor == '\0') {
                break;
            }
            component_start = cursor + 1;
        }
    }
    return true;
}

static bool web_server_decode_filesystem_path(const char *encoded_path,
                                              size_t encoded_length,
                                              char *path,
                                              size_t path_capacity)
{
    size_t input_index = 0U;
    size_t output_index = 0U;

    if (encoded_path == NULL || path == NULL || path_capacity < 2U) {
        return false;
    }
    while (input_index < encoded_length) {
        char character = encoded_path[input_index];

        if (character == '%') {
            int32_t high;
            int32_t low;

            if (encoded_length - input_index < 3U) {
                return false;
            }
            high = web_server_hex_digit(encoded_path[input_index + 1U]);
            low = web_server_hex_digit(encoded_path[input_index + 2U]);
            if (high < 0 || low < 0) {
                return false;
            }
            character = (char)((high << 4) | low);
            input_index += 3U;
        } else {
            if (character == '+') {
                character = ' ';
            }
            input_index++;
        }
        if (character == '\0' || output_index + 1U >= path_capacity) {
            return false;
        }
        path[output_index] = character;
        output_index++;
    }
    path[output_index] = '\0';
    return web_server_filesystem_path_valid(path);
}

static bool web_server_filesystem_query_field(
    const char *name,
    size_t name_length,
    const char *expected_name)
{
    return name != NULL && expected_name != NULL &&
           strlen(expected_name) == name_length &&
           memcmp(name, expected_name, name_length) == 0;
}

static bool web_server_parse_filesystem_request(
    const char *target,
    web_server_filesystem_command_t *command)
{
    static const char endpoint[] = "/api/v1/files?";
    static const char download_endpoint[] = "/api/v1/files/download?";
    const char *selected_endpoint;
    const char *cursor;
    uint8_t field_mask = 0U;

    if (target == NULL || command == NULL) {
        return false;
    }

    memset(command, 0, sizeof(*command));
    if (strncmp(target, download_endpoint,
                sizeof(download_endpoint) - 1U) == 0) {
        selected_endpoint = download_endpoint;
        command->download = true;
    } else if (strncmp(target, endpoint, sizeof(endpoint) - 1U) == 0) {
        selected_endpoint = endpoint;
    } else {
        return false;
    }
    cursor = target + strlen(selected_endpoint);
    while (*cursor != '\0') {
        const char *field_start = cursor;
        const char *value_start;
        const char *field_end = strchr(cursor, '&');
        const char *equals;
        size_t field_length;
        size_t value_length;
        uint8_t field_bit;

        if (field_end == NULL) {
            field_end = cursor + strlen(cursor);
        }
        equals = memchr(field_start, '=', (size_t)(field_end - field_start));
        if (equals == NULL) {
            return false;
        }
        value_start = equals + 1;
        field_length = (size_t)(equals - field_start);
        value_length = (size_t)(field_end - value_start);

        if (web_server_filesystem_query_field(field_start, field_length,
                                              "path")) {
            field_bit = WEB_SERVER_FILESYSTEM_PATH_FIELD_MASK;
            if (!web_server_decode_filesystem_path(
                    value_start, value_length, command->path,
                    sizeof(command->path))) {
                return false;
            }
        } else if (web_server_filesystem_query_field(
                       field_start, field_length, "requestId")) {
            field_bit = WEB_SERVER_FILESYSTEM_REQUEST_ID_FIELD_MASK;
            if (!web_server_parse_unsigned(value_start, value_length,
                                           &command->request_id) ||
                command->request_id == 0U) {
                return false;
            }
        } else if (web_server_filesystem_query_field(
                       field_start, field_length, "offset")) {
            field_bit = WEB_SERVER_FILESYSTEM_OFFSET_FIELD_MASK;
            if (!web_server_parse_unsigned(value_start, value_length,
                                           &command->offset)) {
                return false;
            }
        } else {
            return false;
        }
        if ((field_mask & field_bit) != 0U) {
            return false;
        }
        field_mask |= field_bit;
        cursor = *field_end == '&' ? field_end + 1 : field_end;
    }
    return field_mask == WEB_SERVER_FILESYSTEM_FIELDS_MASK;
}

static bool web_server_parse_header_line(char *line,
                                         size_t *content_length,
                                         web_server_header_parser_t *parser)
{
    char *colon;
    char *value;
    char *value_end;

    if (line == NULL || content_length == NULL || parser == NULL) {
        return false;
    }

    colon = strchr(line, ':');
    if (colon == NULL || colon == line) {
        return false;
    }
    *colon = '\0';
    value = colon + 1;
    while (*value == ' ' || *value == '\t') {
        ++value;
    }
    value_end = value + strlen(value);
    while (value_end > value &&
           (value_end[-1] == ' ' || value_end[-1] == '\t')) {
        --value_end;
    }
    *value_end = '\0';

    if (web_server_ascii_equal(line, "Content-Length")) {
        uint32_t parsed_length;

        if (parser->has_content_length ||
            !web_server_parse_unsigned(value, strlen(value),
                                       &parsed_length) ||
            parsed_length > WEB_SERVER_BODY_MAX) {
            return false;
        }
        *content_length = (size_t)parsed_length;
        parser->has_content_length = true;
    } else if (web_server_ascii_equal(line, "Content-Type")) {
        if (parser->has_content_type) {
            return false;
        }
        parser->has_content_type = true;
        parser->has_json_content_type =
            web_server_ascii_equal(value, "application/json");
    } else if (web_server_ascii_equal(line, "Transfer-Encoding")) {
        return false;
    }
    return true;
}

static bool web_server_parse_headers(web_server_connection_t *connection,
                                     size_t header_length)
{
    web_server_header_parser_t parser;
    char *line_end;
    char *request_line;
    char *method_end;
    char *path_end;
    char *line;
    char *cursor;

    if (connection == NULL || header_length > WEB_SERVER_HEADER_MAX ||
        header_length + 4U > connection->request_length) {
        return false;
    }

    for (size_t index = 0U; index < header_length; ++index) {
        if (connection->request[index] == '\0') {
            return false;
        }
    }
    connection->request[header_length + 2U] = '\0';

    request_line = connection->request;
    line_end = strstr(request_line, "\r\n");
    if (line_end == NULL) {
        return false;
    }
    *line_end = '\0';
    method_end = strchr(request_line, ' ');
    if (method_end == NULL) {
        return false;
    }
    *method_end = '\0';
    path_end = strchr(method_end + 1, ' ');
    if (path_end == NULL ||
        !web_server_set_token(connection->method,
                              sizeof(connection->method),
                              request_line, strlen(request_line)) ||
        !web_server_set_token(connection->path,
                              sizeof(connection->path),
                              method_end + 1,
                              (size_t)(path_end - (method_end + 1))) ||
        (strcmp(path_end + 1, "HTTP/1.1") != 0 &&
         strcmp(path_end + 1, "HTTP/1.0") != 0)) {
        return false;
    }

    parser.has_content_length = false;
    parser.has_content_type = false;
    parser.has_json_content_type = false;
    cursor = line_end + 2;
    while (*cursor != '\0') {
        line = cursor;
        line_end = strstr(line, "\r\n");
        if (line_end == NULL) {
            return false;
        }
        *line_end = '\0';
        if (*line == '\0') {
            break;
        }
        if (!web_server_parse_header_line(line,
                                          &connection->content_length,
                                          &parser)) {
            return false;
        }
        cursor = line_end + 2;
    }

    connection->has_content_length = parser.has_content_length;
    connection->has_json_content_type = parser.has_json_content_type;
    connection->body_offset = header_length + 4U;
    connection->expected_length =
        connection->body_offset + connection->content_length;
    connection->headers_parsed = true;
    return connection->expected_length <= WEB_SERVER_REQUEST_MAX;
}

static bool web_server_find_header_end(const char *buffer,
                                       size_t length,
                                       size_t *header_length)
{
    if (buffer == NULL || header_length == NULL || length < 4U) {
        return false;
    }
    for (size_t index = 0U; index <= length - 4U; ++index) {
        if (buffer[index] == '\r' && buffer[index + 1U] == '\n' &&
            buffer[index + 2U] == '\r' && buffer[index + 3U] == '\n') {
            *header_length = index;
            return true;
        }
    }
    return false;
}

static void web_server_config_from_parameters(
    const ZigbeeParameter_t *parameters,
    web_server_public_config_t *config)
{
    config->pan_id = parameters->PAN_ID;
    config->user_address = parameters->userAddress;
    config->channel = parameters->Channel;
    config->point_type = parameters->pointType;
    config->transfer_model = parameters->transferModel;
    config->baud_rate = parameters->uartBraudRate;
    config->data_bits = parameters->uartDataBits;
    config->stop_bits = parameters->uartStopBits;
    config->parity = parameters->uartParity;
    config->antenna_select = parameters->antennaSelect;
}

static void web_server_config_to_parameters(
    const web_server_public_config_t *config,
    ZigbeeParameter_t *parameters)
{
    parameters->PAN_ID = config->pan_id;
    parameters->userAddress = config->user_address;
    parameters->Channel = config->channel;
    parameters->pointType = config->point_type;
    parameters->transferModel = config->transfer_model;
    parameters->uartBraudRate = config->baud_rate;
    parameters->uartDataBits = config->data_bits;
    parameters->uartStopBits = config->stop_bits;
    parameters->uartParity = config->parity;
    parameters->antennaSelect = config->antenna_select;
}

static bool web_server_assign_config_field(
    const char *name,
    size_t name_length,
    uint32_t value,
    web_server_public_config_t *config,
    uint16_t *field_mask)
{
    if (config == NULL || field_mask == NULL) {
        return false;
    }

    for (size_t index = 0U; index < WEB_SERVER_CONFIG_FIELD_COUNT; ++index) {
        const size_t field_name_length =
            strlen(web_server_config_field_names[index]);
        const uint16_t bit = (uint16_t)(1U << index);

        if (name_length == field_name_length &&
            memcmp(name, web_server_config_field_names[index],
                   name_length) == 0) {
            if ((*field_mask & bit) != 0U) {
                return false;
            }
            *field_mask |= bit;
            switch (index) {
            case 0U:
                if (value < ZIGBEE_PAN_ID_MIN ||
                    value > ZIGBEE_PAN_ID_MAX) {
                    return false;
                }
                config->pan_id = (uint16_t)value;
                break;
            case 1U:
                if (value < ZIGBEE_CHANNEL_MIN ||
                    value > ZIGBEE_CHANNEL_MAX) {
                    return false;
                }
                config->channel = (uint8_t)value;
                break;
            case 2U:
                if (value < ZIGBEE_PAN_ID_MIN ||
                    value > ZIGBEE_PAN_ID_MAX) {
                    return false;
                }
                config->user_address = (uint16_t)value;
                break;
            case 3U:
                if (value < ZIGBEE_POINT_TYPE_MIN ||
                    value > ZIGBEE_POINT_TYPE_MAX) {
                    return false;
                }
                config->point_type = (uint8_t)value;
                break;
            case 4U:
                if (value < ZIGBEE_TRANSFER_MODE_MIN ||
                    value > ZIGBEE_TRANSFER_MODE_MAX) {
                    return false;
                }
                config->transfer_model = (uint8_t)value;
                break;
            case 5U:
                if (value < ZIGBEE_BAUD_RATE_MIN ||
                    value > ZIGBEE_BAUD_RATE_MAX) {
                    return false;
                }
                config->baud_rate = (uint8_t)value;
                break;
            case 6U:
                if (value != ZIGBEE_DATA_BITS_VALUE) {
                    return false;
                }
                config->data_bits = (uint8_t)value;
                break;
            case 7U:
                if (value != ZIGBEE_STOP_BITS_VALUE) {
                    return false;
                }
                config->stop_bits = (uint8_t)value;
                break;
            case 8U:
                if (value < ZIGBEE_PARITY_MIN ||
                    value > ZIGBEE_PARITY_MAX) {
                    return false;
                }
                config->parity = (uint8_t)value;
                break;
            case 9U:
                if (value > ZIGBEE_ANTENNA_MAX) {
                    return false;
                }
                config->antenna_select = (uint8_t)value;
                break;
            default:
                return false;
            }
            return true;
        }
    }
    return false;
}

static void web_server_skip_space(const char **cursor, const char *end)
{
    while (*cursor < end &&
           (**cursor == ' ' || **cursor == '\t' ||
            **cursor == '\r' || **cursor == '\n')) {
        ++(*cursor);
    }
}

static bool web_server_parse_config(const char *body,
                                    size_t body_length,
                                    web_server_public_config_t *config)
{
    const char *cursor;
    const char *end;
    uint16_t field_mask = 0U;

    if (body == NULL || config == NULL) {
        return false;
    }
    cursor = body;
    end = body + body_length;
    web_server_skip_space(&cursor, end);
    if (cursor >= end || *cursor++ != '{') {
        return false;
    }

    web_server_skip_space(&cursor, end);
    while (cursor < end && *cursor != '}') {
        const char *name;
        size_t name_length;
        uint32_t value;

        if (*cursor++ != '"') {
            return false;
        }
        name = cursor;
        while (cursor < end && *cursor != '"' && *cursor != '\\') {
            ++cursor;
        }
        if (cursor >= end || *cursor != '"') {
            return false;
        }
        name_length = (size_t)(cursor - name);
        ++cursor;
        web_server_skip_space(&cursor, end);
        if (cursor >= end || *cursor++ != ':') {
            return false;
        }
        web_server_skip_space(&cursor, end);
        {
            const char *number = cursor;
            while (cursor < end && *cursor >= '0' && *cursor <= '9') {
                ++cursor;
            }
            if (!web_server_parse_unsigned(number,
                                           (size_t)(cursor - number),
                                           &value) ||
                !web_server_assign_config_field(name, name_length, value,
                                                config, &field_mask)) {
                return false;
            }
        }
        web_server_skip_space(&cursor, end);
        if (cursor < end && *cursor == ',') {
            ++cursor;
            web_server_skip_space(&cursor, end);
            continue;
        }
        break;
    }
    if (cursor >= end || *cursor++ != '}' ||
        field_mask != WEB_SERVER_CONFIG_FIELDS_MASK) {
        return false;
    }
    web_server_skip_space(&cursor, end);
    return cursor == end;
}

static bool web_server_parse_empty_object(const char *body,
                                          size_t body_length)
{
    const char *cursor;
    const char *end;

    if (body == NULL) {
        return false;
    }
    cursor = body;
    end = body + body_length;
    web_server_skip_space(&cursor, end);
    if (cursor >= end || *cursor++ != '{') {
        return false;
    }
    web_server_skip_space(&cursor, end);
    if (cursor >= end || *cursor++ != '}') {
        return false;
    }
    web_server_skip_space(&cursor, end);
    return cursor == end;
}

static bool web_server_json_start(const char **cursor, const char *end)
{
    if (cursor == NULL || *cursor == NULL) {
        return false;
    }
    web_server_skip_space(cursor, end);
    if (*cursor >= end || *(*cursor)++ != '{') {
        return false;
    }
    return true;
}

static bool web_server_json_next_field(const char **cursor,
                                       const char *end,
                                       bool *first_field,
                                       char *name,
                                       size_t name_capacity)
{
    size_t name_length = 0U;

    if (cursor == NULL || *cursor == NULL || first_field == NULL ||
        name == NULL || name_capacity < 2U) {
        return false;
    }
    web_server_skip_space(cursor, end);
    if (*cursor >= end || **cursor == '}') {
        return false;
    }
    if (!*first_field) {
        if (**cursor != ',') {
            return false;
        }
        ++(*cursor);
        web_server_skip_space(cursor, end);
        if (*cursor >= end || **cursor == '}') {
            return false;
        }
    }
    if (**cursor != '"') {
        return false;
    }
    ++(*cursor);
    while (*cursor < end && **cursor != '"' && **cursor != '\\' &&
           (unsigned char)**cursor >= 0x20U) {
        if (name_length + 1U >= name_capacity) {
            return false;
        }
        name[name_length++] = **cursor;
        ++(*cursor);
    }
    if (*cursor >= end || **cursor != '"' || name_length == 0U) {
        return false;
    }
    name[name_length] = '\0';
    ++(*cursor);
    web_server_skip_space(cursor, end);
    if (*cursor >= end || *(*cursor)++ != ':') {
        return false;
    }
    *first_field = false;
    return true;
}

static bool web_server_json_finish(const char **cursor, const char *end)
{
    if (cursor == NULL || *cursor == NULL) {
        return false;
    }
    web_server_skip_space(cursor, end);
    if (*cursor >= end || *(*cursor)++ != '}') {
        return false;
    }
    web_server_skip_space(cursor, end);
    return *cursor == end;
}

static bool web_server_json_read_string(const char **cursor,
                                        const char *end,
                                        char *value,
                                        size_t value_capacity)
{
    size_t value_length = 0U;

    if (cursor == NULL || *cursor == NULL || value == NULL ||
        value_capacity < 2U) {
        return false;
    }
    web_server_skip_space(cursor, end);
    if (*cursor >= end || *(*cursor)++ != '"') {
        return false;
    }
    while (*cursor < end && **cursor != '"' && **cursor != '\\' &&
           (unsigned char)**cursor >= 0x20U) {
        if (value_length + 1U >= value_capacity) {
            return false;
        }
        value[value_length++] = **cursor;
        ++(*cursor);
    }
    if (*cursor >= end || **cursor != '"') {
        return false;
    }
    value[value_length] = '\0';
    ++(*cursor);
    return value_length > 0U;
}

static bool web_server_json_read_bool(const char **cursor,
                                      const char *end,
                                      bool *value)
{
    if (cursor == NULL || *cursor == NULL || value == NULL) {
        return false;
    }
    web_server_skip_space(cursor, end);
    if ((size_t)(end - *cursor) >= 4U &&
        memcmp(*cursor, "true", 4U) == 0) {
        *value = true;
        *cursor += 4;
        return true;
    }
    if ((size_t)(end - *cursor) >= 5U &&
        memcmp(*cursor, "false", 5U) == 0) {
        *value = false;
        *cursor += 5;
        return true;
    }
    return false;
}

static bool web_server_json_read_number(const char **cursor,
                                        const char *end,
                                        uint32_t *value)
{
    const char *number;

    if (cursor == NULL || *cursor == NULL || value == NULL) {
        return false;
    }
    web_server_skip_space(cursor, end);
    number = *cursor;
    while (*cursor < end && **cursor >= '0' && **cursor <= '9') {
        ++(*cursor);
    }
    return web_server_parse_unsigned(number,
                                     (size_t)(*cursor - number), value);
}

static bool web_server_parse_ipv4(const char *text, uint8_t address[4])
{
    const char *cursor = text;
    size_t octet_index;

    if (text == NULL || address == NULL) {
        return false;
    }
    for (octet_index = 0U; octet_index < 4U; octet_index++) {
        uint32_t value = 0U;
        size_t digits = 0U;

        while (*cursor >= '0' && *cursor <= '9') {
            value = (value * 10U) + (uint32_t)(*cursor - '0');
            digits++;
            if (value > 255U || digits > 3U) {
                return false;
            }
            ++cursor;
        }
        if (digits == 0U) {
            return false;
        }
        address[octet_index] = (uint8_t)value;
        if (octet_index < 3U) {
            if (*cursor != '.') {
                return false;
            }
            ++cursor;
        } else if (*cursor != '\0') {
            return false;
        }
    }
    return true;
}

static int32_t web_server_settings_field_index(
    const char *name,
    const char *const *field_names,
    size_t field_count)
{
    size_t index;

    for (index = 0U; index < field_count; index++) {
        if (strcmp(name, field_names[index]) == 0) {
            return (int32_t)index;
        }
    }
    return -1;
}

static bool web_server_parse_settings_mqtt(
    const char *body,
    size_t body_length,
    settings_mqtt_config_t *mqtt)
{
    const char *cursor = body;
    const char *const end = body + body_length;
    uint16_t field_mask = 0U;
    bool first_field = true;

    if (body == NULL || mqtt == NULL || !web_server_json_start(&cursor, end)) {
        return false;
    }
    web_server_skip_space(&cursor, end);
    while (cursor < end && *cursor != '}') {
        char name[24];
        uint32_t number;
        bool boolean;
        int32_t index;

        if (!web_server_json_next_field(
                &cursor, end, &first_field, name, sizeof(name))) {
            return false;
        }
        index = web_server_settings_field_index(
            name, web_server_settings_mqtt_field_names,
            WEB_SERVER_SETTINGS_MQTT_FIELD_COUNT);
        if (index < 0 || (field_mask & (uint16_t)(1U << index)) != 0U) {
            return false;
        }
        field_mask |= (uint16_t)(1U << index);
        switch (index) {
        case 0:
            if (!web_server_json_read_bool(&cursor, end, &boolean)) {
                return false;
            }
            mqtt->enabled = boolean;
            break;
        case 1:
            if (!web_server_json_read_bool(&cursor, end, &boolean)) {
                return false;
            }
            mqtt->tls_enabled = boolean;
            break;
        case 2:
            if (!web_server_json_read_string(
                    &cursor, end, mqtt->host, sizeof(mqtt->host))) {
                return false;
            }
            break;
        case 3:
            if (!web_server_json_read_number(&cursor, end, &number) ||
                number > UINT16_MAX) {
                return false;
            }
            mqtt->port = (uint16_t)number;
            break;
        case 4:
            if (!web_server_json_read_string(
                    &cursor, end, mqtt->client_id,
                    sizeof(mqtt->client_id))) {
                return false;
            }
            break;
        case 5:
            if (!web_server_json_read_number(&cursor, end, &number) ||
                number > UINT16_MAX) {
                return false;
            }
            mqtt->keep_alive_seconds = (uint16_t)number;
            break;
        default:
            return false;
        }
        web_server_skip_space(&cursor, end);
    }
    return field_mask == WEB_SERVER_SETTINGS_MQTT_FIELDS_MASK &&
           web_server_json_finish(&cursor, end);
}

static bool web_server_parse_settings_network(
    const char *body,
    size_t body_length,
    settings_ethernet_config_t *ethernet)
{
    const char *cursor = body;
    const char *const end = body + body_length;
    uint16_t field_mask = 0U;
    bool first_field = true;

    if (body == NULL || ethernet == NULL ||
        !web_server_json_start(&cursor, end)) {
        return false;
    }
    web_server_skip_space(&cursor, end);
    while (cursor < end && *cursor != '}') {
        char name[16];
        char address[16];
        bool boolean;
        uint8_t *destination;
        int32_t index;

        if (!web_server_json_next_field(
                &cursor, end, &first_field, name, sizeof(name))) {
            return false;
        }
        index = web_server_settings_field_index(
            name, web_server_settings_network_field_names,
            WEB_SERVER_SETTINGS_NETWORK_FIELD_COUNT);
        if (index < 0 || (field_mask & (uint16_t)(1U << index)) != 0U) {
            return false;
        }
        field_mask |= (uint16_t)(1U << index);
        if (index == 0) {
            if (!web_server_json_read_bool(&cursor, end, &boolean)) {
                return false;
            }
            ethernet->dhcp_enabled = boolean;
        } else {
            destination = index == 1
                              ? ethernet->address
                              : (index == 2 ? ethernet->netmask
                                            : ethernet->gateway);
            if (!web_server_json_read_string(
                    &cursor, end, address, sizeof(address)) ||
                !web_server_parse_ipv4(address, destination)) {
                return false;
            }
        }
        web_server_skip_space(&cursor, end);
    }
    return field_mask == WEB_SERVER_SETTINGS_NETWORK_FIELDS_MASK &&
           web_server_json_finish(&cursor, end);
}

static bool web_server_parse_settings_uart(
    const char *body,
    size_t body_length,
    settings_uart_config_t *uart)
{
    const char *cursor = body;
    const char *const end = body + body_length;
    uint16_t field_mask = 0U;
    bool first_field = true;

    if (body == NULL || uart == NULL || !web_server_json_start(&cursor, end)) {
        return false;
    }
    web_server_skip_space(&cursor, end);
    while (cursor < end && *cursor != '}') {
        char name[20];
        uint32_t number;
        int32_t index;

        if (!web_server_json_next_field(
                &cursor, end, &first_field, name, sizeof(name))) {
            return false;
        }
        index = web_server_settings_field_index(
            name, web_server_settings_uart_field_names,
            WEB_SERVER_SETTINGS_UART_FIELD_COUNT);
        if (index < 0 || (field_mask & (uint16_t)(1U << index)) != 0U ||
            !web_server_json_read_number(&cursor, end, &number) ||
            (number > UINT8_MAX && index != 0)) {
            return false;
        }
        field_mask |= (uint16_t)(1U << index);
        switch (index) {
        case 0:
            uart->baud_rate = number;
            break;
        case 1:
            uart->data_bits = (uint8_t)number;
            break;
        case 2:
            uart->parity = (uint8_t)number;
            break;
        case 3:
            uart->stop_bits = (uint8_t)number;
            break;
        default:
            return false;
        }
        web_server_skip_space(&cursor, end);
    }
    return field_mask == WEB_SERVER_SETTINGS_UART_FIELDS_MASK &&
           web_server_json_finish(&cursor, end);
}

static const char *web_server_settings_save_state_name(
    settings_save_state_t state)
{
    switch (state) {
    case SETTINGS_SAVE_PENDING:
        return "pending";
    case SETTINGS_SAVE_SUCCEEDED:
        return "saved";
    case SETTINGS_SAVE_FAILED:
        return "failed";
    case SETTINGS_SAVE_IDLE:
    default:
        return "idle";
    }
}

static void web_server_format_settings(web_server_connection_t *connection)
{
    settings_config_t config;
    settings_save_state_t save_state;
    char address[16];
    char netmask[16];
    char gateway[16];
    int32_t length;

    if (!settings_get_config(&config) ||
        !settings_get_save_state(&save_state)) {
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "settings_unavailable");
        return;
    }
    (void)snprintf(address, sizeof(address), "%u.%u.%u.%u",
                   (unsigned int)config.ethernet.address[0],
                   (unsigned int)config.ethernet.address[1],
                   (unsigned int)config.ethernet.address[2],
                   (unsigned int)config.ethernet.address[3]);
    (void)snprintf(netmask, sizeof(netmask), "%u.%u.%u.%u",
                   (unsigned int)config.ethernet.netmask[0],
                   (unsigned int)config.ethernet.netmask[1],
                   (unsigned int)config.ethernet.netmask[2],
                   (unsigned int)config.ethernet.netmask[3]);
    (void)snprintf(gateway, sizeof(gateway), "%u.%u.%u.%u",
                   (unsigned int)config.ethernet.gateway[0],
                   (unsigned int)config.ethernet.gateway[1],
                   (unsigned int)config.ethernet.gateway[2],
                   (unsigned int)config.ethernet.gateway[3]);
    length = snprintf(
        connection->response_body, sizeof(connection->response_body),
        "{\"saveState\":\"%s\",\"apply\":\"reboot\","
        "\"mqtt\":{\"enabled\":%s,\"tlsEnabled\":%s,"
        "\"host\":\"%s\",\"port\":%u,\"clientId\":\"%s\","
        "\"keepAliveSeconds\":%u},"
        "\"ethernet\":{\"dhcpEnabled\":%s,\"address\":\"%s\","
        "\"netmask\":\"%s\",\"gateway\":\"%s\"},"
        "\"uart\":{\"uart7\":{\"baudRate\":%lu,\"dataBits\":%u,"
        "\"parity\":%u,\"stopBits\":%u},"
        "\"usart3\":{\"baudRate\":%lu,\"dataBits\":%u,"
        "\"parity\":%u,\"stopBits\":%u},"
        "\"usart10\":{\"baudRate\":%lu,\"dataBits\":%u,"
        "\"parity\":%u,\"stopBits\":%u},"
        "\"uart9ManagedBy\":\"radio-settings\"},"
        "\"productVariant\":null}",
        web_server_settings_save_state_name(save_state),
        config.mqtt.enabled ? "true" : "false",
        config.mqtt.tls_enabled ? "true" : "false", config.mqtt.host,
        (unsigned int)config.mqtt.port, config.mqtt.client_id,
        (unsigned int)config.mqtt.keep_alive_seconds,
        config.ethernet.dhcp_enabled ? "true" : "false",
        address, netmask, gateway,
        (unsigned long)config.uart[0].baud_rate,
        (unsigned int)config.uart[0].data_bits,
        (unsigned int)config.uart[0].parity,
        (unsigned int)config.uart[0].stop_bits,
        (unsigned long)config.uart[1].baud_rate,
        (unsigned int)config.uart[1].data_bits,
        (unsigned int)config.uart[1].parity,
        (unsigned int)config.uart[1].stop_bits,
        (unsigned long)config.uart[2].baud_rate,
        (unsigned int)config.uart[2].data_bits,
        (unsigned int)config.uart[2].parity,
        (unsigned int)config.uart[2].stop_bits);
    if (length < 0 ||
        (size_t)length >= sizeof(connection->response_body)) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    web_server_prepare_response(
        connection, 200U, "OK", "application/json",
        (const uint8_t *)connection->response_body, (size_t)length, false);
}

static void web_server_queue_settings_save(
    web_server_connection_t *connection,
    const settings_config_t *config)
{
    const settings_result_t result = settings_request_save(config);

    if (result == SETTINGS_RESULT_OK) {
        web_server_prepare_json_response(
            connection, 202U, "Accepted",
            "{\"status\":\"pending\",\"apply\":\"reboot\"}");
    } else if (result == SETTINGS_RESULT_BUSY) {
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "settings_busy");
    } else if (result == SETTINGS_RESULT_STORAGE_ERROR) {
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "settings_storage_unavailable");
    } else if (result == SETTINGS_RESULT_INVALID) {
        web_server_prepare_error(connection, 400U, "Bad Request",
                                 "invalid_settings");
    } else {
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "settings_unavailable");
    }
}

static void web_server_prepare_response(web_server_connection_t *connection,
                                        uint16_t status_code,
                                        const char *reason,
                                        const char *content_type,
                                        const uint8_t *body,
                                        size_t body_length,
                                        bool gzip_encoded)
{
    int32_t header_length;

    if (connection == NULL || reason == NULL || content_type == NULL ||
        (body == NULL && body_length != 0U)) {
        return;
    }
    header_length = snprintf(connection->tx_header,
                             sizeof(connection->tx_header),
                             "HTTP/1.1 %u %s\r\n"
                             "Content-Type: %s\r\n"
                             "%s"
                             "Content-Length: %u\r\n"
                             "Cache-Control: no-store\r\n"
                             "X-Content-Type-Options: nosniff\r\n"
                             "Connection: close\r\n\r\n",
                             (unsigned int)status_code, reason, content_type,
                             gzip_encoded ? "Content-Encoding: gzip\r\n" : "",
                             (unsigned int)body_length);
    if (header_length < 0 ||
        (size_t)header_length >= sizeof(connection->tx_header)) {
        return;
    }
    connection->tx_header_length = (size_t)header_length;
    connection->tx_header_offset = 0U;
    connection->tx_body = body;
    connection->tx_body_length = body_length;
    connection->tx_body_offset = 0U;
    connection->response_started = true;
}

static void web_server_prepare_json_response(
    web_server_connection_t *connection,
    uint16_t status_code,
    const char *reason,
    const char *json)
{
    if (connection == NULL || json == NULL) {
        return;
    }
    (void)snprintf(connection->response_body,
                   sizeof(connection->response_body), "%s", json);
    web_server_prepare_response(connection, status_code, reason,
                                "application/json",
                                (const uint8_t *)connection->response_body,
                                strlen(connection->response_body), false);
}

static void web_server_prepare_error(web_server_connection_t *connection,
                                     uint16_t status_code,
                                     const char *reason,
                                     const char *error)
{
    char json[128];

    (void)snprintf(json, sizeof(json), "{\"error\":\"%s\"}", error);
    web_server_prepare_json_response(connection, status_code, reason, json);
}

static bool web_server_json_append_text(char *buffer,
                                        size_t capacity,
                                        size_t *length,
                                        const char *text)
{
    size_t text_length;

    if (buffer == NULL || length == NULL || text == NULL ||
        *length >= capacity) {
        return false;
    }
    text_length = strlen(text);
    if (text_length >= capacity - *length) {
        return false;
    }
    memcpy(&buffer[*length], text, text_length);
    *length += text_length;
    buffer[*length] = '\0';
    return true;
}

static bool web_server_json_append_unsigned(char *buffer,
                                            size_t capacity,
                                            size_t *length,
                                            uint32_t value)
{
    int32_t appended;

    if (buffer == NULL || length == NULL || *length >= capacity) {
        return false;
    }
    appended = snprintf(&buffer[*length], capacity - *length, "%" PRIu32,
                        value);
    if (appended < 0 || (size_t)appended >= capacity - *length) {
        return false;
    }
    *length += (size_t)appended;
    return true;
}

static bool web_server_json_append_string(char *buffer,
                                          size_t capacity,
                                          size_t *length,
                                          const char *value)
{
    if (value == NULL) {
        return false;
    }
    if (!web_server_json_append_text(buffer, capacity, length, "\"")) {
        return false;
    }
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor != '\0'; ++cursor) {
        char escaped[7];
        size_t escaped_length;

        if (*cursor == '"' || *cursor == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*cursor;
            escaped_length = 2U;
        } else if (*cursor < 0x20U) {
            (void)snprintf(escaped, sizeof(escaped), "\\u%04X",
                           (unsigned int)*cursor);
            escaped_length = 6U;
        } else {
            escaped[0] = (char)*cursor;
            escaped_length = 1U;
        }
        if (*length >= capacity ||
            escaped_length >= capacity - *length) {
            return false;
        }
        memcpy(&buffer[*length], escaped, escaped_length);
        *length += escaped_length;
        buffer[*length] = '\0';
    }
    return web_server_json_append_text(buffer, capacity, length, "\"");
}

static void web_server_filesystem_send_pending(
    web_server_connection_t *connection,
    uint32_t request_id)
{
    char json[96];
    int32_t length;

    if (connection == NULL) {
        return;
    }
    length = snprintf(json, sizeof(json),
                      "{\"requestId\":%" PRIu32 ",\"state\":\"pending\"}",
                      request_id);
    if (length < 0 || (size_t)length >= sizeof(json)) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    web_server_prepare_response(
        connection, 202U, "Accepted", "application/json",
        (const uint8_t *)json, (size_t)length, false);
}

static void web_server_filesystem_format_response(
    web_server_connection_t *connection,
    const web_server_filesystem_snapshot_t *snapshot)
{
    char *body;
    size_t capacity;
    size_t length = 0U;
    uint32_t appended_entries = 0U;
    bool serialization_truncated = false;

    if (connection == NULL) {
        return;
    }
    body = connection->response_body;
    capacity = sizeof(connection->response_body);
    if (snapshot == NULL || !snapshot->success) {
        if (snapshot != NULL && snapshot->http_status == 400U) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_offset");
            return;
        }
        web_server_prepare_error(
            connection,
            snapshot != NULL && snapshot->http_status == 404U ? 404U : 503U,
            snapshot != NULL && snapshot->http_status == 404U ? "Not Found"
                                                              : "Service Unavailable",
            snapshot != NULL && snapshot->http_status == 404U
                ? "directory_not_found"
                : "filesystem_read_failed");
        return;
    }

    if (!web_server_json_append_text(body, capacity, &length,
                                     "{\"requestId\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->request_id) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"path\":") ||
        !web_server_json_append_string(body, capacity, &length,
                                       snapshot->path) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"totalBytes\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->total_bytes) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"usedBytes\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->used_bytes) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"freeBytes\":") ||
        !web_server_json_append_unsigned(
            body, capacity, &length,
            snapshot->total_bytes - snapshot->used_bytes) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"fileCount\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->file_count) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"directoryCount\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->directory_count) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"totalEntries\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->total_entries) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"offset\":") ||
        !web_server_json_append_unsigned(body, capacity, &length,
                                         snapshot->offset) ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"entries\":[")) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }

    for (uint8_t index = 0U; index < snapshot->entry_count; ++index) {
        const file_system_dirent_t *const entry = &snapshot->entries[index];
        const size_t entry_start = length;

        if ((appended_entries > 0U &&
             !web_server_json_append_text(body, capacity, &length, ",")) ||
            !web_server_json_append_text(body, capacity, &length,
                                         "{\"name\":") ||
            !web_server_json_append_string(body, capacity, &length,
                                           entry->name) ||
            !web_server_json_append_text(
                body, capacity, &length,
                entry->is_directory ? ",\"isDirectory\":true,\"size\":"
                                    : ",\"isDirectory\":false,\"size\":") ||
            !web_server_json_append_unsigned(body, capacity, &length,
                                             entry->size) ||
            !web_server_json_append_text(body, capacity, &length, "}")) {
            length = entry_start;
            body[length] = '\0';
            serialization_truncated = true;
            break;
        }
        appended_entries++;
    }

    if (!web_server_json_append_text(body, capacity, &length,
                                     "],\"hasMore\":") ||
        !web_server_json_append_text(
            body, capacity, &length,
            serialization_truncated ||
                    snapshot->offset + appended_entries <
                        snapshot->total_entries
                ? "true"
                : "false") ||
        !web_server_json_append_text(body, capacity, &length,
                                     ",\"nextOffset\":") ||
        !web_server_json_append_unsigned(
            body, capacity, &length, snapshot->offset + appended_entries) ||
        !web_server_json_append_text(body, capacity, &length, "}")) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    web_server_prepare_response(
        connection, 200U, "OK", "application/json",
        (const uint8_t *)body, length, false);
}

static void web_server_filesystem_format_download(
    web_server_connection_t *connection,
    const web_server_filesystem_snapshot_t *snapshot)
{
    int32_t header_length;

    if (connection == NULL || snapshot == NULL) {
        return;
    }
    if (!snapshot->success) {
        if (snapshot->http_status == 404U) {
            web_server_prepare_error(connection, 404U, "Not Found",
                                     "file_not_found");
        } else if (snapshot->http_status == 413U) {
            web_server_prepare_error(connection, 413U, "Content Too Large",
                                     "file_too_large");
        } else if (snapshot->http_status == 416U) {
            web_server_prepare_error(connection, 416U,
                                     "Range Not Satisfiable",
                                     "invalid_file_offset");
        } else {
            web_server_prepare_error(connection, 503U, "Service Unavailable",
                                     "file_read_failed");
        }
        return;
    }

    header_length = snprintf(
        connection->tx_header, sizeof(connection->tx_header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %u\r\n"
        "X-File-Size: %" PRIu32 "\r\n"
        "X-File-Offset: %" PRIu32 "\r\n"
        "X-Request-Id: %" PRIu32 "\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Connection: close\r\n\r\n",
        (unsigned int)snapshot->download_length, snapshot->download_size,
        snapshot->offset, snapshot->request_id);
    if (header_length < 0 ||
        (size_t)header_length >= sizeof(connection->tx_header)) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    connection->tx_header_length = (size_t)header_length;
    connection->tx_header_offset = 0U;
    connection->tx_body = web_server_filesystem_download_buffer;
    connection->tx_body_length = snapshot->download_length;
    connection->tx_body_offset = 0U;
    connection->response_started = true;
}

static void web_server_filesystem_handle_request(
    web_server_connection_t *connection)
{
    web_server_filesystem_command_t command;
    bool mounted;
    bool pending;
    bool cached;

    if (connection == NULL) {
        return;
    }
    if (strcmp(connection->method, "GET") != 0 ||
        connection->content_length != 0U) {
        web_server_prepare_error(connection, 405U, "Method Not Allowed",
                                 "method_not_allowed");
        return;
    }
    if (!web_server_parse_filesystem_request(connection->path, &command)) {
        web_server_prepare_error(connection, 400U, "Bad Request",
                                 "invalid_filesystem_request");
        return;
    }

    taskENTER_CRITICAL();
    mounted = web_server_snapshot.storage_mounted;
    pending = web_server_filesystem_snapshot.pending;
    cached = web_server_filesystem_snapshot.ready &&
             web_server_filesystem_snapshot.download == command.download &&
             web_server_filesystem_snapshot.request_id == command.request_id &&
             web_server_filesystem_snapshot.offset == command.offset &&
             strcmp(web_server_filesystem_snapshot.path, command.path) == 0;
    if (cached) {
        web_server_filesystem_response_snapshot =
            web_server_filesystem_snapshot;
    }
    taskEXIT_CRITICAL();

    if (!mounted) {
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "filesystem_unavailable");
        return;
    }
    if (cached) {
        if (command.download) {
            web_server_filesystem_format_download(
                connection, &web_server_filesystem_response_snapshot);
        } else {
            web_server_filesystem_format_response(
                connection, &web_server_filesystem_response_snapshot);
        }
        return;
    }
    if (pending) {
        taskENTER_CRITICAL();
        cached = web_server_filesystem_snapshot.request_id ==
                     command.request_id &&
                 web_server_filesystem_snapshot.download ==
                     command.download &&
                 web_server_filesystem_snapshot.offset == command.offset &&
                 strcmp(web_server_filesystem_snapshot.path,
                        command.path) == 0;
        taskEXIT_CRITICAL();
        if (cached) {
            web_server_filesystem_send_pending(connection,
                                               command.request_id);
        } else {
            web_server_prepare_error(connection, 503U,
                                     "Service Unavailable",
                                     "filesystem_busy");
        }
        return;
    }
    if (web_server_filesystem_queue == NULL) {
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "filesystem_unavailable");
        return;
    }

    taskENTER_CRITICAL();
    if (web_server_filesystem_snapshot.pending) {
        taskEXIT_CRITICAL();
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "filesystem_busy");
        return;
    }
    web_server_filesystem_snapshot.pending = true;
    web_server_filesystem_snapshot.ready = false;
    web_server_filesystem_snapshot.download = command.download;
    web_server_filesystem_snapshot.request_id = command.request_id;
    web_server_filesystem_snapshot.offset = command.offset;
    (void)snprintf(web_server_filesystem_snapshot.path,
                   sizeof(web_server_filesystem_snapshot.path), "%s",
                   command.path);
    taskEXIT_CRITICAL();

    if (xQueueSend(web_server_filesystem_queue, &command, 0U) != pdTRUE) {
        taskENTER_CRITICAL();
        web_server_filesystem_snapshot.pending = false;
        web_server_filesystem_snapshot.ready = false;
        taskEXIT_CRITICAL();
        web_server_prepare_error(connection, 503U, "Service Unavailable",
                                 "filesystem_busy");
        return;
    }
    web_server_filesystem_send_pending(connection, command.request_id);
}

static bool web_server_queue_operation(
    web_server_connection_t *connection,
    web_server_operation_kind_t kind,
    const ZigbeeParameter_t *parameters)
{
    web_server_command_t command;
    bool available;

    memset(&command, 0, sizeof(command));
    command.kind = kind;
    if (parameters != NULL) {
        command.parameters = *parameters;
    }

    taskENTER_CRITICAL();
    available =
        web_server_snapshot.operation_state != WEB_SERVER_OPERATION_PENDING &&
        !web_server_snapshot.radio_busy;
    if (available) {
        web_server_snapshot.operation_state = WEB_SERVER_OPERATION_PENDING;
        web_server_snapshot.operation_kind = kind;
        web_server_snapshot.operation_error = 0U;
    }
    taskEXIT_CRITICAL();

    if (!available || web_server_queue == NULL ||
        xQueueSend(web_server_queue, &command, 0U) != pdTRUE) {
        if (available) {
            web_server_operation_set(WEB_SERVER_OPERATION_FAILURE, kind, 2U);
        }
        web_server_prepare_error(connection, 409U, "Conflict", "busy");
        return false;
    }

    {
        char json[128];
        (void)snprintf(json, sizeof(json),
                       "{\"operation\":{\"kind\":\"%s\","
                       "\"state\":\"pending\",\"error\":0}}",
                       web_server_operation_kind_name(kind));
        web_server_prepare_json_response(connection, 202U, "Accepted", json);
    }
    return true;
}

static void web_server_format_status(web_server_connection_t *connection)
{
    web_server_snapshot_t snapshot;
    char ip[IP4ADDR_STRLEN_MAX];
    char netmask[IP4ADDR_STRLEN_MAX];
    char gateway[IP4ADDR_STRLEN_MAX];
    int32_t length;

    web_server_snapshot_copy(&snapshot);
    (void)ip4addr_ntoa_r(netif_ip4_addr(&gnetif), ip, (int)sizeof(ip));
    (void)ip4addr_ntoa_r(netif_ip4_netmask(&gnetif), netmask,
                         (int)sizeof(netmask));
    (void)ip4addr_ntoa_r(netif_ip4_gw(&gnetif), gateway,
                         (int)sizeof(gateway));

    length = snprintf(
        connection->response_body, sizeof(connection->response_body),
        "{\"network\":{\"linkUp\":%s,\"interfaceUp\":%s,"
        "\"dhcp\":%s,\"ip\":\"%s\",\"netmask\":\"%s\","
        "\"gateway\":\"%s\"},\"storage\":{\"mounted\":%s},"
        "\"operation\":{\"kind\":\"%s\",\"state\":\"%s\","
        "\"error\":%u},\"system\":{\"heapTotalBytes\":%" PRIu32 ","
        "\"heapFreeBytes\":%" PRIu32 ",\"heapMinimumFreeBytes\":%" PRIu32 ","
        "\"cpuAvailable\":%s,\"cpuTenthsPercent\":%u,"
        "\"taskCount\":%" PRIu32 ",\"taskReported\":%u,"
        "\"tasksTruncated\":%s,\"tasks\":[",
        netif_is_link_up(&gnetif) ? "true" : "false",
        netif_is_up(&gnetif) ? "true" : "false",
        dhcp_supplied_address(&gnetif) != 0U ? "true" : "false",
        ip, netmask, gateway,
        snapshot.storage_mounted ? "true" : "false",
        web_server_operation_kind_name(snapshot.operation_kind),
        web_server_operation_state_name(snapshot.operation_state),
        (unsigned int)snapshot.operation_error,
        (uint32_t)snapshot.heap_total_bytes,
        (uint32_t)snapshot.heap_free_bytes,
        (uint32_t)snapshot.heap_minimum_free_bytes,
        snapshot.cpu_available ? "true" : "false",
        (unsigned int)snapshot.cpu_tenths_percent,
        (uint32_t)snapshot.task_count,
        (unsigned int)snapshot.task_reported,
        snapshot.tasks_truncated ? "true" : "false");
    if (length < 0 ||
        (size_t)length >= sizeof(connection->response_body)) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    for (uint8_t index = 0U; index < snapshot.task_reported; ++index) {
        const web_server_task_metric_t *const task = &snapshot.tasks[index];
        int32_t appended;

        appended = snprintf(
            &connection->response_body[(size_t)length],
            sizeof(connection->response_body) - (size_t)length,
            "%s{\"name\":\"%s\",\"state\":\"%s\",\"priority\":%" PRIu32 ","
            "\"stackHighWaterBytes\":%" PRIu32 ",\"cpuTenthsPercent\":%u}",
            index == 0U ? "" : ",", task->name, task->state,
            task->priority,
            task->stack_high_water_bytes,
            (unsigned int)task->cpu_tenths_percent);
        if (appended < 0 ||
            (size_t)appended >=
                sizeof(connection->response_body) - (size_t)length) {
            web_server_prepare_error(connection, 500U,
                                     "Internal Server Error",
                                     "response_error");
            return;
        }
        length += appended;
    }
    const int32_t appended = snprintf(
        &connection->response_body[(size_t)length],
        sizeof(connection->response_body) - (size_t)length, "]}}");
    if (appended < 0 ||
        (size_t)appended >=
            sizeof(connection->response_body) - (size_t)length) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    length += appended;
    web_server_prepare_response(
        connection, 200U, "OK", "application/json",
        (const uint8_t *)connection->response_body, (size_t)length, false);
}

static void web_server_format_radio_config(
    web_server_connection_t *connection)
{
    web_server_snapshot_t snapshot;
    int32_t length;

    web_server_snapshot_copy(&snapshot);
    length = snprintf(
        connection->response_body, sizeof(connection->response_body),
        "{\"initialized\":%s,\"busy\":%s,\"configAvailable\":%s,"
        "\"config\":{"
        "\"panId\":%u,\"channel\":%u,\"userAddress\":%u,"
        "\"pointType\":%u,\"transferModel\":%u,\"baudRate\":%u,"
        "\"dataBits\":%u,\"stopBits\":%u,\"parity\":%u,"
        "\"antennaSelect\":%u}}",
        snapshot.radio_initialized ? "true" : "false",
        snapshot.radio_busy ? "true" : "false",
        snapshot.radio_config_available ? "true" : "false",
        (unsigned int)snapshot.radio_parameters.PAN_ID,
        (unsigned int)snapshot.radio_parameters.Channel,
        (unsigned int)snapshot.radio_parameters.userAddress,
        (unsigned int)snapshot.radio_parameters.pointType,
        (unsigned int)snapshot.radio_parameters.transferModel,
        (unsigned int)snapshot.radio_parameters.uartBraudRate,
        (unsigned int)snapshot.radio_parameters.uartDataBits,
        (unsigned int)snapshot.radio_parameters.uartStopBits,
        (unsigned int)snapshot.radio_parameters.uartParity,
        (unsigned int)snapshot.radio_parameters.antennaSelect);
    if (length < 0 ||
        (size_t)length >= sizeof(connection->response_body)) {
        web_server_prepare_error(connection, 500U, "Internal Server Error",
                                 "response_error");
        return;
    }
    web_server_prepare_response(
        connection, 200U, "OK", "application/json",
        (const uint8_t *)connection->response_body, (size_t)length, false);
}

static bool web_server_is_asset_path(const char *path)
{
    return strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0 ||
           strcmp(path, "/styles.css") == 0 || strcmp(path, "/app.js") == 0 ||
           strcmp(path, "/X-favicon.png") == 0;
}

static bool web_server_is_filesystem_route(const char *path)
{
    static const char endpoint[] = "/api/v1/files";
    static const char download_endpoint[] = "/api/v1/files/download?";

    return path != NULL &&
           (strcmp(path, endpoint) == 0 ||
            (strncmp(path, endpoint, sizeof(endpoint) - 1U) == 0 &&
             path[sizeof(endpoint) - 1U] == '?') ||
            strncmp(path, download_endpoint,
                    sizeof(download_endpoint) - 1U) == 0);
}

static void web_server_serve_asset(web_server_connection_t *connection)
{
    const uint8_t *asset = NULL;
    size_t asset_length = 0U;
    const char *content_type = NULL;
    bool gzip_encoded = false;

    if (strcmp(connection->path, "/") == 0 ||
        strcmp(connection->path, "/index.html") == 0) {
        asset = web_server_index_html_gzip;
        asset_length = web_server_index_html_gzip_length;
        content_type = "text/html; charset=utf-8";
        gzip_encoded = true;
    } else if (strcmp(connection->path, "/styles.css") == 0) {
        asset = web_server_styles_css_gzip;
        asset_length = web_server_styles_css_gzip_length;
        content_type = "text/css; charset=utf-8";
        gzip_encoded = true;
    } else if (strcmp(connection->path, "/app.js") == 0) {
        asset = web_server_app_js_gzip;
        asset_length = web_server_app_js_gzip_length;
        content_type = "application/javascript; charset=utf-8";
        gzip_encoded = true;
    } else if (strcmp(connection->path, "/X-favicon.png") == 0) {
        asset = web_server_favicon_png;
        asset_length = web_server_favicon_png_length;
        content_type = "image/png";
    }

    if (asset == NULL || content_type == NULL) {
        web_server_prepare_error(connection, 404U, "Not Found", "not_found");
        return;
    }
    web_server_prepare_response(connection, 200U, "OK", content_type,
                                asset, asset_length, gzip_encoded);
}

static void web_server_handle_request(web_server_connection_t *connection)
{
    web_server_snapshot_t snapshot;
    const char *body;

    if (connection == NULL) {
        return;
    }
    body = &connection->request[connection->body_offset];
    connection->request[connection->request_length] = '\0';

    if (web_server_is_asset_path(connection->path)) {
        if (strcmp(connection->method, "GET") != 0 ||
            connection->content_length != 0U) {
            web_server_prepare_error(connection, 405U, "Method Not Allowed",
                                     "method_not_allowed");
            return;
        }
        web_server_serve_asset(connection);
        return;
    }

    if (strcmp(connection->path, "/api/v1/status") == 0) {
        if (strcmp(connection->method, "GET") != 0 ||
            connection->content_length != 0U) {
            web_server_prepare_error(connection, 405U, "Method Not Allowed",
                                     "method_not_allowed");
            return;
        }
        web_server_format_status(connection);
        return;
    }

    if (web_server_is_filesystem_route(connection->path)) {
        web_server_filesystem_handle_request(connection);
        return;
    }

    if (strcmp(connection->path, "/api/v1/settings") == 0) {
        if (strcmp(connection->method, "GET") != 0 ||
            connection->content_length != 0U) {
            web_server_prepare_error(connection, 405U, "Method Not Allowed",
                                     "method_not_allowed");
            return;
        }
        web_server_format_settings(connection);
        return;
    }

    if (strcmp(connection->path, "/api/v1/settings/network/confirm") == 0 &&
        strcmp(connection->method, "POST") == 0) {
        settings_config_t config;

        if (!connection->has_content_length ||
            !connection->has_json_content_type ||
            !web_server_parse_empty_object(body, connection->content_length)) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_request");
            return;
        }
        if (!settings_get_config(&config)) {
            web_server_prepare_error(connection, 503U, "Service Unavailable",
                                     "settings_unavailable");
            return;
        }
        if (config.ethernet.dhcp_enabled) {
            web_server_prepare_error(connection, 409U, "Conflict",
                                     "static_network_not_active");
            return;
        }
        settings_confirm_network();
        web_server_prepare_json_response(
            connection, 200U, "OK", "{\"status\":\"confirmed\"}");
        return;
    }

    if ((strcmp(connection->path, "/api/v1/settings/mqtt") == 0 ||
         strcmp(connection->path, "/api/v1/settings/ethernet") == 0 ||
         strcmp(connection->path, "/api/v1/settings/uart7") == 0 ||
         strcmp(connection->path, "/api/v1/settings/usart3") == 0 ||
         strcmp(connection->path, "/api/v1/settings/usart10") == 0) &&
        strcmp(connection->method, "PUT") == 0) {
        settings_config_t config;

        if (!connection->has_content_length ||
            !connection->has_json_content_type) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_request");
            return;
        }
        if (!settings_get_config(&config)) {
            web_server_prepare_error(connection, 503U, "Service Unavailable",
                                     "settings_unavailable");
            return;
        }
        if (strcmp(connection->path, "/api/v1/settings/mqtt") == 0) {
            if (!web_server_parse_settings_mqtt(
                    body, connection->content_length, &config.mqtt)) {
                web_server_prepare_error(connection, 400U, "Bad Request",
                                         "invalid_mqtt_settings");
                return;
            }
        } else if (strcmp(connection->path,
                          "/api/v1/settings/ethernet") == 0) {
            if (!web_server_parse_settings_network(
                    body, connection->content_length, &config.ethernet)) {
                web_server_prepare_error(connection, 400U, "Bad Request",
                                         "invalid_ethernet_settings");
                return;
            }
        } else {
            size_t uart_index;
            settings_uart_config_t uart;
            const char *path = connection->path;

            if (strcmp(path, "/api/v1/settings/uart7") == 0) {
                uart_index = 0U;
            } else if (strcmp(path, "/api/v1/settings/usart3") == 0) {
                uart_index = 1U;
            } else {
                uart_index = 2U;
            }
            uart = config.uart[uart_index];
            if (!web_server_parse_settings_uart(
                    body, connection->content_length, &uart)) {
                web_server_prepare_error(connection, 400U, "Bad Request",
                                         "invalid_uart_settings");
                return;
            }
            config.uart[uart_index] = uart;
        }
        if (!settings_validate(&config)) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_settings");
            return;
        }
        web_server_queue_settings_save(connection, &config);
        return;
    }

    if (strcmp(connection->path, "/api/v1/radio/config") == 0 &&
        strcmp(connection->method, "GET") == 0) {
        if (connection->content_length != 0U) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "unexpected_body");
            return;
        }
        web_server_format_radio_config(connection);
        return;
    }

    if (strcmp(connection->path, "/api/v1/radio/read") == 0 &&
        strcmp(connection->method, "POST") == 0) {
        if (!connection->has_content_length ||
            !connection->has_json_content_type ||
            !web_server_parse_empty_object(body, connection->content_length)) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_request");
            return;
        }
        web_server_snapshot_copy(&snapshot);
        if (!snapshot.radio_initialized) {
            web_server_prepare_error(connection, 503U,
                                     "Service Unavailable",
                                     "radio_unavailable");
            return;
        }
        (void)web_server_queue_operation(
            connection, WEB_SERVER_OPERATION_RADIO_READ, NULL);
        return;
    }

    if (strcmp(connection->path, "/api/v1/radio/config") == 0 &&
        strcmp(connection->method, "PUT") == 0) {
        web_server_public_config_t public_config;
        ZigbeeParameter_t parameters;

        if (!connection->has_content_length ||
            !connection->has_json_content_type) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_request");
            return;
        }
        web_server_snapshot_copy(&snapshot);
        if (!snapshot.radio_initialized) {
            web_server_prepare_error(connection, 503U,
                                     "Service Unavailable",
                                     "radio_unavailable");
            return;
        }
        if (!snapshot.radio_config_available) {
            web_server_prepare_error(connection, 503U,
                                     "Service Unavailable",
                                     "radio_config_unavailable");
            return;
        }
        web_server_config_from_parameters(&snapshot.radio_parameters,
                                          &public_config);
        if (!web_server_parse_config(body, connection->content_length,
                                     &public_config)) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_config");
            return;
        }
        parameters = snapshot.radio_parameters;
        web_server_config_to_parameters(&public_config, &parameters);
        (void)web_server_queue_operation(
            connection, WEB_SERVER_OPERATION_RADIO_UPDATE, &parameters);
        return;
    }

    if (strcmp(connection->path, "/api/v1/radio/read") == 0 ||
        strcmp(connection->path, "/api/v1/radio/config") == 0 ||
        strcmp(connection->path, "/api/v1/settings") == 0 ||
        strncmp(connection->path, "/api/v1/settings/", 17U) == 0) {
        web_server_prepare_error(connection, 405U, "Method Not Allowed",
                                 "method_not_allowed");
        return;
    }
    web_server_prepare_error(connection, 404U, "Not Found", "not_found");
}

static void web_server_abort_connection(web_server_connection_t *connection)
{
    struct tcp_pcb *pcb;

    if (connection == NULL || connection->pcb == NULL) {
        return;
    }
    pcb = connection->pcb;
    connection->pcb = NULL;
    connection->active = false;
    tcp_abort(pcb);
}

static err_t web_server_try_close(web_server_connection_t *connection)
{
    err_t error;
    struct tcp_pcb *pcb;

    if (connection == NULL || connection->pcb == NULL) {
        return ERR_OK;
    }
    pcb = connection->pcb;
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, NULL, 0U);
    tcp_err(pcb, NULL);
    error = tcp_close(pcb);
    if (error == ERR_OK) {
        connection->pcb = NULL;
        connection->active = false;
        connection->close_pending = false;
    } else if (error == ERR_MEM) {
        tcp_arg(pcb, connection);
        tcp_recv(pcb, web_server_receive);
        tcp_sent(pcb, web_server_sent);
        tcp_poll(pcb, web_server_poll, WEB_SERVER_POLL_INTERVAL);
        tcp_err(pcb, web_server_error);
        connection->close_pending = true;
        return ERR_OK;
    } else {
        web_server_abort_connection(connection);
        return ERR_ABRT;
    }
    return error;
}

static err_t web_server_transmit(web_server_connection_t *connection)
{
    struct tcp_pcb *pcb;

    if (connection == NULL || connection->pcb == NULL ||
        !connection->response_started) {
        return ERR_OK;
    }
    pcb = connection->pcb;
    while (tcp_sndbuf(pcb) > 0U) {
        const uint8_t *data;
        size_t remaining;
        size_t chunk_length;
        u16_t send_length;
        err_t error;

        if (connection->tx_header_offset < connection->tx_header_length) {
            data = (const uint8_t *)&connection->tx_header[
                connection->tx_header_offset];
            remaining = connection->tx_header_length -
                        connection->tx_header_offset;
        } else if (connection->tx_body_offset < connection->tx_body_length) {
            data = &connection->tx_body[connection->tx_body_offset];
            remaining = connection->tx_body_length -
                        connection->tx_body_offset;
        } else {
            return web_server_try_close(connection);
        }

        chunk_length = remaining;
        if (chunk_length > WEB_SERVER_TX_CHUNK_MAX) {
            chunk_length = WEB_SERVER_TX_CHUNK_MAX;
        }
        if (chunk_length > tcp_sndbuf(pcb)) {
            chunk_length = tcp_sndbuf(pcb);
        }
        send_length = (u16_t)chunk_length;
        error = tcp_write(pcb, data, send_length, TCP_WRITE_FLAG_COPY);
        if (error == ERR_MEM) {
            break;
        }
        if (error != ERR_OK) {
            web_server_abort_connection(connection);
            return ERR_ABRT;
        }

        if (connection->tx_header_offset < connection->tx_header_length) {
            connection->tx_header_offset += chunk_length;
        } else {
            connection->tx_body_offset += chunk_length;
        }
    }

    {
        const err_t output_error = tcp_output(pcb);
        if (output_error != ERR_OK && output_error != ERR_MEM) {
            web_server_abort_connection(connection);
            return ERR_ABRT;
        }
    }

    if (connection->tx_header_offset == connection->tx_header_length &&
        connection->tx_body_offset == connection->tx_body_length) {
        return web_server_try_close(connection);
    }
    return ERR_OK;
}

static void web_server_filesystem_download_refresh(
    const web_server_filesystem_command_t *command,
    web_server_filesystem_snapshot_t *result)
{
    struct stat file_status;
    int32_t file_descriptor;
    uint32_t remaining;
    uint32_t requested_length;
    bool read_successfully = true;

    file_descriptor = file_system_open(command->path, O_RDONLY, 0U);
    if (file_descriptor < 0) {
        result->http_status = errno == ENOENT ? 404U : 503U;
        read_successfully = false;
    } else {
        if (file_system_fstat(file_descriptor, &file_status) != 0) {
            result->http_status = 503U;
            read_successfully = false;
        } else if (!S_ISREG(file_status.st_mode)) {
            result->http_status = 404U;
            read_successfully = false;
        } else if (file_status.st_size < 0 ||
                   file_status.st_size > INT32_MAX) {
            result->http_status = 413U;
            read_successfully = false;
        } else {
            result->download_size = (uint32_t)file_status.st_size;
            if (command->offset > result->download_size) {
                result->http_status = 416U;
                read_successfully = false;
            }
        }

        if (read_successfully) {
            remaining = result->download_size - command->offset;
            requested_length = remaining;
            if (requested_length >
                WEB_SERVER_FILESYSTEM_DOWNLOAD_CHUNK_SIZE) {
                requested_length =
                    WEB_SERVER_FILESYSTEM_DOWNLOAD_CHUNK_SIZE;
            }
            if (command->offset > 0U &&
                file_system_lseek(file_descriptor,
                                  (int32_t)command->offset, SEEK_SET) < 0) {
                result->http_status = 503U;
                read_successfully = false;
            } else if (requested_length > 0U) {
                const int32_t bytes_read = file_system_read(
                    file_descriptor, web_server_filesystem_download_buffer,
                    requested_length);

                if (bytes_read < 0 ||
                    (uint32_t)bytes_read != requested_length) {
                    result->http_status = 503U;
                    read_successfully = false;
                } else {
                    result->download_length = (uint16_t)bytes_read;
                }
            }
        }
        if (file_system_close(file_descriptor) != 0) {
            result->http_status = 503U;
            read_successfully = false;
        }
    }

    result->success = read_successfully;
    result->ready = true;
    result->pending = false;
    taskENTER_CRITICAL();
    web_server_filesystem_snapshot = *result;
    taskEXIT_CRITICAL();
}

static void web_server_filesystem_refresh(
    const web_server_filesystem_command_t *command)
{
    web_server_filesystem_snapshot_t *const result =
        &web_server_filesystem_work;
    file_system_DIR *directory = NULL;
    bool read_successfully = true;
    int32_t read_result;

    if (command == NULL) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->request_id = command->request_id;
    result->offset = command->offset;
    result->download = command->download;
    (void)snprintf(result->path, sizeof(result->path), "%s", command->path);

    if (command->download) {
        web_server_filesystem_download_refresh(command, result);
        return;
    }

    if (file_system_get_usage(&result->total_bytes,
                              &result->used_bytes) != 0) {
        result->http_status = 503U;
        read_successfully = false;
    } else {
        directory = file_system_opendir(command->path);
        if (directory == NULL) {
            result->http_status = errno == ENOENT ? 404U : 503U;
            read_successfully = false;
        }
    }

    while (read_successfully) {
        file_system_dirent_t entry;

        read_result = file_system_readdir(directory, &entry);
        if (read_result == 0) {
            break;
        }
        if (read_result < 0) {
            result->http_status = 503U;
            read_successfully = false;
            break;
        }
        if (strcmp(entry.name, ".") == 0 ||
            strcmp(entry.name, "..") == 0) {
            continue;
        }
        if (result->total_entries == UINT32_MAX) {
            result->http_status = 503U;
            read_successfully = false;
            break;
        }
        result->total_entries++;
        if (entry.is_directory) {
            result->directory_count++;
        } else {
            result->file_count++;
        }
        if (result->total_entries - 1U >= command->offset &&
            result->entry_count < WEB_SERVER_FILESYSTEM_PAGE_SIZE) {
            result->entries[result->entry_count] = entry;
            result->entry_count++;
        }
    }

    if (directory != NULL) {
        if (file_system_closedir(directory) != 0) {
            result->http_status = 503U;
            read_successfully = false;
        }
    }
    if (read_successfully && command->offset > result->total_entries) {
        result->http_status = 400U;
        read_successfully = false;
    }
    result->success = read_successfully;
    result->ready = true;
    result->pending = false;

    taskENTER_CRITICAL();
    web_server_filesystem_snapshot = *result;
    taskEXIT_CRITICAL();
}

static void web_server_start_core(void *arg)
{
    err_t error;
    struct tcp_pcb *pcb;

    (void)arg;
    pcb = tcp_new();
    if (pcb == NULL) {
        LOGE(WEB_SERVER_TAG, "Could not allocate TCP listener");
        return;
    }
    tcp_bind_netif(pcb, &gnetif);
    error = tcp_bind(pcb, IP_ADDR_ANY, WEB_SERVER_PORT);
    if (error != ERR_OK) {
        LOGE(WEB_SERVER_TAG, "Could not bind TCP port %u: %d",
             (unsigned int)WEB_SERVER_PORT, (int)error);
        tcp_abort(pcb);
        return;
    }
    web_server_listener = tcp_listen(pcb);
    if (web_server_listener == NULL) {
        LOGE(WEB_SERVER_TAG, "Could not listen on TCP port %u",
             (unsigned int)WEB_SERVER_PORT);
        tcp_abort(pcb);
        return;
    }
    tcp_accept(web_server_listener, web_server_accept);
    LOGI(WEB_SERVER_TAG, "Listening on TCP port %u",
         (unsigned int)WEB_SERVER_PORT);
}

bool web_server_init(void)
{
    err_t error;

    if (web_server_startup_queued) {
        return true;
    }
    web_server_queue = xQueueCreateStatic(
        WEB_SERVER_QUEUE_LENGTH, sizeof(web_server_command_t),
        web_server_queue_buffer, &web_server_queue_storage);
    web_server_filesystem_queue = xQueueCreateStatic(
        WEB_SERVER_FILESYSTEM_QUEUE_LENGTH,
        sizeof(web_server_filesystem_command_t),
        web_server_filesystem_queue_buffer,
        &web_server_filesystem_queue_storage);
    if (web_server_queue == NULL ||
        web_server_filesystem_queue == NULL) {
        web_server_queue = NULL;
        web_server_filesystem_queue = NULL;
        return false;
    }
    memset(&web_server_connection, 0, sizeof(web_server_connection));
    memset(&web_server_snapshot, 0, sizeof(web_server_snapshot));
    memset(&web_server_filesystem_snapshot, 0,
           sizeof(web_server_filesystem_snapshot));
    web_server_snapshot.operation_state = WEB_SERVER_OPERATION_IDLE;
    web_server_snapshot.operation_kind = WEB_SERVER_OPERATION_NONE;
    web_server_snapshot.storage_mounted =
        file_system_port_is_mounted();

    error = tcpip_callback(web_server_start_core, NULL);
    if (error != ERR_OK) {
        web_server_queue = NULL;
        web_server_filesystem_queue = NULL;
        return false;
    }
    web_server_startup_queued = true;
    return true;
}

void web_server_process(void)
{
    web_server_command_t command;
    web_server_filesystem_command_t filesystem_command;
    const bool storage_mounted = file_system_port_is_mounted();
    const bool received = web_server_queue != NULL &&
                          xQueueReceive(web_server_queue, &command, 0U) == pdTRUE;
    const bool filesystem_received =
        web_server_filesystem_queue != NULL &&
        xQueueReceive(web_server_filesystem_queue, &filesystem_command, 0U) ==
            pdTRUE;
    const bool initialized = rf_app_is_initialized();
    const bool busy = rf_app_is_busy();
    const ZigbeeParameter_t *parameters = rf_app_get_parameters();

    web_server_monitor_refresh();

    taskENTER_CRITICAL();
    web_server_snapshot.radio_initialized = initialized;
    web_server_snapshot.radio_busy = busy;
    web_server_snapshot.radio_config_available =
        rf_app_has_parameters();
    web_server_snapshot.storage_mounted = storage_mounted;
    if (parameters != NULL) {
        web_server_snapshot.radio_parameters = *parameters;
    }
    taskEXIT_CRITICAL();

    if (received) {
        bool operation_started = false;

        if (!initialized || busy) {
            web_server_operation_set(WEB_SERVER_OPERATION_FAILURE,
                                     command.kind, 2U);
        } else if (command.kind == WEB_SERVER_OPERATION_RADIO_READ) {
            if (rf_app_read_parameters(web_server_operation_complete, NULL)) {
                operation_started = true;
            } else {
                web_server_operation_set(WEB_SERVER_OPERATION_FAILURE,
                                         command.kind, 2U);
            }
        } else if (command.kind == WEB_SERVER_OPERATION_RADIO_UPDATE) {
            if (rf_app_write_parameters(&command.parameters,
                                        web_server_operation_complete, NULL)) {
                operation_started = true;
            } else {
                web_server_operation_set(WEB_SERVER_OPERATION_FAILURE,
                                         command.kind, 2U);
            }
        } else {
            web_server_operation_set(WEB_SERVER_OPERATION_FAILURE,
                                     WEB_SERVER_OPERATION_NONE, 3U);
        }

        if (operation_started) {
            taskENTER_CRITICAL();
            web_server_snapshot.radio_busy = rf_app_is_busy();
            taskEXIT_CRITICAL();
        }
    }
    if (filesystem_received) {
        web_server_filesystem_refresh(&filesystem_command);
    }
}

static void web_server_operation_complete(ZigbeeMesh_t *mesh,
                                         uint8_t result,
                                         void *arg)
{
    const ZigbeeParameter_t *parameters;
    const uint8_t error_code =
        result == ZIGBEE_RES_SUCCESS
            ? 0U
            : (result == 0U ? 1U : result);

    (void)mesh;
    (void)arg;
    parameters = rf_app_get_parameters();
    taskENTER_CRITICAL();
    if (parameters != NULL) {
        web_server_snapshot.radio_parameters = *parameters;
    }
    web_server_snapshot.radio_config_available =
        rf_app_has_parameters();
    web_server_snapshot.radio_busy = rf_app_is_busy();
    web_server_snapshot.operation_error = error_code;
    web_server_snapshot.operation_state =
        result == ZIGBEE_RES_SUCCESS ? WEB_SERVER_OPERATION_SUCCESS
                                     : WEB_SERVER_OPERATION_FAILURE;
    taskEXIT_CRITICAL();
}

static err_t web_server_accept(void *arg,
                               struct tcp_pcb *new_pcb,
                               err_t error)
{
    web_server_connection_t *connection = &web_server_connection;

    (void)arg;
    if (new_pcb == NULL) {
        return ERR_ARG;
    }
    if (error != ERR_OK) {
        tcp_abort(new_pcb);
        return ERR_ABRT;
    }
    if (connection->active) {
        tcp_abort(new_pcb);
        return ERR_ABRT;
    }

    memset(connection, 0, sizeof(*connection));
    connection->pcb = new_pcb;
    connection->active = true;
    tcp_arg(new_pcb, connection);
    tcp_recv(new_pcb, web_server_receive);
    tcp_sent(new_pcb, web_server_sent);
    tcp_err(new_pcb, web_server_error);
    tcp_poll(new_pcb, web_server_poll, WEB_SERVER_POLL_INTERVAL);
    return ERR_OK;
}

static err_t web_server_receive(void *arg,
                                struct tcp_pcb *pcb,
                                struct pbuf *packet,
                                err_t error)
{
    web_server_connection_t *connection = arg;
    size_t new_length;
    u16_t packet_length;
    u16_t copied;
    size_t header_length;

    if (connection == NULL || pcb == NULL) {
        if (packet != NULL) {
            pbuf_free(packet);
        }
        return ERR_ARG;
    }
    if (packet == NULL) {
        if (connection->response_started) {
            return web_server_transmit(connection);
        }
        (void)web_server_try_close(connection);
        return ERR_OK;
    }
    if (connection->response_started) {
        tcp_recved(pcb, packet->tot_len);
        pbuf_free(packet);
        return web_server_transmit(connection);
    }
    connection->idle_poll_count = 0U;
    if (error != ERR_OK) {
        pbuf_free(packet);
        return error;
    }

    packet_length = packet->tot_len;
    tcp_recved(pcb, packet_length);
    new_length = connection->request_length + packet_length;
    if (new_length > WEB_SERVER_REQUEST_MAX ||
        (connection->headers_parsed &&
         new_length > connection->expected_length)) {
        pbuf_free(packet);
        web_server_prepare_error(connection, 413U, "Content Too Large",
                                 "request_too_large");
        return web_server_transmit(connection);
    }
    copied = pbuf_copy_partial(packet,
                               &connection->request[connection->request_length],
                               packet_length,
                               0U);
    pbuf_free(packet);
    if (copied != packet_length) {
        web_server_prepare_error(connection, 400U, "Bad Request",
                                 "invalid_packet");
        return web_server_transmit(connection);
    }
    connection->request_length = new_length;
    connection->request[connection->request_length] = '\0';

    if (!connection->headers_parsed) {
        if (!web_server_find_header_end(connection->request,
                                        connection->request_length,
                                        &header_length)) {
            if (connection->request_length > WEB_SERVER_HEADER_MAX) {
                web_server_prepare_error(connection, 431U,
                                         "Request Header Fields Too Large",
                                         "headers_too_large");
                return web_server_transmit(connection);
            }
            return ERR_OK;
        }
        if (!web_server_parse_headers(connection, header_length)) {
            web_server_prepare_error(connection, 400U, "Bad Request",
                                     "invalid_request");
            return web_server_transmit(connection);
        }
    }

    if (connection->request_length > connection->expected_length) {
        web_server_prepare_error(connection, 400U, "Bad Request",
                                 "pipelining_not_supported");
        return web_server_transmit(connection);
    }
    if (connection->request_length == connection->expected_length) {
        web_server_handle_request(connection);
        return web_server_transmit(connection);
    }
    return ERR_OK;
}

static err_t web_server_sent(void *arg,
                             struct tcp_pcb *pcb,
                             u16_t length)
{
    web_server_connection_t *connection = arg;

    (void)length;
    if (connection == NULL || connection->pcb != pcb) {
        return ERR_ARG;
    }
    if (connection->close_pending) {
        return web_server_try_close(connection);
    }
    return web_server_transmit(connection);
}

static err_t web_server_poll(void *arg, struct tcp_pcb *pcb)
{
    web_server_connection_t *connection = arg;

    if (connection == NULL || connection->pcb != pcb) {
        return ERR_ARG;
    }
    if (connection->close_pending) {
        return web_server_try_close(connection);
    }
    if (!connection->response_started) {
        ++connection->idle_poll_count;
        if (connection->idle_poll_count >=
            WEB_SERVER_REQUEST_TIMEOUT_POLLS) {
            return web_server_try_close(connection);
        }
    }
    return web_server_transmit(connection);
}

static void web_server_error(void *arg, err_t error)
{
    web_server_connection_t *connection = arg;

    (void)error;
    if (connection != NULL) {
        connection->pcb = NULL;
        connection->active = false;
        connection->close_pending = false;
    }
}
