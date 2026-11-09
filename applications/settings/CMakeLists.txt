cmake_minimum_required(VERSION 3.22)

add_library(settings STATIC settings.c)
target_include_directories(settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(settings PUBLIC file_system_port)