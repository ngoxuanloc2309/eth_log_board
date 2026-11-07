/**
  ******************************************************************************
  * @file    sram_diskio_config.h
  * @author  MCD Application Team
  * @brief   Template for the sram_diskio_config.h. This file should be copied and
             under project.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the st_license.txt
  * file in the root directory of this software component.
  * If no st_license.txt file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifndef SRAM_DISKIO_CONFIG_H
#define SRAM_DISKIO_CONFIG_H


#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/

#include "stm32h5xx_hal.h"
#include "app_config.h"
/* Private typedef -----------------------------------------------------------*/
extern uint8_t MSC_Storage[STORAGE_SIZE];
/* Private define ------------------------------------------------------------*/
/* Block size */
#define BLOCK_SIZE                512

/* Base Address */
#define SRAM_DISK_BASE_ADDR       MSC_Storage

/* SRAM Disk size in bytes */
#define SRAM_DISK_SIZE            (STORAGE_SIZE)


#ifdef __cplusplus
}
#endif

#endif /* SRAM_DISKIO_CONFIG_H */