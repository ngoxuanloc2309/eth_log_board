#include "splc_mqtt_port.h"
#include "splc_opt.h"

#if SPLC_MQTT_ENABLE

#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "splc_log.h"
#include "plc_mqtt.h"       /* splc_mqtt_register() */

#define MQTT_PUBLISH_QUEUE_LENGTH 4U

static StaticQueue_t s_config_queue_control;
static StaticQueue_t s_status_queue_control;
static StaticQueue_t s_publish_queue_control;
static splc_mqtt_port_config_t s_config_queue_storage[1];
static splc_mqtt_status_t s_status_queue_storage[1];
static splc_mqtt_port_message_t s_publish_queue_storage[MQTT_PUBLISH_QUEUE_LENGTH];
static QueueHandle_t s_config_queue;
static QueueHandle_t s_status_queue;
static QueueHandle_t s_publish_queue;
static uint32_t s_last_config_generation = UINT32_MAX;
static splc_mqtt_port_publish_fn s_backend;
static void                     *s_backend_ctx;

static TickType_t timeout_ticks(uint32_t timeout_ms)
{
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms != 0U && ticks == 0U) {
        ticks = 1U;
    }
    return ticks;
}

static int mqtt_queue_publish(const char *topic, const char *payload, size_t len,
                              uint8_t qos, bool retain, void *ctx)
{
    splc_mqtt_port_message_t message;
    const size_t topic_len = (topic != NULL) ? strlen(topic) : 0U;

    (void)ctx;
    if (s_publish_queue == NULL || topic_len == 0U ||
        topic_len > SPLC_MQTT_MAX_TOPIC_CHARS || payload == NULL ||
        len >= sizeof message.payload || qos > 2U) {
        return -1;
    }

    memset(&message, 0, sizeof message);
    memcpy(message.topic, topic, topic_len);
    memcpy(message.payload, payload, len);
    message.payload[len] = '\0';
    message.payload_len = len;
    message.qos = qos;
    message.retain = retain;
    return (xQueueSend(s_publish_queue, &message, 0U) == pdPASS) ? 0 : -1;
}

void splc_mqtt_port_set_backend(splc_mqtt_port_publish_fn fn, void *ctx)
{
    s_backend_ctx = ctx;
    s_backend     = fn;
}

static int mqtt_forward(const char *topic, const char *payload, size_t len,
                        uint8_t qos, bool retain, void *ctx)
{
    (void)ctx;
    if (s_backend == NULL) {
        return -1;      /* no MQTT client yet: the library retries after SPLC_MQTT_RETRY_MS */
    }
    return s_backend(topic, payload, len, qos, retain, s_backend_ctx);
}

void splc_mqtt_port_init(void)
{
    s_config_queue = xQueueCreateStatic(
        1U, sizeof(s_config_queue_storage[0]),
        (uint8_t *)s_config_queue_storage, &s_config_queue_control);
    s_status_queue = xQueueCreateStatic(
        1U, sizeof(s_status_queue_storage[0]),
        (uint8_t *)s_status_queue_storage, &s_status_queue_control);
    s_publish_queue = xQueueCreateStatic(
        MQTT_PUBLISH_QUEUE_LENGTH, sizeof(s_publish_queue_storage[0]),
        (uint8_t *)s_publish_queue_storage, &s_publish_queue_control);
    if (s_config_queue == NULL || s_status_queue == NULL || s_publish_queue == NULL) {
        log_error("MQTT_PORT", "failed to create MQTT port queues");
        return;
    }
    if (!splc_mqtt_register(mqtt_forward, NULL)) {
        log_error("MQTT_PORT", "splc_mqtt_register failed");
        return;
    }
    splc_mqtt_port_set_backend(mqtt_queue_publish, NULL);
    s_last_config_generation = UINT32_MAX;
}

void splc_mqtt_port_process(void)
{
    splc_mqtt_port_config_t config;
    splc_mqtt_status_t status;

    if (s_config_queue == NULL || s_status_queue == NULL) {
        return;
    }

    memset(&config, 0, sizeof config);
    splc_mqtt_get_broker(&config.broker, &config.app_configured,
                         &config.generation);
    if (config.generation != s_last_config_generation) {
        if (xQueueOverwrite(s_config_queue, &config) != pdPASS) {
            log_error("MQTT_PORT", "failed to queue broker configuration");
        } else {
            s_last_config_generation = config.generation;
        }
    }

    if (xQueueReceive(s_status_queue, &status, 0U) == pdPASS) {
        splc_mqtt_set_status(&status);
    }
}

bool splc_mqtt_port_take_config(splc_mqtt_port_config_t *out, uint32_t timeout_ms)
{
    return out != NULL && s_config_queue != NULL &&
           xQueueReceive(s_config_queue, out, timeout_ticks(timeout_ms)) == pdPASS;
}

bool splc_mqtt_port_take_publish(splc_mqtt_port_message_t *out, uint32_t timeout_ms)
{
    return out != NULL && s_publish_queue != NULL &&
           xQueueReceive(s_publish_queue, out, timeout_ticks(timeout_ms)) == pdPASS;
}

void splc_mqtt_port_discard_publishes(void)
{
    if (s_publish_queue != NULL) {
        (void)xQueueReset(s_publish_queue);
    }
}

void splc_mqtt_port_report_status(const splc_mqtt_status_t *status)
{
    if (status == NULL || s_status_queue == NULL) {
        return;
    }
    if (xQueueOverwrite(s_status_queue, status) != pdPASS) {
        log_error("MQTT_PORT", "failed to queue MQTT status");
    }
}

#else

void splc_mqtt_port_set_backend(splc_mqtt_port_publish_fn fn, void *ctx) { (void)fn; (void)ctx; }
void splc_mqtt_port_init(void) {}
void splc_mqtt_port_process(void) {}

#endif