cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(synaptix INTERFACE)

add_subdirectory(apps)

add_subdirectory(board)

add_subdirectory(services)

add_subdirectory(utils)

# target_compile_definitions(synaptix INTERFACER
# )

target_link_libraries(synaptix INTERFACE
    apps
    board
    services
    utils
    
)