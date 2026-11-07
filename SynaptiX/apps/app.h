#ifndef APP_H
#define APP_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "board.h"

#include "button.h"
#include "nanomodbus.h"
#include "mmb_serial.h"
#include "mqtt_app.h"
#include "sntp_app.h"
#include "sx_485_io.h"
#include "sx_420mA.h"
#include "sx_metter.h"
#include "sx_ths.h"
#include "app_config.h"
#include "ota.h"
#include "socketserver.h"
#include "mbs_tcp.h"
#include "thingsboard-client.h"

#ifndef NUM_SX485IO_DEV 
#define NUM_SX485IO_DEV 10
#endif

#ifndef NUM_SX420mA_DEV 
#define NUM_SX420mA_DEV 10
#endif

typedef struct ETH_ZBGateWay
{
  /* data */
  char sn[32];
  uint32_t uuid;
  int num_dev_online;
  uint32_t uptime;
  SX_485_IO_t devio[NUM_SX485IO_DEV];
  uint8_t numDevIO;
  SX_420mA_t dev420mA[NUM_SX420mA_DEV];
  uint8_t numDev420mA;
  nmbs_platform_conf mmbs_conf_zigbee;
  nmbs_platform_conf mmbs_conf_rs485[2];
  nmbs_t mbs_zigbee;
  nmbs_t mbs_rs485[2];
  MMBCLient_t mmb_zigbee;
  MMBCLient_t mmb_rs485[2];
  Bootloader_t boot;
  SocketServer tcp_shell;
  ModbusTCPServer mb_tcp;
  ThingsBoard_t thingsboard;
  
} ETH_ZBGateWay_t;

extern ETH_ZBGateWay_t gateway;

#define ZIGBEE_ENABLE 1
#define LORA_ENABLE 0

/**
 * @brief Initializes the application.
 *
 * This function sets up all necessary resources and configurations
 * required for the application to start running.
 */
void app_init(void);
uint32_t app_get_cpu_usage(void);
uint32_t app_get_ram_usage(void);
#ifdef __cplusplus
}
#endif
#endif // APP_H