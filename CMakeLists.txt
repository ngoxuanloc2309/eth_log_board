cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME DTU_Firmware)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# lwIP upstream headers
set(LWIP_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/lwip/src/include" 
)

if(NOT EXISTS "${LWIP_INCLUDE_DIRS}/lwip/opt.h")
    message(FATAL_ERROR "lwIP headers not found: ${LWIP_INCLUDE_DIRS}")
endif()

# Build upstream lwIP stack
add_subdirectory(libs/lwip)

# Make lwIP headers available to consumers
target_include_directories(lwipcore PUBLIC
    "${LWIP_INCLUDE_DIRS}"
)

target_include_directories(lwipallapps PUBLIC
    "${LWIP_INCLUDE_DIRS}"
)

add_subdirectory(lwip)

target_link_libraries(lwipcore PRIVATE tcpip)

target_link_libraries(lwipallapps PRIVATE tcpip)

# Mbed TLS configuration
set(ENABLE_PROGRAMS OFF CACHE BOOL
    "Disable Mbed TLS programs" FORCE)

set(ENABLE_TESTING OFF CACHE BOOL
    "Disable Mbed TLS tests" FORCE)

set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL
    "Disable Mbed TLS package installation" FORCE)

set(MBEDTLS_PROJECT_CONFIG
    "${CMAKE_CURRENT_SOURCE_DIR}/tls/mbedtls_config.h"
)

add_subdirectory(libs/tls)

# Use the firmware's lwIP adapter instead of Mbed TLS's Unix/Windows socket
# implementation.
get_target_property(MBEDTLS_SOURCES mbedtls SOURCES)
list(FILTER MBEDTLS_SOURCES EXCLUDE REGEX "(^|/)net_sockets\\.c$")
set_property(TARGET mbedtls PROPERTY SOURCES "${MBEDTLS_SOURCES}")

foreach(tls_target IN ITEMS mbedcrypto mbedx509 mbedtls)
    if(TARGET ${tls_target})
        target_include_directories(${tls_target} BEFORE PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls"
        )

        target_compile_definitions(${tls_target} PRIVATE
            MBEDTLS_CONFIG_FILE="tls_config.h"
            # MBEDTLS_NO_PLATFORM_ENTROPY
            # MBEDTLS_ENTROPY_HARDWARE_ALT
            # MBEDTLS_PLATFORM_MS_TIME_ALT
        )
    endif()
endforeach()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MBEDTLS_CONFIG_FILE="tls_config.h"
)

foreach(tls_target IN ITEMS mbedcrypto mbedx509 mbedtls)
    if(TARGET ${tls_target})
        target_compile_options(${tls_target} PRIVATE
            -Wno-error=shadow
        )
    endif()
endforeach()

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
    ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls
)
file(GLOB TLS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/*.c")
list(FILTER TLS_SOURCE EXCLUDE REGEX "/net_sockets\\.c$")
# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${TLS_SOURCE}
)

# Build the lwIP-backed Mbed TLS networking module into libmbedtls.
target_sources(mbedtls PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/net_sockets.c"
)

target_include_directories(mbedtls PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/lwip/target"
    "${CMAKE_CURRENT_SOURCE_DIR}/lwip/system"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
)

target_link_libraries(mbedtls PUBLIC
    tcpip
    lwipcore
    lwipallapps
)

add_subdirectory(interface)

add_subdirectory(Drivers)

add_subdirectory(board)

add_subdirectory(applications)

add_subdirectory(services)

add_subdirectory(libs/logger)

target_compile_definitions(logger PRIVATE FREE_RTOS=1)

target_link_libraries(logger PUBLIC stm32cubemx)

# SimplePLC is consumed as a library: the product port supplies host-specific
# transports while the library provides the engine, board, and protocol layers.
set(SPLC_AS_LIBRARY ON)
set(SPLC_OPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port/splc")
set(SPLC_OS_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc"
)
# SPLC_LOGGER_BUILTIN=0 uses the host logger from SPLC layer sources too.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs/logger")
add_subdirectory(libs/simple_plc)
add_subdirectory(port/splc)

target_include_directories(stm32cubemx INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/port/splc"
)
# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/mbedtls
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    tcpip
    lwipcore
    lwipallapps
    mbedtls
    mbedx509
    mbedcrypto
    Application
    logger
    bsp
    services
    splc_dtu_port
    # Add user defined libraries
)

# SimplePLC runs on FreeRTOS (SX_OS_USE_FREERTOS 1): in library mode the host must add the
# sx_os_*() glue objects to its own executable (an OBJECT library inside another OBJECT library
# is NOT carried to the link).
target_sources(${CMAKE_PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:splc_platform_freertos>)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.hex

    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.bin
    COMMENT "Generating HEX and BIN files"
)