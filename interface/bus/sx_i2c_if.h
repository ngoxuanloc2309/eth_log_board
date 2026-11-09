#ifndef SX_I2C_IF_H
#define SX_I2C_IF_H

#include <stdint.h>

#include "sx_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware-independent I2C bus contract.
 *
 * Addresses are unshifted 7-bit I2C addresses. A combined write/read operation
 * must keep the bus transaction contiguous and issue a repeated START between
 * the write and read phases.
 */
typedef struct
{
    void *context;

    /**
     * @brief Write bytes to a 7-bit-addressed I2C target.
     *
     * @param context Implementation context.
     * @param address Unshifted 7-bit target address.
     * @param data Transmit data.
     * @param length Number of bytes to write.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @return Operation status.
     */
    sx_bus_status_t (*write)(void *context,
                             uint8_t address,
                             const uint8_t *data,
                             uint32_t length,
                             uint32_t timeout_ms);

    /**
     * @brief Read bytes from a 7-bit-addressed I2C target.
     *
     * @param context Implementation context.
     * @param address Unshifted 7-bit target address.
     * @param data Receive storage.
     * @param length Number of bytes to read.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @return Operation status.
     */
    sx_bus_status_t (*read)(void *context,
                            uint8_t address,
                            uint8_t *data,
                            uint32_t length,
                            uint32_t timeout_ms);

    /**
     * @brief Write a command and read its response in one transaction.
     *
     * @param context Implementation context.
     * @param address Unshifted 7-bit target address.
     * @param command Bytes to write before the repeated START.
     * @param command_length Number of command bytes.
     * @param response Receive storage.
     * @param response_length Number of response bytes.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @return Operation status.
     */
    sx_bus_status_t (*write_read)(void *context,
                                  uint8_t address,
                                  const uint8_t *command,
                                  uint32_t command_length,
                                  uint8_t *response,
                                  uint32_t response_length,
                                  uint32_t timeout_ms);

    /**
     * @brief Submit an asynchronous I2C write.
     *
     * The data buffer must remain valid until the completion callback returns.
     * The callback is deferred to task context.
     *
     * @param context Implementation context.
     * @param address Unshifted 7-bit target address.
     * @param data Transmit data.
     * @param length Number of bytes to write.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @param callback Completion callback.
     * @param user_context Context passed to the callback.
     * @return SX_BUS_STATUS_OK when accepted, or an error if not accepted.
     */
    sx_bus_status_t (*write_async)(
        void *context,
        uint8_t address,
        const uint8_t *data,
        uint32_t length,
        uint32_t timeout_ms,
        sx_bus_completion_callback_t callback,
        void *user_context);

    /**
     * @brief Submit an asynchronous I2C read.
     *
     * The response buffer must remain valid until the completion callback
     * returns. The callback is deferred to task context.
     *
     * @param context Implementation context.
     * @param address Unshifted 7-bit target address.
     * @param response Receive storage.
     * @param length Number of bytes to read.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @param callback Completion callback.
     * @param user_context Context passed to the callback.
     * @return SX_BUS_STATUS_OK when accepted, or an error if not accepted.
     */
    sx_bus_status_t (*read_async)(
        void *context,
        uint8_t address,
        uint8_t *response,
        uint32_t length,
        uint32_t timeout_ms,
        sx_bus_completion_callback_t callback,
        void *user_context);

    /**
     * @brief Submit an asynchronous combined I2C write/read transaction.
     *
     * Both buffers must remain valid until the completion callback returns.
     * The callback's transferred count is the total payload bytes completed.
     *
     * @param context Implementation context.
     * @param address Unshifted 7-bit target address.
     * @param command Bytes to write before the repeated START.
     * @param command_length Number of command bytes.
     * @param response Receive storage.
     * @param response_length Number of response bytes.
     * @param timeout_ms Timeout in milliseconds or SX_BUS_WAIT_FOREVER.
     * @param callback Completion callback.
     * @param user_context Context passed to the callback.
     * @return SX_BUS_STATUS_OK when accepted, or an error if not accepted.
     */
    sx_bus_status_t (*write_read_async)(
        void *context,
        uint8_t address,
        const uint8_t *command,
        uint32_t command_length,
        uint8_t *response,
        uint32_t response_length,
        uint32_t timeout_ms,
        sx_bus_completion_callback_t callback,
        void *user_context);
} sx_i2c_if_t;

#ifdef __cplusplus
}
#endif

#endif
