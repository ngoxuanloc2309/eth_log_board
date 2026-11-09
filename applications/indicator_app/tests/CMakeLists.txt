cmake_minimum_required(VERSION 3.22)
project(indicator_app_tests LANGUAGES C)

include(CTest)

add_executable(indicator_app_test
    ../indicator_app.c
    test_indicator_app.c
)

target_include_directories(indicator_app_test PRIVATE
    ..
    ../../../interface
)
target_compile_features(indicator_app_test PRIVATE c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(indicator_app_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )
endif()

add_test(NAME indicator_state_and_patterns COMMAND indicator_app_test)
set_tests_properties(indicator_state_and_patterns PROPERTIES TIMEOUT 5)
