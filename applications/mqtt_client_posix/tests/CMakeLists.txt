cmake_minimum_required(VERSION 3.22)
project(mqtt_agent_tests LANGUAGES C)

include(CTest)

get_filename_component(MQTT_CLIENT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(MQTT_ROOT "${MQTT_CLIENT_ROOT}/../../libs/mqtt" ABSOLUTE)
get_filename_component(MBEDTLS_INCLUDE "${MQTT_CLIENT_ROOT}/../../libs/tls/include" ABSOLUTE)

include("${MQTT_ROOT}/source/dependency/coreMQTT/mqttFilePaths.cmake")
include("${MQTT_ROOT}/mqttAgentFilePaths.cmake")

add_executable(mqtt_agent_connect_test
    "${MQTT_CLIENT_ROOT}/mqtt_agent.c"
    "${MQTT_ROOT}/source/dependency/coreMQTT/source/core_mqtt.c"
    "${MQTT_ROOT}/source/dependency/coreMQTT/source/core_mqtt_serializer.c"
    "${MQTT_ROOT}/source/dependency/coreMQTT/source/core_mqtt_state.c"
    "${MQTT_ROOT}/source/core_mqtt_agent.c"
    "${MQTT_ROOT}/source/core_mqtt_agent_command_functions.c"
    mocks/freertos_mocks.c
    mocks/network_mocks.c
    test_mqtt_agent_connect.c
)

target_include_directories(mqtt_agent_connect_test BEFORE PRIVATE
    mocks/include
)

target_include_directories(mqtt_agent_connect_test PRIVATE
    "${MQTT_CLIENT_ROOT}"
    ${MQTT_INCLUDE_PUBLIC_DIRS}
    ${MQTT_AGENT_INCLUDE_PUBLIC_DIRS}
    "${MQTT_ROOT}/source/dependency/coreMQTT/source/include"
    "${MQTT_ROOT}/source/dependency/coreMQTT/source/interface"
    "${MQTT_ROOT}/source/dependency/coreMQTT/source/include/private"
    "${MBEDTLS_INCLUDE}"
)

target_compile_definitions(mqtt_agent_connect_test PRIVATE
    MQTT_DO_NOT_USE_CUSTOM_CONFIG=1
    MQTT_AGENT_DO_NOT_USE_CUSTOM_CONFIG=1
)

foreach(test_case IN ITEMS
        connect_success
        invalid_configuration
        transport_failure
        broker_rejection
        connack_timeout)
    add_test(NAME "mqtt_agent_${test_case}"
             COMMAND mqtt_agent_connect_test "${test_case}")
    set_tests_properties("mqtt_agent_${test_case}" PROPERTIES TIMEOUT 10)
endforeach()
