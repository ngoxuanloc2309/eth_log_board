#ifndef INDICATOR_LED_IF_H
#define INDICATOR_LED_IF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    INDICATOR_LED_POWER = 0,
    INDICATOR_LED_LINK,
    INDICATOR_LED_STATUS,
    INDICATOR_LED_COUNT
} indicator_led_id_t;

typedef enum
{
    INDICATOR_LED_OFF = 0,
    INDICATOR_LED_ON
} indicator_led_output_t;

typedef struct
{
    indicator_led_output_t output[INDICATOR_LED_COUNT];
} indicator_led_frame_t;

/**
 * @brief Board-owned logical LED output operations.
 */
typedef struct
{
    void *context;

    /**
     * @brief Apply one complete logical LED frame.
     *
     * @param context Board implementation context.
     * @param frame Logical outputs in power, link, status index order.
     * @return true when the frame is valid and was accepted.
     */
    bool (*apply_frame)(void *context, const indicator_led_frame_t *frame);
} indicator_led_if_t;

#ifdef __cplusplus
}
#endif

#endif
