#include "mqtt_agent.h"
#include "mqtt_test_mocks.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                   \
    do                                                                     \
    {                                                                      \
        if (!(condition))                                                  \
        {                                                                  \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n",             \
                           __FILE__, __LINE__, #condition);                 \
            return 1;                                                      \
        }                                                                  \
    } while (0)

static MQTTAgentLibraryConfig_t default_config(void)
{
    MQTTAgentLibraryConfig_t config = { 0 };

    config.host = "broker.test";
    config.port = "1883";
    config.client_id = "mqtt-agent-ctest";
    config.keepAlive = 60U;
    config.will_topic = "test/status";
    config.will_message = "offline";
    config.will_qos = MQTTQoS0;
    config.security_mode = NETWORK_SECURITY_PLAIN;
    config.connect_timeout_ms = 2500U;
    return config;
}

static int test_connect_success(void)
{
    MQTTAgentLibrary_t *agent = NULL;
    MQTTAgentLibraryConfig_t config = default_config();
    MQTTStatus_t status;

    mqtt_test_reset();
    status = mqtt_agent_connect(&agent, &config);

    CHECK(status == MQTTSuccess);
    CHECK(agent != NULL);
    CHECK(mqtt_test_network_timeout_ms == config.connect_timeout_ms);
    CHECK(mqtt_test_receive_timeout_configured);
    CHECK(mqtt_test_receive_count > 0U);
    CHECK(mqtt_test_last_connect_packet_type == 0x10U);

    mqtt_agent_destroy(agent);
    CHECK(mqtt_test_network_disconnect_count == 1U);

    mqtt_test_reset();
    agent = NULL;
    status = mqtt_agent_connect(&agent, &config);
    CHECK(status == MQTTSuccess);
    CHECK(agent != NULL);
    mqtt_agent_destroy(agent);
    CHECK(mqtt_test_network_disconnect_count == 1U);
    return 0;
}

static int test_invalid_configuration(void)
{
    MQTTAgentLibrary_t *agent = NULL;
    MQTTAgentLibraryConfig_t config = default_config();

    mqtt_test_reset();
    config.client_id = "";
    CHECK(mqtt_agent_connect(&agent, &config) == MQTTBadParameter);
    CHECK(agent == NULL);
    CHECK(mqtt_test_network_timeout_ms == 0U);

    config = default_config();
    config.security_mode = NETWORK_SECURITY_TLS;
    config.ca = NULL;
    CHECK(mqtt_agent_connect(&agent, &config) == MQTTBadParameter);
    CHECK(agent == NULL);
    return 0;
}

static int test_transport_failure_cleans_up(void)
{
    MQTTAgentLibrary_t *agent = NULL;
    MQTTAgentLibraryConfig_t config = default_config();

    mqtt_test_reset();
    mqtt_test_network_connect_result = -1;
    CHECK(mqtt_agent_connect(&agent, &config) == MQTTRecvFailed);
    CHECK(agent == NULL);
    CHECK(mqtt_test_network_disconnect_count == 0U);

    mqtt_test_reset();
    CHECK(mqtt_agent_connect(&agent, &config) == MQTTSuccess);
    CHECK(agent != NULL);
    mqtt_agent_destroy(agent);
    return 0;
}

static int test_broker_rejection_cleans_up(void)
{
    MQTTAgentLibrary_t *agent = NULL;
    MQTTAgentLibraryConfig_t config = default_config();

    mqtt_test_reset();
    mqtt_test_connack_mode = MQTT_TEST_CONNACK_REFUSED;
    CHECK(mqtt_agent_connect(&agent, &config) == MQTTServerRefused);
    CHECK(agent == NULL);
    CHECK(mqtt_test_network_disconnect_count == 1U);
    return 0;
}

static int test_connack_timeout_returns(void)
{
    MQTTAgentLibrary_t *agent = NULL;
    MQTTAgentLibraryConfig_t config = default_config();
    MQTTStatus_t status;

    mqtt_test_reset();
    mqtt_test_connack_mode = MQTT_TEST_CONNACK_TIMEOUT;
    config.connect_timeout_ms = 100U;
    status = mqtt_agent_connect(&agent, &config);
    CHECK(status == MQTTNoDataAvailable);
    CHECK(agent == NULL);
    CHECK(mqtt_test_receive_count > 0U);
    CHECK(mqtt_test_network_disconnect_count == 1U);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        (void)fprintf(stderr, "expected one test case argument\n");
        return 1;
    }

    if (strcmp(argv[1], "connect_success") == 0)
    {
        return test_connect_success();
    }
    if (strcmp(argv[1], "invalid_configuration") == 0)
    {
        return test_invalid_configuration();
    }
    if (strcmp(argv[1], "transport_failure") == 0)
    {
        return test_transport_failure_cleans_up();
    }
    if (strcmp(argv[1], "broker_rejection") == 0)
    {
        return test_broker_rejection_cleans_up();
    }
    if (strcmp(argv[1], "connack_timeout") == 0)
    {
        return test_connack_timeout_returns();
    }

    (void)fprintf(stderr, "unknown test case: %s\n", argv[1]);
    return 1;
}
