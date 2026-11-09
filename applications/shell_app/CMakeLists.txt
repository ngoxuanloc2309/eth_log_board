cmake_minimum_required(VERSION 3.22)

add_library(shell_app STATIC)

target_sources(shell_app PUBLIC "cli_shell_command.c")

target_link_libraries(shell_app PUBLIC shell rf_app file_system file_system_port)