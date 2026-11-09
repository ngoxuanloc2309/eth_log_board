cmake_minimum_required(VERSION 3.22)

add_library(lwip_app STATIC)

target_sources(lwip_app PRIVATE lwip.c)

target_include_directories(lwip_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lwip_app PUBLIC lwip_target)