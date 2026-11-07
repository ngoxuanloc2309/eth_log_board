#ifndef __FLASH_H
#define __FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "app_config.h"

#if PLATFORM_CONFIG == STM32H5

#include "stm32h5xx_hal_flash_ex.h"
#include "stm32h5xx_hal_flash.h"

#define FLASH_START_ADDRESS 0x08000000
#define FLASH_SECTOR_SIZE   8*1024 //bytes
#define FLASH_SECTOR_NUM    256



#endif


#ifdef __cplusplus
}
#endif

#endif // __FLASH_H