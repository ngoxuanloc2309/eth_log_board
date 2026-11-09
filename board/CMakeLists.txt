cmake_minimum_required(VERSION 3.22)

enable_language(C CXX ASM)

add_library(bsp STATIC)

file(GLOB BSP_SRC "dtu_board.c"
                  "dev/gpio/sx_gpio.c"
                  "dev/i2c/sx_i2c.c"
                  "dev/spi/sx_spi.c"
                  "dev/flash/sx_w25q.c"
                  "dev/timer/sx_timer.c"
                  "dev/uart/sx_uart.c")

target_sources(bsp PRIVATE ${BSP_SRC})

target_include_directories(bsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/gpio
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/i2c
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/spi
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/flash
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/timer
                ${CMAKE_CURRENT_SOURCE_DIR}/dev/uart
)

target_link_libraries(bsp PUBLIC stm32cubemx bus_interfaces w25q_driver)