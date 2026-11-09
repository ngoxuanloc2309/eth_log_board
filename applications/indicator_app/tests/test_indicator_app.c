#include "indicator_app.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_startup_and_independent_link_output(void)
{
    indicator_app_t app;
    indicator_led_frame_t frame;

    assert(indicator_app_init(&app) == INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, false, true, 0U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_POWER] == INDICATOR_LED_OFF);
    assert(frame.output[INDICATOR_LED_LINK] == INDICATOR_LED_ON);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);

    assert(indicator_app_process(&app, true, false, 50U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_POWER] == INDICATOR_LED_ON);
    assert(frame.output[INDICATOR_LED_LINK] == INDICATOR_LED_OFF);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
}

static void test_heartbeat_timing_and_wrap(void)
{
    indicator_app_t app;
    indicator_led_frame_t frame;
    const uint32_t start_ms = UINT32_MAX - 50U;

    assert(indicator_app_init(&app) == INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, false, start_ms, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
    assert(indicator_app_process(
               &app, true, false,
               (uint32_t)(start_ms + INDICATOR_APP_HEARTBEAT_ON_MS),
               &frame) == INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);
    assert(indicator_app_process(
               &app, true, false,
               (uint32_t)(start_ms + INDICATOR_APP_HEARTBEAT_PERIOD_MS),
               &frame) == INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
}

static void test_each_error_code_has_expected_blink_count(void)
{
    static const indicator_error_t errors[] = {
        INDICATOR_ERROR_CONFIGURATION,
        INDICATOR_ERROR_STORAGE,
        INDICATOR_ERROR_NETWORK,
        INDICATOR_ERROR_MQTT,
        INDICATOR_ERROR_RADIO,
        INDICATOR_ERROR_UART,
        INDICATOR_ERROR_INTERNAL,
        INDICATOR_ERROR_UNKNOWN,
    };
    static const uint32_t blink_counts[] = {
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
    };
    indicator_app_t app;
    indicator_led_frame_t frame;
    size_t index;

    assert(INDICATOR_ERROR_NONE == 0x0000);
    assert(INDICATOR_ERROR_CONFIGURATION == 0x0002);
    assert(INDICATOR_ERROR_STORAGE == 0x0004);
    assert(INDICATOR_ERROR_NETWORK == 0x0008);
    assert(INDICATOR_ERROR_MQTT == 0x0010);
    assert(INDICATOR_ERROR_RADIO == 0x0020);
    assert(INDICATOR_ERROR_UART == 0x0040);
    assert(INDICATOR_ERROR_INTERNAL == 0x0080);
    assert(INDICATOR_ERROR_UNKNOWN == 0x0100);

    for (index = 0U; index < (sizeof(errors) / sizeof(errors[0])); index++) {
        const uint32_t pulse_start_ms =
                (blink_counts[index] - 1U) *
                (INDICATOR_APP_ERROR_PULSE_ON_MS +
                 INDICATOR_APP_ERROR_PULSE_OFF_MS);
        const uint32_t code_end_ms =
                (blink_counts[index] *
                 (INDICATOR_APP_ERROR_PULSE_ON_MS +
                  INDICATOR_APP_ERROR_PULSE_OFF_MS)) -
                INDICATOR_APP_ERROR_PULSE_OFF_MS;
        const uint32_t cycle_ms =
                blink_counts[index] *
                INDICATOR_APP_ERROR_PERIOD_PER_BLINK_MS;

        assert(indicator_app_init(&app) == INDICATOR_RESULT_OK);
        assert(indicator_app_set_error(&app, errors[index], true) ==
                   INDICATOR_RESULT_OK);
        assert(indicator_app_process(&app, true, false, 0U, &frame) ==
                   INDICATOR_RESULT_OK);
        assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
        assert(indicator_app_process(&app, true, false, pulse_start_ms,
                                         &frame) == INDICATOR_RESULT_OK);
        assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
        assert(indicator_app_process(&app, true, false, code_end_ms,
                                         &frame) == INDICATOR_RESULT_OK);
        assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);
        assert(indicator_app_process(&app, true, false,
                                     blink_counts[index] *
                                         (INDICATOR_APP_ERROR_PULSE_ON_MS +
                                          INDICATOR_APP_ERROR_PULSE_OFF_MS),
                                     &frame) == INDICATOR_RESULT_OK);
        assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);
        assert(indicator_app_process(&app, true, false, cycle_ms - 1U,
                                     &frame) == INDICATOR_RESULT_OK);
        assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);
        assert(indicator_app_process(&app, true, false, cycle_ms,
                                     &frame) == INDICATOR_RESULT_OK);
        assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
    }
}

static void test_storage_code_group_pause(void)
{
    indicator_app_t app;
    indicator_led_frame_t frame;

    assert(indicator_app_init(&app) == INDICATOR_RESULT_OK);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_STORAGE, true) ==
           INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, false, 0U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, false, 599U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
    assert(indicator_app_process(&app, true, false, 600U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);
    assert(indicator_app_process(&app, true, false, 1999U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);
    assert(indicator_app_process(&app, true, false, 2000U, &frame) ==
           INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
}

static void test_active_error_priority_and_clear(void)
{
    indicator_app_t app;
    indicator_led_frame_t frame;

    assert(indicator_app_init(&app) == INDICATOR_RESULT_OK);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_MQTT, true) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, true, 0U, &frame) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_STORAGE, true) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, true, 10U, &frame) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(
                   &app, true, true,
                   10U + INDICATOR_APP_ERROR_PULSE_ON_MS +
                       INDICATOR_APP_ERROR_PULSE_OFF_MS,
                   &frame) == INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);

    assert(indicator_app_set_error(&app, INDICATOR_ERROR_INTERNAL, true) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, true, 420U, &frame) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(
                   &app, true, true,
                   420U + 6U * (INDICATOR_APP_ERROR_PULSE_ON_MS +
                                INDICATOR_APP_ERROR_PULSE_OFF_MS),
                   &frame) == INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);

    assert(indicator_app_set_error(&app, INDICATOR_ERROR_INTERNAL, false) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, true, 2830U, &frame) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(
                   &app, true, true,
                   2830U + 2U * (INDICATOR_APP_ERROR_PULSE_ON_MS +
                                 INDICATOR_APP_ERROR_PULSE_OFF_MS),
                   &frame) == INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_OFF);

    assert(indicator_app_set_error(&app, INDICATOR_ERROR_STORAGE, false) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, true, 3640U, &frame) ==
               INDICATOR_RESULT_OK);
    assert(indicator_app_process(&app, true, true, 3690U, &frame) ==
               INDICATOR_RESULT_OK);
    assert(frame.output[INDICATOR_LED_STATUS] == INDICATOR_LED_ON);
}

static void test_invalid_arguments_and_uninitialized_state(void)
{
    indicator_app_t app = {0};
    indicator_led_frame_t frame;

    assert(indicator_app_init(NULL) == INDICATOR_RESULT_INVALID_ARGUMENT);
    assert(indicator_app_set_error(NULL, INDICATOR_ERROR_STORAGE, true) ==
           INDICATOR_RESULT_INVALID_ARGUMENT);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_STORAGE, true) ==
           INDICATOR_RESULT_NOT_INITIALIZED);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_NONE, true) ==
           INDICATOR_RESULT_NOT_INITIALIZED);

    assert(indicator_app_init(&app) == INDICATOR_RESULT_OK);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_NONE, true) ==
           INDICATOR_RESULT_INVALID_ARGUMENT);
    assert(indicator_app_set_error(&app, INDICATOR_ERROR_COUNT, true) ==
           INDICATOR_RESULT_INVALID_ARGUMENT);
    assert(indicator_app_set_error(
               &app, (indicator_error_t)0x0001, true) ==
           INDICATOR_RESULT_INVALID_ARGUMENT);
    assert(indicator_app_process(&app, true, false, 0U, NULL) ==
           INDICATOR_RESULT_INVALID_ARGUMENT);
    assert(indicator_app_process(NULL, true, false, 0U, &frame) ==
           INDICATOR_RESULT_INVALID_ARGUMENT);
}

int main(void)
{
    test_startup_and_independent_link_output();
    test_heartbeat_timing_and_wrap();
    test_each_error_code_has_expected_blink_count();
    test_storage_code_group_pause();
    test_active_error_priority_and_clear();
    test_invalid_arguments_and_uninitialized_state();
    (void)puts("Indicator application tests passed");
    return 0;
}
