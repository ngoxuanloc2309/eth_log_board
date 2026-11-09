cmake_minimum_required(VERSION 3.22)

set(LITTLEFS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../libs/fs")

add_library(littlefs STATIC
    "${LITTLEFS_ROOT}/lfs.c"
    "${LITTLEFS_ROOT}/lfs_util.c"
)

target_include_directories(littlefs PUBLIC "${LITTLEFS_ROOT}")
target_compile_definitions(littlefs PUBLIC LFS_NO_MALLOC)

add_library(file_system STATIC
    file_system.c
)

target_include_directories(file_system PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(file_system PUBLIC w25q_driver littlefs)

add_library(file_system_port STATIC
    file_system_port.c
)

target_include_directories(file_system_port PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(file_system_port PUBLIC file_system bsp)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
