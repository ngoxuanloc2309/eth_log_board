cmake_minimum_required(VERSION 3.22)

add_library(lwip_sys STATIC)

target_sources(lwip_sys PRIVATE "OS/sys_arch.c")

target_include_directories(lwip_sys PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                           "${LWIP_INCLUDE_DIR}"
)

target_link_libraries(lwip_sys PUBLIC stm32cubemx lwipcore lwipallapps lwip_target)