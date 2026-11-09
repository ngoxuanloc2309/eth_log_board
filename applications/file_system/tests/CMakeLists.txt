cmake_minimum_required(VERSION 3.22)
project(file_system_tests LANGUAGES C)

include(CTest)

get_filename_component(FIRMWARE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LITTLEFS_ROOT "${FIRMWARE_ROOT}/libs/fs")
set(W25Q_ROOT "${FIRMWARE_ROOT}/drivers/device/w25q")
set(FILE_SYSTEM_ROOT "${FIRMWARE_ROOT}/applications/file_system")
set(BUS_INTERFACE_ROOT "${FIRMWARE_ROOT}/interface/bus")

add_executable(file_system_test
    "${FILE_SYSTEM_ROOT}/file_system.c"
    "${LITTLEFS_ROOT}/lfs.c"
    "${LITTLEFS_ROOT}/lfs_util.c"
    "${W25Q_ROOT}/w25q.c"
    test_file_system.c
)

target_include_directories(file_system_test PRIVATE
    "${FILE_SYSTEM_ROOT}"
    "${LITTLEFS_ROOT}"
    "${W25Q_ROOT}"
    "${BUS_INTERFACE_ROOT}"
)

target_compile_definitions(file_system_test PRIVATE LFS_NO_MALLOC)

add_test(NAME file_system_posix_operations COMMAND file_system_test)
set_tests_properties(file_system_posix_operations PROPERTIES TIMEOUT 20)
