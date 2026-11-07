cmake_minimum_required(VERSION 3.22)

add_library(tusb_port STATIC)

target_sources(tusb_port PRIVATE
  usb_descriptors.c
  usb_netif.c
)

target_include_directories(tusb_port PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tusb_port PRIVATE stm32cubemx tinyusb lwip synaptix)