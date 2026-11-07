cmake_minimum_required(VERSION 3.22)

enable_language(C ASM)
# Add libraries
add_library(libraries STATIC)
# Add core http
add_subdirectory(coreHTTP)

target_compile_definitions(core_http INTERFACE 
    HTTP_DO_NOT_USE_CUSTOM_CONFIG
)

add_subdirectory(tinyusb/src)

add_library(tinyusb STATIC)

tinyusb_target_add(tinyusb)

file(GLOB USB_NET_LIB_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/lib/networking/*.c")

target_sources(tinyusb PRIVATE 
        # ${CMAKE_CURRENT_SOURCE_DIR}/../TUSB/usb_descriptors.c
        ${USB_NET_LIB_SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/portable/st/stm32_fsdev/hcd_stm32_fsdev.c
        )
target_include_directories(tinyusb PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/common 
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb/src/device)

target_link_libraries(tinyusb PUBLIC 
    lwip
    synaptix
    tusb_port
    stm32cubemx
    )

# Link Libraries
target_link_libraries(libraries PUBLIC 
                    core_http
                    tinyusb
                    )