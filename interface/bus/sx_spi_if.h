#ifndef SX_SPI_IF_H
#define SX_SPI_IF_H

#include <stdint.h>

#include "sx_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware-independent SPI bus contract.
 *
 * The implementation owns peripheral serialization. Blocking operations
 * wait for bus ownership according to timeout_ms; asynchronous submissions
 * may return SX_BUS_STATUS_BUSY when the implementation's request slot is in
 * use. Chip-select is device-specific and is intentionally not controlled
 * here.
 */
typedef struct
{
    void *context;

    /**
     * @brief Perform a blocking full-duplex transfer from task context.
     *
     * @param context Implementation context.
     * @param tx Transmit data; must remain valid until this function returns.
     * @param rx Receive storage; must remain valid until this function returns.
     * @param length Number of bytes to transfer; both buffers are required.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @return Operation status.
     */
    sx_bus_status_t (*transfer)(void *context,
                                const uint8_t *tx,
                                uint8_t *rx,
                                uint32_t length,
                                uint32_t timeout_ms);

    /**
     * @brief Submit an asynchronous full-duplex transfer.
     *
     * The TX and RX buffers must remain valid until the completion callback
     * returns. The callback is deferred to task context.
     *
     * @param context Implementation context.
     * @param tx Transmit data.
     * @param rx Receive storage.
     * @param length Number of bytes to transfer; both buffers are required.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @param callback Completion callback.
     * @param user_context Context passed to the callback.
     * @return SX_BUS_STATUS_OK when accepted, or an error if not accepted.
     */
    sx_bus_status_t (*transfer_async)(
        void *context,
        const uint8_t *tx,
        uint8_t *rx,
        uint32_t length,
        uint32_t timeout_ms,
        sx_bus_completion_callback_t callback,
        void *user_context);
} sx_spi_if_t;

#ifdef __cplusplus
}
#endif

#endif
