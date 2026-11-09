#ifndef SX_UART_H
#define SX_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SX_UART_RX_BUFFER_MAX 512U
#define SX_UART_DEFAULT_RX_BUFFER_SIZE 256U

typedef enum
{
    SX_UART_PORT_LPUART1 = 0,
    SX_UART_PORT_UART7,
    SX_UART_PORT_USART3,
    SX_UART_PORT_USART10,
    SX_UART_PORT_UART9,
    SX_UART_PORT_MAX
} sx_uart_port_t;

typedef enum
{
    SX_UART_MODE_POLLING = 0,
    SX_UART_MODE_INTERRUPT,
    SX_UART_MODE_DMA
} sx_uart_mode_t;

typedef enum
{
    SX_UART_DATA_7_BITS = 7,
    SX_UART_DATA_8_BITS = 8
} sx_uart_word_length_t;

typedef struct
{
    sx_uart_mode_t mode;
    size_t rx_buffer_size;
    uint32_t Baudrate;
    sx_uart_word_length_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
} sx_uart_config_t;

typedef struct
{
    uint32_t hal_errors;
    uint32_t rx_overflows;
} sx_uart_stats_t;

/*
 * UART peripherals must already be initialized by the application (for
 * example, with the MX_*_UART_Init functions). DMA mode additionally requires
 * linked RX and TX DMA handles.
 */
bool sx_uart_install(sx_uart_port_t port, const sx_uart_config_t *config);
bool sx_uart_uninstall(sx_uart_port_t port);
bool sx_uart_is_installed(sx_uart_port_t port);
bool sx_uart_get_stats(sx_uart_port_t port, sx_uart_stats_t *stats);

/*
 * Change the HAL word length while the driver is not installed. Only 7- and
 * 8-bit data are supported by this byte-oriented API.
 */
bool sx_uart_set_word_length(sx_uart_port_t port,
                             sx_uart_word_length_t data_bits);

/**
 * @brief Get the number of buffered RX bytes.
 *
 * The returned count is a snapshot and may change as bytes arrive or are
 * read. Polling mode has no RX buffer to query.
 *
 * @param port UART port to query.
 * @return Buffered RX byte count, or -1 if the port is invalid, uninstalled,
 *         or using polling mode.
 */
int32_t sx_uart_get_rx_bytes_available(sx_uart_port_t port);

/*
 * Read/write byte counts, or -1 on HAL/argument/timeout failure.
 * Polling mode uses the HAL blocking APIs; interrupt and DMA modes wait on
 * FreeRTOS synchronization while the HAL transfers asynchronously.
 * timeout_ticks applies to each call; portMAX_DELAY waits indefinitely.
 * Quiesce all read/write users before uninstalling a port.
 */
int32_t sx_uart_read(sx_uart_port_t port,
                     uint8_t *buffer,
                     size_t length,
                     TickType_t timeout_ticks);
int32_t sx_uart_write(sx_uart_port_t port,
                      const uint8_t *buffer,
                      size_t length,
                      TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif
