#ifndef BUTTON_CONTROL_IF_H
#define BUTTON_CONTROL_IF_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Board-independent button and system-control operations.
 */
typedef struct
{
    void *context;

    /**
     * @brief Read the logical state of the device button.
     *
     * @param context Implementation context.
     * @return true when the button is pressed.
     */
    bool (*is_pressed)(void *context);

    /**
     * @brief Request a device reboot from task context.
     *
     * @param context Implementation context.
     */
    void (*request_reboot)(void *context);

    /**
     * @brief Enter the board's firmware-upgrade boot mode from task context.
     *
     * @param context Implementation context.
     */
    void (*request_upgrade_mode)(void *context);
} button_control_if_t;

#ifdef __cplusplus
}
#endif

#endif
