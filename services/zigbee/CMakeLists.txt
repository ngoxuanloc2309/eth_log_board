cmake_minimum_required(VERSION 3.22)

add_library(zigbee STATIC)

target_sources(zigbee PRIVATE "zigbee.c")

target_include_directories(zigbee PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(zigbee PRIVATE logger)