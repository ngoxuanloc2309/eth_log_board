/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lptim.c
  * @brief   This file provides code for the configuration
  *          of the LPTIM instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "lptim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

LPTIM_HandleTypeDef hlptim1;
LPTIM_HandleTypeDef hlptim2;
LPTIM_HandleTypeDef hlptim3;

/* LPTIM1 init function */
void MX_LPTIM1_Init(void)
{

  /* USER CODE BEGIN LPTIM1_Init 0 */

  /* USER CODE END LPTIM1_Init 0 */

  /* USER CODE BEGIN LPTIM1_Init 1 */

  /* USER CODE END LPTIM1_Init 1 */
  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.Period = 250-1;
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.RepetitionCounter = 100-1;
  if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */

}
/* LPTIM2 init function */
void MX_LPTIM2_Init(void)
{

  /* USER CODE BEGIN LPTIM2_Init 0 */

  /* USER CODE END LPTIM2_Init 0 */

  /* USER CODE BEGIN LPTIM2_Init 1 */

  /* USER CODE END LPTIM2_Init 1 */
  hlptim2.Instance = LPTIM2;
  hlptim2.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim2.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim2.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim2.Init.Period = 250-1;
  hlptim2.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim2.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim2.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim2.Init.RepetitionCounter = 100-1;
  if (HAL_LPTIM_Init(&hlptim2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM2_Init 2 */

  /* USER CODE END LPTIM2_Init 2 */

}
/* LPTIM3 init function */
void MX_LPTIM3_Init(void)
{

  /* USER CODE BEGIN LPTIM3_Init 0 */

  /* USER CODE END LPTIM3_Init 0 */

  /* USER CODE BEGIN LPTIM3_Init 1 */

  /* USER CODE END LPTIM3_Init 1 */
  hlptim3.Instance = LPTIM3;
  hlptim3.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim3.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim3.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim3.Init.Period = 250-1;
  hlptim3.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim3.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim3.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim3.Init.RepetitionCounter = 99-1;
  if (HAL_LPTIM_Init(&hlptim3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM3_Init 2 */

  /* USER CODE END LPTIM3_Init 2 */

}

void HAL_LPTIM_MspInit(LPTIM_HandleTypeDef* lptimHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(lptimHandle->Instance==LPTIM1)
  {
  /* USER CODE BEGIN LPTIM1_MspInit 0 */

  /* USER CODE END LPTIM1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
    PeriphClkInitStruct.Lptim1ClockSelection = RCC_LPTIM1CLKSOURCE_PCLK3;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPTIM1 clock enable */
    __HAL_RCC_LPTIM1_CLK_ENABLE();

    /* LPTIM1 interrupt Init */
    HAL_NVIC_SetPriority(LPTIM1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LPTIM1_IRQn);
  /* USER CODE BEGIN LPTIM1_MspInit 1 */

  /* USER CODE END LPTIM1_MspInit 1 */
  }
  else if(lptimHandle->Instance==LPTIM2)
  {
  /* USER CODE BEGIN LPTIM2_MspInit 0 */

  /* USER CODE END LPTIM2_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM2;
    PeriphClkInitStruct.Lptim2ClockSelection = RCC_LPTIM2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPTIM2 clock enable */
    __HAL_RCC_LPTIM2_CLK_ENABLE();

    /* LPTIM2 interrupt Init */
    HAL_NVIC_SetPriority(LPTIM2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LPTIM2_IRQn);
  /* USER CODE BEGIN LPTIM2_MspInit 1 */

  /* USER CODE END LPTIM2_MspInit 1 */
  }
  else if(lptimHandle->Instance==LPTIM3)
  {
  /* USER CODE BEGIN LPTIM3_MspInit 0 */

  /* USER CODE END LPTIM3_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM3;
    PeriphClkInitStruct.Lptim3ClockSelection = RCC_LPTIM3CLKSOURCE_PCLK3;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* LPTIM3 clock enable */
    __HAL_RCC_LPTIM3_CLK_ENABLE();

    /* LPTIM3 interrupt Init */
    HAL_NVIC_SetPriority(LPTIM3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(LPTIM3_IRQn);
  /* USER CODE BEGIN LPTIM3_MspInit 1 */

  /* USER CODE END LPTIM3_MspInit 1 */
  }
}

void HAL_LPTIM_MspDeInit(LPTIM_HandleTypeDef* lptimHandle)
{

  if(lptimHandle->Instance==LPTIM1)
  {
  /* USER CODE BEGIN LPTIM1_MspDeInit 0 */

  /* USER CODE END LPTIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPTIM1_CLK_DISABLE();

    /* LPTIM1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(LPTIM1_IRQn);
  /* USER CODE BEGIN LPTIM1_MspDeInit 1 */

  /* USER CODE END LPTIM1_MspDeInit 1 */
  }
  else if(lptimHandle->Instance==LPTIM2)
  {
  /* USER CODE BEGIN LPTIM2_MspDeInit 0 */

  /* USER CODE END LPTIM2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPTIM2_CLK_DISABLE();

    /* LPTIM2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(LPTIM2_IRQn);
  /* USER CODE BEGIN LPTIM2_MspDeInit 1 */

  /* USER CODE END LPTIM2_MspDeInit 1 */
  }
  else if(lptimHandle->Instance==LPTIM3)
  {
  /* USER CODE BEGIN LPTIM3_MspDeInit 0 */

  /* USER CODE END LPTIM3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LPTIM3_CLK_DISABLE();

    /* LPTIM3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(LPTIM3_IRQn);
  /* USER CODE BEGIN LPTIM3_MspDeInit 1 */

  /* USER CODE END LPTIM3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

