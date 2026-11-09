#ifndef W25Q_H
#define W25Q_H

#include <stdbool.h>
#include <stdint.h>

#include "sx_spi_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q_JEDEC_ID_SIZE (3U)
#define W25Q_PAGE_SIZE_BYTES (256U)
#define W25Q_SECTOR_SIZE_BYTES (4096U)
#define W25Q_BLOCK_SIZE_BYTES (65536U)
#define W25Q_MAX_COMMAND_HEADER_SIZE (5U)
#define W25Q_TRANSFER_BUFFER_SIZE \
    (W25Q_PAGE_SIZE_BYTES + W25Q_MAX_COMMAND_HEADER_SIZE)

typedef enum
{
    W25Q_STATUS_OK = 0,
    W25Q_STATUS_INVALID_PARAM,
    W25Q_STATUS_NOT_INITIALIZED,
    W25Q_STATUS_TIMEOUT,
    W25Q_STATUS_BUSY,
    W25Q_STATUS_IO_ERROR,
    W25Q_STATUS_UNSUPPORTED_DEVICE,
    W25Q_STATUS_OUT_OF_RANGE
} w25q_status_t;

typedef struct
{
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
} w25q_jedec_id_t;

typedef sx_bus_status_t (*w25q_chip_select_fn)(void *context, bool active);
typedef uint32_t (*w25q_get_time_ms_fn)(void *context);
typedef void (*w25q_delay_ms_fn)(void *context, uint32_t delay_ms);

typedef struct
{
    const sx_spi_if_t *spi;
    void *chip_select_context;
    w25q_chip_select_fn chip_select;
    void *time_context;
    w25q_get_time_ms_fn get_time_ms;
    w25q_delay_ms_fn delay_ms;
    uint32_t transfer_timeout_ms;
} w25q_config_t;

typedef struct
{
    const sx_spi_if_t *spi;
    void *chip_select_context;
    w25q_chip_select_fn chip_select;
    void *time_context;
    w25q_get_time_ms_fn get_time_ms;
    w25q_delay_ms_fn delay_ms;
    uint32_t transfer_timeout_ms;
    uint32_t capacity_bytes;
    uint8_t address_bytes;
    bool initialized;
    uint8_t tx_buffer[W25Q_TRANSFER_BUFFER_SIZE];
    uint8_t rx_buffer[W25Q_TRANSFER_BUFFER_SIZE];
} w25q_t;

/**
 * @brief Initialize a W25Q device and detect its JEDEC identity/capacity.
 *
 * Caller must serialize calls using one device context. All callbacks execute
 * synchronously in the caller's task context.
 *
 * @param device Device context to initialize.
 * @param config SPI, chip-select, timing callbacks, and transfer timeout.
 * @param jedec_id Optional storage for the detected JEDEC ID.
 * @return W25Q status.
 */
w25q_status_t w25q_init(w25q_t *device,
                        const w25q_config_t *config,
                        w25q_jedec_id_t *jedec_id);

/**
 * @brief Read bytes from the flash.
 *
 * @param device Initialized device.
 * @param address Byte address to read from.
 * @param data Destination buffer.
 * @param length Number of bytes to read.
 * @return W25Q status.
 */
w25q_status_t w25q_read(w25q_t *device,
                        uint32_t address,
                        uint8_t *data,
                        uint32_t length);

/**
 * @brief Program bytes, splitting writes at flash page boundaries.
 *
 * Flash programming only clears bits; erase sectors/blocks before changing
 * any programmed zero bits back to one.
 *
 * @param device Initialized device.
 * @param address Byte address to program.
 * @param data Source buffer.
 * @param length Number of bytes to program.
 * @param timeout_ms Maximum time to wait for each program operation.
 * @return W25Q status.
 */
w25q_status_t w25q_page_program(w25q_t *device,
                                uint32_t address,
                                const uint8_t *data,
                                uint32_t length,
                                uint32_t timeout_ms);

/**
 * @brief Erase one aligned 4 KiB sector.
 *
 * @param device Initialized device.
 * @param address Address of the sector; must be 4 KiB aligned.
 * @param timeout_ms Maximum erase wait time.
 * @return W25Q status.
 */
w25q_status_t w25q_erase_sector_4k(w25q_t *device,
                                   uint32_t address,
                                   uint32_t timeout_ms);

/**
 * @brief Erase one aligned 64 KiB block.
 *
 * @param device Initialized device.
 * @param address Address of the block; must be 64 KiB aligned.
 * @param timeout_ms Maximum erase wait time.
 * @return W25Q status.
 */
w25q_status_t w25q_erase_block_64k(w25q_t *device,
                                   uint32_t address,
                                   uint32_t timeout_ms);

/**
 * @brief Erase the entire flash chip.
 *
 * @param device Initialized device.
 * @param timeout_ms Maximum erase wait time.
 * @return W25Q status.
 */
w25q_status_t w25q_erase_chip(w25q_t *device, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
