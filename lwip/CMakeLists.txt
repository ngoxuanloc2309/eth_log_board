cmake_minimum_required(VERSION 3.22)

add_library(tcpip INTERFACE)

add_subdirectory(app)

add_subdirectory(system)

add_subdirectory(target)

target_link_libraries(tcpip INTERFACE lwipcore lwip_target lwip_app)