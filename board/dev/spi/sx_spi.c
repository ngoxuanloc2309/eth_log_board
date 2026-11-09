#include "sx_spi.h"

#include <limits.h>
#include <stddef.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "spi.h"
#include "task.h"

#define SX_SPI_ASYNC_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2U)
#define SX_SPI_ASYNC_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)
#define SX_SPI_HAL_MAX_TRANSFER_LENGTH (UINT16_MAX)
#define SX_SPI_TIMEOUT_ROUNDING_NUMERATOR (999U)
#define SX_SPI_MILLISECONDS_PER_SECOND (1000U)
#define SX_SPI_ASYNC_QUEUE_LENGTH (1U)

typedef struct
{
    const uint8_t *tx;
    uint8_t *rx;
    uint32_t length;
    uint32_t timeout_ms;
    sx_bus_completion_callback_t callback;
    void *user_context;
} sx_spi_async_request_t;

typedef struct
{
    SPI_HandleTypeDef *handle;
    SemaphoreHandle_t bus_mutex;
    StaticSemaphore_t bus_mutex_storage;
    QueueHandle_t async_requests;
    StaticQueue_t async_queue_storage;
    uint8_t async_queue_bytes[sizeof(sx_spi_async_request_t) *
                              SX_SPI_ASYNC_QUEUE_LENGTH];
    SemaphoreHandle_t async_complete;
    StaticSemaphore_t async_complete_storage;
    StaticTask_t async_task_storage;
    StackType_t async_task_stack[SX_SPI_ASYNC_TASK_STACK_DEPTH];
    TaskHandle_t async_task;
    bool initialized;
    bool async_busy;
    volatile bool async_active;
    volatile sx_bus_status_t async_status;
} sx_spi_state_t;

static sx_spi_state_t sx_spi_state;

static sx_bus_status_t sx_spi_map_hal_status(HAL_StatusTypeDef status)
{
    switch (status) {
    case HAL_OK:
        return SX_BUS_STATUS_OK;
    case HAL_BUSY:
        return SX_BUS_STATUS_BUSY;
    case HAL_TIMEOUT:
        return SX_BUS_STATUS_TIMEOUT;
    case HAL_ERROR:
    default:
        return SX_BUS_STATUS_IO_ERROR;
    }
}

static TickType_t sx_spi_timeout_to_ticks(uint32_t timeout_ms)
{
    uint64_t ticks;

    if (timeout_ms == SX_BUS_WAIT_FOREVER) {
        return portMAX_DELAY;
    }

    ticks = ((uint64_t)timeout_ms * (uint64_t)configTICK_RATE_HZ +
             SX_SPI_TIMEOUT_ROUNDING_NUMERATOR) /
            SX_SPI_MILLISECONDS_PER_SECOND;
    if (timeout_ms > 0U && ticks == 0U) {
        ticks = 1U;
    }
    if (ticks >= (uint64_t)portMAX_DELAY) {
        ticks = (uint64_t)portMAX_DELAY - 1U;
    }
    return (TickType_t)ticks;
}

static sx_bus_status_t sx_spi_transfer(void *context,
                                       const uint8_t *tx,
                                       uint8_t *rx,
                                       uint32_t length,
                                       uint32_t timeout_ms)
{
    sx_spi_state_t *state = context;
    sx_bus_status_t status;
    HAL_StatusTypeDef hal_status;

    if (state != &sx_spi_state || tx == NULL || rx == NULL ||
        length == 0U || length > SX_SPI_HAL_MAX_TRANSFER_LENGTH) {
        return SX_BUS_STATUS_INVALID_PARAM;
    }
    if (!state->initialized) {
        return SX_BUS_STATUS_NOT_READY;
    }
    if (xSemaphoreTake(state->bus_mutex,
                       sx_spi_timeout_to_ticks(timeout_ms)) != pdTRUE) {
        return SX_BUS_STATUS_TIMEOUT;
    }

    hal_status = HAL_SPI_TransmitReceive(
        state->handle, tx, rx, (uint16_t)length,
        timeout_ms == SX_BUS_WAIT_FOREVER ? HAL_MAX_DELAY : timeout_ms);
    status = sx_spi_map_hal_status(hal_status);
    if (hal_status == HAL_TIMEOUT &&
        HAL_SPI_Abort(state->handle) != HAL_OK) {
        status = SX_BUS_STATUS_IO_ERROR;
    }
    if (xSemaphoreGive(state->bus_mutex) != pdTRUE) {
        status = SX_BUS_STATUS_IO_ERROR;
    }
    return status;
}

static void sx_spi_async_task(void *argument)
{
    sx_spi_state_t *state = argument;

    for (;;) {
        sx_spi_async_request_t request;
        sx_bus_status_t status;
        uint32_t transferred;
        BaseType_t mutex_acquired;
        BaseType_t transfer_completed;
        bool timed_out;

        if (xQueueReceive(state->async_requests, &request, portMAX_DELAY) !=
            pdTRUE) {
            continue;
        }

        mutex_acquired = xSemaphoreTake(
            state->bus_mutex, sx_spi_timeout_to_ticks(request.timeout_ms));
        if (mutex_acquired != pdTRUE) {
            status = SX_BUS_STATUS_TIMEOUT;
            transferred = 0U;
        } else if (xSemaphoreTake(state->async_complete, 0U) == pdTRUE) {
            status = SX_BUS_STATUS_IO_ERROR;
            transferred = 0U;
            if (xSemaphoreGive(state->bus_mutex) != pdTRUE) {
                status = SX_BUS_STATUS_IO_ERROR;
            }
        } else {
            state->async_status = SX_BUS_STATUS_IO_ERROR;
            state->async_active = true;
            {
                HAL_StatusTypeDef hal_status = HAL_SPI_TransmitReceive_IT(
                    state->handle, request.tx, request.rx,
                    (uint16_t)request.length);
                if (hal_status != HAL_OK) {
                    state->async_active = false;
                    status = sx_spi_map_hal_status(hal_status);
                    transferred = 0U;
                } else {
                    transfer_completed = xSemaphoreTake(
                        state->async_complete,
                        sx_spi_timeout_to_ticks(request.timeout_ms));
                    if (transfer_completed == pdTRUE) {
                        status = state->async_status;
                        transferred = status == SX_BUS_STATUS_OK
                                          ? request.length
                                          : 0U;
                    } else {
                        taskENTER_CRITICAL();
                        timed_out = state->async_active;
                        if (timed_out) {
                            state->async_active = false;
                        }
                        taskEXIT_CRITICAL();
                        if (timed_out) {
                            status = HAL_SPI_Abort(state->handle) == HAL_OK
                                         ? SX_BUS_STATUS_TIMEOUT
                                         : SX_BUS_STATUS_IO_ERROR;
                            transferred = 0U;
                        } else if (xSemaphoreTake(state->async_complete,
                                                 0U) == pdTRUE) {
                            status = state->async_status;
                            transferred = status == SX_BUS_STATUS_OK
                                              ? request.length
                                              : 0U;
                        } else {
                            status = SX_BUS_STATUS_IO_ERROR;
                            transferred = 0U;
                        }
                    }
                }
            }

            if (xSemaphoreGive(state->bus_mutex) != pdTRUE) {
                status = SX_BUS_STATUS_IO_ERROR;
                transferred = 0U;
            }
        }

        taskENTER_CRITICAL();
        state->async_busy = false;
        taskEXIT_CRITICAL();
        request.callback(request.user_context, status, transferred);
    }
}

static sx_bus_status_t sx_spi_transfer_async(
    void *context,
    const uint8_t *tx,
    uint8_t *rx,
    uint32_t length,
    uint32_t timeout_ms,
    sx_bus_completion_callback_t callback,
    void *user_context)
{
    sx_spi_state_t *state = context;
    sx_spi_async_request_t request = {
        .tx = tx,
        .rx = rx,
        .length = length,
        .timeout_ms = timeout_ms,
        .callback = callback,
        .user_context = user_context,
    };

    if (state != &sx_spi_state || tx == NULL || rx == NULL ||
        length == 0U || length > SX_SPI_HAL_MAX_TRANSFER_LENGTH ||
        callback == NULL) {
        return SX_BUS_STATUS_INVALID_PARAM;
    }
    if (!state->initialized) {
        return SX_BUS_STATUS_NOT_READY;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return SX_BUS_STATUS_NOT_READY;
    }

    taskENTER_CRITICAL();
    if (state->async_busy) {
        taskEXIT_CRITICAL();
        return SX_BUS_STATUS_BUSY;
    }
    state->async_busy = true;
    taskEXIT_CRITICAL();

    if (xQueueSend(state->async_requests, &request, 0U) != pdTRUE) {
        taskENTER_CRITICAL();
        state->async_busy = false;
        taskEXIT_CRITICAL();
        return SX_BUS_STATUS_BUSY;
    }
    return SX_BUS_STATUS_OK;
}

bool sx_spi_init(void)
{
    sx_spi_state_t *state = &sx_spi_state;

    if (state->initialized || hspi1.Instance != SPI1 ||
        HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
        return false;
    }

    state->handle = &hspi1;
    state->bus_mutex =
        xSemaphoreCreateMutexStatic(&state->bus_mutex_storage);
    state->async_complete =
        xSemaphoreCreateBinaryStatic(&state->async_complete_storage);
    state->async_requests = xQueueCreateStatic(
        SX_SPI_ASYNC_QUEUE_LENGTH, sizeof(sx_spi_async_request_t),
        state->async_queue_bytes, &state->async_queue_storage);
    if (state->bus_mutex == NULL || state->async_complete == NULL ||
        state->async_requests == NULL) {
        if (state->bus_mutex != NULL) {
            vSemaphoreDelete(state->bus_mutex);
            state->bus_mutex = NULL;
        }
        if (state->async_complete != NULL) {
            vSemaphoreDelete(state->async_complete);
            state->async_complete = NULL;
        }
        if (state->async_requests != NULL) {
            vQueueDelete(state->async_requests);
            state->async_requests = NULL;
        }
        return false;
    }

    state->async_task = xTaskCreateStatic(
        sx_spi_async_task, "sx_spi", SX_SPI_ASYNC_TASK_STACK_DEPTH, state,
        SX_SPI_ASYNC_TASK_PRIORITY, state->async_task_stack,
        &state->async_task_storage);
    if (state->async_task == NULL) {
        vQueueDelete(state->async_requests);
        vSemaphoreDelete(state->async_complete);
        vSemaphoreDelete(state->bus_mutex);
        state->async_requests = NULL;
        state->async_complete = NULL;
        state->bus_mutex = NULL;
        return false;
    }

    state->initialized = true;
    return true;
}

const sx_spi_if_t *sx_spi_get_interface(void)
{
    static const sx_spi_if_t spi_interface = {
        .context = &sx_spi_state,
        .transfer = sx_spi_transfer,
        .transfer_async = sx_spi_transfer_async,
    };

    return sx_spi_state.initialized ? &spi_interface : NULL;
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *handle)
{
    sx_spi_state_t *state = &sx_spi_state;
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t give_result;

    if (handle != state->handle || !state->async_active) {
        return;
    }
    state->async_status = SX_BUS_STATUS_OK;
    state->async_active = false;
    give_result = xSemaphoreGiveFromISR(state->async_complete,
                                        &higher_priority_task_woken);
    if (give_result != pdTRUE) {
        state->async_status = SX_BUS_STATUS_IO_ERROR;
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *handle)
{
    sx_spi_state_t *state = &sx_spi_state;
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t give_result;

    if (handle != state->handle || !state->async_active) {
        return;
    }
    state->async_status = SX_BUS_STATUS_IO_ERROR;
    state->async_active = false;
    give_result = xSemaphoreGiveFromISR(state->async_complete,
                                        &higher_priority_task_woken);
    if (give_result != pdTRUE) {
        state->async_status = SX_BUS_STATUS_IO_ERROR;
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}
