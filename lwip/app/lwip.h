/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : LWIP.h
  * Description        : This file provides code for the configuration
  *                      of the LWIP.
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
  *************************************************************************

  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __mx_lwip_H
#define __mx_lwip_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include <ethernetif.h>
/* Includes for RTOS ---------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#if WITH_RTOS
#include "lwip/tcpip.h"
#endif /* WITH_RTOS */

/* USER CODE BEGIN 0 */
#include "main.h"
extern struct netif gnetif;
/* USER CODE END 0 */

/* Global Variables ----------------------------------------------------------*/
extern ETH_HandleTypeDef heth;

/* LWIP init function */
void MX_LWIP_Init();

/**
 * @brief Select the IPv4 configuration to apply during MX_LWIP_Init().
 *
 * @param dhcp_enabled true to start DHCP; false to use the static values.
 * @param address Static IPv4 address, ignored when DHCP is enabled.
 * @param netmask Static IPv4 netmask, ignored when DHCP is enabled.
 * @param gateway Static IPv4 default gateway, ignored when DHCP is enabled.
 * @return true when the configuration was accepted.
 */
bool MX_LWIP_ConfigureIPv4(bool dhcp_enabled,
                           const uint8_t address[4],
                           const uint8_t netmask[4],
                           const uint8_t gateway[4]);

/**
 * @brief Queue an IPv4 reconfiguration on the lwIP TCP/IP thread.
 *
 * @param dhcp_enabled true to start DHCP; false to use the static values.
 * @param address Static IPv4 address, ignored when DHCP is enabled.
 * @param netmask Static IPv4 netmask, ignored when DHCP is enabled.
 * @param gateway Static IPv4 default gateway, ignored when DHCP is enabled.
 * @return true when the reconfiguration callback was queued.
 */
bool MX_LWIP_ApplyIPv4(bool dhcp_enabled,
                       const uint8_t address[4],
                       const uint8_t netmask[4],
                       const uint8_t gateway[4]);

/**
 * @brief Read the current Ethernet physical-link state.
 *
 * @return true when the Ethernet link is up.
 */
bool MX_LWIP_IsLinkUp(void);

#if !WITH_RTOS
/* USER CODE BEGIN 1 */
/* Function defined in lwip.c to:
 *   - Read a received packet from the Ethernet buffers
 *   - Send it to the lwIP stack for handling
 *   - Handle timeouts if NO_SYS_NO_TIMERS not set
 */
void MX_LWIP_Process(void);

/* USER CODE END 1 */
#endif /* WITH_RTOS */

#ifdef __cplusplus
}
#endif
#endif /*__ mx_lwip_H */

/**
  * @}
  */

/**
  * @}
  */
