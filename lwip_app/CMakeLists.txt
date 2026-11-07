cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(lwip_apps STATIC)

file(GLOB LWIP_APPS_SOURCE
    "App/*.c"
    "Target/*.c"
)

target_sources(lwip_apps PRIVATE ${LWIP_APPS_SOURCE})

target_include_directories(lwip_apps PUBLIC 
    "App"
    "Target"
)

target_link_libraries(lwip_apps PUBLIC 
    lwip
    stm32cubemx
    webserver
    synaptix
)