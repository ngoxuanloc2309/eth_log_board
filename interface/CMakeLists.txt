add_library(bus_interfaces INTERFACE)

target_include_directories(bus_interfaces INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/bus
    ${CMAKE_CURRENT_SOURCE_DIR}
)
