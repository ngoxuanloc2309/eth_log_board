cmake_minimum_required(VERSION 3.22)
project(rf_app_topology_tests LANGUAGES C)

include(CTest)

get_filename_component(RF_APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(XLINK_ROOT
    "${RF_APP_ROOT}/../xlink_zb44" ABSOLUTE
)
get_filename_component(ZIGBEE_ROOT
    "${RF_APP_ROOT}/../../services/zigbee" ABSOLUTE
)

add_executable(rf_app_topology_test
    test_rf_app_topology.c
    "${XLINK_ROOT}/xlink_zb44.c"
)

target_include_directories(rf_app_topology_test BEFORE PRIVATE
    mocks/include
)

target_include_directories(rf_app_topology_test PRIVATE
    "${RF_APP_ROOT}"
    "${XLINK_ROOT}"
    "${ZIGBEE_ROOT}"
)

add_test(NAME rf_app_topology_parser COMMAND rf_app_topology_test)
set_tests_properties(rf_app_topology_parser PROPERTIES TIMEOUT 10)
