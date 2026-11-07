cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(services STATIC)

file(GLOB SERVICES_SOURCE
    "cJSON/*.c"
    "cqueue/*.c"
    "fatfs/src/*.c"
    "fatfs/src/option/*.c"
    "fatfs/Target/*.c"
    "fatfs/App/*.c"
    "spif/*.c"
    "flash/*.c"
    "littlefs/*.c"
    "logger/*.c"
    "mmb_serial/*.c"
    "modbus/functions/*.c"
    "nanomodbus/*.c"
    "rpc/*.c"
    "shell/*.c"
    "socket_client/*.c"
    "socket_server/*.c"
    "sx_dev/*.c"
    "tcp_client/*.c"
    "tcp_server/*.c"
    "thingsboard/*.c"
    "udp_client/*.c"
    "udp_server/*.c"
    "zigbee/*.c"
)

target_sources(services PRIVATE ${SERVICES_SOURCE})

target_include_directories(services PUBLIC 
    "cJSON"
    "cqueue"
    "fatfs/src"
    "fatfs/Target"
    "fatfs/App"
    "spif"
    "flash"
    "littlefs"
    "logger"
    "mmb_serial"
    "modbus/include"
    "nanomodbus"
    "rpc"
    "shell"
    "socket_client"
    "socket_server"
    "sx_dev"
    "tcp_client"
    "tcp_server"
    "thingsboard"
    "udp_client"
    "udp_server"
    "zigbee"
)

target_link_libraries(services PRIVATE 
        stm32cubemx
        apps
        board
        utils
        lwip
        webserver
        tls
)   