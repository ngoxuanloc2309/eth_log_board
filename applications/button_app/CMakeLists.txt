cmake_minimum_required(VERSION 3.22)

add_library(button_app STATIC
    button_app.c
)

target_include_directories(button_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(button_app PUBLIC c_std_11)