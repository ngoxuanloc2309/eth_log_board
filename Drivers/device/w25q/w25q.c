#include "w25q.h"

#include <stddef.h>
#include <string.h>

#include "w25q_reg.h"

static w25q_status_t w25q_map_bus_status(sx_bus_status_t status)
{
    switch (status) {
    case SX_BUS_STATUS_OK:
        return W25Q_STATUS_OK;
    case SX_BUS_STATUS_INVALID_PARAM:
        return W25Q_STATUS_INVALID_PARAM;
    case SX_BUS_STATUS_NOT_READY:
        return W25Q_STATUS_NOT_INITIALIZED;
    case SX_BUS_STATUS_BUSY:
        return W25Q_STATUS_BUSY;
    case SX_BUS_STATUS_TIMEOUT:
        return W25Q_STATUS_TIMEOUT;
    case SX_BUS_STATUS_IO_ERROR:
    default:
        return W25Q_STATUS_IO_ERROR;
    }
}

static w25q_status_t w25q_transfer(w25q_t *device,
                                   const uint8_t *tx,
                                   uint8_t *rx,
                                   uint32_t length)
{
    sx_bus_status_t status;
    sx_bus_status_t release_status;

    status = device->chip_select(device->chip_select_context, true);
    if (status != SX_BUS_STATUS_OK) {
        return w25q_map_bus_status(status);
    }

    status = device->spi->transfer(device->spi->context, tx, rx, length,
                                   device->transfer_timeout_ms);
    release_status =
        device->chip_select(device->chip_select_context, false);
    if (release_status != SX_BUS_STATUS_OK) {
        return w25q_map_bus_status(release_status);
    }
    return w25q_map_bus_status(status);
}

static void w25q_encode_address(uint8_t *buffer,
                                uint32_t address,
                                uint8_t address_bytes)
{
    uint8_t index;

    for (index = 0U; index < address_bytes; index++) {
        uint8_t shift = (uint8_t)((address_bytes - index - 1U) * 8U);
        buffer[1U + index] = (uint8_t)(address >> shift);
    }
}

static w25q_status_t w25q_read_status(w25q_t *device, uint8_t *status)
{
    uint8_t tx[2] = {W25Q_CMD_READ_STATUS_REGISTER_1, 0xFFU};
    uint8_t rx[2] = {0U, 0U};
    w25q_status_t result;

    result = w25q_transfer(device, tx, rx, (uint32_t)sizeof(tx));
    if (result == W25Q_STATUS_OK) {
        *status = rx[1];
    }
    return result;
}

static w25q_status_t w25q_check_ready(w25q_t *device)
{
    uint8_t status;
    w25q_status_t result = w25q_read_status(device, &status);

    if (result != W25Q_STATUS_OK) {
        return result;
    }
    return (status & W25Q_STATUS_BUSY_MASK) == 0U
               ? W25Q_STATUS_OK
               : W25Q_STATUS_BUSY;
}

static w25q_status_t w25q_wait_ready(w25q_t *device, uint32_t timeout_ms)
{
    uint32_t start_time = device->get_time_ms(device->time_context);

    for (;;) {
        uint8_t status;
        w25q_status_t result = w25q_read_status(device, &status);

        if (result != W25Q_STATUS_OK) {
            return result;
        }
        if ((status & W25Q_STATUS_BUSY_MASK) == 0U) {
            return W25Q_STATUS_OK;
        }
        if ((uint32_t)(device->get_time_ms(device->time_context) -
                       start_time) >= timeout_ms) {
            return W25Q_STATUS_TIMEOUT;
        }

        device->delay_ms(device->time_context,
                         W25Q_STATUS_POLL_INTERVAL_MS);
    }
}

static w25q_status_t w25q_write_enable(w25q_t *device)
{
    uint8_t tx = W25Q_CMD_WRITE_ENABLE;
    uint8_t rx = 0U;
    uint8_t status_register;
    w25q_status_t result;

    result = w25q_transfer(device, &tx, &rx, 1U);
    if (result != W25Q_STATUS_OK) {
        return result;
    }

    result = w25q_read_status(device, &status_register);
    if (result != W25Q_STATUS_OK) {
        return result;
    }
    if ((status_register & W25Q_STATUS_WRITE_ENABLE_MASK) == 0U) {
        return W25Q_STATUS_IO_ERROR;
    }
    return W25Q_STATUS_OK;
}

static bool w25q_is_range_valid(const w25q_t *device,
                                uint32_t address,
                                uint32_t length)
{
    return address <= device->capacity_bytes &&
           length <= (device->capacity_bytes - address);
}

static w25q_status_t w25q_erase_command(w25q_t *device,
                                        uint8_t opcode,
                                        uint32_t address,
                                        uint32_t timeout_ms)
{
    uint8_t command[W25Q_MAX_COMMAND_HEADER_SIZE] = {0U};
    uint8_t response[W25Q_MAX_COMMAND_HEADER_SIZE] = {0U};
    uint32_t command_length = 1U;
    w25q_status_t result;

    result = w25q_wait_ready(device, timeout_ms);
    if (result != W25Q_STATUS_OK) {
        return result;
    }
    result = w25q_write_enable(device);
    if (result != W25Q_STATUS_OK) {
        return result;
    }

    command[0] = opcode;
    if (device->address_bytes == W25Q_MAX_ADDRESS_BYTES) {
        w25q_encode_address(command, address, device->address_bytes);
        command_length += W25Q_MAX_ADDRESS_BYTES;
    } else {
        w25q_encode_address(command, address, device->address_bytes);
        command_length += device->address_bytes;
    }

    result = w25q_transfer(device, command, response, command_length);
    if (result != W25Q_STATUS_OK) {
        return result;
    }
    return w25q_wait_ready(device, timeout_ms);
}

static w25q_status_t w25q_erase_aligned(w25q_t *device,
                                        uint32_t address,
                                        uint32_t erase_size,
                                        uint8_t opcode_3byte,
                                        uint8_t opcode_4byte,
                                        uint32_t timeout_ms)
{
    uint8_t opcode;

    if (device == NULL || !device->initialized) {
        return W25Q_STATUS_NOT_INITIALIZED;
    }
    if (address % erase_size != 0U ||
        !w25q_is_range_valid(device, address, erase_size)) {
        return W25Q_STATUS_OUT_OF_RANGE;
    }

    opcode = device->address_bytes == W25Q_MAX_ADDRESS_BYTES
                 ? opcode_4byte
                 : opcode_3byte;
    return w25q_erase_command(device, opcode, address, timeout_ms);
}

w25q_status_t w25q_init(w25q_t *device,
                        const w25q_config_t *config,
                        w25q_jedec_id_t *jedec_id)
{
    w25q_config_t config_copy;
    uint8_t tx[4] = {W25Q_CMD_READ_JEDEC_ID, 0xFFU, 0xFFU, 0xFFU};
    uint8_t rx[4] = {0U, 0U, 0U, 0U};
    w25q_status_t result;

    if (device == NULL || config == NULL || config->spi == NULL ||
        config->spi->transfer == NULL || config->chip_select == NULL ||
        config->get_time_ms == NULL || config->delay_ms == NULL) {
        return W25Q_STATUS_INVALID_PARAM;
    }

    config_copy = *config;
    memset(device, 0, sizeof(*device));
    device->spi = config_copy.spi;
    device->chip_select_context = config_copy.chip_select_context;
    device->chip_select = config_copy.chip_select;
    device->time_context = config_copy.time_context;
    device->get_time_ms = config_copy.get_time_ms;
    device->delay_ms = config_copy.delay_ms;
    device->transfer_timeout_ms = config_copy.transfer_timeout_ms;

    result = w25q_transfer(device, tx, rx, (uint32_t)sizeof(tx));
    if (result != W25Q_STATUS_OK) {
        return result;
    }

    if (rx[1] != W25Q_MANUFACTURER_ID_WINBOND ||
        rx[3] < W25Q_JEDEC_DENSITY_MIN ||
        rx[3] > W25Q_JEDEC_DENSITY_MAX) {
        return W25Q_STATUS_UNSUPPORTED_DEVICE;
    }

    if (jedec_id != NULL) {
        jedec_id->manufacturer_id = rx[1];
        jedec_id->memory_type = rx[2];
        jedec_id->capacity_id = rx[3];
    }

    device->capacity_bytes = UINT32_C(1) << rx[3];
    device->address_bytes =
        rx[3] > W25Q_JEDEC_3BYTE_ADDRESS_MAX ? 4U : 3U;
    device->initialized = true;
    return W25Q_STATUS_OK;
}

w25q_status_t w25q_read(w25q_t *device,
                        uint32_t address,
                        uint8_t *data,
                        uint32_t length)
{
    w25q_status_t result;

    if (device == NULL || !device->initialized) {
        return W25Q_STATUS_NOT_INITIALIZED;
    }
    if (data == NULL && length > 0U) {
        return W25Q_STATUS_INVALID_PARAM;
    }
    if (!w25q_is_range_valid(device, address, length)) {
        return W25Q_STATUS_OUT_OF_RANGE;
    }
    if (length == 0U) {
        return W25Q_STATUS_OK;
    }

    result = w25q_check_ready(device);
    if (result != W25Q_STATUS_OK) {
        return result;
    }

    while (length > 0U) {
        uint32_t chunk_length =
            length > W25Q_PAGE_SIZE ? W25Q_PAGE_SIZE : length;
        uint32_t header_length = (uint32_t)device->address_bytes + 1U;
        uint32_t transfer_length = header_length + chunk_length;
        uint32_t index;

        device->tx_buffer[0] =
            device->address_bytes == W25Q_MAX_ADDRESS_BYTES
                ? W25Q_CMD_READ_DATA_4BYTE
                : W25Q_CMD_READ_DATA;
        w25q_encode_address(device->tx_buffer, address,
                            device->address_bytes);
        for (index = 0U; index < chunk_length; index++) {
            device->tx_buffer[header_length + index] = 0xFFU;
        }

        result = w25q_transfer(device, device->tx_buffer, device->rx_buffer,
                               transfer_length);
        if (result != W25Q_STATUS_OK) {
            return result;
        }
        memcpy(data, &device->rx_buffer[header_length], chunk_length);
        data += chunk_length;
        address += chunk_length;
        length -= chunk_length;
    }
    return W25Q_STATUS_OK;
}

w25q_status_t w25q_page_program(w25q_t *device,
                                uint32_t address,
                                const uint8_t *data,
                                uint32_t length,
                                uint32_t timeout_ms)
{
    if (device == NULL || !device->initialized) {
        return W25Q_STATUS_NOT_INITIALIZED;
    }
    if (data == NULL && length > 0U) {
        return W25Q_STATUS_INVALID_PARAM;
    }
    if (!w25q_is_range_valid(device, address, length)) {
        return W25Q_STATUS_OUT_OF_RANGE;
    }

    while (length > 0U) {
        uint32_t page_offset = address % W25Q_PAGE_SIZE;
        uint32_t chunk_length = W25Q_PAGE_SIZE - page_offset;
        uint32_t header_length = (uint32_t)device->address_bytes + 1U;
        uint32_t transfer_length;
        w25q_status_t result;

        if (chunk_length > length) {
            chunk_length = length;
        }
        result = w25q_wait_ready(device, timeout_ms);
        if (result != W25Q_STATUS_OK) {
            return result;
        }
        result = w25q_write_enable(device);
        if (result != W25Q_STATUS_OK) {
            return result;
        }

        device->tx_buffer[0] =
            device->address_bytes == W25Q_MAX_ADDRESS_BYTES
                ? W25Q_CMD_PAGE_PROGRAM_4BYTE
                : W25Q_CMD_PAGE_PROGRAM;
        w25q_encode_address(device->tx_buffer, address,
                            device->address_bytes);
        memcpy(&device->tx_buffer[header_length], data, chunk_length);
        transfer_length = header_length + chunk_length;
        result = w25q_transfer(device, device->tx_buffer, device->rx_buffer,
                               transfer_length);
        if (result != W25Q_STATUS_OK) {
            return result;
        }
        result = w25q_wait_ready(device, timeout_ms);
        if (result != W25Q_STATUS_OK) {
            return result;
        }

        address += chunk_length;
        data += chunk_length;
        length -= chunk_length;
    }
    return W25Q_STATUS_OK;
}

w25q_status_t w25q_erase_sector_4k(w25q_t *device,
                                   uint32_t address,
                                   uint32_t timeout_ms)
{
    return w25q_erase_aligned(
        device, address, W25Q_SECTOR_SIZE_4K, W25Q_CMD_SECTOR_ERASE_4K,
        W25Q_CMD_SECTOR_ERASE_4K_4BYTE, timeout_ms);
}

w25q_status_t w25q_erase_block_64k(w25q_t *device,
                                   uint32_t address,
                                   uint32_t timeout_ms)
{
    return w25q_erase_aligned(
        device, address, W25Q_BLOCK_SIZE_64K, W25Q_CMD_BLOCK_ERASE_64K,
        W25Q_CMD_BLOCK_ERASE_64K_4BYTE, timeout_ms);
}

w25q_status_t w25q_erase_chip(w25q_t *device, uint32_t timeout_ms)
{
    uint8_t tx = W25Q_CMD_CHIP_ERASE;
    uint8_t rx = 0U;
    w25q_status_t result;

    if (device == NULL || !device->initialized) {
        return W25Q_STATUS_NOT_INITIALIZED;
    }
    result = w25q_wait_ready(device, timeout_ms);
    if (result != W25Q_STATUS_OK) {
        return result;
    }
    result = w25q_write_enable(device);
    if (result != W25Q_STATUS_OK) {
        return result;
    }
    result = w25q_transfer(device, &tx, &rx, 1U);
    if (result != W25Q_STATUS_OK) {
        return result;
    }
    return w25q_wait_ready(device, timeout_ms);
}
