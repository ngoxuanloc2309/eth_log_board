/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "bootloader.h"
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

IWDG_HandleTypeDef hiwdg;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_IWDG_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
#define APPLICATION_ADDRESS APP1_START_ADDR
#define SYSTEM_BOOTLOADER_ADDRESS 0x0BF97000
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch){
  HAL_UART_Transmit(&huart1,(uint8_t*)&ch,1,HAL_MAX_DELAY);
  return 1;
}

static void goto_application(uint32_t address)
{
  // printf("Gonna Jump to Application 0x%08X\r\n",APPLICATION_ADDRESS);
  void (*app_reset_handler)(void) = (void *)(*((volatile uint32_t *)(address + 4U)));
  // __set_MSP(*(volatile uint32_t*) 0x08040000);
  // Turn OFF the Green Led to tell the user that Bootloader is not running
  // HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET );    //Green LED OFF
  __disable_irq();
  /* Reset the Clock */
  HAL_RCC_DeInit();
  HAL_DeInit();
  HAL_UART_DeInit(&huart1);
  // HAL_DCACHE_DeInit(&hdcache1);
  // HAL_ICACHE_Disable();
  // HAL_ICACHE_DeInit();
  // __HAL_IWDG_DISABLE_WRITE_ACCESS(&hiwdg);
  
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  
  __set_MSP(*(volatile uint32_t *)address);
  /* Jump to application */
  SCB->VTOR = address;
  __enable_irq();
  app_reset_handler(); // call the app reset handler
}

void boot_init(Bootloader_t *boot)
{
  if (boot_load(boot) == false)
  {
    printf( "Can't load boot info\r\n");
  }
}
bool boot_load(Bootloader_t *boot)
{
  memcpy(boot,(void*)APP1_HEADER_ADDRESS,sizeof(Bootloader_t));
  if(boot->magic_nb != MAGIC_NUMBER){
        printf( "Not match magic number, create new boot header\r\n");
        printf( "Bootloader_t size : %u\r\n", sizeof(Bootloader_t));
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
        boot->crc = 0;
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
  memcpy(&tamp, (void *)APP1_HEADER_ADDRESS, sizeof(Bootloader_t));
  if (memcmp(&tamp, boot, sizeof(Bootloader_t)) != 0)
  {
    printf( "Can't write app1 header\r\n");
    return false;
  }
  bsp_flash_unlock();
  bsp_flash_erase(APP2_HEADER_ADDRESS,1);
  bsp_flash_program(APP2_HEADER_ADDRESS,(uint8_t*) boot, sizeof(Bootloader_t));
  bsp_flash_lock();
  memcpy(&tamp, (void *)APP2_HEADER_ADDRESS, sizeof(Bootloader_t));
  if (memcmp(&tamp, boot, sizeof(Bootloader_t)) != 0)
  {
    printf( "Can't write app2 header\r\n");
    return false;
  }
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

Bootloader_t synaptix_boot;

static void swap_firmware(AppInfo_t *app1, AppInfo_t *app2)
{
    bsp_flash_unlock();
    uint32_t app1_address = app1->address;
    uint32_t app2_address = app2->address;
    printf("Swap firmware from 0x%08X to 0x%08X\r\n",app2_address,app1_address);
    for(uint32_t i = 0;i< APP1_SECTOR_NUM; i+=SWAP_SECTOR_NUM){
        printf("Swap sector %u/%u\r\n",i+1,APP1_SECTOR_NUM);
        bsp_flash_erase(SWAP_ADDRESS,SWAP_SECTOR_NUM);
        bsp_flash_program(SWAP_ADDRESS,(uint8_t*)(app1_address),SWAP_SIZE);
        bsp_flash_erase(app1_address,SWAP_SECTOR_NUM);
        bsp_flash_program(app1_address,(uint8_t*)(app2_address),SWAP_SIZE);
        bsp_flash_erase(app2_address,SWAP_SECTOR_NUM);
        bsp_flash_program(app2_address,(uint8_t*)(SWAP_ADDRESS),SWAP_SIZE);
        app1_address += SWAP_SIZE;
        app2_address += SWAP_SIZE;
        HAL_IWDG_Refresh(&hiwdg);
    }
    bsp_flash_lock();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_IWDG_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  
  boot_init(&synaptix_boot);
  if(synaptix_boot.isNewFw == 1){
    swap_firmware(&synaptix_boot.app_info[0],&synaptix_boot.app_info[1]);
    printf("Firmware update done, verify CRC\r\n");
    uint16_t crc = firmware_crc((uint8_t *)APP1_START_ADDR, APP1_SECTOR_NUM * FLASH_SECTOR_SIZE);
    if (synaptix_boot.crc != crc)
    {
      printf("Not match CRC %u-%u", synaptix_boot.crc, crc);
      goto_application(SYSTEM_BOOTLOADER_ADDRESS);
    }
    synaptix_boot.isNewFw = 3;
    boot_save(&synaptix_boot);
    printf("Update done\r\n");
  }
  else if(synaptix_boot.isNewFw == 3){ // Rollback done, jump to new firmware
    printf("Rollback firmware\r\n");
    swap_firmware(&synaptix_boot.app_info[0],&synaptix_boot.app_info[1]);
    synaptix_boot.isNewFw = 0;
    boot_save(&synaptix_boot);
  }
  else if(synaptix_boot.isNewFw == 2){
    printf("Jump System bootloader\r\n");
    synaptix_boot.isNewFw = 0;
    boot_save(&synaptix_boot);
    HAL_Delay(1000);
    goto_application(SYSTEM_BOOTLOADER_ADDRESS);
  }

  printf("Synaptix bootloader\r\n");
  printf("Jump to 0x%08X\r\n",APPLICATION_ADDRESS);
  HAL_Delay(1000);
  goto_application(APPLICATION_ADDRESS);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    HAL_IWDG_Refresh(&hiwdg);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 250;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  hiwdg.Init.EWI = 0;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
