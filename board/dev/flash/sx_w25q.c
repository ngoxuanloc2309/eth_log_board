#include "sx_w25q.h"

#include "FreeRTOS.h"
#include "main.h"
#include "sx_spi.h"
#include "task.h"

#define SX_W25Q_SPI_TIMEOUT_MS (100U)

static w25q_t sx_w25q_device;

static sx_bus_status_t sx_w25q_chip_select(void *context, bool active)
{
    if (context != &sx_w25q_device) {
        return SX_BUS_STATUS_INVALID_PARAM;
    }

    HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return SX_BUS_STATUS_OK;
}

static uint32_t sx_w25q_get_time_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static void sx_w25q_delay_ms(void *context, uint32_t delay_ms)
{
    TickType_t delay_ticks;

    (void)context;
    if (delay_ms == 0U) {
        return;
    }
    delay_ticks = pdMS_TO_TICKS(delay_ms);
    if (delay_ticks == 0U) {
        delay_ticks = 1U;
    }
    vTaskDelay(delay_ticks);
}

w25q_status_t sx_w25q_init(void)
{
    const sx_spi_if_t *spi;

    if (sx_w25q_device.initialized) {
        return W25Q_STATUS_OK;
    }
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return W25Q_STATUS_NOT_INITIALIZED;
    }

    spi = sx_spi_get_interface();
    if (spi == NULL) {
        return W25Q_STATUS_NOT_INITIALIZED;
    }
    {
        const w25q_config_t config = {
            .spi = spi,
            .chip_select_context = &sx_w25q_device,
            .chip_select = sx_w25q_chip_select,
            .time_context = NULL,
            .get_time_ms = sx_w25q_get_time_ms,
            .delay_ms = sx_w25q_delay_ms,
            .transfer_timeout_ms = SX_W25Q_SPI_TIMEOUT_MS,
        };

        return w25q_init(&sx_w25q_device, &config, NULL);
    }
}

w25q_t *sx_w25q_get_device(void)
{
    return sx_w25q_device.initialized ? &sx_w25q_device : NULL;
}
