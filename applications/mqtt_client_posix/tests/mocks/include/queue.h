#ifndef MQTT_TEST_QUEUE_H
#define MQTT_TEST_QUEUE_H

#include "FreeRTOS.h"

typedef struct MockQueue *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t queue_length, UBaseType_t item_size);
BaseType_t xQueueSend(QueueHandle_t queue,
                      const void *item,
                      TickType_t wait_ticks);
BaseType_t xQueueReceive(QueueHandle_t queue,
                         void *item,
                         TickType_t wait_ticks);
BaseType_t xQueueReset(QueueHandle_t queue);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue);
void vQueueDelete(QueueHandle_t queue);

#endif
