#include "sx_uart.h"

#include <limits.h>
#include "main.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "stm32h5xx_hal_dma.h"
#include "stm32h5xx_hal_uart.h"
#include "stm32h5xx_hal_uart_ex.h"
#include "usart.h"

#define SX_UART_DMA_RX_SIZE 128U
#define SX_UART_RX_TRIGGER_LEVEL 1U

typedef struct
{
    UART_HandleTypeDef *handle;
    sx_uart_mode_t mode;
    volatile bool installed;
    uint8_t rx_byte;
    uint8_t dma_rx_buffer[SX_UART_DMA_RX_SIZE];
    StaticStreamBuffer_t rx_stream_storage;
    uint8_t rx_stream_bytes[SX_UART_RX_BUFFER_MAX];
    StreamBufferHandle_t rx_stream;
    StaticSemaphore_t rx_mutex_storage;
    SemaphoreHandle_t rx_mutex;
    StaticSemaphore_t tx_mutex_storage;
    SemaphoreHandle_t tx_mutex;
    StaticSemaphore_t tx_complete_storage;
    SemaphoreHandle_t tx_complete;
    volatile uint32_t error_count;
    volatile uint32_t rx_overflow_count;
} sx_uart_state_t;

static sx_uart_state_t sx_uart_states[SX_UART_PORT_MAX] = {
    {.handle = &hlpuart1},
    {.handle = &huart7},
    {.handle = &huart3},
    {.handle = &huart10},
    {.handle = &huart9}
};

static USART_TypeDef *uart_handle[SX_UART_PORT_MAX] = {LPUART1, UART7, USART3, USART10, UART9};

static sx_uart_state_t *sx_uart_get_state(sx_uart_port_t port)
{
    if ((unsigned int)port >= SX_UART_PORT_MAX) {
        return NULL;
    }
    return &sx_uart_states[port];
}
static USART_TypeDef *sx_uart_get_handle(sx_uart_port_t port){
    if ((unsigned int)port >= SX_UART_PORT_MAX) {
        return NULL;
    }
    return uart_handle[port];
}
static sx_uart_state_t *sx_uart_find_state(UART_HandleTypeDef *handle)
{
    size_t index;

    for (index = 0U; index < SX_UART_PORT_MAX; index++) {
        if (sx_uart_states[index].handle == handle) {
            return &sx_uart_states[index];
        }
    }
    return NULL;
}

static uint32_t sx_uart_timeout_ms(TickType_t timeout_ticks)
{
    uint64_t timeout_ms;

    if (timeout_ticks == portMAX_DELAY) {
        return HAL_MAX_DELAY;
    }
    timeout_ms = (uint64_t)timeout_ticks * (uint64_t)portTICK_PERIOD_MS;
    if (timeout_ms >= HAL_MAX_DELAY) {
        return HAL_MAX_DELAY - 1U;
    }
    return (uint32_t)timeout_ms;
}

static BaseType_t sx_uart_start_rx(sx_uart_state_t *state)
{
    if (state->mode == SX_UART_MODE_INTERRUPT) {
        return HAL_UART_Receive_IT(state->handle, &state->rx_byte, 1U) ==
                       HAL_OK
                   ? pdPASS
                   : pdFAIL;
    }
    if (state->mode == SX_UART_MODE_DMA) {
        HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_DMA(
            state->handle, state->dma_rx_buffer, SX_UART_DMA_RX_SIZE);
        if (status == HAL_OK && state->handle->hdmarx != NULL) {
            __HAL_DMA_DISABLE_IT(state->handle->hdmarx, DMA_IT_HT);
            return pdPASS;
        }
    }
    return pdFAIL;
}

static void sx_uart_store_rx_from_isr(sx_uart_state_t *state,
                                      const uint8_t *buffer,
                                      size_t length)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    size_t stored;

    stored = xStreamBufferSendFromISR(state->rx_stream, buffer, length,
                                     &higher_priority_task_woken);
    if (stored != length) {
        state->rx_overflow_count++;
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

bool sx_uart_install(sx_uart_port_t port, const sx_uart_config_t *config)
{
    sx_uart_state_t *state = sx_uart_get_state(port);
    USART_TypeDef *uart = sx_uart_get_handle(port);
    size_t rx_buffer_size;

    if (state == NULL || uart == NULL || config == NULL || state->installed ||
        config->mode < SX_UART_MODE_POLLING ||
        config->mode > SX_UART_MODE_DMA) {
        return false;
    }

    state->handle->Instance = uart;
    state->handle->Init.BaudRate = config->Baudrate;
    state->handle->Init.WordLength = (config->WordLength == SX_UART_DATA_7_BITS) ? UART_WORDLENGTH_7B : UART_WORDLENGTH_8B;
    state->handle->Init.StopBits = config->StopBits;
    state->handle->Init.Parity = config->Parity;
    state->handle->Init.Mode = UART_MODE_TX_RX;
    state->handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    state->handle->Init.OverSampling = UART_OVERSAMPLING_16;
    state->handle->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    state->handle->Init.ClockPrescaler = UART_PRESCALER_DIV1;
    state->handle->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(state->handle) != HAL_OK)
    {
        return false;
    }
    if (HAL_UARTEx_SetTxFifoThreshold(state->handle, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        return false;
    }
    if (HAL_UARTEx_SetRxFifoThreshold(state->handle, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        return false;
    }
    if (HAL_UARTEx_DisableFifoMode(state->handle) != HAL_OK)
    {
        return false;
    }
    if (state->handle->gState != HAL_UART_STATE_READY ||
        state->handle->RxState != HAL_UART_STATE_READY) {
        return false;
    }

    rx_buffer_size = config->rx_buffer_size;
    if (rx_buffer_size == 0U) {
        rx_buffer_size = SX_UART_DEFAULT_RX_BUFFER_SIZE;
    }
    if (rx_buffer_size < 2U || rx_buffer_size > SX_UART_RX_BUFFER_MAX) {
        return false;
    }
    if (state->handle->Init.WordLength == UART_WORDLENGTH_9B &&
        state->handle->Init.Parity == UART_PARITY_NONE) {
        return false;
    }
    if (config->mode == SX_UART_MODE_DMA &&
        (state->handle->hdmarx == NULL || state->handle->hdmatx == NULL)) {
        return false;
    }

    state->mode = config->mode;
    state->error_count = 0U;
    state->rx_overflow_count = 0U;
    state->rx_mutex = xSemaphoreCreateMutexStatic(&state->rx_mutex_storage);
    state->tx_mutex = xSemaphoreCreateMutexStatic(&state->tx_mutex_storage);
    state->tx_complete =
        xSemaphoreCreateBinaryStatic(&state->tx_complete_storage);
    if (state->rx_mutex == NULL || state->tx_mutex == NULL ||
        state->tx_complete == NULL) {
        if (state->rx_mutex != NULL) {
            vSemaphoreDelete(state->rx_mutex);
        }
        if (state->tx_mutex != NULL) {
            vSemaphoreDelete(state->tx_mutex);
        }
        if (state->tx_complete != NULL) {
            vSemaphoreDelete(state->tx_complete);
        }
        state->rx_mutex = NULL;
        state->tx_mutex = NULL;
        state->tx_complete = NULL;
        return false;
    }

    if (state->mode != SX_UART_MODE_POLLING) {
        state->rx_stream = xStreamBufferCreateStatic(
            rx_buffer_size, SX_UART_RX_TRIGGER_LEVEL, state->rx_stream_bytes,
            &state->rx_stream_storage);
        if (state->rx_stream == NULL) {
            vSemaphoreDelete(state->rx_mutex);
            vSemaphoreDelete(state->tx_mutex);
            vSemaphoreDelete(state->tx_complete);
            state->rx_mutex = NULL;
            state->tx_mutex = NULL;
            state->tx_complete = NULL;
            return false;
        }

        state->installed = true;
        if (sx_uart_start_rx(state) != pdPASS) {
            state->installed = false;
            vStreamBufferDelete(state->rx_stream);
            state->rx_stream = NULL;
            vSemaphoreDelete(state->rx_mutex);
            vSemaphoreDelete(state->tx_mutex);
            vSemaphoreDelete(state->tx_complete);
            state->rx_mutex = NULL;
            state->tx_mutex = NULL;
            state->tx_complete = NULL;
            return false;
        }
    } else {
        state->rx_stream = NULL;
        state->installed = true;
    }

    return true;
}

bool sx_uart_uninstall(sx_uart_port_t port)
{
    sx_uart_state_t *state = sx_uart_get_state(port);

    if (state == NULL || !state->installed) {
        return false;
    }

    state->installed = false;
    if (state->mode != SX_UART_MODE_POLLING) {
        (void)HAL_UART_Abort(state->handle);
    }

    if (state->rx_stream != NULL) {
        vStreamBufferDelete(state->rx_stream);
        state->rx_stream = NULL;
    }
    vSemaphoreDelete(state->tx_mutex);
    vSemaphoreDelete(state->tx_complete);
    vSemaphoreDelete(state->rx_mutex);
    state->rx_mutex = NULL;
    state->tx_mutex = NULL;
    state->tx_complete = NULL;
    return true;
}

bool sx_uart_is_installed(sx_uart_port_t port)
{
    sx_uart_state_t *state = sx_uart_get_state(port);

    return state != NULL && state->installed;
}

bool sx_uart_get_stats(sx_uart_port_t port, sx_uart_stats_t *stats)
{
    sx_uart_state_t *state = sx_uart_get_state(port);

    if (state == NULL || stats == NULL || !state->installed) {
        return false;
    }
    stats->hal_errors = state->error_count;
    stats->rx_overflows = state->rx_overflow_count;
    return true;
}

bool sx_uart_set_word_length(sx_uart_port_t port,
                             sx_uart_word_length_t data_bits)
{
    sx_uart_state_t *state = sx_uart_get_state(port);
    uint32_t previous_word_length;
    uint32_t requested_word_length;

    if (state == NULL || state->installed ||
        (data_bits != SX_UART_DATA_7_BITS &&
         data_bits != SX_UART_DATA_8_BITS) ||
        state->handle->gState != HAL_UART_STATE_READY) {
        return false;
    }

    previous_word_length = state->handle->Init.WordLength;
    requested_word_length = data_bits == SX_UART_DATA_7_BITS
                                ? UART_WORDLENGTH_7B
                                : UART_WORDLENGTH_8B;
    state->handle->Init.WordLength = requested_word_length;
    if (HAL_UART_Init(state->handle) == HAL_OK) {
        return true;
    }

    state->handle->Init.WordLength = previous_word_length;
    (void)HAL_UART_Init(state->handle);
    return false;
}

int32_t sx_uart_get_rx_bytes_available(sx_uart_port_t port)
{
    sx_uart_state_t *state = sx_uart_get_state(port);

    if (state == NULL || !state->installed ||
        state->mode == SX_UART_MODE_POLLING || state->rx_stream == NULL) {
        return -1;
    }

    return (int32_t)xStreamBufferBytesAvailable(state->rx_stream);
}

int32_t sx_uart_read(sx_uart_port_t port,
                     uint8_t *buffer,
                     size_t length,
                     TickType_t timeout_ticks)
{
    sx_uart_state_t *state = sx_uart_get_state(port);
    size_t received;

    if (state == NULL || !state->installed ||
        (buffer == NULL && length > 0U) || length > INT32_MAX) {
        return -1;
    }
    if (length == 0U) {
        return 0;
    }
    if (xSemaphoreTake(state->rx_mutex, timeout_ticks) != pdTRUE) {
        return -1;
    }

    if (state->mode == SX_UART_MODE_POLLING) {
        if (length > UINT16_MAX ||
            HAL_UART_Receive(state->handle, buffer, (uint16_t)length,
                             sx_uart_timeout_ms(timeout_ticks)) != HAL_OK) {
            (void)xSemaphoreGive(state->rx_mutex);
            return -1;
        }
        (void)xSemaphoreGive(state->rx_mutex);
        return (int32_t)length;
    }

    received = xStreamBufferReceive(state->rx_stream, buffer, length,
                                    timeout_ticks);
    (void)xSemaphoreGive(state->rx_mutex);
    return received == 0U ? -1 : (int32_t)received;
}

int32_t sx_uart_write(sx_uart_port_t port,
                      const uint8_t *buffer,
                      size_t length,
                      TickType_t timeout_ticks)
{
    sx_uart_state_t *state = sx_uart_get_state(port);
    HAL_StatusTypeDef hal_status;
    uint32_t errors_before_transfer;

    if (state == NULL || !state->installed ||
        (buffer == NULL && length > 0U) || length > UINT16_MAX ||
        length > INT32_MAX) {
        return -1;
    }
    if (length == 0U) {
        return 0;
    }

    if (state->mode == SX_UART_MODE_POLLING) {
        if (xSemaphoreTake(state->tx_mutex, timeout_ticks) != pdTRUE) {
            return -1;
        }
        if (HAL_UART_Transmit(state->handle, (uint8_t *)buffer,
                              (uint16_t)length,
                              sx_uart_timeout_ms(timeout_ticks)) != HAL_OK) {
            (void)xSemaphoreGive(state->tx_mutex);
            return -1;
        }
        (void)xSemaphoreGive(state->tx_mutex);
        return (int32_t)length;
    }

    if (xSemaphoreTake(state->tx_mutex, timeout_ticks) != pdTRUE) {
        return -1;
    }
    (void)xSemaphoreTake(state->tx_complete, 0U);
    errors_before_transfer = state->error_count;

    hal_status = state->mode == SX_UART_MODE_INTERRUPT
                     ? HAL_UART_Transmit_IT(state->handle, buffer,
                                            (uint16_t)length)
                     : HAL_UART_Transmit_DMA(state->handle, buffer,
                                             (uint16_t)length);
    if (hal_status != HAL_OK ||
        xSemaphoreTake(state->tx_complete, timeout_ticks) != pdTRUE ||
        state->error_count != errors_before_transfer) {
        (void)HAL_UART_AbortTransmit(state->handle);
        (void)xSemaphoreGive(state->tx_mutex);
        return -1;
    }

    (void)xSemaphoreGive(state->tx_mutex);
    return (int32_t)length;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *handle)
{
    sx_uart_state_t *state = sx_uart_find_state(handle);

    if (state == NULL || !state->installed ||
        state->mode != SX_UART_MODE_INTERRUPT) {
        return;
    }

    sx_uart_store_rx_from_isr(state, &state->rx_byte, 1U);
    if (HAL_UART_Receive_IT(state->handle, &state->rx_byte, 1U) != HAL_OK) {
        state->error_count++;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *handle, uint16_t size)
{
    sx_uart_state_t *state = sx_uart_find_state(handle);

    if (state == NULL || !state->installed ||
        state->mode != SX_UART_MODE_DMA) {
        return;
    }

    if (size > 0U) {
        sx_uart_store_rx_from_isr(state, state->dma_rx_buffer, size);
    }
    if (sx_uart_start_rx(state) != pdPASS) {
        state->error_count++;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *handle)
{
    sx_uart_state_t *state = sx_uart_find_state(handle);

    if (state == NULL || !state->installed ||
        state->mode == SX_UART_MODE_POLLING) {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    (void)xSemaphoreGiveFromISR(state->tx_complete,
                                &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
    sx_uart_state_t *state = sx_uart_find_state(handle);

    if (state == NULL || !state->installed) {
        return;
    }

    state->error_count++;
    if (state->tx_complete != NULL) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        (void)xSemaphoreGiveFromISR(state->tx_complete,
                                    &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
    if (state->handle->RxState == HAL_UART_STATE_READY &&
        state->mode != SX_UART_MODE_POLLING &&
        sx_uart_start_rx(state) != pdPASS) {
        state->error_count++;
    }
}
