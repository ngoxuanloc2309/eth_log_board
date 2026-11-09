#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include <stdlib.h>
#include <string.h>

struct MockQueue
{
    UBaseType_t queue_length;
    UBaseType_t item_size;
    UBaseType_t head;
    UBaseType_t count;
    uint8_t *storage;
};

static TickType_t current_tick;

QueueHandle_t xQueueCreate(UBaseType_t queue_length, UBaseType_t item_size)
{
    QueueHandle_t queue = calloc(1U, sizeof(*queue));

    if (queue == NULL)
    {
        return NULL;
    }

    queue->storage = calloc(queue_length, item_size);
    if (queue->storage == NULL)
    {
        free(queue);
        return NULL;
    }

    queue->queue_length = queue_length;
    queue->item_size = item_size;
    return queue;
}

BaseType_t xQueueSend(QueueHandle_t queue,
                      const void *item,
                      TickType_t wait_ticks)
{
    UBaseType_t tail;

    (void)wait_ticks;
    if (queue == NULL || item == NULL || queue->count == queue->queue_length)
    {
        return pdFAIL;
    }

    tail = (queue->head + queue->count) % queue->queue_length;
    memcpy(&queue->storage[tail * queue->item_size], item, queue->item_size);
    queue->count++;
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t queue,
                         void *item,
                         TickType_t wait_ticks)
{
    if (queue == NULL || item == NULL)
    {
        return pdFAIL;
    }

    if (queue->count == 0U)
    {
        current_tick += wait_ticks;
        return pdFAIL;
    }

    memcpy(item, &queue->storage[queue->head * queue->item_size],
           queue->item_size);
    queue->head = (queue->head + 1U) % queue->queue_length;
    queue->count--;
    return pdPASS;
}

BaseType_t xQueueReset(QueueHandle_t queue)
{
    if (queue == NULL)
    {
        return pdFAIL;
    }

    queue->head = 0U;
    queue->count = 0U;
    return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t queue)
{
    return queue == NULL ? 0U : queue->count;
}

void vQueueDelete(QueueHandle_t queue)
{
    if (queue != NULL)
    {
        free(queue->storage);
        free(queue);
    }
}

TickType_t xTaskGetTickCount(void)
{
    return current_tick++;
}

BaseType_t xTaskCreate(TaskFunction_t task,
                       const char *name,
                       uint32_t stack_depth,
                       void *parameter,
                       UBaseType_t priority,
                       TaskHandle_t *task_handle)
{
    (void)task;
    (void)name;
    (void)stack_depth;
    (void)parameter;
    (void)priority;
    if (task_handle != NULL)
    {
        *task_handle = (TaskHandle_t)1;
    }
    return pdPASS;
}

TaskHandle_t xTaskCreateStatic(TaskFunction_t task,
                               const char *name,
                               uint32_t stack_depth,
                               void *parameter,
                               UBaseType_t priority,
                               StackType_t *stack_buffer,
                               StaticTask_t *task_buffer)
{
    (void)task;
    (void)name;
    (void)stack_depth;
    (void)parameter;
    (void)priority;
    (void)stack_buffer;
    (void)task_buffer;
    return (TaskHandle_t)1;
}

void vTaskDelete(TaskHandle_t task)
{
    (void)task;
}
