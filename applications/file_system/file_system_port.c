#include "file_system_port.h"

#include <errno.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "file_system.h"
#include "semphr.h"
#include "sx_w25q.h"
#include "task.h"

static StaticSemaphore_t file_system_port_mutex_storage;
static SemaphoreHandle_t file_system_port_mutex;
static bool file_system_port_mounted;

static bool file_system_port_lock(void *context)
{
    SemaphoreHandle_t mutex = context;

    return mutex != NULL &&
           xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
}

static void file_system_port_unlock(void *context)
{
    SemaphoreHandle_t mutex = context;

    if (mutex == NULL || xSemaphoreGive(mutex) != pdTRUE) {
        configASSERT(false);
    }
}

static int file_system_port_map_w25q_error(w25q_status_t status)
{
    switch (status) {
    case W25Q_STATUS_INVALID_PARAM:
        return EINVAL;
    case W25Q_STATUS_NOT_INITIALIZED:
        return ENODEV;
    case W25Q_STATUS_TIMEOUT:
        return ETIMEDOUT;
    case W25Q_STATUS_BUSY:
        return EBUSY;
    case W25Q_STATUS_OUT_OF_RANGE:
        return ERANGE;
    case W25Q_STATUS_UNSUPPORTED_DEVICE:
        return ENODEV;
    case W25Q_STATUS_IO_ERROR:
    default:
        return EIO;
    }
}

static int32_t file_system_port_prepare(file_system_config_t *config)
{
    w25q_status_t flash_status;

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        errno = EPERM;
        return -1;
    }

    flash_status = sx_w25q_init();
    if (flash_status != W25Q_STATUS_OK) {
        errno = file_system_port_map_w25q_error(flash_status);
        return -1;
    }

    if (file_system_port_mutex == NULL) {
        file_system_port_mutex = xSemaphoreCreateMutexStatic(
            &file_system_port_mutex_storage);
        if (file_system_port_mutex == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }

    config->flash = sx_w25q_get_device();
    config->lock_context = file_system_port_mutex;
    config->lock = file_system_port_lock;
    config->unlock = file_system_port_unlock;
    if (config->flash == NULL) {
        errno = ENODEV;
        return -1;
    }
    return 0;
}

int32_t file_system_port_init(void)
{
    file_system_config_t config;

    if (file_system_port_is_mounted()) {
        return 0;
    }
    if (file_system_port_prepare(&config) != 0) {
        return -1;
    }
    if (file_system_mount(&config) != 0) {
        return -1;
    }

    taskENTER_CRITICAL();
    file_system_port_mounted = true;
    taskEXIT_CRITICAL();
    return 0;
}

bool file_system_port_is_mounted(void)
{
    bool mounted;

    taskENTER_CRITICAL();
    mounted = file_system_port_mounted;
    taskEXIT_CRITICAL();
    return mounted;
}

int32_t file_system_port_format(void)
{
    file_system_config_t config;

    if (file_system_port_is_mounted()) {
        errno = EBUSY;
        return -1;
    }
    if (file_system_port_prepare(&config) != 0) {
        return -1;
    }
    return file_system_format(&config);
}
