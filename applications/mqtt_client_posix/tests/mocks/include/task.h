#ifndef MQTT_TEST_TASK_H
#define MQTT_TEST_TASK_H

#include "FreeRTOS.h"

#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)

TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(TaskFunction_t task,
                       const char *name,
                       uint32_t stack_depth,
                       void *parameter,
                       UBaseType_t priority,
                       TaskHandle_t *task_handle);
TaskHandle_t xTaskCreateStatic(TaskFunction_t task,
                               const char *name,
                               uint32_t stack_depth,
                               void *parameter,
                               UBaseType_t priority,
                               StackType_t *stack_buffer,
                               StaticTask_t *task_buffer);
void vTaskDelete(TaskHandle_t task);

#endif
