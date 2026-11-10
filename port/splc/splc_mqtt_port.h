#ifndef SPLC_MQTT_PORT_H
#define SPLC_MQTT_PORT_H
/*
 * splc_mqtt_port.h - product port (outside the simple_plc library)
 *
 * The library renders MQTT topics/payloads and hands them to one non-blocking publish callback.
 * This port queues publishes and broker configurations for the host MQTT task, then queues status
 * back to the engine task so PLC services are only called from their owning task.
 *
 * The connection status goes the other way: the host calls splc_mqtt_set_status() (plc_mqtt.h).
 * Compiled empty unless SPLC_MQTT_ENABLE = 1.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "splc_opt.h"
#if SPLC_MQTT_ENABLE
#include "plc_mqtt.h"

#define SPLC_MQTT_PORT_TOPIC_SIZE (SPLC_MQTT_MAX_TOPIC_CHARS + 1U)

typedef struct {
    splc_mqtt_broker_t broker;
    bool app_configured;
    uint32_t generation;
} splc_mqtt_port_config_t;

typedef struct {
    char topic[SPLC_MQTT_PORT_TOPIC_SIZE];
    char payload[SPLC_MQTT_PAYLOAD_MAX];
    size_t payload_len;
    uint8_t qos;
    bool retain;
} splc_mqtt_port_message_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Same contract as splc_mqtt_publish_fn: runs in the engine task, must not block,
 * payload is NUL-terminated and reused after the call. Return 0 = accepted, < 0 = busy/failed. */
typedef int (*splc_mqtt_port_publish_fn)(const char *topic, const char *payload, size_t len,
                                         uint8_t qos, bool retain, void *ctx);

/* Optional override for the default bounded-queue backend. */
void splc_mqtt_port_set_backend(splc_mqtt_port_publish_fn fn, void *ctx);

/* Register the forwarder with the library. Call before plc_engine_init(), from the engine task. */
void splc_mqtt_port_init(void);
void splc_mqtt_port_process(void);

#if SPLC_MQTT_ENABLE
/* Engine-task pump: transfer broker snapshots and host status updates without cross-task calls
 * into the single-threaded PLC services. */
/* Host MQTT task APIs. These are bounded queue operations; no PLC service is called here. */
bool splc_mqtt_port_take_config(splc_mqtt_port_config_t *out, uint32_t timeout_ms);
bool splc_mqtt_port_take_publish(splc_mqtt_port_message_t *out, uint32_t timeout_ms);
void splc_mqtt_port_discard_publishes(void);
void splc_mqtt_port_report_status(const splc_mqtt_status_t *status);
#endif

#ifdef __cplusplus
}
#endif
#endif /* SPLC_MQTT_PORT_H */