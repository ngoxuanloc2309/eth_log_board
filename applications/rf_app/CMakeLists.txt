cmake_minimum_required(VERSION 3.22)

add_library(rf_app STATIC rf_app.c)

target_include_directories(rf_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(rf_app PUBLIC bsp zigbee xlink_zb44 PRIVATE logger)
