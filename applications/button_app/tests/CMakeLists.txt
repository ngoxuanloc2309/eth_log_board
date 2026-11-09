cmake_minimum_required(VERSION 3.22)
project(button_app_tests LANGUAGES C)

include(CTest)

add_executable(button_app_test
    ../button_app.c
    test_button_app.c
)

target_include_directories(button_app_test PRIVATE ..)
target_compile_features(button_app_test PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(button_app_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )
endif()

add_test(NAME button_gesture_state_machine COMMAND button_app_test)
set_tests_properties(button_gesture_state_machine PROPERTIES TIMEOUT 5)
