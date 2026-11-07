cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(tls STATIC)

file(GLOB TLS_SOURCE
    "mbedTLS/library/*.c"
    "Target/*.c"
)

target_sources(tls PRIVATE ${TLS_SOURCE})

target_include_directories(tls PUBLIC 
    "mbedTLS/include"
    "mbedTLS/include/mbedtls"
    "Target"
)

target_compile_definitions(tls PUBLIC
    MBEDTLS_CONFIG_FILE="mbedtls_config.h"
)

target_link_libraries(tls PRIVATE 
        stm32cubemx
        lwip
        lwip_apps
)   