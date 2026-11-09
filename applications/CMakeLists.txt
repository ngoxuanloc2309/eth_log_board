cmake_minimum_required(VERSION 3.22)

add_library(Application INTERFACE)

# add_subdirectory(mqtt-client)

add_subdirectory(file_system)
add_subdirectory(button_app)
add_subdirectory(indicator_app)
add_subdirectory(settings)
add_subdirectory(mqtt_client_posix)
add_subdirectory(tcp)
add_subdirectory(rf_app)
add_subdirectory(shell_app)
add_subdirectory(xlink_zb44)
add_subdirectory(web_server)
target_link_libraries(Application INTERFACE
    file_system
    file_system_port
    button_app
    indicator_app
    settings
    mqtt_client_posix
    tcp_application
    rf_app
    shell_app
    xlink_zb44
    web_server
)