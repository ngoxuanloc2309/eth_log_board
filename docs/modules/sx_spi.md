# SPI1 Bus Adapter

## Purpose

Provide the hardware-independent `sx_spi_if_t` contract using the STM32H5
SPI1 peripheral initialized by CubeMX.

## Responsibilities

- Serialize synchronous and asynchronous full-duplex transfers.
- Translate HAL outcomes to `sx_bus_status_t`.
- Defer asynchronous completion callbacks to a dedicated FreeRTOS task.
- Use statically allocated RTOS objects; no heap allocation is performed.

## Public API

- `sx_spi_init()` — initialize the adapter after `MX_SPI1_Init()`.
- `sx_spi_get_interface()` — return the initialized `sx_spi_if_t`.

The SPI HAL supports transfers up to `UINT16_MAX` bytes per operation. The
adapter does not control chip select; the device-specific layer must assert
and release its own CS around complete transactions. Calls must be made from
task context.

## Data Flow

```text
Device driver
    ↓ sx_spi_if_t
sx_spi (BSP)
    ↓ HAL_SPI_TransmitReceive / HAL_SPI_TransmitReceive_IT
STM32H5 SPI1
```

## Initialization

`main()` calls `sx_spi_init()` immediately after `MX_SPI1_Init()`. The
application/device driver retrieves the interface with
`sx_spi_get_interface()` and supplies it to its device configuration.

## Thread Safety and Async Completion

A static FreeRTOS mutex serializes SPI1 and provides priority inheritance.
Synchronous transfers block while waiting for the bus and use the HAL
blocking transfer API. A bounded one-entry static queue carries asynchronous
requests to the service task, which owns the mutex during the HAL interrupt
transfer. The SPI ISR only records completion and signals the service task.
The service task invokes the callback and reports the requested byte count
on success, or zero on failure.

Asynchronous TX/RX buffers must remain valid until the callback returns.
Callbacks run on the `sx_spi` task, must be short and non-blocking, and should
not perform high-priority real-time work. The timeout bounds the wait for bus
ownership and, separately, the interrupt transfer. SPI1's interrupt priority
must remain at or below the FreeRTOS maximum syscall priority (numerically
greater than or equal to
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`).
