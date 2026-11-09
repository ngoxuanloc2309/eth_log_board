/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : LWIP.c
 * Description        : This file provides initialization code for LWIP
 *                      middleWare.
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
#include <lwip.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"
#if defined(__CC_ARM) /* MDK ARM Compiler */
#include "lwip/sio.h"
#endif /* MDK ARM Compiler */
#include <ethernetif.h>
#include <stdatomic.h>
#include <string.h>
#include "logger.h"
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
/* Private function prototypes -----------------------------------------------*/
static void ethernet_link_status_updated(struct netif *netif);
static void ethernet_status_callback(struct netif *netif);
/* ETH Variables initialization ----------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN 1 */
static const char *TAG = "SYS";
static bool lwip_ipv4_dhcp_enabled = true;
static bool lwip_dhcp_started;
static uint8_t lwip_ipv4_address[4];
static uint8_t lwip_ipv4_netmask[4];
static uint8_t lwip_ipv4_gateway[4];
static atomic_bool lwip_link_is_up;

static void lwip_apply_ipv4_core(void *argument)
{
  ip4_addr_t address;
  ip4_addr_t netmask;
  ip4_addr_t gateway;

  (void)argument;
  if (lwip_dhcp_started) {
    dhcp_stop(&gnetif);
    lwip_dhcp_started = false;
  }

  if (lwip_ipv4_dhcp_enabled) {
    IP4_ADDR(&address, 0U, 0U, 0U, 0U);
    IP4_ADDR(&netmask, 0U, 0U, 0U, 0U);
    IP4_ADDR(&gateway, 0U, 0U, 0U, 0U);
  } else {
    IP4_ADDR(&address, lwip_ipv4_address[0], lwip_ipv4_address[1],
             lwip_ipv4_address[2], lwip_ipv4_address[3]);
    IP4_ADDR(&netmask, lwip_ipv4_netmask[0], lwip_ipv4_netmask[1],
             lwip_ipv4_netmask[2], lwip_ipv4_netmask[3]);
    IP4_ADDR(&gateway, lwip_ipv4_gateway[0], lwip_ipv4_gateway[1],
             lwip_ipv4_gateway[2], lwip_ipv4_gateway[3]);
  }
  netif_set_addr(&gnetif, &address, &netmask, &gateway);

  if (lwip_ipv4_dhcp_enabled) {
    const err_t start_result = dhcp_start(&gnetif);
    if (start_result != ERR_OK) {
      LOGE(TAG, "Could not start DHCP: %d", (int)start_result);
      return;
    }
    lwip_dhcp_started = true;
  }
}
/* USER CODE END 1 */

bool MX_LWIP_ConfigureIPv4(bool dhcp_enabled,
                           const uint8_t address[4],
                           const uint8_t static_netmask[4],
                           const uint8_t gateway[4])
{
  if (!dhcp_enabled &&
      (address == NULL || static_netmask == NULL || gateway == NULL)) {
    return false;
  }
  lwip_ipv4_dhcp_enabled = dhcp_enabled;
  if (!dhcp_enabled) {
    (void)memcpy(lwip_ipv4_address, address, sizeof(lwip_ipv4_address));
    (void)memcpy(lwip_ipv4_netmask, static_netmask,
                 sizeof(lwip_ipv4_netmask));
    (void)memcpy(lwip_ipv4_gateway, gateway, sizeof(lwip_ipv4_gateway));
  }
  return true;
}

bool MX_LWIP_ApplyIPv4(bool dhcp_enabled,
                       const uint8_t address[4],
                       const uint8_t static_netmask[4],
                       const uint8_t gateway[4])
{
  if (!MX_LWIP_ConfigureIPv4(dhcp_enabled, address, static_netmask, gateway)) {
    return false;
  }
  return tcpip_callback(lwip_apply_ipv4_core, NULL) == ERR_OK;
}

/* Variables Initialization */
struct netif gnetif;
ip4_addr_t ipaddr;
ip4_addr_t netmask;
ip4_addr_t gw;

bool MX_LWIP_IsLinkUp(void)
{
  return atomic_load_explicit(&lwip_link_is_up, memory_order_relaxed);
}

/* USER CODE BEGIN OS_THREAD_ATTR_CMSIS_RTOS_V2 */
#define INTERFACE_THREAD_STACK_SIZE (1024)
osThreadAttr_t attributes;
/* USER CODE END OS_THREAD_ATTR_CMSIS_RTOS_V2 */

/* USER CODE BEGIN 2 */
/* USER CODE END 2 */

/**
 * LwIP initialization function
 */
void MX_LWIP_Init()
{
  /* Initialize the LwIP stack with RTOS */
  tcpip_init(NULL, NULL);

  if (lwip_ipv4_dhcp_enabled) {
    IP4_ADDR(&ipaddr, 0U, 0U, 0U, 0U);
    IP4_ADDR(&netmask, 0U, 0U, 0U, 0U);
    IP4_ADDR(&gw, 0U, 0U, 0U, 0U);
  } else {
    IP4_ADDR(&ipaddr, lwip_ipv4_address[0], lwip_ipv4_address[1],
             lwip_ipv4_address[2], lwip_ipv4_address[3]);
    IP4_ADDR(&netmask, lwip_ipv4_netmask[0], lwip_ipv4_netmask[1],
             lwip_ipv4_netmask[2], lwip_ipv4_netmask[3]);
    IP4_ADDR(&gw, lwip_ipv4_gateway[0], lwip_ipv4_gateway[1],
             lwip_ipv4_gateway[2], lwip_ipv4_gateway[3]);
  }

  /* Add and configure the interface from the TCP/IP core thread. */
  if (netifapi_netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL,
                         &ethernetif_init, &tcpip_input) != ERR_OK) {
    LOGE(TAG, "Could not add Ethernet network interface");
    Error_Handler();
  }

  /* Registers the default network interface */
  if (netifapi_netif_set_default(&gnetif) != ERR_OK) {
    LOGE(TAG, "Could not select Ethernet network interface");
    Error_Handler();
  }
  
  /* We must always bring the network interface up connection or not... */
  if (netifapi_netif_set_up(&gnetif) != ERR_OK) {
    LOGE(TAG, "Could not bring Ethernet network interface up");
    Error_Handler();
  }
  
  /* Set the link callback function, this function is called on change of link status*/
  netif_set_link_callback(&gnetif, ethernet_link_status_updated);
  netif_set_status_callback(&gnetif, ethernet_status_callback);
  atomic_store_explicit(&lwip_link_is_up, netif_is_link_up(&gnetif),
                        memory_order_relaxed);
  /* Create the Ethernet link handler thread */
  /* USER CODE BEGIN H7_OS_THREAD_NEW_CMSIS_RTOS_V2 */
  memset(&attributes, 0x0, sizeof(osThreadAttr_t));
  attributes.name = "EthLink";
  attributes.stack_size = INTERFACE_THREAD_STACK_SIZE;
  attributes.priority = osPriorityHigh;
  osThreadNew(ethernet_link_thread, &gnetif, &attributes);
  /* USER CODE END H7_OS_THREAD_NEW_CMSIS_RTOS_V2 */

  if (lwip_ipv4_dhcp_enabled) {
    const err_t start_result = netifapi_dhcp_start(&gnetif);
    if (start_result == ERR_OK) {
      lwip_dhcp_started = true;
    } else {
      LOGE(TAG, "Could not start DHCP: %d", (int)start_result);
    }
  }
  /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

#ifdef USE_OBSOLETE_USER_CODE_SECTION_4
/* Kept to help code migration. (See new 4_1, 4_2... sections) */
/* Avoid to use this user section which will become obsolete. */
/* USER CODE BEGIN 4 */
/* USER CODE END 4 */
#endif



static void ethernet_status_callback(struct netif *netif)
{
  if (netif_is_up(netif)) {
    LOGD(TAG, "TCP IP Started");
    LOGD(TAG, "IP : %s", ipaddr_ntoa(&netif->ip_addr));
    LOGD(TAG, "NETMASK : %s", ipaddr_ntoa(&netif->netmask));
    LOGD(TAG, "DEFAULT GW : %s", ipaddr_ntoa(&netif->gw));
  } else {
    LOGD(TAG, "TCP IP Stopped");
  }
}

/**
 * @brief  Notify the User about the network interface config status
 * @param  netif: the network interface
 * @retval None
 */
static void ethernet_link_status_updated(struct netif *netif)
{
  atomic_store_explicit(&lwip_link_is_up, netif_is_link_up(netif),
                        memory_order_relaxed);
  ethernet_status_callback(netif);
  if (netif_is_up(netif))
  {
    /* USER CODE BEGIN 5 */
    LOGD(TAG,"ETH cable plug");
    /* USER CODE END 5 */
  }
  else /* netif is down */
  {
    /* USER CODE BEGIN 6 */
    LOGD(TAG,"ETH cable un-plug");
    /* USER CODE END 6 */
  }
}

#if defined(__CC_ARM) /* MDK ARM Compiler */
/**
 * Opens a serial device for communication.
 *
 * @param devnum device number
 * @return handle to serial device if successful, NULL otherwise
 */
sio_fd_t sio_open(u8_t devnum)
{
  sio_fd_t sd;

  /* USER CODE BEGIN 7 */
  sd = 0; // dummy code
          /* USER CODE END 7 */

  return sd;
}

/**
 * Sends a single character to the serial device.
 *
 * @param c character to send
 * @param fd serial device handle
 *
 * @note This function will block until the character can be sent.
 */
void sio_send(u8_t c, sio_fd_t fd)
{
  /* USER CODE BEGIN 8 */
  /* USER CODE END 8 */
}

/**
 * Reads from the serial device.
 *
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received - may be 0 if aborted by sio_read_abort
 *
 * @note This function will block until data can be received. The blocking
 * can be cancelled by calling sio_read_abort().
 */
u32_t sio_read(sio_fd_t fd, u8_t *data, u32_t len)
{
  u32_t recved_bytes;

  /* USER CODE BEGIN 9 */
  recved_bytes = 0; // dummy code
                    /* USER CODE END 9 */
  return recved_bytes;
}

/**
 * Tries to read from the serial device. Same as sio_read but returns
 * immediately if no data is available and never blocks.
 *
 * @param fd serial device handle
 * @param data pointer to data buffer for receiving
 * @param len maximum length (in bytes) of data to receive
 * @return number of bytes actually received
 */
u32_t sio_tryread(sio_fd_t fd, u8_t *data, u32_t len)
{
  u32_t recved_bytes;

  /* USER CODE BEGIN 10 */
  recved_bytes = 0; // dummy code
                    /* USER CODE END 10 */
  return recved_bytes;
}
#endif /* MDK ARM Compiler */
