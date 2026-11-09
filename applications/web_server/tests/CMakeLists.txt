cmake_minimum_required(VERSION 3.22)

project(web_server_mock_api C)

add_executable(web_server_mock_api
    mock_api_server.c
)

target_compile_features(web_server_mock_api PRIVATE c_std_11)
target_compile_definitions(web_server_mock_api PRIVATE
    WEB_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/.."
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(web_server_mock_api PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Werror
    )
endif()
