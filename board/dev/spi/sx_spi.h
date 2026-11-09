#ifndef SX_SPI_H
#define SX_SPI_H

#include <stdbool.h>

#include "sx_spi_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the SPI1 bus adapter.
 *
 * SPI1 must already be initialized by the platform. This function creates
 * all synchronization objects, the deferred completion task, and the
 * hardware-independent interface using static memory.
 *
 * @return true on success; false if SPI1 or any RTOS resource is unavailable.
 */
bool sx_spi_init(void);

/**
 * @brief Get the initialized SPI bus interface.
 *
 * @return SPI interface, or NULL if sx_spi_init() has not succeeded.
 */
const sx_spi_if_t *sx_spi_get_interface(void);

#ifdef __cplusplus
}
#endif

#endif