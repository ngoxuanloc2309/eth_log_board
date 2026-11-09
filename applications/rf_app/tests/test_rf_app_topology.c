#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../rf_app.c"

static char test_ack[RF_TOPOLOGY_ACK_BUFFER_SIZE];
static size_t test_ack_length;
static uint32_t test_ack_count;

static void test_driver_write(uint8_t *buffer, uint32_t length)
{
    assert(buffer != NULL);
    assert((size_t)length < sizeof(test_ack));
    memcpy(test_ack, buffer, length);
    test_ack[length] = '\0';
    test_ack_length = length;
    test_ack_count++;
}

static void test_set_frame(const char *frame)
{
    size_t length = strlen(frame);

    assert(length < sizeof(zigbee_topology.rx_buff));
    memcpy(zigbee_topology.rx_buff, frame, length);
    zigbee_topology.rx_buff[length] = '\0';
    zigbee_topology.rx_index = (uint32_t)length;
}

static void test_receive_frame(const char *frame)
{
    size_t index;
    size_t length = strlen(frame);

    for (index = 0U; index < length; index++) {
        Zigbee_Topology_receive_char((uint8_t)frame[index]);
    }
}

static void test_reset_topology(void)
{
    memset(&zigbee_topology, 0, sizeof(zigbee_topology));
    memset(test_ack, 0, sizeof(test_ack));
    test_ack_length = 0U;
    test_ack_count = 0U;
    rf_app_zigbee_driver.p_write = test_driver_write;
}

static void test_valid_ping_updates_device(void)
{
    XLink_ZB44_t *device;

    test_reset_topology();
    test_set_frame(":2,PING,1,0,1,0,10,20,30,40,104,");

    assert(rf_app_topology_parse_line());

    device = &zigbee_topology.xlink_dev[2U];
    assert(device->id == 2U);
    assert(device->isConnect);
    assert(device->timeout == 0U);
    assert(device->input[0]);
    assert(!device->input[1]);
    assert(device->input[2]);
    assert(!device->input[3]);
    assert(device->counter[0] == 10U);
    assert(device->counter[1] == 20U);
    assert(device->counter[2] == 30U);
    assert(device->counter[3] == 40U);
    assert(test_ack_count == 0U);
}

static void test_valid_event_sends_ack(void)
{
    test_reset_topology();
    test_set_frame(":3,EVENT,0,1,0,1,1,2,3,4,15,");

    assert(rf_app_topology_parse_line());
    assert(test_ack_count == 1U);
    assert(test_ack_length == strlen("3,ACK,\r\n"));
    assert(strcmp(test_ack, "3,ACK,\r\n") == 0);
    assert(zigbee_topology.xlink_dev[3U].counter[3] == 4U);
}

static void test_accepts_uint32_counters(void)
{
    XLink_ZB44_t *device;

    test_reset_topology();
    test_set_frame(":0,PING,0,0,0,0,4294967295,0,1,2,2,");

    assert(rf_app_topology_parse_line());

    device = &zigbee_topology.xlink_dev[0U];
    assert(device->counter[0] == UINT32_MAX);
    assert(device->counter[1] == 0U);
    assert(device->counter[2] == 1U);
    assert(device->counter[3] == 2U);
}

static void test_rejects_bad_checksum_without_mutation(void)
{
    XLink_ZB44_t *device;

    test_reset_topology();
    device = &zigbee_topology.xlink_dev[1U];
    device->id = 9U;
    device->input[0] = true;
    device->counter[0] = 77U;
    test_set_frame(":1,EVENT,0,0,0,0,1,2,3,4,99,");

    assert(!rf_app_topology_parse_line());
    assert(device->id == 9U);
    assert(device->input[0]);
    assert(device->counter[0] == 77U);
    assert(test_ack_count == 0U);
}

static void test_rejects_invalid_fields(void)
{
    test_reset_topology();
    test_set_frame(":1,EVENT,2,0,0,0,1,2,3,4,13,");
    assert(!rf_app_topology_parse_line());
    assert(test_ack_count == 0U);

    test_set_frame(":32,PING,0,0,0,0,1,2,3,4,42,");
    assert(!rf_app_topology_parse_line());

    test_set_frame(":1,PING,0,0,0,0,4294967296,2,3,4,10,");
    assert(!rf_app_topology_parse_line());
}

static void test_accepts_frame_without_trailing_comma(void)
{
    test_reset_topology();
    test_set_frame(":4,PING,0,0,0,0,1,2,3,4,14");

    assert(rf_app_topology_parse_line());
    assert(zigbee_topology.xlink_dev[4U].counter[3] == 4U);
}

static void test_receive_requires_colon_and_crlf(void)
{
    static const char frame[] = ":5,EVENT,1,0,1,0,5,6,7,8,33,\r\n";
    size_t index;

    test_reset_topology();
    test_receive_frame("noise");
    for (index = 0U; index < (sizeof(frame) - 3U); index++) {
        Zigbee_Topology_receive_char((uint8_t)frame[index]);
    }
    assert(test_ack_count == 0U);
    assert(!zigbee_topology.xlink_dev[5U].isConnect);

    Zigbee_Topology_receive_char((uint8_t)'\r');
    assert(test_ack_count == 0U);
    Zigbee_Topology_receive_char((uint8_t)'\n');

    assert(test_ack_count == 1U);
    assert(strcmp(test_ack, "5,ACK,\r\n") == 0);
    assert(zigbee_topology.xlink_dev[5U].isConnect);
}

int main(void)
{
    test_valid_ping_updates_device();
    test_valid_event_sends_ack();
    test_accepts_uint32_counters();
    test_rejects_bad_checksum_without_mutation();
    test_rejects_invalid_fields();
    test_accepts_frame_without_trailing_comma();
    test_receive_requires_colon_and_crlf();
    return 0;
}

bool sx_uart_is_installed(uint32_t port)
{
    (void)port;
    return true;
}

int32_t sx_uart_write(uint32_t port,
                      const uint8_t *buffer,
                      size_t length,
                      uint32_t timeout_ticks)
{
    (void)port;
    (void)buffer;
    (void)timeout_ticks;
    return (int32_t)length;
}

int32_t sx_uart_read(uint32_t port,
                     uint8_t *buffer,
                     size_t length,
                     uint32_t timeout_ticks)
{
    (void)port;
    (void)buffer;
    (void)timeout_ticks;
    return (int32_t)length;
}

int32_t sx_uart_get_rx_bytes_available(uint32_t port)
{
    (void)port;
    return 0;
}

void zigbee_init(ZigbeeMesh_t *zigbee, ZigbeeDriver_t *driver)
{
    (void)zigbee;
    (void)driver;
}

void zigbee_poll(ZigbeeMesh_t *zigbee, uint32_t timestamp)
{
    (void)zigbee;
    (void)timestamp;
}

void zigbee_connect(ZigbeeMesh_t *zigbee, ZB_Callback_Func callback, void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}

void zigbee_reset_module(ZigbeeMesh_t *zigbee,
                         ZB_Callback_Func callback,
                         void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}

void zigbee_read_module(ZigbeeMesh_t *zigbee,
                        ZB_Callback_Func callback,
                        void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}

void zigbee_write_module(ZigbeeMesh_t *zigbee,
                         ZB_Callback_Func callback,
                         void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}

void zigbee_request_signal(ZigbeeMesh_t *zigbee,
                           ZB_Callback_Func callback,
                           void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}

void zigbee_enter_low_power(ZigbeeMesh_t *zigbee,
                            ZB_Callback_Func callback,
                            void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}

void zigbee_query_ed_pos(ZigbeeMesh_t *zigbee,
                         ZB_Callback_Func callback,
                         void *arg)
{
    (void)zigbee;
    (void)callback;
    (void)arg;
}
