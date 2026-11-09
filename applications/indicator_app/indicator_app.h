#ifndef INDICATOR_APP_H
#define INDICATOR_APP_H

#include "indicator_led_if.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INDICATOR_APP_UPDATE_PERIOD_MS (50U)
#define INDICATOR_APP_HEARTBEAT_PERIOD_MS (2000U)
#define INDICATOR_APP_HEARTBEAT_ON_MS (100U)
#define INDICATOR_APP_ERROR_PULSE_ON_MS (200U)
#define INDICATOR_APP_ERROR_PULSE_OFF_MS (200U)
#define INDICATOR_APP_ERROR_PERIOD_PER_BLINK_MS (1000U)

typedef enum
{
    INDICATOR_ERROR_NONE = 0x0000,
    INDICATOR_ERROR_CONFIGURATION = (0x0001U << 1U),
    INDICATOR_ERROR_STORAGE = (0x0001U << 2U),
    INDICATOR_ERROR_NETWORK = (0x0001U << 3U),
    INDICATOR_ERROR_MQTT = (0x0001U << 4U),
    INDICATOR_ERROR_RADIO = (0x0001U << 5U),
    INDICATOR_ERROR_UART = (0x0001U << 6U),
    INDICATOR_ERROR_INTERNAL = (0x0001U << 7U),
    INDICATOR_ERROR_UNKNOWN = (0x0001U << 8U),
    INDICATOR_ERROR_COUNT = (0x0001U << 9U)
} indicator_error_t;

typedef enum
{
    INDICATOR_RESULT_OK = 0,
    INDICATOR_RESULT_INVALID_ARGUMENT,
    INDICATOR_RESULT_NOT_INITIALIZED
} indicator_result_t;

typedef struct
{
    bool initialized;
    bool pattern_initialized;
    bool previous_application_ready;
    uint16_t active_error_mask;
    indicator_error_t selected_error;
    uint32_t pattern_started_ms;
} indicator_app_t;

/**
 * @brief Initialize indicator state owned by the caller.
 *
 * @param app State storage.
 * @return INDICATOR_RESULT_OK or an explicit argument error.
 */
indicator_result_t indicator_app_init(indicator_app_t *app);

/**
 * @brief Mark one subsystem error as active or cleared.
 *
 * Active errors are selected for display by documented priority, independent
 * of the enum's underlying value.
 *
 * @param app Initialized indicator state.
 * @param error Error to update; NONE is not a reportable error.
 * @param active true while the owning subsystem considers the fault active.
 * @return INDICATOR_RESULT_OK, INVALID_ARGUMENT, or NOT_INITIALIZED.
 */
indicator_result_t indicator_app_set_error(indicator_app_t *app,
                                           indicator_error_t error,
                                           bool active);

/**
 * @brief Compute logical power, link, and status LED outputs.
 *
 * @param app Initialized indicator state.
 * @param application_ready true once application startup is complete.
 * @param link_up Current physical Ethernet link state.
 * @param now_ms Monotonic wrapping millisecond timestamp.
 * @param frame Receives the complete logical LED frame.
 * @return INDICATOR_RESULT_OK, INVALID_ARGUMENT, or NOT_INITIALIZED.
 */
indicator_result_t indicator_app_process(indicator_app_t *app,
                                         bool application_ready,
                                         bool link_up,
                                         uint32_t now_ms,
                                         indicator_led_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif
