#ifndef MQTT_TEST_MOCKS_H
#define MQTT_TEST_MOCKS_H

#include <stdbool.h>
#include <stdint.h>

#include "core_mqtt.h"
#include "network_posix.h"

typedef enum
{
    MQTT_TEST_CONNACK_SUCCESS,
    MQTT_TEST_CONNACK_REFUSED,
    MQTT_TEST_CONNACK_TIMEOUT
} MQTTTestConnackMode_t;

extern MQTTTestConnackMode_t mqtt_test_connack_mode;
extern int mqtt_test_network_connect_result;
extern uint32_t mqtt_test_network_timeout_ms;
extern uint32_t mqtt_test_network_disconnect_count;
extern uint32_t mqtt_test_receive_count;
extern uint8_t mqtt_test_last_connect_packet_type;
extern bool mqtt_test_receive_timeout_configured;

void mqtt_test_reset(void);

#endif
