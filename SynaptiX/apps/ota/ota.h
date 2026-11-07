
#ifndef __OTA_H
#define __OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"
#include "app_config.h"

#define MAGIC_NUMBER 0x1234ABCE 
#define NUMBER_APPS 2
#define FACTORY_APP_EN 0
// #define STM32_H563ZI 1

#define APP_1 0
#define APP_2 1

#define FLASH_PROG_STEP_SIZE             ((uint8_t)16U)
#define SECTOR_SIZE             (FLASH_SECTOR_SIZE)

#define BOOTLOADER_START_ADDR   0x08000000
#define BOOTLOADER_SECTOR_START FLASH_SECTOR_0
#define BOOTLOADER_NUM_SECTER   8
#define BOOTLOADER_SIZE         (BOOTLOADER_NUM_SECTER*SECTOR_SIZE) // Bytes
    
#define APP1_START_ADDR         0x08010000
#define APP1_SECTOR_START       FLASH_SECTOR_8
#define APP1_SECTOR_NUM         96
#define APP1_SIZE               (APP1_SECTOR_NUM*SECTOR_SIZE) // Bytes
#define APP1_BANK               FLASH_BANK_1

#define APP2_START_ADDR         0x08110000
#define APP2_SECTOR_START       FLASH_SECTOR_8
#define APP2_SECTOR_NUM         96
#define APP2_SIZE               (APP2_SECTOR_NUM*SECTOR_SIZE) // Bytes
#define APP2_BANK               FLASH_BANK_2

#define APP1_HEADER_ADDRESS      0x080FE000
#define APP1_HEADER_SECTOR       FLASH_SECTOR_127
#define APP1_HEADER_SECTOR_NUM   1
#define APP1_HEADER_SIZE         (APP1_HEADER_SECTOR_NUM*SECTOR_SIZE) // Bytes

#define APP2_HEADER_ADDRESS      0x081FE000
#define APP2_HEADER_SECTOR       FLASH_SECTOR_127
#define APP2_HEADER_SECTOR_NUM   1
#define APP2_HEADER_SIZE         (APP2_HEADER_SECTOR_NUM*SECTOR_SIZE) // Bytes

typedef struct AppInfo{
    uint32_t sector; // 4 bytes
    uint32_t numSector; // 4 bytes
    uint32_t bank; // 4 bytes
    uint32_t address; // 4 bytes
    uint32_t sector_size; // 4 bytes
    uint32_t app_size; // 4 bytes
}AppInfo_t; // 24 bytes

typedef struct Bootloader{
    uint8_t isNewFw;
    AppInfo_t app_info[NUMBER_APPS]; // 48 bytes
#if FACTORY_APP_EN
    AppInfo_t factory_app;
#endif
    uint32_t magic_nb; // 4 bytes
    uint16_t crc;
    uint8_t current_app;
    uint8_t reserved[4];
}Bootloader_t; // 64 bytes

/**
 * @brief Initializes the bootloader.
 *
 * This function sets up the bootloader using the provided Bootloader_t structure.
 *
 * @param boot Pointer to a Bootloader_t structure containing bootloader configuration and state.
 */
void boot_init(Bootloader_t *boot);
/**
 * @brief Loads the bootloader using the provided Bootloader_t structure.
 *
 * This function initiates the bootloader process with the specified bootloader
 * configuration or context.
 *
 * @param boot Pointer to a Bootloader_t structure containing bootloader information.
 * @return true if the bootloader was loaded successfully, false otherwise.
 */
bool boot_load(Bootloader_t *boot);
/**
 * @brief Saves the current bootloader state or configuration.
 *
 * This function persists the provided Bootloader_t structure, typically to non-volatile memory,
 * to ensure that bootloader settings or state can be restored after a reset or power cycle.
 *
 * @param boot Pointer to a Bootloader_t structure containing the bootloader state to be saved.
 * @return true if the operation was successful, false otherwise.
 */
bool boot_save(Bootloader_t *boot);

/**
 * @brief Initializes the OTA (Over-The-Air) update process.
 *
 * Prepares the system to receive a new firmware image of the specified size.
 *
 * @param new_firmware_size The size (in bytes) of the new firmware to be received.
 * @return true if the OTA process was successfully initialized, false otherwise.
 */
bool mg_ota_begin(size_t new_firmware_size);
/**
 * @brief Writes OTA (Over-The-Air) update data to the device.
 *
 * This function receives a buffer containing OTA update data and writes it to the appropriate
 * storage or memory location. It is typically called repeatedly with chunks of data until the
 * entire update has been transferred.
 *
 * @param buf Pointer to the buffer containing the data to write.
 * @param len Length of the data in bytes.
 * @return true if the data was written successfully, false otherwise.
 */
bool mg_ota_write(const void *buf, size_t len);
/**
 * @brief Finalizes the OTA (Over-The-Air) update process.
 *
 * This function should be called after the OTA update process is complete.
 * It performs any necessary cleanup or finalization steps required to
 * properly end the OTA session.
 *
 * @return true if the OTA process ended successfully, false otherwise.
 */
bool mg_ota_end(void);

#ifdef __cplusplus
}
#endif

#endif // __OTA_H