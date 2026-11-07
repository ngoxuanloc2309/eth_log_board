#include "ota.h"
#include "logger.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "board.h"

// #include "apps.h"

static const char *TAG = "OTA";

static uint32_t total_size = 0;
static uint32_t current_size = 0;
static volatile uint32_t address = 0;
uint32_t percent = 0;
Bootloader_t boot_patition;
// #if PLATFORM_CONFIG == STM32H5
#define OTA_USER_SWAP_BANK 0

static void ota_restart_timer_callback( TimerHandle_t xTimer ){
	bsp_restart();
}

static int erase_sector_bank1(uint32_t sector)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_type = {.Banks = FLASH_BANK_1, .Sector = sector, .NbSectors = 1, .TypeErase = FLASH_TYPEERASE_SECTORS};
    uint32_t sector_err = 0;
    HAL_StatusTypeDef err = HAL_FLASHEx_Erase(&erase_type, &sector_err);
    if (err == HAL_OK)
    {
        log_info(TAG, "Erase app : sector = %lu, nbSector = %lu", sector, 1);
        HAL_FLASH_Lock();
        return 0;
    }
    HAL_FLASH_Lock();
    log_error(TAG, "Can't erase flash %d,%lu", err, sector_err);
    return -1;
}

static int erase_sector_bank2(uint32_t sector)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_type = {.Banks = FLASH_BANK_2, .Sector = sector, .NbSectors = 1, .TypeErase = FLASH_TYPEERASE_SECTORS};
    uint32_t sector_err = 0;
    HAL_StatusTypeDef err = HAL_FLASHEx_Erase(&erase_type, &sector_err);
    if (err == HAL_OK)
    {
        log_info(TAG, "Erase app : sector = %lu, nbSector = %lu", sector, 1);
        HAL_FLASH_Lock();
        return 0;
    }
    HAL_FLASH_Lock();
    log_error(TAG, "Can't erase flash %d,%lu", err, sector_err);
    return -1;
}

static int flash_sector(uint32_t address, void *data, size_t size, size_t len)
{

    if ((size * len) % 16 != 0)
        return -1;
    HAL_FLASH_Unlock();
    uint32_t data128[4];
    log_info(TAG, "Flash address : 0x%08X, size : %lu bytes", address, size * len);
    for (size_t i = 0; i < size * len; i += 16)
    {
        memcpy(data128, data + i, 16);
        HAL_StatusTypeDef err = HAL_FLASH_Program((uint32_t)FLASH_TYPEPROGRAM_QUADWORD, address + i,(uint32_t) data128);
        if (err != HAL_OK)
        {
            log_error(TAG, "Flash false address : 0x%08X , i = %lu, err = %d", address + i, i, err);
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    log_info(TAG, "Flash success");

    return 0;
}

static void OPENBL_FLASH_Program(uint32_t FlashAddress, uint32_t DataAddress)
{
  /* Clear all FLASH errors flags before starting write operation */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, FlashAddress, DataAddress);
}

static void flash_write(uint32_t Address,uint8_t *pData,uint32_t DataLength){
  uint32_t index;
  __ALIGNED(4) uint8_t data[FLASH_PROG_STEP_SIZE] = {0x0U};
  uint8_t remaining;

  if ((pData != NULL) && (DataLength != 0U))
  {
    /* Unlock the flash memory for write operation */
    HAL_FLASH_Unlock();

    /* Clear error programming flags */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    /* Program double-word by double-word (16 bytes) */
    while ((DataLength >> 4U) > 0U)
    {
      for (index = 0U; index < FLASH_PROG_STEP_SIZE; index++)
      {
        data[index] = *(pData + index);
      }

      OPENBL_FLASH_Program(Address, (uint32_t)data);

      Address    += FLASH_PROG_STEP_SIZE;
      pData      += FLASH_PROG_STEP_SIZE;
      DataLength -= FLASH_PROG_STEP_SIZE;
    }

    /* If remaining count, go back to fill the rest with 0xFF */
    if (DataLength > 0U)
    {
      remaining = FLASH_PROG_STEP_SIZE - DataLength;

      /* Copy the remaining bytes */
      for (index = 0U; index < DataLength; index++)
      {
        data[index] = *(pData + index);
      }

      /* Fill the upper bytes with 0xFF */
      for (index = 0U; index < remaining; index++)
      {
        data[index + DataLength] = 0xFFU;
      }

      /* FLASH word program */
      OPENBL_FLASH_Program(Address, (uint32_t)data);
    }

    /* Lock the Flash to disable the flash control register access */
    HAL_FLASH_Lock();
  } 
}

void boot_init(Bootloader_t *boot)
{
  if (boot_load(boot) == false)
  {
    log_error(TAG, "Can't load boot info");
  }
}
bool boot_load(Bootloader_t *boot)
{
  memcpy(boot,(void*)APP1_HEADER_ADDRESS,sizeof(Bootloader_t));
  if(boot->magic_nb != MAGIC_NUMBER){
        log_warn(TAG, "Not match magic number, create new boot header");
        log_info(TAG, "Bootloader_t size : %lu", sizeof(Bootloader_t));
        boot->app_info[0].address = APP1_START_ADDR;
        boot->app_info[0].sector = APP1_SECTOR_START;
        boot->app_info[0].numSector = APP1_SECTOR_NUM;
        boot->app_info[0].sector_size = SECTOR_SIZE;
        boot->app_info[0].app_size = APP1_SIZE;
        boot->app_info[0].bank = APP1_BANK;
        boot->app_info[1].address = APP2_START_ADDR;
        boot->app_info[1].sector = APP2_SECTOR_START;
        boot->app_info[1].numSector = APP2_SECTOR_NUM;
        boot->app_info[1].sector_size = SECTOR_SIZE;
        boot->app_info[1].app_size = APP2_SIZE;
        boot->app_info[1].bank = APP2_BANK;
        boot->magic_nb = MAGIC_NUMBER;
        boot->isNewFw = false;
        boot->current_app = APP_1;
        return boot_save(boot);
  }
  return true;
}
bool boot_save(Bootloader_t *boot)
{
  Bootloader_t tamp;
  bsp_flash_unlock();
  bsp_flash_erase(APP1_HEADER_ADDRESS,1);
  bsp_flash_program(APP1_HEADER_ADDRESS,(uint8_t*) boot, sizeof(Bootloader_t));
  bsp_flash_lock();
  // memcpy(&tamp, (void *)APP1_HEADER_ADDRESS, sizeof(Bootloader_t));
  // if (memcmp(&tamp, boot, sizeof(Bootloader_t)) != 0)
  // {
  //   log_error(TAG, "Can't write app1 header");
  //   return false;
  // }
  // bsp_flash_unlock();
  // bsp_flash_erase(APP2_HEADER_ADDRESS,1);
  // bsp_flash_program(APP2_HEADER_ADDRESS, boot, sizeof(Bootloader_t));
  // bsp_flash_lock();
  // memcpy(&tamp, (void *)APP2_HEADER_ADDRESS, sizeof(Bootloader_t));
  // if (memcmp(&tamp, boot, sizeof(Bootloader_t)) != 0)
  // {
  //   log_error(TAG, "Can't write app2 header");
  //   return false;
  // }
  return true;
}

static uint16_t firmware_crc( const uint8_t *buf, uint32_t len )
{
	uint16_t crc = 0xFFFF;
	char i = 0;

	while(len--)
	{
		crc ^= (*buf++);

		for(i = 0; i < 8; i++)
		{
			if( crc & 1 )
			{
				crc >>= 1;
				crc ^= 0xA001;
			}
			else
			{
				crc >>= 1;
			}
		}
	}

	return crc;
}

bool mg_ota_begin(size_t new_firmware_size)
{
  log_info(TAG, "New firmware, size - %lu bytes", new_firmware_size);
  if (new_firmware_size == 0) {
    log_error(TAG, "Firmware size is zero");
    return false;
  }
  if(!boot_load(&boot_patition)) {
    log_error(TAG, "Can't load boot info");
    return false;
  }
  total_size = new_firmware_size;
  current_size = 0;
  address = APP2_START_ADDR;
  percent = 0;

  // FLASH_EraseInitTypeDef erase = {.Banks = APP2_BANK,.NbSectors = APP2_SECTOR_NUM,.Sector = APP2_SECTOR_NUM,.TypeErase = TYPEERASE_SECTORS};
  // uint32_t erase_sector = 0;

  bsp_flash_unlock();// HAL_FLASH_Unlock();
  // if(HAL_OK != HAL_FLASHEx_Erase(&erase,&erase_sector)){
  //   log_error(TAG,"Can't erase sector %u",erase_sector);
  //   HAL_FLASH_Lock();
  //   return false;
  // }
  if(0 != bsp_flash_erase(APP2_START_ADDR,APP2_SECTOR_NUM)){
    log_error(TAG,"Can't erase sector ");
    bsp_flash_lock();
    return false;
  }
  bsp_flash_lock();
  // HAL_FLASH_Lock();
  return true;
}
bool mg_ota_write(const void *buf, size_t len)
{
  // flash_write(address, (uint8_t *)buf, len);
  bsp_flash_unlock();
  bsp_flash_program(address,buf,len);
  bsp_flash_lock();
  current_size += len;
  address += len;
  uint32_t new_percent = (current_size * 100) / total_size;
  if (new_percent > percent)
  {
    percent = new_percent;
    // log_info(TAG, "Upload firmware : %02d %% | Wrote 0x%08X , %lu/%lu bytes , size %lu bytes", percent, address - len, current_size, total_size, len);
    switch (percent)
    {
    case 25:
      /* code */
      log_info(TAG, "[=====>   25%%         ]");
      break;
    case 50:
      log_info(TAG, "[=========50%%>        ]");
      break;
    case 75:
      log_info(TAG, "[=========75%%====>    ]");
      break;
    case 100:
      log_info(TAG, "[=========100%%========]");
      break;
    default:
      break;
    }
  }
  return true;
}
bool mg_ota_end(void)
{
  if (current_size != total_size) {
  log_error(TAG, "Firmware write incomplete: %lu/%lu", current_size, total_size);
  return false;
  }
  log_info(TAG, "File download complete");
  log_info(TAG,"Read progress :");
  log_info(TAG,"[=========100%%========]");
  // Verify the written firmware
  log_info(TAG, "Download verified successfully");
#if OTA_USER_SWAP_BANK
  FLASH_OBProgramInitTypeDef OBInit;
  HAL_FLASHEx_OBGetConfig(&OBInit);
  if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_DISABLE)
  {
    /*Swap to bank2 */
    /*Set OB SWAP_BANK_OPT to swap Bank2*/
    OBInit.OptionType = OPTIONBYTE_USER;
    OBInit.USERType = OB_USER_SWAP_BANK;
    OBInit.USERConfig = OB_SWAP_BANK_ENABLE;
    HAL_FLASHEx_OBProgram(&OBInit);

    /* Launch Option bytes loading */
    HAL_FLASH_OB_Launch();
    /* Reset the MCU */
    HAL_NVIC_SystemReset();
  }
  else
  {
    /* Swap to bank1 */
    /*Set OB SWAP_BANK_OPT to swap Bank1*/
    OBInit.OptionType = OPTIONBYTE_USER;
    OBInit.USERType = OB_USER_SWAP_BANK;
    OBInit.USERConfig = OB_SWAP_BANK_DISABLE;
    HAL_FLASHEx_OBProgram(&OBInit);

    /* Launch Option bytes loading */
    HAL_FLASH_OB_Launch();

    /* Reset the MCU */
    HAL_NVIC_SystemReset();
  }
#endif
  // boot_patition.app_address = OTA_APP1_ADDRESS;
  boot_patition.crc = firmware_crc((const uint8_t*)APP2_START_ADDR,APP2_SIZE);
  boot_patition.isNewFw = 1;
  vTaskDelay(1000); // Delay to ensure all writes are complete
  bool res = boot_save(&boot_patition);
  if(res) {
    log_info(TAG,"Running new firmware at 0x%08X", boot_patition.app_info[0].address);
    // bsp_restart();
    TimerHandle_t ota_timer = xTimerCreate("OTA_Restart_Timer", pdMS_TO_TICKS(5000), pdFALSE, ( void * ) 0, ota_restart_timer_callback);
		xTimerStart( ota_timer, 0 );
    return true;
  }
  log_error(TAG,"Can't save ota patition");
  return false;
}

// #else // Custom define

// #endif
