cmake_minimum_required(VERSION 3.22)

add_library(services INTERFACE)

add_subdirectory(zigbee)
add_subdirectory(shell)
target_include_directories(services INTERFACE )

target_link_libraries(services INTERFACE zigbee
                                         shell
)