#ifndef RF_APP_TEST_SX_UART_H
#define RF_APP_TEST_SX_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SX_UART_RX_BUFFER_MAX (512U)
#define SX_UART_PORT_UART9 (4U)
#define portMAX_DELAY (UINT32_MAX)
#define pdMS_TO_TICKS(milliseconds) (milliseconds)

bool sx_uart_is_installed(uint32_t port);
int32_t sx_uart_write(uint32_t port,
                      const uint8_t *buffer,
                      size_t length,
                      uint32_t timeout_ticks);
int32_t sx_uart_read(uint32_t port,
                     uint8_t *buffer,
                     size_t length,
                     uint32_t timeout_ticks);
int32_t sx_uart_get_rx_bytes_available(uint32_t port);

#endif
