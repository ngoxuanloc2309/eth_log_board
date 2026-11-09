#ifndef BUTTON_APP_H
#define BUTTON_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_APP_SCAN_PERIOD_MS (10U)
#define BUTTON_APP_DEBOUNCE_MS (30U)
#define BUTTON_APP_DOUBLE_CLICK_MS (400U)
#define BUTTON_APP_UPGRADE_HOLD_MS (10000U)

typedef enum
{
    BUTTON_APP_ACTION_NONE = 0,
    BUTTON_APP_ACTION_REBOOT,
    BUTTON_APP_ACTION_UPGRADE_MODE,
    BUTTON_APP_ACTION_ERROR
} button_app_action_t;

/**
 * @brief Gesture recognition state for one physical button.
 */
typedef struct
{
    bool initialized;
    bool sample_initialized;
    bool candidate_pressed;
    bool stable_pressed;
    bool click_pending;
    bool second_press;
    bool terminal;
    uint32_t candidate_since_ms;
    uint32_t press_started_ms;
    uint32_t first_release_ms;
} button_app_t;

/**
 * @brief Initialize button gesture state.
 *
 * @param app State storage owned by the caller.
 * @return true when initialized; false when app is null.
 */
bool button_app_init(button_app_t *app);

/**
 * @brief Process one logical button sample and report a one-shot action.
 *
 * @param app Initialized gesture state.
 * @param is_pressed Current button sample from the board interface.
 * @param now_ms Monotonic wrapping millisecond counter.
 * @return Requested action, or BUTTON_APP_ACTION_ERROR for invalid state.
 */
button_app_action_t button_app_process(button_app_t *app,
                                      bool is_pressed,
                                      uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif