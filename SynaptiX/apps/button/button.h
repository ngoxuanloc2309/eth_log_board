#ifndef BUTTON_H
#define BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "board.h"

typedef enum BUTTON_EVENT{
    BT_EV_RELEASE = 0,
    BT_EV_CLICKED = 1,
    BT_EV_DOUBLE_CLICK = 2,
    BT_EV_TRIPPER_CLICK = 3,
    BT_EV_HOLDED,
}BUTTON_EVENT_t;

typedef int (*BT_Handlle_t)(BUTTON_EVENT_t event,void *arg);

/**
 * @brief Initializes a button with the specified handle and arguments.
 * 
 * @param handle Button handle for identifying the button instance.
 * @param arg Pointer to optional arguments for button configuration.
 * 
 * @return void
 * 
 * @note Ensure that the button handle is valid before calling this function.
 */
void button_init(BT_Handlle_t handle,void *arg);
/**
 * @brief Polls the button state and processes button events.
 * 
 * This function should be called periodically to check the button status
 * and handle any button press or release events based on the provided timestamp.
 * 
 * @param time_stamp The current timestamp in milliseconds used for debouncing
 *                   and tracking button press duration.
 * 
 * @return void
 */
void button_poll(int time_stamp);

#ifdef __cplusplus
}
#endif

#endif