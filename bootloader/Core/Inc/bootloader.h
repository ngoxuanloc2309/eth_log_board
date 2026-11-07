#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include <stm32h563xx.h>
#include <stm32h5xx_hal_flash.h>
#include <stm32h5xx_hal_flash_ex.h>
#include <stdbool.h>
#include "main.h"

#define MAGIC_NUMBER 0x1234ABCE
#define NUMBER_APPS 2
#define FACTORY_APP_EN 0
// #define STM32_H563ZI 1

#define APP_1 0
#define APP_2 1

#define FLASH_PROG_STEP_SIZE             ((uint8_t)16U)
#define SECTOR_SIZE             (8*1024)

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

#define SWAP_ADDRESS             0x08100000
#define SWAP_SECTOR              FLASH_SECTOR_0
#define SWAP_SECTOR_NUM          1
#define SWAP_SIZE               (SWAP_SECTOR_NUM*SECTOR_SIZE) // Bytes
#define SWAP_BANK                FLASH_BANK_2

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

void boot_init(Bootloader_t *boot);
bool boot_load(Bootloader_t *boot);
bool boot_save(Bootloader_t *boot);

/**
 * @brief Unlocks the FLASH control register access.
 *
 * This macro calls the HAL_FLASH_Unlock() function to enable write access to the FLASH control registers.
 * It is typically used before performing any FLASH memory operations such as programming or erasing.
 *
 * @note Ensure to lock the FLASH control register after operations by calling the appropriate lock function.
 */
#define bsp_flash_unlock() HAL_FLASH_Unlock()
/**
 * @brief   Locks the FLASH control register access.
 *
 * This macro calls the HAL_FLASH_Lock() function to disable write access
 * to the FLASH control registers, preventing accidental modification of
 * FLASH memory. It is typically used after FLASH operations are complete
 * to enhance system safety.
 */
#define bsp_flash_lock() HAL_FLASH_Lock()
#define NUMBER_SECTOR_OF_BANK 128
static inline uint32_t bsp_flash_get_sector(uint32_t address){
    uint32_t sector = ( (address - FLASH_BASE) / FLASH_SECTOR_SIZE);
    return sector % NUMBER_SECTOR_OF_BANK;
}

static inline uint32_t bsp_flash_get_bank(uint32_t address){
   uint32_t sector = ( (address - FLASH_BASE) / FLASH_SECTOR_SIZE);

   return (sector < NUMBER_SECTOR_OF_BANK)  ? FLASH_BANK_1 : FLASH_BANK_2 ;
}
/**
 * @brief Erases one or more pages in flash memory starting from the specified address.
 *
 * This function erases a specified number of flash memory pages, beginning at the given page address.
 *
 * @param page_address The starting address of the flash page to erase.
 * @param num_pages The number of consecutive pages to erase.
 * @return uint32_t Status code indicating success or failure of the erase operation.
 */
static inline uint32_t bsp_flash_erase(uint32_t address,uint32_t num_sector){
    FLASH_EraseInitTypeDef erase = {.Banks = bsp_flash_get_bank(address),.NbSectors = num_sector,.Sector = bsp_flash_get_sector(address),.TypeErase = FLASH_TYPEERASE_SECTORS};
    uint32_t page_err = 0;
    if(HAL_FLASHEx_Erase(&erase,&page_err) != HAL_OK){
        return page_err;
    }
    return 0;
}

/**
 * @brief Programs data to the flash memory at the specified address.
 *
 * This function writes a block of data to the flash memory starting at the given address.
 *
 * @param address The starting address in flash memory where data will be programmed.
 * @param data Pointer to the data buffer to be written to flash.
 * @param len The number of bytes to program from the data buffer.
 * @return uint32_t Status code indicating success or failure of the operation.
 */
static inline uint32_t bsp_flash_program(uint32_t addr,uint8_t *buf,uint32_t len) {
    // HAL_FLASH_Unlock();
    __disable_irq();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    for (uint32_t i = 0; i < len; i += 16) {
        HAL_IWDG_Refresh(&hiwdg);
        uint8_t quadword[16];
        memset(quadword, 0xFF, 16);
        memcpy(quadword, &buf[i], (len - i >= 16) ? 16 : (len - i));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, addr + i, (uint32_t)quadword) != HAL_OK) {
            __enable_irq();
            // HAL_FLASH_Lock();
            return -1;
        }
    }
    __enable_irq();
    // HAL_FLASH_Lock();
    return 0;
}

/**
 * @brief Reads data from flash memory at the specified address.
 *
 * @param address The starting address in flash memory to read from.
 * @param data Pointer to the buffer where the read data will be stored.
 * @param len Number of bytes to read from flash memory.
 * @return uint32_t Status code indicating success or failure of the read operation.
 */
static inline uint32_t bsp_flash_read(uint32_t address,void *data,uint32_t len){
    memcpy(data,(void*)address,len);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif