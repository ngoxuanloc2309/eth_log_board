cmake_minimum_required(VERSION 3.22)

add_library(web_server STATIC
    web_server.c
)

target_include_directories(web_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(web_server PUBLIC
    file_system_port
    rf_app
    settings
    tcpip
    logger
)
