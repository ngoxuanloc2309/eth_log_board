#ifndef RF_APP_H
#define RF_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "zigbee.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the RF application and start the Zigbee module handshake.
 *
 * @return true when initialization was started, otherwise false.
 */
bool rf_app_init(void);

/**
 * @brief Advance Zigbee protocol processing.
 *
 * @param elapsed_ms Time elapsed since the previous call, in milliseconds.
 */
void rf_app_process(uint32_t elapsed_ms);

/**
 * @brief Check whether the RF application has completed initialization.
 *
 * @return true when initialized, otherwise false.
 */
bool rf_app_is_initialized(void);

/**
 * @brief Check whether a Zigbee operation is in progress.
 *
 * @return true when an operation is pending, otherwise false.
 */
bool rf_app_is_busy(void);

/**
 * @brief Check whether a successful module-parameter read is available.
 *
 * @return true after at least one successful parameter read.
 */
bool rf_app_has_parameters(void);

/**
 * @brief Get the latest Zigbee module parameters.
 *
 * @return Pointer to the RF application's current parameter snapshot, or NULL
 *         before initialization.
 */
const ZigbeeParameter_t *rf_app_get_parameters(void);

/**
 * @brief Connect to the Zigbee module.
 *
 * @param callback Optional completion callback.
 * @param arg Callback context.
 * @return true if the operation was started, otherwise false.
 */
bool rf_app_connect(ZB_Callback_Func callback, void *arg);

/**
 * @brief Read Zigbee module parameters.
 *
 * @param callback Optional completion callback.
 * @param arg Callback context.
 * @return true if the operation was started, otherwise false.
 */
bool rf_app_read_parameters(ZB_Callback_Func callback, void *arg);

/**
 * @brief Write Zigbee module parameters.
 *
 * @param parameters Parameters to write.
 * @param callback Optional completion callback.
 * @param arg Callback context.
 * @return true if the operation was started, otherwise false.
 */
bool rf_app_write_parameters(const ZigbeeParameter_t *parameters,
                             ZB_Callback_Func callback,
                             void *arg);

/**
 * @brief Reset the Zigbee module.
 *
 * @param callback Optional completion callback.
 * @param arg Callback context.
 * @return true if the operation was started, otherwise false.
 */
bool rf_app_reset(ZB_Callback_Func callback, void *arg);

/**
 * @brief Put the Zigbee module into low-power mode.
 *
 * @param callback Optional completion callback.
 * @param arg Callback context.
 * @return true if the operation was started, otherwise false.
 */
bool rf_app_sleep(ZB_Callback_Func callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif