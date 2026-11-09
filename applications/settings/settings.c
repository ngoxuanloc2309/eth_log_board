#include "settings.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

#include "FreeRTOS.h"
#include "file_system.h"
#include "file_system_port.h"
#include "queue.h"
#include "task.h"

#define SETTINGS_RECORD_SIZE (255U)
#define SETTINGS_RECORD_CRC_SIZE (4U)
#define SETTINGS_RECORD_DATA_SIZE (SETTINGS_RECORD_SIZE - SETTINGS_RECORD_CRC_SIZE)
#define SETTINGS_RECORD_MAGIC (0x53585453UL)
#define SETTINGS_SCHEMA_VERSION (1U)
#define SETTINGS_QUEUE_LENGTH (1U)
#define SETTINGS_TASK_STACK_SIZE (1024U)
#define SETTINGS_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)
#define SETTINGS_MQTT_HOST_OFFSET (23U)
#define SETTINGS_MQTT_CLIENT_ID_OFFSET (153U)
#define SETTINGS_UART_OFFSET (219U)
#define SETTINGS_DEFAULT_MQTT_PORT (1883U)
#define SETTINGS_DEFAULT_MQTT_KEEP_ALIVE (60U)
#define SETTINGS_DEFAULT_UART_BAUD (115200U)
#define SETTINGS_DEFAULT_UART_DATA_BITS (8U)
#define SETTINGS_DEFAULT_UART_PARITY (0U)
#define SETTINGS_DEFAULT_UART_STOP_BITS (1U)
#define SETTINGS_DEFAULT_NETWORK_TIMEOUT_MS (60000U)

typedef struct
{
    settings_config_t config;
} settings_save_request_t;

static const char SETTINGS_RECORD_PATHS[2][18] = {
    "/settings-a.bin",
    "/settings-b.bin",
};

static StaticQueue_t settings_queue_storage;
static uint8_t settings_queue_buffer[SETTINGS_QUEUE_LENGTH *
                                    sizeof(settings_save_request_t)];
static QueueHandle_t settings_queue;
static StaticTask_t settings_task_control_block;
static StackType_t settings_task_stack[SETTINGS_TASK_STACK_SIZE];
static settings_config_t settings_config;
static settings_save_state_t settings_save_state;
static uint32_t settings_generation;
static bool settings_initialized;
static bool settings_network_confirmed;
static bool settings_save_active;

static void settings_task(void *argument);

static uint16_t settings_read_u16(const uint8_t *buffer, size_t offset)
{
    return (uint16_t)((uint16_t)buffer[offset] |
                      ((uint16_t)buffer[offset + 1U] << 8U));
}

static uint32_t settings_read_u32(const uint8_t *buffer, size_t offset)
{
    return (uint32_t)buffer[offset] |
           ((uint32_t)buffer[offset + 1U] << 8U) |
           ((uint32_t)buffer[offset + 2U] << 16U) |
           ((uint32_t)buffer[offset + 3U] << 24U);
}

static void settings_write_u16(uint8_t *buffer,
                               size_t offset,
                               uint16_t value)
{
    buffer[offset] = (uint8_t)(value & 0xFFU);
    buffer[offset + 1U] = (uint8_t)(value >> 8U);
}

static void settings_write_u32(uint8_t *buffer,
                               size_t offset,
                               uint32_t value)
{
    buffer[offset] = (uint8_t)(value & 0xFFU);
    buffer[offset + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[offset + 2U] = (uint8_t)((value >> 16U) & 0xFFU);
    buffer[offset + 3U] = (uint8_t)(value >> 24U);
}

static uint32_t settings_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t index;

    for (index = 0U; index < length; index++) {
        uint8_t bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

static bool settings_string_valid(const char *text,
                                  size_t capacity,
                                  bool allow_dot,
                                  bool allow_hyphen,
                                  bool allow_colon)
{
    size_t index;

    if (text == NULL || capacity < 2U || text[0] == '\0') {
        return false;
    }
    for (index = 0U; index < capacity; index++) {
        const unsigned char value = (unsigned char)text[index];

        if (value == '\0') {
            return index > 0U;
        }
        if (isalnum(value) == 0 &&
            !(allow_dot && value == '.') &&
            !(allow_hyphen && value == '-') &&
            !(allow_colon && value == ':')) {
            return false;
        }
    }
    return false;
}

static bool settings_uart_valid(const settings_uart_config_t *config)
{
    static const uint32_t supported_baud_rates[] = {
        9600U, 19200U, 38400U, 57600U, 115200U
    };
    size_t index;
    bool baud_supported = false;

    if (config == NULL ||
        (config->data_bits != 7U && config->data_bits != 8U) ||
        config->parity > 2U ||
        (config->stop_bits != 1U && config->stop_bits != 2U)) {
        return false;
    }
    for (index = 0U;
         index < sizeof(supported_baud_rates) / sizeof(supported_baud_rates[0]);
         index++) {
        if (config->baud_rate == supported_baud_rates[index]) {
            baud_supported = true;
            break;
        }
    }
    return baud_supported;
}

static bool settings_ipv4_valid(const settings_ethernet_config_t *config)
{
    uint32_t address;
    uint32_t mask;
    uint32_t gateway;
    uint32_t inverse_mask;
    uint32_t host;

    if (config == NULL) {
        return false;
    }
    address = ((uint32_t)config->address[0] << 24U) |
              ((uint32_t)config->address[1] << 16U) |
              ((uint32_t)config->address[2] << 8U) |
              (uint32_t)config->address[3];
    mask = ((uint32_t)config->netmask[0] << 24U) |
           ((uint32_t)config->netmask[1] << 16U) |
           ((uint32_t)config->netmask[2] << 8U) |
           (uint32_t)config->netmask[3];
    gateway = ((uint32_t)config->gateway[0] << 24U) |
              ((uint32_t)config->gateway[1] << 16U) |
              ((uint32_t)config->gateway[2] << 8U) |
              (uint32_t)config->gateway[3];
    inverse_mask = ~mask;
    if (mask == 0U || (inverse_mask & (inverse_mask + 1U)) != 0U ||
        address == 0U || address == 0xFFFFFFFFUL ||
        gateway == 0U || gateway == 0xFFFFFFFFUL) {
        return false;
    }
    host = address & inverse_mask;
    if (host == 0U || host == inverse_mask ||
        (address & mask) != (gateway & mask) ||
        (gateway & inverse_mask) == 0U ||
        (gateway & inverse_mask) == inverse_mask) {
        return false;
    }
    return true;
}

void settings_confirm_network(void)
{
    taskENTER_CRITICAL();
    settings_network_confirmed = true;
    taskEXIT_CRITICAL();
}

bool settings_network_is_confirmed(void)
{
    bool confirmed;

    taskENTER_CRITICAL();
    confirmed = settings_network_confirmed;
    taskEXIT_CRITICAL();
    return confirmed;
}

bool settings_validate(const settings_config_t *config)
{
    size_t index;

    if (config == NULL ||
        !settings_string_valid(config->mqtt.host,
                               sizeof(config->mqtt.host), true, true, false) ||
        !settings_string_valid(config->mqtt.client_id,
                               sizeof(config->mqtt.client_id),
                               true, true, true) ||
        config->mqtt.port == 0U ||
        config->mqtt.keep_alive_seconds == 0U ||
        config->mqtt.keep_alive_seconds > 3600U ||
        (!config->ethernet.dhcp_enabled &&
         !settings_ipv4_valid(&config->ethernet))) {
        return false;
    }
    for (index = 0U; index < SETTINGS_UART_COUNT; index++) {
        if (!settings_uart_valid(&config->uart[index])) {
            return false;
        }
    }
    return true;
}

static void settings_defaults(settings_config_t *config)
{
    size_t index;

    memset(config, 0, sizeof(*config));
    config->ethernet.dhcp_enabled = true;
    config->mqtt.enabled = true;
    (void)memcpy(config->mqtt.host, "broker.emqx.io",
                 sizeof("broker.emqx.io"));
    config->mqtt.port = SETTINGS_DEFAULT_MQTT_PORT;
    (void)memcpy(config->mqtt.client_id, "synaptix-dtu-h563",
                 sizeof("synaptix-dtu-h563"));
    config->mqtt.keep_alive_seconds = SETTINGS_DEFAULT_MQTT_KEEP_ALIVE;
    for (index = 0U; index < SETTINGS_UART_COUNT; index++) {
        config->uart[index].baud_rate = SETTINGS_DEFAULT_UART_BAUD;
        config->uart[index].data_bits = SETTINGS_DEFAULT_UART_DATA_BITS;
        config->uart[index].parity = SETTINGS_DEFAULT_UART_PARITY;
        config->uart[index].stop_bits = SETTINGS_DEFAULT_UART_STOP_BITS;
    }
}

static void settings_encode(const settings_config_t *config,
                            uint32_t generation,
                            uint8_t record[SETTINGS_RECORD_SIZE])
{
    size_t offset = SETTINGS_UART_OFFSET;
    size_t index;

    memset(record, 0, SETTINGS_RECORD_SIZE);
    settings_write_u32(record, 0U, SETTINGS_RECORD_MAGIC);
    settings_write_u16(record, 4U, SETTINGS_SCHEMA_VERSION);
    settings_write_u32(record, 6U, generation);
    record[10U] = config->ethernet.dhcp_enabled ? 1U : 0U;
    (void)memcpy(&record[11U], config->ethernet.address, 4U);
    (void)memcpy(&record[15U], config->ethernet.netmask, 4U);
    (void)memcpy(&record[19U], config->ethernet.gateway, 4U);
    record[22U] = (uint8_t)((config->mqtt.enabled ? 1U : 0U) |
                            (config->mqtt.tls_enabled ? 2U : 0U));
    (void)memcpy(&record[SETTINGS_MQTT_HOST_OFFSET],
                 config->mqtt.host, sizeof(config->mqtt.host));
    settings_write_u16(record, 151U, config->mqtt.port);
    (void)memcpy(&record[SETTINGS_MQTT_CLIENT_ID_OFFSET],
                 config->mqtt.client_id, sizeof(config->mqtt.client_id));
    settings_write_u16(record, 217U, config->mqtt.keep_alive_seconds);
    for (index = 0U; index < SETTINGS_UART_COUNT; index++) {
        settings_write_u32(record, offset, config->uart[index].baud_rate);
        record[offset + 4U] = config->uart[index].data_bits;
        record[offset + 5U] = config->uart[index].parity;
        record[offset + 6U] = config->uart[index].stop_bits;
        offset += 8U;
    }
    settings_write_u32(record, SETTINGS_RECORD_DATA_SIZE,
                       settings_crc32(record, SETTINGS_RECORD_DATA_SIZE));
}

static bool settings_decode(const uint8_t record[SETTINGS_RECORD_SIZE],
                            settings_config_t *config,
                            uint32_t *generation)
{
    size_t offset = SETTINGS_UART_OFFSET;
    size_t index;

    if (settings_read_u32(record, 0U) != SETTINGS_RECORD_MAGIC ||
        settings_read_u16(record, 4U) != SETTINGS_SCHEMA_VERSION ||
        settings_read_u32(record, SETTINGS_RECORD_DATA_SIZE) !=
            settings_crc32(record, SETTINGS_RECORD_DATA_SIZE) ||
        record[10U] > 1U || (record[22U] & 0xFCU) != 0U) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    *generation = settings_read_u32(record, 6U);
    config->ethernet.dhcp_enabled = record[10U] != 0U;
    (void)memcpy(config->ethernet.address, &record[11U], 4U);
    (void)memcpy(config->ethernet.netmask, &record[15U], 4U);
    (void)memcpy(config->ethernet.gateway, &record[19U], 4U);
    config->mqtt.enabled = (record[22U] & 0x01U) != 0U;
    config->mqtt.tls_enabled = (record[22U] & 0x02U) != 0U;
    (void)memcpy(config->mqtt.host, &record[SETTINGS_MQTT_HOST_OFFSET],
                 sizeof(config->mqtt.host));
    config->mqtt.host[sizeof(config->mqtt.host) - 1U] = '\0';
    config->mqtt.port = settings_read_u16(record, 151U);
    (void)memcpy(config->mqtt.client_id,
                 &record[SETTINGS_MQTT_CLIENT_ID_OFFSET],
                 sizeof(config->mqtt.client_id));
    config->mqtt.client_id[sizeof(config->mqtt.client_id) - 1U] = '\0';
    config->mqtt.keep_alive_seconds = settings_read_u16(record, 217U);
    for (index = 0U; index < SETTINGS_UART_COUNT; index++) {
        config->uart[index].baud_rate = settings_read_u32(record, offset);
        config->uart[index].data_bits = record[offset + 4U];
        config->uart[index].parity = record[offset + 5U];
        config->uart[index].stop_bits = record[offset + 6U];
        offset += 8U;
    }
    return settings_validate(config);
}

static bool settings_read_slot(uint8_t slot,
                               settings_config_t *config,
                               uint32_t *generation)
{
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint8_t trailing_byte;
    size_t total_read = 0U;
    int32_t file;
    bool success = false;

    file = file_system_open(SETTINGS_RECORD_PATHS[slot], O_RDONLY, 0U);
    if (file < 0) {
        return false;
    }
    while (total_read < sizeof(record)) {
        const int32_t read_count =
            file_system_read(file, &record[total_read],
                             (uint32_t)(sizeof(record) - total_read));
        if (read_count <= 0) {
            break;
        }
        total_read += (size_t)read_count;
    }
    if (total_read == sizeof(record) &&
        file_system_read(file, &trailing_byte, 1U) == 0) {
        success = settings_decode(record, config, generation);
    }
    if (file_system_close(file) != 0) {
        success = false;
    }
    return success;
}

static bool settings_slot_exists(uint8_t slot, bool *exists)
{
    struct stat status;

    if (exists == NULL) {
        return false;
    }
    errno = 0;
    if (file_system_stat(SETTINGS_RECORD_PATHS[slot], &status) == 0) {
        *exists = true;
        return true;
    }
    if (errno == ENOENT) {
        *exists = false;
        return true;
    }
    return false;
}

static bool settings_write_slot(uint8_t slot,
                                const settings_config_t *config,
                                uint32_t generation)
{
    uint8_t record[SETTINGS_RECORD_SIZE];
    size_t total_written = 0U;
    int32_t file;
    bool success = false;

    settings_encode(config, generation, record);
    file = file_system_open(SETTINGS_RECORD_PATHS[slot],
                            O_WRONLY | O_CREAT | O_TRUNC, 0U);
    if (file < 0) {
        return false;
    }
    while (total_written < sizeof(record)) {
        const int32_t write_count =
            file_system_write(file, &record[total_written],
                              (uint32_t)(sizeof(record) - total_written));
        if (write_count <= 0) {
            break;
        }
        total_written += (size_t)write_count;
    }
    if (total_written == sizeof(record) && file_system_sync(file) == 0) {
        success = true;
    }
    if (file_system_close(file) != 0) {
        success = false;
    }
    return success;
}

static void settings_task(void *argument)
{
    settings_save_request_t request;

    (void)argument;
    for (;;) {
        if (xQueueReceive(settings_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!file_system_port_is_mounted() ||
            !settings_write_slot((uint8_t)((settings_generation + 1U) & 1U),
                                 &request.config,
                                 settings_generation + 1U)) {
            taskENTER_CRITICAL();
            settings_save_state = SETTINGS_SAVE_FAILED;
            settings_save_active = false;
            taskEXIT_CRITICAL();
            continue;
        }
        taskENTER_CRITICAL();
        settings_config = request.config;
        settings_generation++;
        settings_save_state = SETTINGS_SAVE_SUCCEEDED;
        settings_save_active = false;
        taskEXIT_CRITICAL();
    }
}

settings_result_t settings_init(void)
{
    settings_config_t first_config;
    settings_config_t second_config;
    uint32_t first_generation = 0U;
    uint32_t second_generation = 0U;
    bool first_exists = false;
    bool second_exists = false;
    bool first_valid = false;
    bool second_valid = false;
    settings_result_t result = SETTINGS_RESULT_OK;

    if (settings_initialized) {
        return SETTINGS_RESULT_OK;
    }
    settings_defaults(&settings_config);
    settings_generation = 0U;
    if (file_system_port_is_mounted()) {
        if (!settings_slot_exists(0U, &first_exists) ||
            !settings_slot_exists(1U, &second_exists)) {
            result = SETTINGS_RESULT_STORAGE_ERROR;
        } else {
            first_valid = first_exists &&
                          settings_read_slot(0U, &first_config,
                                             &first_generation);
            second_valid = second_exists &&
                           settings_read_slot(1U, &second_config,
                                              &second_generation);
            if ((first_exists && !first_valid) ||
                (second_exists && !second_valid)) {
                result = SETTINGS_RESULT_STORAGE_ERROR;
            }
        }
    } else {
        result = SETTINGS_RESULT_STORAGE_ERROR;
    }
    if (first_valid &&
        (!second_valid ||
         (int32_t)(first_generation - second_generation) > 0)) {
        settings_config = first_config;
        settings_generation = first_generation;
    } else if (second_valid) {
        settings_config = second_config;
        settings_generation = second_generation;
    }
    settings_queue = xQueueCreateStatic(
        SETTINGS_QUEUE_LENGTH, sizeof(settings_save_request_t),
        settings_queue_buffer, &settings_queue_storage);
    if (settings_queue == NULL ||
        xTaskCreateStatic(settings_task, "settings", SETTINGS_TASK_STACK_SIZE,
                          NULL, SETTINGS_TASK_PRIORITY,
                          settings_task_stack,
                          &settings_task_control_block) == NULL) {
        settings_queue = NULL;
        return SETTINGS_RESULT_STORAGE_ERROR;
    }
    settings_save_state = SETTINGS_SAVE_IDLE;
    settings_save_active = false;
    settings_initialized = true;
    return result;
}

bool settings_get_config(settings_config_t *config)
{
    if (config == NULL || !settings_initialized) {
        return false;
    }
    taskENTER_CRITICAL();
    *config = settings_config;
    taskEXIT_CRITICAL();
    return true;
}

settings_result_t settings_request_save(const settings_config_t *config)
{
    settings_save_request_t request;
    settings_save_state_t previous_state;

    if (!settings_initialized) {
        return SETTINGS_RESULT_NOT_INITIALIZED;
    }
    if (!settings_validate(config)) {
        return SETTINGS_RESULT_INVALID;
    }
    if (!file_system_port_is_mounted()) {
        return SETTINGS_RESULT_STORAGE_ERROR;
    }
    request.config = *config;
    taskENTER_CRITICAL();
    if (settings_save_active) {
        taskEXIT_CRITICAL();
        return SETTINGS_RESULT_BUSY;
    }
    previous_state = settings_save_state;
    settings_save_active = true;
    settings_save_state = SETTINGS_SAVE_PENDING;
    taskEXIT_CRITICAL();
    if (xQueueSend(settings_queue, &request, 0U) != pdTRUE) {
        taskENTER_CRITICAL();
        settings_save_active = false;
        settings_save_state = previous_state;
        taskEXIT_CRITICAL();
        return SETTINGS_RESULT_BUSY;
    }
    return SETTINGS_RESULT_OK;
}

bool settings_get_save_state(settings_save_state_t *state)
{
    if (state == NULL || !settings_initialized) {
        return false;
    }
    taskENTER_CRITICAL();
    *state = settings_save_state;
    taskEXIT_CRITICAL();
    return true;
}