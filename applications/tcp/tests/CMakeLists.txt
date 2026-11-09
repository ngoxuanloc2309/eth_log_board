cmake_minimum_required(VERSION 3.22)
project(tcp_client_tests LANGUAGES C)

include(CTest)

get_filename_component(TCP_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_executable(tcp_client_ring_test
    "${TCP_ROOT}/tcp_client.c"
    test_tcp_client.c
)

target_include_directories(tcp_client_ring_test BEFORE PRIVATE
    mocks/include
)

target_include_directories(tcp_client_ring_test PRIVATE
    "${TCP_ROOT}"
)

add_test(NAME tcp_client_ring_buffers COMMAND tcp_client_ring_test)
