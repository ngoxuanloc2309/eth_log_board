cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(board STATIC)

file(GLOB BOARD_SOURCE
    "*.c"
    "spif/*.c"
    "qspif/*.c"
)

target_sources(board PRIVATE ${BOARD_SOURCE})

target_include_directories(board PUBLIC 
            .
            spif
            qspif
)

target_link_libraries(board PRIVATE 
        stm32cubemx
        apps
        services
        utils
        lwip
        libraries
)   