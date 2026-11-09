#include "indicator_app.h"

#include <stddef.h>
#include <string.h>

static bool indicator_app_error_mask(indicator_error_t error,
                                     uint16_t *mask)
{
    if (mask == NULL) {
        return false;
    }

    switch (error) {
    case INDICATOR_ERROR_CONFIGURATION:
    case INDICATOR_ERROR_STORAGE:
    case INDICATOR_ERROR_NETWORK:
    case INDICATOR_ERROR_MQTT:
    case INDICATOR_ERROR_RADIO:
    case INDICATOR_ERROR_UART:
    case INDICATOR_ERROR_INTERNAL:
    case INDICATOR_ERROR_UNKNOWN:
        *mask = (uint16_t)error;
        return true;
    case INDICATOR_ERROR_NONE:
    case INDICATOR_ERROR_COUNT:
    default:
        return false;
    }
}

static indicator_error_t indicator_app_select_error(uint16_t active_error_mask)
{
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_INTERNAL) != 0U) {
        return INDICATOR_ERROR_INTERNAL;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_CONFIGURATION) != 0U) {
        return INDICATOR_ERROR_CONFIGURATION;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_STORAGE) != 0U) {
        return INDICATOR_ERROR_STORAGE;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_NETWORK) != 0U) {
        return INDICATOR_ERROR_NETWORK;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_RADIO) != 0U) {
        return INDICATOR_ERROR_RADIO;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_UART) != 0U) {
        return INDICATOR_ERROR_UART;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_MQTT) != 0U) {
        return INDICATOR_ERROR_MQTT;
    }
    if ((active_error_mask & (uint16_t)INDICATOR_ERROR_UNKNOWN) != 0U) {
        return INDICATOR_ERROR_UNKNOWN;
    }
    return INDICATOR_ERROR_NONE;
}

static uint8_t indicator_app_error_blink_count(indicator_error_t error)
{
    switch (error) {
    case INDICATOR_ERROR_CONFIGURATION:
        return 1U;
    case INDICATOR_ERROR_STORAGE:
        return 2U;
    case INDICATOR_ERROR_NETWORK:
        return 3U;
    case INDICATOR_ERROR_MQTT:
        return 4U;
    case INDICATOR_ERROR_RADIO:
        return 5U;
    case INDICATOR_ERROR_UART:
        return 6U;
    case INDICATOR_ERROR_INTERNAL:
        return 7U;
    case INDICATOR_ERROR_UNKNOWN:
        return 8U;
    case INDICATOR_ERROR_NONE:
    case INDICATOR_ERROR_COUNT:
    default:
        return 0U;
    }
}

static bool indicator_app_status_output(indicator_error_t error,
                                        uint32_t elapsed_ms)
{
    uint32_t cycle_ms;
    uint32_t code_ms;
    uint32_t phase_ms;
    uint8_t blink_count;

    if (error == INDICATOR_ERROR_NONE) {
        return (elapsed_ms % INDICATOR_APP_HEARTBEAT_PERIOD_MS) <
               INDICATOR_APP_HEARTBEAT_ON_MS;
    }

    blink_count = indicator_app_error_blink_count(error);
    if (blink_count == 0U) {
        return false;
    }

    code_ms = ((uint32_t)blink_count *
               (INDICATOR_APP_ERROR_PULSE_ON_MS +
                INDICATOR_APP_ERROR_PULSE_OFF_MS)) -
              INDICATOR_APP_ERROR_PULSE_OFF_MS;
    cycle_ms = (uint32_t)blink_count *
               INDICATOR_APP_ERROR_PERIOD_PER_BLINK_MS;
    phase_ms = elapsed_ms % cycle_ms;
    if (phase_ms >= code_ms) {
        return false;
    }

    return (phase_ms %
            (INDICATOR_APP_ERROR_PULSE_ON_MS +
             INDICATOR_APP_ERROR_PULSE_OFF_MS)) <
           INDICATOR_APP_ERROR_PULSE_ON_MS;
}

indicator_result_t indicator_app_init(indicator_app_t *app)
{
    if (app == NULL) {
        return INDICATOR_RESULT_INVALID_ARGUMENT;
    }

    (void)memset(app, 0, sizeof(*app));
    app->initialized = true;
    app->selected_error = INDICATOR_ERROR_NONE;
    return INDICATOR_RESULT_OK;
}

indicator_result_t indicator_app_set_error(indicator_app_t *app,
                                           indicator_error_t error,
                                           bool active)
{
    uint16_t error_mask;

    if (app == NULL) {
        return INDICATOR_RESULT_INVALID_ARGUMENT;
    }
    if (!app->initialized) {
        return INDICATOR_RESULT_NOT_INITIALIZED;
    }
    if (!indicator_app_error_mask(error, &error_mask)) {
        return INDICATOR_RESULT_INVALID_ARGUMENT;
    }

    if (active) {
        app->active_error_mask |= error_mask;
    } else {
        app->active_error_mask &= (uint16_t)~error_mask;
    }
    return INDICATOR_RESULT_OK;
}

indicator_result_t indicator_app_process(indicator_app_t *app,
                                         bool application_ready,
                                         bool link_up,
                                         uint32_t now_ms,
                                         indicator_led_frame_t *frame)
{
    indicator_error_t selected_error;
    uint32_t elapsed_ms;

    if (app == NULL || frame == NULL) {
        return INDICATOR_RESULT_INVALID_ARGUMENT;
    }
    if (!app->initialized) {
        return INDICATOR_RESULT_NOT_INITIALIZED;
    }

    selected_error =
        indicator_app_select_error(app->active_error_mask);
    if (!app->pattern_initialized ||
        app->previous_application_ready != application_ready ||
        app->selected_error != selected_error) {
        app->pattern_started_ms = now_ms;
        app->selected_error = selected_error;
        app->previous_application_ready = application_ready;
        app->pattern_initialized = true;
    }

    frame->output[INDICATOR_LED_POWER] =
        application_ready ? INDICATOR_LED_ON : INDICATOR_LED_OFF;
    frame->output[INDICATOR_LED_LINK] =
        link_up ? INDICATOR_LED_ON : INDICATOR_LED_OFF;
    frame->output[INDICATOR_LED_STATUS] = INDICATOR_LED_OFF;

    if (!application_ready) {
        return INDICATOR_RESULT_OK;
    }

    elapsed_ms = (uint32_t)(now_ms - app->pattern_started_ms);
    frame->output[INDICATOR_LED_STATUS] =
        indicator_app_status_output(selected_error, elapsed_ms)
            ? INDICATOR_LED_ON
            : INDICATOR_LED_OFF;
    return INDICATOR_RESULT_OK;
}
