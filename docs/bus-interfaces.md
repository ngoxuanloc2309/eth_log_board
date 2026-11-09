# Bus Interface Contracts

## Purpose

The contracts in `interface/bus/` let device drivers use SPI, I2C, and UART/USART
without depending on STM32 HAL handles or FreeRTOS types. Implementations are
owned by the BSP and may use STM32H563 HAL and FreeRTOS internally.

```text
Device driver
     ↓
sx_spi_if_t / sx_i2c_if_t / sx_uart_if_t
     ↓
BSP bus implementation
     ↓
STM32H563 HAL
```

## Ownership and thread safety

- The BSP owns peripheral setup, interrupt handling, serialization, and
  completion deferral.
- Bus calls are made from task context, not from an ISR.
- The implementation serializes operations. Blocking operations may wait
  for bus ownership according to their timeout. An asynchronous submission
  may return `SX_BUS_STATUS_BUSY` if its bounded request slot is occupied.
- SPI chip-select is owned by the device-specific layer; the generic SPI
  contract does not assert or release chip-select.
- Asynchronous operation buffers remain caller-owned and must stay valid until
  the completion callback returns.
- Completion and UART receive callbacks run in task context. Implementations
  must defer them from the HAL ISR using FreeRTOS-safe synchronization.
- An accepted asynchronous operation calls its completion callback exactly
  once, including on timeout or I/O error. A rejected submission does not call
  the callback.
- UART receive data passed to its callback is implementation-owned and valid
  only until the callback returns.
- Do not uninstall or destroy an implementation while operations or callbacks
  are active.

## Public contracts

| Contract | Operations | Target/availability |
|----------|------------|---------------------|
| `sx_spi_if_t` | Blocking and asynchronous full-duplex transfer | `sx_spi` adapts initialized STM32H5 SPI1 |
| `sx_i2c_if_t` | Write, read, and combined write/read, blocking and asynchronous | Contract only; no I2C peripheral is configured |
| `sx_uart_if_t` | Blocking/asynchronous TX and callback-based stream RX | Configured UART/USART ports; adapter to this contract is not yet implemented |

`SX_BUS_WAIT_FOREVER` requests an unbounded wait for blocking calls. I2C
addresses are unshifted 7-bit values. Combined I2C operations must use a
repeated START between command and response phases.

## Implementation status

The `sx_spi` BSP adapter binds `sx_spi_if_t` to CubeMX-initialized SPI1 and
uses statically allocated FreeRTOS synchronization. The existing `sx_uart` BSP
API is not yet an adapter for `sx_uart_if_t`. The I2C adapter remains deferred
until an I2C peripheral, pins, and CubeMX/HAL initialization are selected.
