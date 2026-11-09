#ifndef SX_UART_IF_H
#define SX_UART_IF_H

#include <stdint.h>

#include "sx_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Receive-data callback for an asynchronous UART stream.
 *
 * The data is owned by the implementation and is valid only for the duration
 * of the callback. Copy it if it must be retained. Callbacks run in task
 * context, never directly from an interrupt handler.
 *
 * @param user_context Context supplied when receive was started.
 * @param data Received bytes.
 * @param length Number of received bytes.
 * @param status Receive result.
 */
typedef void (*sx_uart_rx_callback_t)(void *user_context,
                                      const uint8_t *data,
                                      uint32_t length,
                                      sx_bus_status_t status);

/**
 * @brief Hardware-independent UART/USART stream contract.
 *
 * The same contract covers UART and USART peripherals. Implementations
 * serialize TX operations and report SX_BUS_STATUS_BUSY when a TX operation is
 * already active.
 */
typedef struct
{
    void *context;

    /**
     * @brief Write bytes and wait for completion from task context.
     *
     * @param context Implementation context.
     * @param data Transmit data; valid until this function returns.
     * @param length Number of bytes to write.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @return Operation status.
     */
    sx_bus_status_t (*write)(void *context,
                             const uint8_t *data,
                             uint32_t length,
                             uint32_t timeout_ms);

    /**
     * @brief Submit an asynchronous write.
     *
     * The TX buffer remains owned by the caller and must remain valid until
     * the completion callback returns. The callback is deferred to task
     * context. Every accepted operation completes once, including on timeout.
     *
     * @param context Implementation context.
     * @param data Transmit data.
     * @param length Number of bytes to write.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @param callback Completion callback.
     * @param user_context Context passed to the callback.
     * @return SX_BUS_STATUS_OK when accepted, or an error if not accepted.
     */
    sx_bus_status_t (*write_async)(
        void *context,
        const uint8_t *data,
        uint32_t length,
        uint32_t timeout_ms,
        sx_bus_completion_callback_t callback,
        void *user_context);

    /**
     * @brief Start continuous receive delivery.
     *
     * Only one receive callback may be registered for a UART interface.
     * Calling this while receiving returns SX_BUS_STATUS_BUSY.
     */
    sx_bus_status_t (*start_rx)(void *context,
                                sx_uart_rx_callback_t callback,
                                void *user_context);

    /**
     * @brief Stop continuous receive delivery.
     *
     * On success, no later receive callback will begin. A callback already
     * executing in task context may finish before this function returns.
     */
    sx_bus_status_t (*stop_rx)(void *context);
} sx_uart_if_t;

#ifdef __cplusplus
}
#endif

#endif
