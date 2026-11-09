cmake_minimum_required(VERSION 3.22)
project(w25q_driver_tests LANGUAGES C)

include(CTest)

get_filename_component(W25Q_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(INTERFACE_ROOT
    "${W25Q_ROOT}/../../../interface/bus" ABSOLUTE
)

add_executable(w25q_driver_test
    "${W25Q_ROOT}/w25q.c"
    test_w25q.c
)

target_include_directories(w25q_driver_test PRIVATE
    "${W25Q_ROOT}"
    "${INTERFACE_ROOT}"
)

add_test(NAME w25q_driver_operations COMMAND w25q_driver_test)
set_tests_properties(w25q_driver_operations PROPERTIES TIMEOUT 10)
