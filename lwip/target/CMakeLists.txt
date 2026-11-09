cmake_minimum_required(VERSION 3.22)

add_library(lwip_target STATIC)

target_sources(lwip_target PRIVATE "dp83848.c" 
                                   "ethernetif.c"
                                   "lan8742.c" )
                                   
target_include_directories(lwip_target PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lwip_target PUBLIC lwip_sys logger)

