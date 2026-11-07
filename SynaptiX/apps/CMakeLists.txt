cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(apps STATIC)

file(GLOB APP_SOURCE
    "*.c"
    "button/*.c"
    "filesystem/*.c"
    "function/*.c"
    "http_server/*.c"
    "modbus_slave/*.c"
    "mqtt/*.c"
    "ota/*.c"
    "rf_app/*.c"
    "settings/*.c"
    "shell/*.c"
    "sntp/*.c"
    "sx_420mA/*.c"
    "sx_485_io/*.c"
    "sx_metter/*.c"
    "sx_ths/*.c"
    "transparent/*.c"
)

target_sources(apps PRIVATE ${APP_SOURCE})

target_include_directories(apps PUBLIC 
            .
            button
            filesystem
            "function"
            http_server
            modbus_slave
            mqtt
            ota
            rf_app
            settings
            shell
            sntp
            sx_420mA
            sx_485_io
            sx_metter
            sx_ths
            transparent
)

target_link_libraries(apps PRIVATE 
        stm32cubemx
        board
        services
        utils
        lwip
        lwip_apps
        webserver
        libraries
)   