cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(webserver STATIC)

file(GLOB WEB_SOURCE
    "*.c"
)

target_sources(webserver PRIVATE ${WEB_SOURCE})

target_include_directories(webserver PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(webserver PRIVATE
        stm32cubemx
        synaptix
        lwip
        lwip_apps
)   