/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : Target/lwipopts.h
  * Description        : This file overrides LwIP stack default configuration
  *                      done in opt.h file.
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

/* Define to prevent recursive inclusion --------------------------------------*/
#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#include "main.h"
#include "logger.h"
/*-----------------------------------------------------------------------------*/
/* Current version of LwIP supported by CubeMx: 2.1.2 -*/
/*-----------------------------------------------------------------------------*/

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

#ifdef __cplusplus
 extern "C" {
#endif

/* STM32CubeMX Specific Parameters (not defined in opt.h) ---------------------*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- WITH_RTOS enabled (Since FREERTOS is set) -----*/
#define WITH_RTOS 1
#define NO_SYS 0
/* Temporary workaround to avoid conflict on errno defined in STM32CubeIDE and lwip sys_arch.c errno */
#undef LWIP_PROVIDE_ERRNO
/*----- CHECKSUM_BY_HARDWARE enabled -----*/
#define CHECKSUM_BY_HARDWARE 1
/*-----------------------------------------------------------------------------*/

/* LWIP_SO_RCVBUF is enabled => this requires INT_MAX definition in limits.h --*/
#include "limits.h"

/* LwIP Stack Parameters (modified compared to initialization value in opt.h) -*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- Value in opt.h for LWIP_DHCP: 0 -----*/
#define LWIP_DHCP 1
/*----- Default Value for LWIP_DNS: 0 ---*/
#define LWIP_DNS 1
/*----- Default Value for MEMP_NUM_UDP_PCB: 4 ---*/
#define MEMP_NUM_UDP_PCB 20
/*----- Default Value for MEMP_NUM_TCP_PCB: 5 ---*/
#define MEMP_NUM_TCP_PCB 20
/*----- Default Value for LWIP_TCPIP_CORE_LOCKING: 0 ---*/
#define LWIP_TCPIP_CORE_LOCKING 0
/*----- Default Value for LWIP_TCPIP_CORE_LOCKING_INPUT: 0 ---*/
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0
/* Netif state changes from the Ethernet link task are marshalled to tcpip_thread. */
#define LWIP_NETIF_API 1
/*----- Value in opt.h for MEM_ALIGNMENT: 1 -----*/
#define MEM_ALIGNMENT 4
/*----- Default Value for MEM_SIZE: 1600 ---*/
// #define MEM_SIZE 2048
/*----- Value in opt.h for LWIP_ETHERNET: LWIP_ARP || PPPOE_SUPPORT -*/
#define LWIP_ETHERNET 1
/*----- Value in opt.h for LWIP_DNS_SECURE: (LWIP_DNS_SECURE_RAND_XID | LWIP_DNS_SECURE_NO_MULTIPLE_OUTSTANDING | LWIP_DNS_SECURE_RAND_SRC_PORT) -*/
#define LWIP_DNS_SECURE 7
/*----- Value in opt.h for TCP_SND_QUEUELEN: (4*TCP_SND_BUF + (TCP_MSS - 1))/TCP_MSS -----*/
//#define TCP_SND_QUEUELEN 16
/*----- Value in opt.h for TCP_SNDLOWAT: LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/2), (2 * TCP_MSS) + 1), (TCP_SND_BUF) - 1) -*/
#define TCP_SNDLOWAT 1071
/*----- Value in opt.h for TCP_SNDQUEUELOWAT: LWIP_MAX(TCP_SND_QUEUELEN)/2, 5) -*/
#define TCP_SNDQUEUELOWAT 5
/*----- Value in opt.h for TCP_WND_UPDATE_THRESHOLD: LWIP_MIN(TCP_WND/4, TCP_MSS*4) -----*/
#define TCP_WND_UPDATE_THRESHOLD 536
/*----- Value in opt.h for LWIP_NETIF_LINK_CALLBACK: 0 -----*/
#define LWIP_NETIF_LINK_CALLBACK 1
/*----- Value in opt.h for TCPIP_THREAD_STACKSIZE: 0 -----*/
#define TCPIP_THREAD_STACKSIZE 2048
/*----- Value in opt.h for TCPIP_THREAD_PRIO: 1 -----*/
#define TCPIP_THREAD_PRIO 24
/*----- Value in opt.h for TCPIP_MBOX_SIZE: 0 -----*/
#define TCPIP_MBOX_SIZE 6
/*----- Value in opt.h for SLIPIF_THREAD_STACKSIZE: 0 -----*/
#define SLIPIF_THREAD_STACKSIZE 2048
/*----- Value in opt.h for SLIPIF_THREAD_PRIO: 1 -----*/
#define SLIPIF_THREAD_PRIO 3
/*----- Value in opt.h for DEFAULT_THREAD_STACKSIZE: 0 -----*/
#define DEFAULT_THREAD_STACKSIZE 2048
/*----- Value in opt.h for DEFAULT_THREAD_PRIO: 1 -----*/
#define DEFAULT_THREAD_PRIO 3
/*----- Value in opt.h for DEFAULT_UDP_RECVMBOX_SIZE: 0 -----*/
#define DEFAULT_UDP_RECVMBOX_SIZE 6
/*----- Value in opt.h for DEFAULT_TCP_RECVMBOX_SIZE: 0 -----*/
#define DEFAULT_TCP_RECVMBOX_SIZE 6
/*----- Value in opt.h for DEFAULT_ACCEPTMBOX_SIZE: 0 -----*/
#define DEFAULT_ACCEPTMBOX_SIZE 6
/*----- Default Value for LWIP_TCPIP_TIMEOUT: 0 ---*/
#define LWIP_TCPIP_TIMEOUT 1
/*----- Default Value for LWIP_TCP_KEEPALIVE: 0 ---*/
#define LWIP_TCP_KEEPALIVE 0
/*----- Default Value for LWIP_SO_SNDTIMEO: 0 ---*/
#define LWIP_SO_SNDTIMEO 1
/*----- Default Value for LWIP_SO_RCVTIMEO: 0 ---*/
#define LWIP_SO_RCVTIMEO 1
/*----- Default Value for LWIP_SO_SNDRCVTIMEO_NONSTANDARD: 0 ---*/
#define LWIP_SO_SNDRCVTIMEO_NONSTANDARD 1
/*----- Default Value for LWIP_SO_RCVBUF: 0 ---*/
#define LWIP_SO_RCVBUF 1
/*----- Default Value for LWIP_SO_LINGER: 0 ---*/
#define LWIP_SO_LINGER 1
/*----- Value in opt.h for RECV_BUFSIZE_DEFAULT: INT_MAX -----*/
#define RECV_BUFSIZE_DEFAULT 2000000000
/*----- Default Value for LWIP_TCP_CLOSE_TIMEOUT_MS_DEFAULT: 20000 ---*/
#define LWIP_TCP_CLOSE_TIMEOUT_MS_DEFAULT 2000
/*----- Value in opt.h for LWIP_STATS: 1 -----*/
#define LWIP_STATS 1
/*----- Value in opt.h for CHECKSUM_GEN_IP: 1 -----*/
#define CHECKSUM_GEN_IP 1
/*----- Value in opt.h for CHECKSUM_GEN_UDP: 1 -----*/
#define CHECKSUM_GEN_UDP 1
/*----- Value in opt.h for CHECKSUM_GEN_TCP: 1 -----*/
#define CHECKSUM_GEN_TCP 1
/*----- Value in opt.h for CHECKSUM_GEN_ICMP: 1 -----*/
#define CHECKSUM_GEN_ICMP 1
/*----- Value in opt.h for CHECKSUM_GEN_ICMP6: 1 -----*/
#define CHECKSUM_GEN_ICMP6 1
/*----- Value in opt.h for CHECKSUM_CHECK_IP: 1 -----*/
#define CHECKSUM_CHECK_IP 1
/*----- Value in opt.h for CHECKSUM_CHECK_UDP: 1 -----*/
#define CHECKSUM_CHECK_UDP 1
/*----- Value in opt.h for CHECKSUM_CHECK_TCP: 1 -----*/
#define CHECKSUM_CHECK_TCP 1
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP: 1 -----*/
#define CHECKSUM_CHECK_ICMP 1
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP6: 1 -----*/
#define CHECKSUM_CHECK_ICMP6 1
/*-----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */
#define MEMP_NUM_SYS_TIMEOUT        16
#define LWIP_DEBUG 1
#define MQTT_DEBUG LWIP_DBG_ON
#ifdef LWIP_PLATFORM_DIAG
#undef LWIP_PLATFORM_DIAG
#endif
#define LWIP_LOGD(...) LOGD("lwip",__VA_ARGS__)
#define LWIP_PLATFORM_DIAG(x) do {LWIP_LOGD x;} while(0)

#define MEM_LIBC_MALLOC 0
#define MEMP_MEM_MALLOC 0

#define MQTT_CONNECT_TIMOUT  10
#define MEM_SIZE                (40 * 1024)      // Nếu bạn có RAM đủ, có thể tăng đến 20–24KB
#define MEMP_NUM_PBUF           64
#define MEMP_NUM_TCP_SEG        64
#define PBUF_POOL_SIZE          16
#define PBUF_POOL_BUFSIZE       1024             // Giữ nguyên nếu MTU mạng bạn < 1500
#define TCP_WND                 (2048)      // Giữ nguyên nếu MTU mạng bạn < 1500
#define TCP_MSS                 512              // PPP thường dùng 512, kiểm tra lại nếu bạn đã đổi
#define TCP_SND_BUF             (4 * TCP_MSS)    // 2048
#define TCP_SND_QUEUELEN        (8 * TCP_SND_BUF / TCP_MSS)  // Đảm bảo TCP không bị nghẽn hàng đợi
#define MEMP_NUM_NETCONN        20
#define LWIP_SNTP                   1
#define SNTP_SERVER_DNS         1
#define SNTP_MAX_SERVERS        4
#define TCP_DEBUG                       LWIP_DBG_OFF
#define ETHARP_DEBUG                    LWIP_DBG_OFF
#define PBUF_DEBUG                      LWIP_DBG_OFF
#define IP_DEBUG                        LWIP_DBG_OFF
#define TCPIP_DEBUG                     LWIP_DBG_OFF
#define DHCP_DEBUG                      LWIP_DBG_OFF
#define UDP_DEBUG                       LWIP_DBG_OFF
#define DNS_DEBUG                       LWIP_DBG_OFF
#define SNTP_DEBUG                  LWIP_DBG_ON
#define NETCONN_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define MQTT_REQ_MAX_IN_FLIGHT 10
#define MQTT_REQ_TIMEOUT       5
#define MQTT_TOPIC_LENGTH 128
#define MQTT_PAYLOAD_LENGTH 4096
#define MQTT_CYCLIC_TIMER_INTERVAL 1
#define MQTT_VAR_HEADER_BUFFER_LEN (MQTT_TOPIC_LENGTH+MQTT_PAYLOAD_LENGTH+8)
#define MQTT_OUTPUT_RINGBUF_SIZE MQTT_VAR_HEADER_BUFFER_LEN*MQTT_REQ_MAX_IN_FLIGHT
#define LWIP_NETIF_STATUS_CALLBACK 1
#define SNTP_UPDATE_DELAY (60 * 1000 * 10)
extern void sntp_set_time(uint32_t sntp_time);
#define SNTP_SET_SYSTEM_TIME    sntp_set_time
#define LWIP_ALTCP 0
#define LWIP_USE_EXTERNAL_MBEDTLS 0
#define LWIP_ALTCP_TLS 0
#define LWIP_ALTCP_TLS_MBEDTLS 0
#define ALTCP_MBEDTLS_DEBUG LWIP_DBG_OFF
#define MEMP_OVERFLOW_CHECK 2
#define MEMP_SANITY_CHECK 1
#define MEM_OVERFLOW_CHECK 2
#define MEM_SANITY_CHECK 1
#define LWIP_SOCKET               1
#define LWIP_NETCONN              1
#define LWIP_TCP                  1
#define LWIP_TCPIP_CORE_LOCK_CHECK 0
#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT 0
#define LWIP_STATS_DISPLAY  1
#define MEM_STATS           1
#define MEMP_STATS          1
#define SYS_STATS           1
/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /*__LWIPOPTS__H__ */
