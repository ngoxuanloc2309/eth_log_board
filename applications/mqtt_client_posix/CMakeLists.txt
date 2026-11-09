cmake_minimum_required(VERSION 3.22)

include(${CMAKE_CURRENT_LIST_DIR}/../../libs/mqtt/source/dependency/coreMQTT/mqttFilePaths.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../libs/mqtt/mqttAgentFilePaths.cmake)

add_library(mqtt_client_posix STATIC)

file(GLOB MQTT_AGENT_SRC     ${MQTT_SOURCES}
                            ${MQTT_SERIALIZER_SOURCES}
                            ${MQTT_AGENT_SOURCES}
                            "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

target_sources(mqtt_client_posix PRIVATE ${MQTT_AGENT_SRC})

target_include_directories(mqtt_client_posix PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MQTT_INCLUDE_PUBLIC_DIRS}
    ${MQTT_AGENT_INCLUDE_PUBLIC_DIRS}
    "${CMAKE_CURRENT_LIST_DIR}/../../libs/mqtt/source/dependency/coreMQTT/source/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../libs/mqtt/source/dependency/coreMQTT/source/interface"
)

target_include_directories(mqtt_client_posix PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../libs/mqtt/source/dependency/coreMQTT/source/include/private"
)

target_compile_definitions(mqtt_client_posix PUBLIC
    MQTT_DO_NOT_USE_CUSTOM_CONFIG=1
    MQTT_AGENT_DO_NOT_USE_CUSTOM_CONFIG=1
)

target_compile_definitions(mqtt_client_posix PRIVATE
    APPLICATION_NETWORK_USE_LWIP=1
)

target_link_libraries(mqtt_client_posix PUBLIC
    stm32cubemx
    tcpip
    lwipcore
    lwipallapps
    mbedtls
    mbedx509
    mbedcrypto
    logger
)

target_link_options(mqtt_client_posix INTERFACE
    "LINKER:--wrap=MQTT_ProcessLoop"
)