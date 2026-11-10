cmake_minimum_required(VERSION 3.22)

add_library(splc_dtu_port OBJECT
    splc_host_rs485_port.c
    splc_host_zigbee_port.c
    splc_mqtt_port.c
    splc_port.c
    splc_rtc_port.c
    splc_tcp_port.c
    splc_adc_port.c
)

target_include_directories(splc_dtu_port PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(splc_dtu_port PUBLIC
    splc
    bsp
    logger
    mqtt_client_posix
    stm32cubemx
    tcpip
    lwipcore
    lwipallapps
)


target_compile_features(splc_dtu_port PUBLIC c_std_11)