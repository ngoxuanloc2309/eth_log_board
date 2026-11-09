cmake_minimum_required(VERSION 3.22)

add_library(indicator_app STATIC
    indicator_app.c
)

target_include_directories(indicator_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../interface
)
target_compile_features(indicator_app PUBLIC c_std_11)