# STM32 UART driver

The board UART interface wraps the CubeMX-initialized LPUART1, UART7, USART3,
and USART10 peripherals. Initialize the selected peripheral with its `MX_*`
function before installing the driver. Install/uninstall and read/write calls
must run in task context.

Choose the transfer mode per UART:

- `SX_UART_MODE_POLLING` uses blocking HAL transfers.
- `SX_UART_MODE_INTERRUPT` continuously receives one byte at a time through
  HAL interrupts and buffers received bytes in a statically backed FreeRTOS
  stream buffer.
- `SX_UART_MODE_DMA` uses HAL receive-to-idle DMA and DMA transmit, with RX and
  TX DMA handles linked to the CubeMX UART handle.

For example:

```c
sx_uart_config_t config = {
    .mode = SX_UART_MODE_INTERRUPT,
    .rx_buffer_size = 256U
};

if (!sx_uart_install(SX_UART_PORT_USART3, &config)) {
    /* Handle initialization failure. */
}
```

RX storage is statically allocated and limited to 512 bytes per port. A zero
configured RX size selects the 256-byte default. The project currently has no
UART DMA handles configured, so DMA mode will fail cleanly until RX/TX DMA
channels and their IRQ handlers are configured in CubeMX. Do not uninstall a
port while tasks are reading or writing it. The byte-oriented API supports
7-bit and 8-bit data; 9-bit data without parity is rejected.

`sx_uart_get_rx_bytes_available()` reports the current number of buffered RX
bytes in interrupt and DMA modes. It returns `-1` for an invalid or uninstalled
port, or for polling mode, which has no RX buffer to query.
