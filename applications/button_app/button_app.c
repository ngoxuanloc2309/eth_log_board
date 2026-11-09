#include "button_app.h"

#include <stddef.h>
#include <string.h>

static bool button_app_elapsed_at_least(uint32_t now_ms,
                                        uint32_t start_ms,
                                        uint32_t duration_ms)
{
    return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

bool button_app_init(button_app_t *app)
{
    if (app == NULL) {
        return false;
    }

    (void)memset(app, 0, sizeof(*app));
    app->initialized = true;
    return true;
}

button_app_action_t button_app_process(button_app_t *app,
                                      bool is_pressed,
                                      uint32_t now_ms)
{
    bool candidate_began_in_double_click_window = false;

    if (app == NULL || !app->initialized) {
        return BUTTON_APP_ACTION_ERROR;
    }
    if (app->terminal) {
        return BUTTON_APP_ACTION_NONE;
    }

    if (!app->sample_initialized) {
        app->sample_initialized = true;
        app->candidate_pressed = is_pressed;
        app->candidate_since_ms = now_ms;
    } else if (is_pressed != app->candidate_pressed) {
        app->candidate_pressed = is_pressed;
        app->candidate_since_ms = now_ms;
    }

    if (app->click_pending && app->candidate_pressed &&
        !app->stable_pressed) {
        candidate_began_in_double_click_window =
            (uint32_t)(app->candidate_since_ms - app->first_release_ms) <=
            BUTTON_APP_DOUBLE_CLICK_MS;
    }

    if (app->candidate_pressed != app->stable_pressed &&
        button_app_elapsed_at_least(now_ms, app->candidate_since_ms,
                                    BUTTON_APP_DEBOUNCE_MS)) {
        app->stable_pressed = app->candidate_pressed;
        if (app->stable_pressed) {
            if (app->click_pending &&
                candidate_began_in_double_click_window) {
                app->click_pending = false;
                app->second_press = true;
            } else {
                app->click_pending = false;
                app->second_press = false;
            }
            app->press_started_ms = now_ms;
        } else {
            if (button_app_elapsed_at_least(
                    app->candidate_since_ms, app->press_started_ms,
                    BUTTON_APP_UPGRADE_HOLD_MS)) {
                app->terminal = true;
                return BUTTON_APP_ACTION_UPGRADE_MODE;
            }
            if (app->second_press) {
                app->terminal = true;
                return BUTTON_APP_ACTION_REBOOT;
            }
            app->click_pending = true;
            app->first_release_ms = now_ms;
        }
    }

    if (app->stable_pressed && app->candidate_pressed &&
        button_app_elapsed_at_least(now_ms, app->press_started_ms,
                                    BUTTON_APP_UPGRADE_HOLD_MS)) {
        app->click_pending = false;
        app->terminal = true;
        return BUTTON_APP_ACTION_UPGRADE_MODE;
    }

    if (app->click_pending &&
        button_app_elapsed_at_least(now_ms, app->first_release_ms,
                                    BUTTON_APP_DOUBLE_CLICK_MS + 1U) &&
        !candidate_began_in_double_click_window) {
        app->click_pending = false;
        app->second_press = false;
    }

    return BUTTON_APP_ACTION_NONE;
}