cmake_minimum_required(VERSION 3.22)

add_library(tcp_application STATIC
    tcp_client.c
    tcp_server.c
)

target_include_directories(tcp_application
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tcp_application
    PUBLIC
        stm32cubemx
        tcpip
        logger
)
