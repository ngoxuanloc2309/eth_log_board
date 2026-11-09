#ifndef DTU_BOARD_H
#define DTU_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h563xx.h"
#include "stm32h5xx_hal.h"
#include "usart.h"
#include "lptim.h"
#include "spi.h"
#include "gpio.h"
#include "rtc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "timers.h"
#include "button_control_if.h"
#include "indicator_led_if.h"

#define bsp_delay(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#define bsp_restart NVIC_SystemReset


void enter_bootloader_mode(void);

/**
 * @brief Get the board implementation of button and system-control inputs.
 *
 * @return Board-owned interface table.
 */
const button_control_if_t *dtu_board_button_control_if(void);

/**
 * @brief Get the board implementation of the logical indicator LEDs.
 *
 * @return Board-owned LED interface table.
 */
const indicator_led_if_t *dtu_board_indicator_led_if(void);



#ifdef __cplusplus
}
#endif
#endif