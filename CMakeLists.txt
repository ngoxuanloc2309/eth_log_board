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
set(CMAKE_PROJECT_NAME RF_Gateway)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})
set(CMAKE_PROJECT_VERSION 2.0.0)
message(STATUS "Project version: ${CMAKE_PROJECT_VERSION}")
message(STATUS "Project name: ${CMAKE_PROJECT_NAME}")
# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

add_subdirectory(SynaptiX)

add_subdirectory(lwip_app)

add_subdirectory(lwip)

add_subdirectory(TLS)

add_subdirectory(mongoose)

add_subdirectory(usb)

add_subdirectory(libs)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    SynaptiX/apps
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE 
    "LINKER:-u,_printf_float"
    "LINKER:-u,_scanf_float"
)
# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

target_link_libraries(stm32cubemx INTERFACE
    synaptix

)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    synaptix
    lwip
    webserver
    tls
    lwip_apps
    tusb_port
    libraries
    # Add user defined libraries
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.hex

    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}>
            ${PROJECT_NAME}.bin

    COMMENT "Generating HEX and BIN files"
)

# add_custom_command(
#     TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND STM32_Programmer_CLI
#             -c port=SWD mode=UR reset=HWrst
#             -w ${PROJECT_NAME}.hex 0x08010000
#             -v
#             -rst
#     COMMENT "Programming STM32..."
# )