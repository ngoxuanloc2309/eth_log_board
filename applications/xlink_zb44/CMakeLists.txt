cmake_minimum_required(VERSION 3.22)

add_library(xlink_zb44 STATIC)

target_sources(xlink_zb44 PRIVATE "xlink_zb44.c")

target_include_directories(xlink_zb44 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(xlink_zb44 
                            PUBLIC bsp 
                            PRIVATE logger)