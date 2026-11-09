#ifndef SX_BUS_H
#define SX_BUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Timeout value for operations that wait indefinitely.
 */
#define SX_BUS_WAIT_FOREVER (UINT32_MAX)

/**
 * @brief Result codes shared by bus interfaces.
 */
typedef enum
{
    SX_BUS_STATUS_OK = 0,
    SX_BUS_STATUS_INVALID_PARAM,
    SX_BUS_STATUS_NOT_READY,
    SX_BUS_STATUS_BUSY,
    SX_BUS_STATUS_TIMEOUT,
    SX_BUS_STATUS_IO_ERROR
} sx_bus_status_t;

/**
 * @brief Completion callback for asynchronous bus operations.
 *
 * Implementations must invoke this callback from task context, never directly
 * from an interrupt handler. The transferred count is the number of payload
 * bytes completed; it may be less than the requested length on failure.
 * Every accepted asynchronous operation invokes its callback exactly once.
 * Rejected submissions do not invoke the callback.
 *
 * @param user_context Context supplied when the operation was submitted.
 * @param status Operation result.
 * @param transferred Number of payload bytes transferred.
 */
typedef void (*sx_bus_completion_callback_t)(void *user_context,
                                             sx_bus_status_t status,
                                             uint32_t transferred);

#ifdef __cplusplus
}
#endif

#endif
