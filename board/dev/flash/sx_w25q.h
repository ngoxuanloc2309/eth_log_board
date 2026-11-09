#ifndef SX_W25Q_H
#define SX_W25Q_H

#include <stdbool.h>

#include "w25q.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the board W25Q flash device on SPI1.
 *
 * SPI1 must be initialized and the FreeRTOS scheduler must be running.
 * The flash chip select is controlled by this BSP adapter.
 *
 * @return W25Q status.
 */
w25q_status_t sx_w25q_init(void);

/**
 * @brief Get the initialized board W25Q device.
 *
 * The returned context is shared state. Callers must serialize all access;
 * the file-system port provides this serialization for LittleFS operations.
 *
 * @return Device context, or NULL if initialization has not succeeded.
 */
w25q_t *sx_w25q_get_device(void);

#ifdef __cplusplus
}
#endif

#endif
