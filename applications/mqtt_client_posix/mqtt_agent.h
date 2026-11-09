#ifndef MQTT_AGENT_LIBRARY_H
#define MQTT_AGENT_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#include "core_mqtt_agent.h"
#include "network_posix.h"

typedef struct MQTTAgentLibrary MQTTAgentLibrary_t;

typedef struct
{
    const char *host;
    const char *port;
    const char *ca;
    uint32_t ca_len;
    const char *client_id;
    const char *username;
    const char *password;
    size_t keepAlive;
    /* Will message info*/
    const char *will_topic;
    const char *will_message;
    bool will_retain;
    MQTTQoS_t will_qos;
    /* TLS requires ca_file; plain TCP ignores it. */
    NetworkSecurityMode_t security_mode;
    /* Overall TCP/TLS/MQTT CONNACK timeout; 0 uses the default. */
    uint32_t connect_timeout_ms;
    uint32_t command_timeout_ms;
    MQTTAgentIncomingPublishCallback_t incoming_publish_callback;
    void *incoming_publish_context;
} MQTTAgentLibraryConfig_t;

/* The command-pool callbacks support one active library instance per process. */
/*
 * Establishes the TLS and MQTT connections and initializes the agent.
 * Call mqtt_agent_start() from a FreeRTOS task after the scheduler starts.
 * Config strings must remain valid for the duration of the connection.
 */
MQTTStatus_t mqtt_agent_connect(MQTTAgentLibrary_t **agent,
                                const MQTTAgentLibraryConfig_t *config);

/* Starts the MQTT-Agent command loop as a FreeRTOS task. */
MQTTStatus_t mqtt_agent_start(MQTTAgentLibrary_t *agent);

/* Returns true while the MQTT-Agent command loop task is still running. */
bool mqtt_agent_is_running(const MQTTAgentLibrary_t *agent);

/*
 * These synchronous operations must be called from a FreeRTOS task and must
 * not be invoked concurrently on the same agent instance.
 */
MQTTStatus_t mqtt_agent_subscribe(MQTTAgentLibrary_t *agent,
                                  const char *topic,
                                  MQTTQoS_t qos);
MQTTStatus_t mqtt_agent_publish(MQTTAgentLibrary_t *agent,
                                const char *topic,
                                const void *payload,
                                size_t payload_length,
                                MQTTQoS_t qos);

/* Stops the command loop and sends MQTT DISCONNECT. Call from a task. */
MQTTStatus_t mqtt_agent_disconnect(MQTTAgentLibrary_t *agent);

/* Releases local resources; disconnect first if the agent was started. */
void mqtt_agent_destroy(MQTTAgentLibrary_t *agent);

/**
 * @brief Release MQTT agent static task storage after FreeRTOS reclaims a task.
 *
 * Call from the FreeRTOS idle hook after its deleted-task cleanup pass.
 */
void mqtt_agent_idle_process(void);

#endif
