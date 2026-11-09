#include "rf_app.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "sx_uart.h"
#include "xlink_zb44.h"

#define MAX_XLINK_DEV (32U)
#define MAX_CRC_ERR_NUM (20U)
#define RF_APP_UART_TX_TIMEOUT pdMS_TO_TICKS(1000)
#define RF_APP_ZIGBEE_UART_PORT (SX_UART_PORT_UART9)
#define RF_TOPOLOGY_SYNC_mS (10000U)
#define RF_TOPOLOGY_FIELD_COUNT (11U)
#define RF_TOPOLOGY_FIELD_CAPACITY (RF_TOPOLOGY_FIELD_COUNT + 1U)
#define RF_TOPOLOGY_ACK_BUFFER_SIZE (16U)
#define RF_TOPOLOGY_CRC_MODULUS (256U)

static const char RF_APP_TAG[] = "RF_APP";

typedef struct ZigbeeTopology{
    XLink_ZB44_t xlink_dev[MAX_XLINK_DEV];
    ZigbeeMesh_t *zigbee;
    uint16_t crc_err_num;
    uint8_t num_dev_connect;
    uint32_t tick;
    uint32_t dev_id;
    uint8_t rx_buff[SX_UART_RX_BUFFER_MAX];
    uint32_t rx_index;
    bool rx_discarding;
    bool rx_awaiting_lf;
}ZigbeeTopology_t;



static ZigbeeMesh_t rf_app_zigbee;
static ZigbeeDriver_t rf_app_zigbee_driver = {
    .p_write = NULL,
    .p_read = NULL,
    .available = NULL,
};

static ZigbeeTopology_t zigbee_topology = {
    .crc_err_num = 0,
    .num_dev_connect = 0,
    .zigbee = &rf_app_zigbee,
    .tick = 0,
    .dev_id = 0,
    .rx_index = 0,
    .rx_discarding = false,
    .rx_awaiting_lf = false
};

static bool rf_app_initialized;

static void Zigbee_Topology_receive_char(uint8_t c);
static bool rf_app_topology_parse_line(void);
static bool rf_app_topology_parse_uint32(const char *text, uint32_t *value);
static void rf_app_topology_send_ack(uint8_t id);

static void rf_app_uart_write(uint8_t *buffer, uint32_t length)
{
    int32_t written;

    written = sx_uart_write(RF_APP_ZIGBEE_UART_PORT, buffer, length,
                            RF_APP_UART_TX_TIMEOUT);
    if (written != (int32_t)length) {
        LOGE(RF_APP_TAG, "Zigbee UART write failed");
    }
}

static void rf_app_uart_read(uint8_t *buffer, uint32_t length)
{
    int32_t received;

    received = sx_uart_read(RF_APP_ZIGBEE_UART_PORT, buffer, length, 10U);
    if (received != (int32_t)length) {
        LOGE(RF_APP_TAG, "Zigbee UART read failed");
    }
}

static uint32_t rf_app_uart_available(void)
{
    int32_t available = sx_uart_get_rx_bytes_available(RF_APP_ZIGBEE_UART_PORT);

    if (available < 0) {
        LOGE(RF_APP_TAG, "Unable to query Zigbee UART RX data");
        return 0U;
    }
    return (uint32_t)available;
}

static bool rf_app_can_start_operation(void)
{
    if (!rf_app_initialized) {
        LOGE(RF_APP_TAG, "RF application is not initialized");
        return false;
    }
    if (rf_app_zigbee.event.event != ZB_EVENT_NONE) {
        LOGW(RF_APP_TAG, "Zigbee operation is already in progress");
        return false;
    }
    return true;
}

bool rf_app_init(void)
{
    if (rf_app_initialized) {
        return true;
    }
    if (!sx_uart_is_installed(RF_APP_ZIGBEE_UART_PORT)) {
        LOGE(RF_APP_TAG, "Zigbee UART is not installed");
        return false;
    }

    rf_app_zigbee_driver.p_write = rf_app_uart_write;
    rf_app_zigbee_driver.p_read = rf_app_uart_read;
    rf_app_zigbee_driver.available = rf_app_uart_available;
    zigbee_init(&rf_app_zigbee, &rf_app_zigbee_driver);
    rf_app_initialized = true;
    for (size_t i = 0U; i < MAX_XLINK_DEV; i++) {
        Xlink_ZB44_Init(&zigbee_topology.xlink_dev[i]);
    }
    return true;
}

void rf_app_process(uint32_t elapsed_ms)
{
    if (!rf_app_initialized) {
        return;
    }
    zigbee_poll(&rf_app_zigbee, elapsed_ms);
    if (rf_app_is_busy()) {
        return;
    }
    if(zigbee_topology.crc_err_num >= MAX_CRC_ERR_NUM){
        zigbee_reset_module(&rf_app_zigbee,NULL,NULL);
        zigbee_topology.crc_err_num = 0;
    }
    zigbee_topology.tick += elapsed_ms;
    Xlink_ZB44_Poll(&zigbee_topology.xlink_dev[zigbee_topology.dev_id],elapsed_ms*MAX_XLINK_DEV);
    if (zigbee_topology.tick % RF_TOPOLOGY_SYNC_mS == 0U) {

    }
    uint32_t len = rf_app_uart_available();
    for (uint32_t i = 0U; i < len; i++) {
        uint8_t c = 0;
        rf_app_uart_read(&c,1);
        Zigbee_Topology_receive_char(c);
    }
}

bool rf_app_is_initialized(void)
{
    return rf_app_initialized;
}

bool rf_app_is_busy(void)
{
    return rf_app_initialized &&
           rf_app_zigbee.event.event != ZB_EVENT_NONE;
}

bool rf_app_has_parameters(void)
{
    return rf_app_initialized && rf_app_zigbee.parameters_valid;
}

const ZigbeeParameter_t *rf_app_get_parameters(void)
{
    if (!rf_app_initialized) {
        return NULL;
    }
    return &rf_app_zigbee.param;
}

bool rf_app_connect(ZB_Callback_Func callback, void *arg)
{
    if (!rf_app_can_start_operation()) {
        return false;
    }
    zigbee_connect(&rf_app_zigbee, callback, arg);
    return true;
}

bool rf_app_read_parameters(ZB_Callback_Func callback, void *arg)
{
    if (!rf_app_can_start_operation()) {
        return false;
    }
    zigbee_read_module(&rf_app_zigbee, callback, arg);
    return true;
}

bool rf_app_write_parameters(const ZigbeeParameter_t *parameters,
                             ZB_Callback_Func callback,
                             void *arg)
{
    if (parameters == NULL || !rf_app_can_start_operation()) {
        return false;
    }
    rf_app_zigbee.param = *parameters;
    zigbee_write_module(&rf_app_zigbee, callback, arg);
    return true;
}

bool rf_app_reset(ZB_Callback_Func callback, void *arg)
{
    if (!rf_app_can_start_operation()) {
        return false;
    }
    zigbee_reset_module(&rf_app_zigbee, callback, arg);
    return true;
}

bool rf_app_sleep(ZB_Callback_Func callback, void *arg)
{
    if (!rf_app_can_start_operation()) {
        return false;
    }
    zigbee_enter_low_power(&rf_app_zigbee, callback, arg);
    return true;
}

static void Zigbee_Topology_receive_char(uint8_t c)
{
    if (zigbee_topology.rx_discarding) {
        if (c == (uint8_t)'\n') {
            zigbee_topology.rx_discarding = false;
            zigbee_topology.rx_awaiting_lf = false;
            zigbee_topology.rx_index = 0U;
        }
        return;
    }

    if (zigbee_topology.rx_awaiting_lf) {
        if (c == (uint8_t)'\n') {
            zigbee_topology.rx_awaiting_lf = false;
        } else {
            zigbee_topology.rx_index = 0U;
            zigbee_topology.rx_awaiting_lf = false;
            if (c != (uint8_t)':') {
                return;
            }
            zigbee_topology.rx_buff[0] = c;
            zigbee_topology.rx_index = 1U;
            return;
        }

        zigbee_topology.rx_buff[zigbee_topology.rx_index] = '\0';
        if (zigbee_topology.rx_index > 0U &&
            !rf_app_topology_parse_line()) {
            zigbee_topology.crc_err_num++;
            LOGW(RF_APP_TAG, "Invalid Zigbee topology frame");
        }
        zigbee_topology.rx_index = 0U;
        return;
    }

    if (zigbee_topology.rx_index == 0U) {
        if (c == (uint8_t)':') {
            zigbee_topology.rx_buff[0] = c;
            zigbee_topology.rx_index = 1U;
        }
        return;
    }

    if (c == (uint8_t)'\r') {
        zigbee_topology.rx_awaiting_lf = true;
        return;
    }
    if (c == (uint8_t)'\n' || c == 0U || c < 0x20U || c > 0x7EU) {
        zigbee_topology.rx_discarding = true;
        zigbee_topology.rx_index = 0U;
        LOGW(RF_APP_TAG, "Non-ASCII byte in Zigbee topology frame");
        return;
    }

    if (zigbee_topology.rx_index >=
        (sizeof(zigbee_topology.rx_buff) - 1U)) {
        zigbee_topology.rx_index = 0U;
        zigbee_topology.rx_discarding = true;
        LOGW(RF_APP_TAG, "Zigbee topology frame exceeds RX buffer");
        return;
    }

    zigbee_topology.rx_buff[zigbee_topology.rx_index] = c;
    zigbee_topology.rx_index++;
}

static bool rf_app_topology_parse_line(void)
{
    char *fields[RF_TOPOLOGY_FIELD_CAPACITY];
    size_t field_count = 1U;
    size_t index;
    size_t line_start = 1U;
    uint32_t values[RF_TOPOLOGY_FIELD_COUNT];
    uint16_t checksum = 0U;
    uint8_t expected_checksum;
    uint8_t id;
    bool is_event;
    XLink_ZB44_t *device;

    if (zigbee_topology.rx_buff[0] != (uint8_t)':') {
        return false;
    }

    fields[0] = (char *)&zigbee_topology.rx_buff[line_start];
    for (index = line_start; index < zigbee_topology.rx_index; index++) {
        if (zigbee_topology.rx_buff[index] == (uint8_t)',') {
            if (field_count >= RF_TOPOLOGY_FIELD_CAPACITY) {
                return false;
            }
            zigbee_topology.rx_buff[index] = '\0';
            fields[field_count] =
                (char *)&zigbee_topology.rx_buff[index + 1U];
            field_count++;
        }
    }

    if (field_count == RF_TOPOLOGY_FIELD_CAPACITY &&
        fields[field_count - 1U][0] == '\0') {
        field_count--;
    }
    if (field_count != RF_TOPOLOGY_FIELD_COUNT ||
        (strcmp(fields[1], "PING") != 0 &&
         strcmp(fields[1], "EVENT") != 0)) {
        return false;
    }
    is_event = strcmp(fields[1], "EVENT") == 0;

    for (index = 0U; index < RF_TOPOLOGY_FIELD_COUNT; index++) {
        if (index == 1U) {
            values[index] = 0U;
        } else if (!rf_app_topology_parse_uint32(fields[index],
                                                 &values[index])) {
            return false;
        }
        if (index != 1U && index != (RF_TOPOLOGY_FIELD_COUNT - 1U)) {
            checksum = (uint16_t)(
                (checksum + (uint16_t)(values[index] & 0xFFU)) %
                RF_TOPOLOGY_CRC_MODULUS);
        }
    }

    expected_checksum = (uint8_t)(checksum % RF_TOPOLOGY_CRC_MODULUS);
    if (values[0] >= MAX_XLINK_DEV ||
        values[RF_TOPOLOGY_FIELD_COUNT - 1U] != expected_checksum) {
        return false;
    }
    for (index = 0U; index < XLINK_NUMBER_INPUT; index++) {
        if (values[2U + index] > 1U) {
            return false;
        }
    }

    id = values[0];
    device = &zigbee_topology.xlink_dev[id];
    device->id = id;
    device->isConnect = true;
    device->timeout = 0U;
    for (index = 0U; index < XLINK_NUMBER_INPUT; index++) {
        device->input[index] = values[2U + index] != 0U;
    }
    for (index = 0U; index < XLINK_NUMBER_COUNTER; index++) {
        device->counter[index] = values[6U + index];
    }

    if (is_event) {
        rf_app_topology_send_ack(id);
    }
    return true;
}

static bool rf_app_topology_parse_uint32(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    size_t index;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return false;
    }

    for (index = 0U; text[index] != '\0'; index++) {
        uint32_t digit;

        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        digit = (uint32_t)(text[index] - '0');
        if (parsed > (UINT32_MAX - digit) / 10U) {
            return false;
        }
        parsed = parsed * 10U + digit;
    }

    *value = parsed;
    return true;
}

static void rf_app_topology_send_ack(uint8_t id)
{
    char ack[RF_TOPOLOGY_ACK_BUFFER_SIZE];
    int32_t length;

    if (rf_app_zigbee_driver.p_write == NULL) {
        LOGE(RF_APP_TAG, "Cannot ACK topology event: Zigbee driver unavailable");
        return;
    }

    length = (int32_t)snprintf(ack, sizeof(ack), "%" PRIu32 ",ACK,\r\n",
                               (uint32_t)id);
    if (length <= 0 || (size_t)length >= sizeof(ack)) {
        LOGE(RF_APP_TAG, "Failed to format topology event ACK");
        return;
    }

    rf_app_zigbee_driver.p_write((uint8_t *)ack, (uint32_t)length);
}