#include "dtu_board.h"
#include "main.h"

#include <stddef.h>

const char *dev[] = {
    "/dev/gpio/led0",
    "/dev/gpio/led1",
    "/dev/gpio/led2",
    "/dev/uart/"
};
#define SYSTEM_BOOTLOADER_ADDRESS  0x0BF97000UL
#define DTU_BUTTON_ACTIVE_LEVEL (GPIO_PIN_RESET)
#define DTU_LED_ACTIVE_HIGH (1U)

static bool dtu_board_button_is_pressed(void *context)
{
    (void)context;
    return HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin) ==
           DTU_BUTTON_ACTIVE_LEVEL;
}

static void dtu_board_request_reboot(void *context)
{
    (void)context;
    NVIC_SystemReset();
}

static void dtu_board_request_upgrade_mode(void *context)
{
    (void)context;
    enter_bootloader_mode();
}

static const button_control_if_t dtu_board_button_control = {
    .context = NULL,
    .is_pressed = dtu_board_button_is_pressed,
    .request_reboot = dtu_board_request_reboot,
    .request_upgrade_mode = dtu_board_request_upgrade_mode,
};

const button_control_if_t *dtu_board_button_control_if(void)
{
    return &dtu_board_button_control;
}

static bool dtu_board_indicator_frame_is_valid(
    const indicator_led_frame_t *frame)
{
    uint32_t index;

    if (frame == NULL) {
        return false;
    }

    for (index = 0U; index < (uint32_t)INDICATOR_LED_COUNT; index++) {
        if (frame->output[index] != INDICATOR_LED_OFF &&
            frame->output[index] != INDICATOR_LED_ON) {
            return false;
        }
    }
    return true;
}

static GPIO_PinState dtu_board_indicator_level(
    indicator_led_output_t output)
{
    const GPIO_PinState active_level =
        (DTU_LED_ACTIVE_HIGH != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    const GPIO_PinState inactive_level =
        (DTU_LED_ACTIVE_HIGH != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET;

    return (output == INDICATOR_LED_ON) ? active_level : inactive_level;
}

static bool dtu_board_indicator_apply_frame(
    void *context,
    const indicator_led_frame_t *frame)
{
    (void)context;
    if (!dtu_board_indicator_frame_is_valid(frame)) {
        return false;
    }

    HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin,
                      dtu_board_indicator_level(
                          frame->output[INDICATOR_LED_POWER]));
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
                      dtu_board_indicator_level(
                          frame->output[INDICATOR_LED_LINK]));
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin,
                      dtu_board_indicator_level(
                          frame->output[INDICATOR_LED_STATUS]));
    return true;
}

static const indicator_led_if_t dtu_board_indicator_led = {
    .context = NULL,
    .apply_frame = dtu_board_indicator_apply_frame,
};

const indicator_led_if_t *dtu_board_indicator_led_if(void)
{
    return &dtu_board_indicator_led;
}

void enter_bootloader_mode(void)
{
    uint32_t boot_msp =
        *(volatile uint32_t *)SYSTEM_BOOTLOADER_ADDRESS;

    uint32_t boot_reset =
        *(volatile uint32_t *)(SYSTEM_BOOTLOADER_ADDRESS + 4U);

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    HAL_RCC_DeInit();
    HAL_DeInit();

    for (uint32_t i = 0; i < 8U; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SCB->VTOR = SYSTEM_BOOTLOADER_ADDRESS;

    /* Cortex-M33: remove stack limit that may block ROM bootloader */
    __set_MSPLIM(0x20000000U);

    __DSB();
    __ISB();

    __set_MSP(boot_msp);

    ((void (*)(void))boot_reset)();

    while (1) {
        __NOP();
    }
}