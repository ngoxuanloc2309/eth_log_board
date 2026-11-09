#include "w25q.h"
#include "w25q_reg.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    bool selected;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
    uint8_t status;
    uint8_t last_opcode;
    uint8_t last_program_opcode;
    uint8_t last_erase_opcode;
    uint32_t program_count;
    uint32_t erase_count;
    uint32_t now_ms;
} mock_flash_t;

static mock_flash_t flash;
static sx_spi_if_t spi;
static w25q_t device;

static sx_bus_status_t mock_spi_transfer(void *context,
                                         const uint8_t *tx,
                                         uint8_t *rx,
                                         uint32_t length,
                                         uint32_t timeout_ms)
{
    mock_flash_t *mock = context;
    uint8_t opcode;

    (void)timeout_ms;
    assert(mock != NULL);
    assert(mock->selected);
    assert(tx != NULL);
    assert(rx != NULL);
    assert(length > 0U);
    memset(rx, 0xFF, length);

    opcode = tx[0];
    mock->last_opcode = opcode;
    switch (opcode) {
    case W25Q_CMD_READ_JEDEC_ID:
        assert(length == 4U);
        rx[1] = mock->manufacturer_id;
        rx[2] = mock->memory_type;
        rx[3] = mock->capacity_id;
        break;
    case W25Q_CMD_READ_STATUS_REGISTER_1:
        assert(length == 2U);
        rx[1] = mock->status;
        break;
    case W25Q_CMD_WRITE_ENABLE:
        assert(length == 1U);
        mock->status |= W25Q_STATUS_WRITE_ENABLE_MASK;
        break;
    case W25Q_CMD_READ_DATA:
    case W25Q_CMD_READ_DATA_4BYTE: {
        uint32_t header_length =
            opcode == W25Q_CMD_READ_DATA_4BYTE ? 5U : 4U;
        uint32_t index;

        assert(length >= header_length);
        for (index = header_length; index < length; index++) {
            rx[index] = (uint8_t)(index - header_length);
        }
        break;
    }
    case W25Q_CMD_PAGE_PROGRAM:
    case W25Q_CMD_PAGE_PROGRAM_4BYTE:
        mock->last_program_opcode = opcode;
        mock->program_count++;
        mock->status &= (uint8_t)~W25Q_STATUS_WRITE_ENABLE_MASK;
        break;
    case W25Q_CMD_SECTOR_ERASE_4K:
    case W25Q_CMD_SECTOR_ERASE_4K_4BYTE:
    case W25Q_CMD_BLOCK_ERASE_64K:
    case W25Q_CMD_BLOCK_ERASE_64K_4BYTE:
    case W25Q_CMD_CHIP_ERASE:
        mock->last_erase_opcode = opcode;
        mock->erase_count++;
        mock->status &= (uint8_t)~W25Q_STATUS_WRITE_ENABLE_MASK;
        break;
    default:
        assert(false);
        break;
    }

    return SX_BUS_STATUS_OK;
}

static sx_bus_status_t mock_chip_select(void *context, bool active)
{
    mock_flash_t *mock = context;

    assert(mock != NULL);
    if (active) {
        assert(!mock->selected);
    } else {
        assert(mock->selected);
    }
    mock->selected = active;
    return SX_BUS_STATUS_OK;
}

static uint32_t mock_get_time_ms(void *context)
{
    mock_flash_t *mock = context;

    return mock->now_ms;
}

static void mock_delay_ms(void *context, uint32_t delay_ms)
{
    mock_flash_t *mock = context;

    mock->now_ms += delay_ms;
}

static void test_setup(uint8_t capacity_id)
{
    w25q_config_t config;

    memset(&flash, 0, sizeof(flash));
    flash.manufacturer_id = W25Q_MANUFACTURER_ID_WINBOND;
    flash.memory_type = 0x40U;
    flash.capacity_id = capacity_id;
    spi.context = &flash;
    spi.transfer = mock_spi_transfer;
    spi.transfer_async = NULL;

    config.spi = &spi;
    config.chip_select_context = &flash;
    config.chip_select = mock_chip_select;
    config.time_context = &flash;
    config.get_time_ms = mock_get_time_ms;
    config.delay_ms = mock_delay_ms;
    config.transfer_timeout_ms = 50U;

    assert(w25q_init(&device, &config, NULL) == W25Q_STATUS_OK);
}

static void test_init_detects_3byte_device(void)
{
    w25q_jedec_id_t jedec_id;

    memset(&flash, 0, sizeof(flash));
    flash.manufacturer_id = W25Q_MANUFACTURER_ID_WINBOND;
    flash.memory_type = 0x40U;
    flash.capacity_id = 0x18U;
    spi.context = &flash;
    spi.transfer = mock_spi_transfer;

    {
        const w25q_config_t config = {
            .spi = &spi,
            .chip_select_context = &flash,
            .chip_select = mock_chip_select,
            .time_context = &flash,
            .get_time_ms = mock_get_time_ms,
            .delay_ms = mock_delay_ms,
            .transfer_timeout_ms = 50U,
        };
        assert(w25q_init(&device, &config, &jedec_id) == W25Q_STATUS_OK);
    }

    assert(jedec_id.manufacturer_id == W25Q_MANUFACTURER_ID_WINBOND);
    assert(jedec_id.memory_type == 0x40U);
    assert(jedec_id.capacity_id == 0x18U);
    assert(device.capacity_bytes == UINT32_C(16) * 1024U * 1024U);
    assert(device.address_bytes == 3U);
}

static void test_read_and_page_boundary_program(void)
{
    uint8_t data[2] = {0U, 0U};
    const uint8_t program_data[2] = {0xA5U, 0x5AU};

    test_setup(0x18U);
    assert(w25q_read(&device, 0x100U, data, sizeof(data)) == W25Q_STATUS_OK);
    assert(data[0] == 0U);
    assert(data[1] == 1U);

    assert(w25q_page_program(&device, 0xFFU, program_data,
                             sizeof(program_data), 10U) == W25Q_STATUS_OK);
    assert(flash.program_count == 2U);
    assert(flash.last_program_opcode == W25Q_CMD_PAGE_PROGRAM);
}

static void test_erase_alignment_and_chip_erase(void)
{
    test_setup(0x18U);
    assert(w25q_erase_sector_4k(&device, 0U, 10U) == W25Q_STATUS_OK);
    assert(flash.last_erase_opcode == W25Q_CMD_SECTOR_ERASE_4K);
    assert(w25q_erase_sector_4k(&device, 1U, 10U) ==
           W25Q_STATUS_OUT_OF_RANGE);

    assert(w25q_erase_block_64k(&device, 0U, 10U) == W25Q_STATUS_OK);
    assert(flash.last_erase_opcode == W25Q_CMD_BLOCK_ERASE_64K);
    assert(w25q_erase_chip(&device, 10U) == W25Q_STATUS_OK);
    assert(flash.last_erase_opcode == W25Q_CMD_CHIP_ERASE);
    assert(flash.erase_count == 3U);
}

static void test_large_device_uses_4byte_opcodes(void)
{
    test_setup(0x19U);
    assert(device.capacity_bytes == UINT32_C(32) * 1024U * 1024U);
    assert(device.address_bytes == 4U);
    assert(w25q_page_program(&device, 0x01000000U,
                             (const uint8_t *)"x", 1U, 10U) ==
           W25Q_STATUS_OK);
    assert(flash.last_program_opcode == W25Q_CMD_PAGE_PROGRAM_4BYTE);
    assert(w25q_erase_sector_4k(&device, 0x01000000U, 10U) ==
           W25Q_STATUS_OK);
    assert(flash.last_erase_opcode == W25Q_CMD_SECTOR_ERASE_4K_4BYTE);
}

static void test_busy_flash_and_out_of_range(void)
{
    uint8_t data;

    test_setup(0x18U);
    flash.status = W25Q_STATUS_BUSY_MASK;
    assert(w25q_read(&device, 0U, &data, 1U) == W25Q_STATUS_BUSY);
    assert(w25q_erase_sector_4k(&device, 0U, 2U) == W25Q_STATUS_TIMEOUT);
    assert(flash.now_ms >= 2U);

    flash.status = 0U;
    assert(w25q_read(&device, device.capacity_bytes, &data, 1U) ==
           W25Q_STATUS_OUT_OF_RANGE);
}

static void test_rejects_non_winbond_jedec_id(void)
{
    w25q_config_t config = {
        .spi = &spi,
        .chip_select_context = &flash,
        .chip_select = mock_chip_select,
        .time_context = &flash,
        .get_time_ms = mock_get_time_ms,
        .delay_ms = mock_delay_ms,
        .transfer_timeout_ms = 50U,
    };

    memset(&flash, 0, sizeof(flash));
    flash.manufacturer_id = 0x01U;
    flash.memory_type = 0x40U;
    flash.capacity_id = 0x18U;
    spi.context = &flash;
    spi.transfer = mock_spi_transfer;

    assert(w25q_init(&device, &config, NULL) ==
           W25Q_STATUS_UNSUPPORTED_DEVICE);
    assert(!device.initialized);
}

int main(void)
{
    test_init_detects_3byte_device();
    test_read_and_page_boundary_program();
    test_erase_alignment_and_chip_erase();
    test_large_device_uses_4byte_opcodes();
    test_busy_flash_and_out_of_range();
    test_rejects_non_winbond_jedec_id();
    return 0;
}
