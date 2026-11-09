cmake_minimum_required(VERSION 3.22)

add_library(w25q_driver STATIC w25q.c)

target_include_directories(w25q_driver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(w25q_driver PUBLIC bus_interfaces)
