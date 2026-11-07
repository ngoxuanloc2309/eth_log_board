/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

#include "fatfs.h"
#include "logger.h"
#include "board.h"
#include "app_config.h"
static const char *TAG = "FATFS";

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */
uint8_t rtext[FF_MAX_SS];/* File read buffer */
/* USER CODE END Variables */

int MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&SRAMDISK_Driver, USERPath);

  /* USER CODE BEGIN Init */
  log_debug(TAG,"FATFS_LinkDriver res : %d",retUSER);
FS_MOUNT:
  FRESULT ret = f_mount(&USERFatFS,USERPath,1);
  if(ret != FR_OK){
    log_error(TAG,"Can't mount %s",USERPath);
    log_info(TAG,"Formating...");
    ret = f_mkfs(USERPath,0, rtext, sizeof(rtext));
    if(ret != FR_OK){
      log_error(TAG,"Can't format , err : %d",ret);
      return ret;
    }
    bsp_delay(1);
    goto FS_MOUNT;
  }
  log_info(TAG,"FATFS mount %s successfully",USERPath);
  char path[20];
  sprintf(path,"%s/Readme.md",USERPath);
  FRESULT fret = f_open(&USERFile,path,FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if (fret != FR_OK) {
    log_error(TAG,"Can't open file %s",path);
    f_close(&USERFile);
    return fret;
  }
  UINT bytesWrote;
	// fret = f_write(&USERFile, "This is SX-ETH-ZB Gateway\r\n", strlen("This is SX-ETH-ZB Gateway\r\n"), &bytesWrote);
  f_printf(&USERFile, "Vendor : %s\r\n", BOARD_VENDOR);
  f_printf(&USERFile, "URL : %s\r\n", BOARD_URL);
  f_printf(&USERFile, "Board name: %s\r\n", BOARD_NAME);
  f_printf(&USERFile, "Version : V%d.%d.%d.rc%d\r\n", BSP_VERSION_MAJOR, BSP_VERSION_MINOR, BSP_VERSION_PATCH, BSP_VERSION_RC);
  f_printf(&USERFile, "UUID : 0x%08X\r\n", bsp_get_chip_id());
  f_close(&USERFile);

  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;

  /* Get volume information and free clusters of drive 1 */
  fret = f_getfree(USERPath, &fre_clust, &fs);
  if (fret){
    log_error(TAG,"Can't read free space");
    return fret;
  }

  /* Get total sectors and free sectors */
  tot_sect = (fs->n_fatent - 2) * fs->csize;
  fre_sect = fre_clust * fs->csize;

  /* Print the free space (assuming 512 bytes/sector) */
  log_info(TAG,"%lu KiB total drive space/%lu KiB available.", tot_sect / 2, fre_sect / 2);
  return fret;
  /* additional user code for init */
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  return 0;
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
