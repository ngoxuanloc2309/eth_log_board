cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(utils STATIC)

file(GLOB UTILS_SOURCE
    "*.c"
)

target_sources(utils PRIVATE ${UTILS_SOURCE})

target_include_directories(utils PUBLIC 
            .
)

target_link_libraries(utils PRIVATE 
        stm32cubemx
)   