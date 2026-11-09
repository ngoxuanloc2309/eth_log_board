#include "button_app.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_single_click_is_ignored(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    assert(button_app_process(&app, false, 0U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 10U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 40U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 100U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 130U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 531U) == BUTTON_APP_ACTION_NONE);
}

static void test_double_click_requests_reboot_once(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    assert(button_app_process(&app, false, 0U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 10U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 40U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 100U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 130U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 500U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 530U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 550U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 580U) == BUTTON_APP_ACTION_REBOOT);
    assert(button_app_process(&app, false, 590U) == BUTTON_APP_ACTION_NONE);
}

static void test_second_press_started_in_window_can_end_later(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    (void)button_app_process(&app, false, 100U);
    (void)button_app_process(&app, false, 130U);

    assert(button_app_process(&app, true, 520U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 550U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 560U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 590U) == BUTTON_APP_ACTION_REBOOT);
}

static void test_second_press_at_double_click_boundary_is_accepted(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    (void)button_app_process(&app, false, 100U);
    (void)button_app_process(&app, false, 130U);

    assert(button_app_process(&app, true, 530U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 560U) == BUTTON_APP_ACTION_NONE);
    (void)button_app_process(&app, false, 570U);
    assert(button_app_process(&app, false, 600U) == BUTTON_APP_ACTION_REBOOT);
}

static void test_late_second_press_is_not_a_double_click(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    (void)button_app_process(&app, false, 100U);
    (void)button_app_process(&app, false, 130U);
    (void)button_app_process(&app, false, 531U);

    assert(button_app_process(&app, true, 540U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 570U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 580U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 610U) == BUTTON_APP_ACTION_NONE);
}

static void test_debounce_rejects_contact_bounce(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, false, 20U);
    (void)button_app_process(&app, true, 30U);
    assert(button_app_process(&app, true, 60U) == BUTTON_APP_ACTION_NONE);
    (void)button_app_process(&app, false, 70U);
    (void)button_app_process(&app, true, 80U);
    (void)button_app_process(&app, false, 90U);
    assert(button_app_process(&app, false, 120U) == BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 521U) == BUTTON_APP_ACTION_NONE);
}

static void test_long_hold_requests_upgrade_once(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    assert(button_app_process(&app, true, 10039U) ==
           BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true, 10040U) ==
           BUTTON_APP_ACTION_UPGRADE_MODE);
    assert(button_app_process(&app, true, 10050U) ==
           BUTTON_APP_ACTION_NONE);
}

static void test_short_hold_never_requests_upgrade(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    assert(button_app_process(&app, true, 10039U) ==
           BUTTON_APP_ACTION_NONE);
    (void)button_app_process(&app, false, 10039U);
    assert(button_app_process(&app, false, 10069U) ==
           BUTTON_APP_ACTION_NONE);
}

static void test_release_at_hold_threshold_requests_upgrade(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    assert(button_app_process(&app, false, 10040U) ==
           BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, false, 10070U) ==
           BUTTON_APP_ACTION_UPGRADE_MODE);
}

static void test_long_hold_preempts_pending_double_click(void)
{
    button_app_t app;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, 0U);
    (void)button_app_process(&app, true, 10U);
    (void)button_app_process(&app, true, 40U);
    (void)button_app_process(&app, false, 100U);
    (void)button_app_process(&app, false, 130U);
    (void)button_app_process(&app, true, 500U);
    (void)button_app_process(&app, true, 530U);
    assert(button_app_process(&app, true, 10530U) ==
           BUTTON_APP_ACTION_UPGRADE_MODE);
}

static void test_hold_timer_wraps_safely(void)
{
    button_app_t app;
    const uint32_t press_time = UINT32_MAX - 60U;

    assert(button_app_init(&app));
    (void)button_app_process(&app, false, UINT32_MAX - 100U);
    (void)button_app_process(&app, true, UINT32_MAX - 90U);
    (void)button_app_process(&app, true, press_time);
    assert(button_app_process(&app, true,
                              (uint32_t)(press_time + 9999U)) ==
           BUTTON_APP_ACTION_NONE);
    assert(button_app_process(&app, true,
                              (uint32_t)(press_time + 10000U)) ==
           BUTTON_APP_ACTION_UPGRADE_MODE);
}

static void test_invalid_state_is_reported(void)
{
    button_app_t app = {0};

    assert(!button_app_init(NULL));
    assert(button_app_process(NULL, false, 0U) == BUTTON_APP_ACTION_ERROR);
    assert(button_app_process(&app, false, 0U) == BUTTON_APP_ACTION_ERROR);
}

int main(void)
{
    test_single_click_is_ignored();
    test_double_click_requests_reboot_once();
    test_second_press_started_in_window_can_end_later();
    test_second_press_at_double_click_boundary_is_accepted();
    test_late_second_press_is_not_a_double_click();
    test_debounce_rejects_contact_bounce();
    test_long_hold_requests_upgrade_once();
    test_short_hold_never_requests_upgrade();
    test_release_at_hold_threshold_requests_upgrade();
    test_long_hold_preempts_pending_double_click();
    test_hold_timer_wraps_safely();
    test_invalid_state_is_reported();
    (void)puts("Button gesture tests passed");
    return 0;
}
