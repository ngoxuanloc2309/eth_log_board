#include "mqtt_agent.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "logger.h"
#include "network_posix.h"
#include <stddef.h>
#include <string.h>
#include "lwip/errno.h"
#include <string.h>
#include "lwip/sockets.h"
#include <sys/time.h>

#define MQTT_PACKET_BUFFER_SIZE 4096U
#define MQTT_COMMAND_QUEUE_SIZE 16U
#define MQTT_COMMAND_POOL_SIZE 16U
#define MQTT_QUEUE_WAIT_MS 5000U
#define MQTT_DEFAULT_CONNECT_TIMEOUT_MS 10000U
#define MQTT_DEFAULT_COMMAND_TIMEOUT_MS 15000U
#define MQTT_TASK_STACK_SIZE 8192U
#define MQTT_AGENT_TASK_PRIORITY ( tskIDLE_PRIORITY + 2U )
#define MQTT_AGENT_LOG_TAG "MQTT_AGENT"

struct MQTTAgentMessageContext
{
    QueueHandle_t command_queue;
};

typedef struct
{
    MQTTAgentCommand_t command;
} CommandSlot_t;

struct MQTTAgentLibrary
{
    struct MQTTAgentMessageContext message_context;
    QueueHandle_t command_pool;
    QueueHandle_t completion_queue;
    QueueHandle_t agent_done_queue;
    MQTTAgentContext_t agent_context;
    NetworkContext_t network_context;
    TaskHandle_t task_handle;
    MQTTStatus_t loop_status;
    uint8_t packet_buffer[MQTT_PACKET_BUFFER_SIZE];
    CommandSlot_t command_slots[MQTT_COMMAND_POOL_SIZE];
    uint32_t command_timeout_ms;
    bool connected;
    bool started;
    bool operation_timed_out;
    bool network_ready;
    StaticTask_t task_control_block;
    StackType_t task_stack[MQTT_TASK_STACK_SIZE];
};

static MQTTAgentLibrary_t agent_storage;
static MQTTAgentLibrary_t *active_agent;
static bool agent_storage_in_use;
static bool agent_task_storage_in_use;
static bool agent_task_cleanup_pending;

static bool claim_agent_task_storage(void)
{
    bool available;

    taskENTER_CRITICAL();
    available = !agent_task_storage_in_use;
    if (available)
    {
        agent_task_storage_in_use = true;
    }
    taskEXIT_CRITICAL();

    return available;
}

static void release_agent_task_storage_if_unused(void)
{
    taskENTER_CRITICAL();
    if (!agent_task_cleanup_pending)
    {
        agent_task_storage_in_use = false;
    }
    taskEXIT_CRITICAL();
}

void mqtt_agent_idle_process(void)
{
    taskENTER_CRITICAL();
    if (agent_task_cleanup_pending)
    {
        agent_task_cleanup_pending = false;
        agent_task_storage_in_use = false;
    }
    taskEXIT_CRITICAL();
}

static TickType_t milliseconds_to_ticks(uint32_t milliseconds)
{
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    return (milliseconds > 0U && ticks == 0U) ? 1U : ticks;
}

static bool message_send(MQTTAgentMessageContext_t *context,
                         MQTTAgentCommand_t *const *command,
                         uint32_t block_time_ms)
{
    return context != NULL &&
           command != NULL &&
           *command != NULL &&
           xQueueSend(context->command_queue,
                      command,
                      milliseconds_to_ticks(block_time_ms)) == pdPASS;
}

static bool message_receive(MQTTAgentMessageContext_t *context,
                            MQTTAgentCommand_t **command,
                            uint32_t block_time_ms)
{
    return context != NULL &&
           command != NULL &&
           xQueueReceive(context->command_queue,
                         command,
                         milliseconds_to_ticks(block_time_ms)) == pdPASS;
}

static MQTTAgentCommand_t *command_get(uint32_t block_time_ms)
{
    MQTTAgentCommand_t *command = NULL;
    MQTTAgentLibrary_t *agent = active_agent;

    if (agent != NULL &&
        xQueueReceive(agent->command_pool,
                      &command,
                      milliseconds_to_ticks(block_time_ms)) == pdPASS)
    {
        (void)memset(command, 0, sizeof(*command));
        return command;
    }
    return command;
}

static bool command_release(MQTTAgentCommand_t *command)
{
    MQTTAgentLibrary_t *agent = active_agent;
    if (agent == NULL || command == NULL)
    {
        return false;
    }

    for (size_t i = 0U; i < MQTT_COMMAND_POOL_SIZE; i++)
    {
        if (&agent->command_slots[i].command == command)
        {
            return xQueueSend(agent->command_pool, &command, 0U) == pdPASS;
        }
    }
    return false;
}

static uint32_t get_time_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static int set_receive_poll_timeout(int socket_fd)
{
    const int timeout_ms = 1000;

    return setsockopt(socket_fd,
                      SOL_SOCKET,
                      SO_RCVTIMEO,
                      &timeout_ms,
                      sizeof(timeout_ms));
}

static void default_incoming_publish(MQTTAgentContext_t *context,
                                     uint16_t packet_id,
                                     MQTTPublishInfo_t *publish_info)
{
    (void)context;
    (void)packet_id;

    if (publish_info == NULL)
    {
        return;
    }

    LOGI(MQTT_AGENT_LOG_TAG, "Received publish on %.*s (%u bytes)",
         (int)publish_info->topicNameLength,
         publish_info->pTopicName,
         publish_info->payloadLength);
    if (publish_info->pPayload != NULL && publish_info->payloadLength > 0U)
    {
        size_t log_length = publish_info->payloadLength;
        if (log_length > 512U)
        {
            log_length = 512U;
        }
        LOGI(MQTT_AGENT_LOG_TAG, "Message: %.*s%s",
             (int)log_length,
             (const char *)publish_info->pPayload,
             log_length < publish_info->payloadLength ? "..." : "");
    }
}

static void command_complete(MQTTAgentCommandContext_t *callback_context,
                             MQTTAgentReturnInfo_t *return_info)
{
    MQTTAgentLibrary_t *agent = (MQTTAgentLibrary_t *)callback_context;
    if (agent == NULL || return_info == NULL ||
        xQueueSend(agent->completion_queue, &return_info->returnCode, 0U) != pdPASS)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "Could not deliver MQTT command completion");
    }
}

static MQTTStatus_t wait_for_completion(MQTTAgentLibrary_t *agent)
{
    const uint32_t timeout_ms = agent->command_timeout_ms;
    uint32_t elapsed_ms = 0U;

    while (elapsed_ms < timeout_ms)
    {
        MQTTStatus_t status;
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        if (wait_ms > 250U)
        {
            wait_ms = 250U;
        }

        if (xQueueReceive(agent->completion_queue,
                          &status,
                          milliseconds_to_ticks(wait_ms)) == pdPASS)
        {
            return status;
        }

        elapsed_ms += wait_ms;
    }

    agent->operation_timed_out = true;
    LOGE(MQTT_AGENT_LOG_TAG, "Timed out waiting for MQTT command completion");
    return MQTTRecvFailed;
}

static void agent_task(void *parameter)
{
    MQTTAgentLibrary_t *agent = (MQTTAgentLibrary_t *)parameter;
    agent->loop_status = MQTTAgent_CommandLoop(&agent->agent_context);
    if (MQTTAgent_CancelAll(&agent->agent_context) != MQTTSuccess)
    {
        LOGE(MQTT_AGENT_LOG_TAG,
             "Could not cancel pending MQTT commands after agent loop exit");
    }
    taskENTER_CRITICAL();
    agent_task_cleanup_pending = true;
    taskEXIT_CRITICAL();
    if (xQueueSend(agent->agent_done_queue,
                   &agent->loop_status,
                   portMAX_DELAY) != pdPASS)
    {
        LOGE(MQTT_AGENT_LOG_TAG,
             "Could not report MQTT agent task completion");
    }
    vTaskDelete(NULL);
}

static void release_queues(MQTTAgentLibrary_t *agent)
{
    if (agent->agent_done_queue != NULL)
    {
        vQueueDelete(agent->agent_done_queue);
    }
    if (agent->completion_queue != NULL)
    {
        vQueueDelete(agent->completion_queue);
    }
    if (agent->command_pool != NULL)
    {
        vQueueDelete(agent->command_pool);
    }
    if (agent->message_context.command_queue != NULL)
    {
        vQueueDelete(agent->message_context.command_queue);
    }
}

static void release_agent_storage(MQTTAgentLibrary_t *agent)
{
    if (agent->network_ready)
    {
        network_posix_disconnect(&agent->network_context);
    }
    release_queues(agent);
    if (active_agent == agent)
    {
        active_agent = NULL;
    }
    (void)memset(agent, 0, offsetof(MQTTAgentLibrary_t, task_control_block));
    agent->network_context.socket_fd = -1;
    agent_storage_in_use = false;
    release_agent_task_storage_if_unused();
}

MQTTStatus_t mqtt_agent_connect(MQTTAgentLibrary_t **agent_out,
                                const MQTTAgentLibraryConfig_t *config)
{
    if (agent_out == NULL || config == NULL ||
        config->host == NULL || config->host[0] == '\0' ||
        config->port == NULL || config->port[0] == '\0' ||
        (config->security_mode != NETWORK_SECURITY_TLS &&
         config->security_mode != NETWORK_SECURITY_PLAIN) ||
        (config->security_mode == NETWORK_SECURITY_TLS &&
         (config->ca == NULL || config->ca[0] == '\0')) ||
        config->client_id == NULL || config->client_id[0] == '\0')
    {
        return MQTTBadParameter;
    }
    *agent_out = NULL;
    if (agent_storage_in_use)
    {
        return MQTTIllegalState;
    }
    if (!claim_agent_task_storage())
    {
        return MQTTIllegalState;
    }

    agent_storage_in_use = true;
    MQTTAgentLibrary_t *agent = &agent_storage;
    active_agent = agent;
    (void)memset(agent, 0, offsetof(MQTTAgentLibrary_t, task_control_block));
    agent->network_context.socket_fd = -1;
    agent->command_timeout_ms = config->command_timeout_ms != 0U
                                    ? config->command_timeout_ms
                                    : MQTT_DEFAULT_COMMAND_TIMEOUT_MS;
    agent->loop_status = MQTTSuccess;

    uint32_t connect_timeout_ms =
        config->connect_timeout_ms != 0U
            ? config->connect_timeout_ms
            : MQTT_DEFAULT_CONNECT_TIMEOUT_MS;
    uint32_t connect_start_ms;

    agent->message_context.command_queue =
        xQueueCreate(MQTT_COMMAND_QUEUE_SIZE, sizeof(MQTTAgentCommand_t *));
    agent->command_pool =
        xQueueCreate(MQTT_COMMAND_POOL_SIZE, sizeof(MQTTAgentCommand_t *));
    agent->completion_queue = xQueueCreate(1U, sizeof(MQTTStatus_t));
    agent->agent_done_queue = xQueueCreate(1U, sizeof(MQTTStatus_t));
    if (agent->message_context.command_queue == NULL ||
        agent->command_pool == NULL ||
        agent->completion_queue == NULL ||
        agent->agent_done_queue == NULL)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "Could not allocate FreeRTOS MQTT queues");
        release_agent_storage(agent);
        return MQTTNoMemory;
    }

    for (size_t i = 0U; i < MQTT_COMMAND_POOL_SIZE; i++)
    {
        MQTTAgentCommand_t *command = &agent->command_slots[i].command;
        if (xQueueSend(agent->command_pool, &command, 0U) != pdPASS)
        {
            LOGE(MQTT_AGENT_LOG_TAG,
                 "Could not initialize MQTT command pool");
            release_agent_storage(agent);
            return MQTTNoMemory;
        }
    }

    connect_start_ms = get_time_ms();
    if (network_posix_connect_mode(&agent->network_context,
                                   config->host,
                                   config->port,
                                   config->security_mode,
                                   config->ca,
                                   config->ca_len,
                                   connect_timeout_ms) != 0)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "MQTT TLS connection failed");
        release_agent_storage(agent);
        return MQTTRecvFailed;
    }
    agent->network_ready = true;

    TransportInterface_t transport = {
        .recv = (TransportRecv_t)network_posix_recv,
        .send = (TransportSend_t)network_posix_send,
        .writev = NULL,
        .pNetworkContext = &agent->network_context
    };
    MQTTFixedBuffer_t network_buffer = {
        .pBuffer = agent->packet_buffer,
        .size = sizeof(agent->packet_buffer)
    };
    MQTTAgentMessageInterface_t message_interface = {
        .pMsgCtx = (MQTTAgentMessageContext_t *)&agent->message_context,
        .send = message_send,
        .recv = message_receive,
        .getCommand = command_get,
        .releaseCommand = command_release
    };

    MQTTStatus_t status = MQTTAgent_Init(&agent->agent_context,
                                         &message_interface,
                                         &network_buffer,
                                         &transport,
                                         get_time_ms,
                                         config->incoming_publish_callback != NULL
                                             ? config->incoming_publish_callback
                                             : default_incoming_publish,
                                         config->incoming_publish_context);
    if (status != MQTTSuccess)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "MQTTAgent_Init failed: %s",
             MQTT_Status_strerror(status));
        release_agent_storage(agent);
        return status;
    }

    MQTTConnectInfo_t connect_info = {0};
    connect_info.cleanSession = !config->persistent_session;
    connect_info.keepAliveSeconds = config->keepAlive;
    connect_info.pClientIdentifier = config->client_id;
    connect_info.clientIdentifierLength = strlen(config->client_id);
    if (config->username != NULL)
    {
        connect_info.pUserName = config->username;
        connect_info.userNameLength = strlen(config->username);
    }
    if (config->password != NULL)
    {
        connect_info.pPassword = config->password;
        connect_info.passwordLength = strlen(config->password);
    }

    bool session_present = false;
    static MQTTPublishInfo_t will_message ;

    will_message.dup = false;
    will_message.pTopicName = config->will_topic;
    will_message.topicNameLength = strlen(config->will_topic);
    will_message.pPayload = config->will_message;
    will_message.payloadLength = strlen(config->will_message);
    will_message.retain = config->will_retain;
    will_message.qos = config->will_qos;

    uint32_t elapsed_connect_ms = get_time_ms() - connect_start_ms;
    if (elapsed_connect_ms >= connect_timeout_ms)
    {
        LOGE(MQTT_AGENT_LOG_TAG,
             "MQTT connection setup timed out before CONNACK");
        release_agent_storage(agent);
        return MQTTRecvFailed;
    }

    LOGI(MQTT_AGENT_LOG_TAG, "Sending MQTT CONNECT to %s:%s",
         config->host, config->port);
    status = MQTT_Connect(&agent->agent_context.mqttContext,
                          &connect_info,
                          &will_message,
                          connect_timeout_ms - elapsed_connect_ms,
                          &session_present);
    if (status != MQTTSuccess)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "MQTT_Connect failed: %s",
             MQTT_Status_strerror(status));
        release_agent_storage(agent);
        return status;
    }
    agent->connected = true;
    LOGI(MQTT_AGENT_LOG_TAG, "Connected to MQTT broker %s:%s",
         config->host, config->port);

    if (set_receive_poll_timeout(agent->network_context.socket_fd) != 0)
    {
        LOGE(MQTT_AGENT_LOG_TAG,
             "Could not configure MQTT receive polling: %s",
             strerror(errno));
        (void)MQTT_Disconnect(&agent->agent_context.mqttContext);
        agent->connected = false;
        release_agent_storage(agent);
        return MQTTBadParameter;
    }
    agent->agent_context.mqttContext.transportInterface.recv =
        (TransportRecv_t)network_posix_recv_poll;

    *agent_out = agent;
    return MQTTSuccess;
}

MQTTStatus_t mqtt_agent_start(MQTTAgentLibrary_t *agent)
{
    if (agent == NULL || !agent->connected || agent->started)
    {
        return MQTTBadParameter;
    }
    agent->task_handle = xTaskCreateStatic(agent_task,
                                           "mqtt-agent",
                                           MQTT_TASK_STACK_SIZE,
                                           agent,
                                           MQTT_AGENT_TASK_PRIORITY,
                                           agent->task_stack,
                                           &agent->task_control_block);
    if (agent->task_handle == NULL)
    {
        LOGE(MQTT_AGENT_LOG_TAG,
             "Could not create FreeRTOS MQTT agent task");
        release_agent_task_storage_if_unused();
        return MQTTNoMemory;
    }
    agent->started = true;
    return MQTTSuccess;
}

bool mqtt_agent_is_running(const MQTTAgentLibrary_t *agent)
{
    return agent != NULL && agent->started &&
           agent->agent_done_queue != NULL &&
           uxQueueMessagesWaiting(agent->agent_done_queue) == 0U;
}

MQTTStatus_t mqtt_agent_subscribe(MQTTAgentLibrary_t *agent,
                                  const char *topic,
                                  MQTTQoS_t qos)
{
    if (agent == NULL || topic == NULL || topic[0] == '\0' ||
        qos < MQTTQoS0 || qos > MQTTQoS2 ||
        !agent->started || !agent->connected || agent->operation_timed_out)
    {
        return MQTTBadParameter;
    }

    MQTTSubscribeInfo_t subscribe_info = {0};
    subscribe_info.qos = qos;
    subscribe_info.pTopicFilter = topic;
    subscribe_info.topicFilterLength = strlen(topic);
    MQTTAgentSubscribeArgs_t subscribe_args = {
        .pSubscribeInfo = &subscribe_info,
        .numSubscriptions = 1U
    };
    MQTTAgentCommandInfo_t command_info = {
        .cmdCompleteCallback = command_complete,
        .pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *)agent,
        .blockTimeMs = MQTT_QUEUE_WAIT_MS
    };

    (void)xQueueReset(agent->completion_queue);
    MQTTStatus_t status = MQTTAgent_Subscribe(&agent->agent_context,
                                               &subscribe_args,
                                               &command_info);
    if (status != MQTTSuccess)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "Could not queue subscription: %s",
             MQTT_Status_strerror(status));
        return status;
    }

    status = wait_for_completion(agent);
    if (status == MQTTSuccess)
    {
        LOGI(MQTT_AGENT_LOG_TAG, "Subscribed to topic %s", topic);
    }
    else
    {
        LOGE(MQTT_AGENT_LOG_TAG, "MQTT subscribe failed: %s",
             MQTT_Status_strerror(status));
    }
    return status;
}

MQTTStatus_t mqtt_agent_publish(MQTTAgentLibrary_t *agent,
                                const char *topic,
                                const void *payload,
                                size_t payload_length,
                                MQTTQoS_t qos)
{
    return mqtt_agent_publish_with_options(agent, topic, payload,
                                           payload_length, qos, false);
}

MQTTStatus_t mqtt_agent_publish_with_options(MQTTAgentLibrary_t *agent,
                                             const char *topic,
                                             const void *payload,
                                             size_t payload_length,
                                             MQTTQoS_t qos,
                                             bool retain)
{
    if (agent == NULL || topic == NULL || topic[0] == '\0' ||
        (payload == NULL && payload_length != 0U) ||
        qos < MQTTQoS0 || qos > MQTTQoS2 ||
        !agent->started || !agent->connected || agent->operation_timed_out)
    {
        return MQTTBadParameter;
    }

    MQTTPublishInfo_t publish_info = {0};
    publish_info.qos = qos;
    publish_info.pTopicName = topic;
    publish_info.topicNameLength = strlen(topic);
    publish_info.pPayload = payload;
    publish_info.payloadLength = payload_length;
    publish_info.retain = retain;
    MQTTAgentCommandInfo_t command_info = {
        .cmdCompleteCallback = command_complete,
        .pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *)agent,
        .blockTimeMs = MQTT_QUEUE_WAIT_MS
    };

    (void)xQueueReset(agent->completion_queue);
    MQTTStatus_t status = MQTTAgent_Publish(&agent->agent_context,
                                            &publish_info,
                                            &command_info);
    if (status != MQTTSuccess)
    {
        LOGE(MQTT_AGENT_LOG_TAG, "Could not queue publish: %s",
             MQTT_Status_strerror(status));
        return status;
    }

    status = wait_for_completion(agent);
    if (status == MQTTSuccess)
    {
        LOGI(MQTT_AGENT_LOG_TAG, "Published to topic %s", topic);
    }
    else
    {
        LOGE(MQTT_AGENT_LOG_TAG, "MQTT publish failed: %s",
             MQTT_Status_strerror(status));
    }
    return status;
}

MQTTStatus_t mqtt_agent_disconnect(MQTTAgentLibrary_t *agent)
{
    bool send_disconnect = true;

    if (agent == NULL)
    {
        return MQTTBadParameter;
    }
    if (!agent->connected)
    {
        return MQTTSuccess;
    }

    MQTTStatus_t status = MQTTSuccess;
    if (agent->started)
    {
        if (mqtt_agent_is_running(agent))
        {
            MQTTAgentCommandInfo_t terminate_info = {0};
            terminate_info.blockTimeMs = MQTT_QUEUE_WAIT_MS;
            status = MQTTAgent_Terminate(&agent->agent_context,
                                         &terminate_info);
            if (status != MQTTSuccess)
            {
                LOGE(MQTT_AGENT_LOG_TAG,
                     "Could not terminate MQTT agent: %s",
                     MQTT_Status_strerror(status));
                (void)shutdown(agent->network_context.socket_fd, SHUT_RDWR);
                send_disconnect = false;
            }
        }

        MQTTStatus_t loop_status = MQTTSuccess;
        if (xQueueReceive(agent->agent_done_queue,
                          &loop_status,
                          milliseconds_to_ticks(agent->command_timeout_ms)) != pdPASS)
        {
            LOGE(MQTT_AGENT_LOG_TAG,
                 "Timed out waiting for MQTT agent task to stop");
            (void)shutdown(agent->network_context.socket_fd, SHUT_RDWR);
            if (xQueueReceive(agent->agent_done_queue,
                              &loop_status,
                              portMAX_DELAY) != pdPASS)
            {
                LOGE(MQTT_AGENT_LOG_TAG,
                     "Could not confirm MQTT agent task termination");
                return MQTTRecvFailed;
            }
            status = MQTTRecvFailed;
        }
        agent->started = false;
        if (loop_status != MQTTSuccess)
        {
            LOGE(MQTT_AGENT_LOG_TAG, "MQTT agent loop failed: %s",
                 MQTT_Status_strerror(loop_status));
            status = loop_status;
            send_disconnect = false;
        }
    }

    if (agent->connected)
    {
        if (send_disconnect)
        {
            MQTTStatus_t disconnect_status =
                MQTT_Disconnect(&agent->agent_context.mqttContext);
            if (disconnect_status != MQTTSuccess)
            {
                LOGW(MQTT_AGENT_LOG_TAG, "MQTT disconnect returned: %s",
                     MQTT_Status_strerror(disconnect_status));
                status = disconnect_status;
            }
        }
        else
        {
            LOGW(MQTT_AGENT_LOG_TAG,
                 "Skipping MQTT DISCONNECT because the agent loop already stopped");
        }
        agent->connected = false;
    }
    return status;
}

void mqtt_agent_destroy(MQTTAgentLibrary_t *agent)
{
    if (agent == NULL)
    {
        return;
    }

    if (agent->started)
    {
        LOGE(MQTT_AGENT_LOG_TAG,
             "mqtt_agent_disconnect() must complete before destroy");
        return;
    }

    if (agent->connected)
    {
        (void)MQTT_Disconnect(&agent->agent_context.mqttContext);
    }
    agent->connected = false;
    release_agent_storage(agent);
}
