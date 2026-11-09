cmake_minimum_required(VERSION 3.22)

add_library(shell STATIC)

target_sources(shell PRIVATE "cli_shell.c")

target_include_directories(shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(shell PRIVATE)