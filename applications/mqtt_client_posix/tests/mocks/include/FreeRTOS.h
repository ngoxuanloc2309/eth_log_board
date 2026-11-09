#ifndef MQTT_TEST_FREERTOS_H
#define MQTT_TEST_FREERTOS_H

#include <stdint.h>

typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;
typedef int32_t BaseType_t;
typedef uint32_t StackType_t;
typedef struct
{
    uint32_t placeholder[16];
} StaticTask_t;
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1U
#define pdMS_TO_TICKS(milliseconds) ((TickType_t)(milliseconds))
#define tskIDLE_PRIORITY 0U

#endif
