#include "rf_app.h"
#include "board.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"
#include "app_settings.h"
#include "thingsboard-client.h"
// #include "app_config.h"
// #include "indicator.h"

static const char *TAG_Driver = "RFDriver";
static const char *TAG = "RFApp";

// Indicator_t indicator;

#if LORA_ENABLE

LoRaMesh_t lora;
LoRaDriver_t lora_driver;

static void lora_connect_cb(LoRaMesh_t *lora, uint8_t isSuccess, void *arg)
{
  if (isSuccess == LORA_RES_SUCCESS)
  {
    log_info(TAG, "Connect to Lora module success");
    log_info(TAG, "Lora FW Version : V%d", lora->fw_version);
    indicator_set_net_status(&indicator, EVENT_NET_CONNECTED);
  }
  else if (isSuccess == LORA_RES_FAIL)
  {
    log_error(TAG, "Connect to Lora module fail");
    indicator_set_net_status(&indicator, EVENT_NET_DISCONNECTED);
  }
  else
  {
    log_error(TAG, "Connect to Lora module timeout");
    indicator_set_net_status(&indicator, EVENT_NET_DISCONNECTED);
  }
}
static void lora_read_cb(LoRaMesh_t *lora, uint8_t isSuccess, void *arg)
{
  if (isSuccess == LORA_RES_SUCCESS)
  {
    LoraParameter_t *param = &lora->param;
    log_info(TAG, "Read Lora module param success");
    log_info(TAG, "PAN ID : 0x%04X", param->PAN_ID);
    log_info(TAG, "Channel : %d", param->Channel);
    log_info(TAG, "User Address : 0x%04X", param->userAddress);
    log_info(TAG, "Point Type : %d", param->pointType);
    log_info(TAG, "Transfer Model : %d", param->transferModel);
    log_info(TAG, "Uart Baudrate : %d", param->uartBraudRate);
    log_info(TAG, "Uart Data bits : %d", param->uartDataBits);
    log_info(TAG, "Uart Stop bits : %d", param->uartStopBits);
    log_info(TAG, "Uart Parity : %d", param->uartParity);
    log_info(TAG, "Antenna Select : %d", param->antennaSelect);
    log_info(TAG, "Is Security : %d", param->isSecurity);
    log_info(TAG, "Security Code : 0x%02X 0x%02X 0x%02X 0x%02X", param->securityCode[0], param->securityCode[1], param->securityCode[2], param->securityCode[3]);
    log_info(TAG, "Short Address : 0x%04X", param->shortAddress);
    log_info(TAG, "Mac Address : 0x%02X%02X%02X%02X%02X%02X%02X%02X", param->macAddress[0], param->macAddress[1], param->macAddress[2], param->macAddress[3], param->macAddress[4], param->macAddress[5], param->macAddress[6], param->macAddress[7]);
  }
  else if (isSuccess == LORA_RES_FAIL)
  {
    log_error(TAG, "Read Lora module param fail");
  }
  else
  {
    log_error(TAG, "Read Lora module param timeout");
  }
}

#endif

#if ZIGBEE_ENABLE
ZigbeeMesh_t zigbee;
ZigbeeDriver_t zigbee_driver;
static void zigbee_connect_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess, void *arg);
static void zigbee_read_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess, void *arg);
static void zigbee_connect_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess, void *arg)
{
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    log_info(TAG, "Connect to Zigbee module success");
    log_info(TAG, "Zigbee FW Version : V%d.%d", zigbee->fw_version[0], zigbee->fw_version[1]);
    // zigbee_read_module(&zigbee, zigbee_read_cb);
    // indicator_set_net_status(&indicator, EVENT_NET_CONNECTED);
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Connect to Zigbee module fail");
    // indicator_set_net_status(&indicator, EVENT_NET_DISCONNECTED);
  }
  else
  {
    log_error(TAG, "Connect to Zigbee module timeout");
    // indicator_set_net_status(&indicator, EVENT_NET_DISCONNECTED);
  }
}

static void zigbee_read_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess, void *arg)
{
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    ZigbeeParameter_t *param = &zigbee->param;
    log_info(TAG, "Read Zigbee module param success");
    log_info(TAG, "PAN ID : 0x%04X", param->PAN_ID);
    log_info(TAG, "Channel : %d", param->Channel);
    log_info(TAG, "User Address : 0x%04X", param->userAddress);
    log_info(TAG, "Point Type : %d", param->pointType);
    log_info(TAG, "Transfer Model : %d", param->transferModel);
    log_info(TAG, "Uart Baudrate : %d", param->uartBraudRate);
    log_info(TAG, "Uart Data bits : %d", param->uartDataBits);
    log_info(TAG, "Uart Stop bits : %d", param->uartStopBits);
    log_info(TAG, "Uart Parity : %d", param->uartParity);
    log_info(TAG, "Antenna Select : %d", param->antennaSelect);
    log_info(TAG, "Is Security : %d", param->isSecurity);
    log_info(TAG, "Security Code : 0x%02X 0x%02X 0x%02X 0x%02X", param->securityCode[0], param->securityCode[1], param->securityCode[2], param->securityCode[3]);
    log_info(TAG, "Short Address : 0x%04X", param->shortAddress);
    log_info(TAG, "Mac Address : 0x%02X%02X%02X%02X%02X%02X%02X%02X", param->macAddress[0], param->macAddress[1], param->macAddress[2], param->macAddress[3], param->macAddress[4], param->macAddress[5], param->macAddress[6], param->macAddress[7]);
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Read Zigbee module param fail");
  }
  else
  {
    log_error(TAG, "Read Zigbee module param timeout");
  }
}
static void zigbee_reset_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess, void *arg)
{
}
#endif
uint32_t rf_time_stamp = 0;

static uint32_t rf_get_available()
{
  uint32_t length = 0;
  uart_get_buffered_data_len(RF_PORT, (size_t *)&length);
  log_debug(TAG_Driver, "RF Available : %u", length);
  return length;
}
static void rf_write(uint8_t *buff, uint32_t len)
{
  log_debug(TAG_Driver, "RF Write : %u", len);
  log_print_hex(LOGGER_DEBUG, TAG_Driver, buff, len);
  uart_write_bytes(RF_PORT, buff, len);
}
static void rf_read(uint8_t *buff, uint32_t len)
{
  uint32_t length = uart_read_bytes(RF_PORT, buff, len, 100);
  if (length > 0)
  {
    log_debug(TAG_Driver, "RF Read : %u", len);
    log_print_hex(LOGGER_INFO, TAG_Driver, buff, len);
  }
}

#if ZIGBEE_APP_ENABLE
TaskHandle_t rf_app_task_handle = NULL;
int32_t mmb_read(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
                 void *arg)
{
  int byte_read = uart_read_bytes(RF_PORT, buf,count,byte_timeout_ms);
  return byte_read;
}
int32_t mmb_write(const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
                  void *arg)
{
  uart_write_bytes(RF_PORT, buf, count);
  return count;
}

static void RFApp(void *arg);

#endif

#if MODBUS_APP_ENABLE

int32_t mmb_rs485_read(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
                       void *arg)
{
  uint32_t byte_read = 0;
  byte_read = uart_read_bytes(MMB_SERIAL_PORT, buf,count,byte_timeout_ms);
  return byte_read;
}
int32_t mmb_rs485_write(const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
                        void *arg)
{
  uart_write_bytes(MMB_SERIAL_PORT, buf, count);
  return count;
}

int32_t mmb_rs485_2_read(uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
                         void *arg)
{
  uint32_t byte_read = uart_read_bytes(RS485_2_PORT, buf,count,byte_timeout_ms);
  return byte_read;
}
int32_t mmb_rs485_2_write(const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms,
                          void *arg)
{
  uart_write_bytes(RS485_2_PORT, buf, count);
  return count;
}

#endif

void rf_app_init()
{
  // indicator_init(&indicator);
#if LORA_ENABLE
  lora_driver.write = rf_write;
  lora_driver.read = rf_read;
  lora_driver.available = rf_get_available;
  lora_init(&lora, &lora_driver);
  lora_connect(&lora, lora_connect_cb, NULL);
  lora_read_module(&lora, lora_read_cb, NULL);
#endif
#if ZIGBEE_ENABLE
  zigbee_driver.p_write = rf_write;
  zigbee_driver.p_read = rf_read;
  zigbee_driver.available = rf_get_available;
  zigbee_init(&zigbee, &zigbee_driver);
  zigbee_connect(&zigbee, zigbee_connect_cb, NULL);
  zigbee_read_module(&zigbee, zigbee_read_cb, NULL);
  zigbee_reset_module(&zigbee, zigbee_reset_cb, NULL);

#endif

#if ZIGBEE_APP_ENABLE

  nmbs_platform_conf_create(&gateway.mmbs_conf_zigbee);
  gateway.mmbs_conf_zigbee.read = mmb_read;
  gateway.mmbs_conf_zigbee.write = mmb_write;
  gateway.mmbs_conf_zigbee.transport = NMBS_TRANSPORT_RTU;
  gateway.mmbs_conf_zigbee.arg = NULL;
  mmb_init(&gateway.mmb_zigbee, &gateway.mbs_zigbee, &gateway.mmbs_conf_zigbee, RF_PORT);

#endif
#if MODBUS_APP_ENABLE

  nmbs_platform_conf_create(&gateway.mmbs_conf_rs485[0]);
  gateway.mmbs_conf_rs485[0].read = mmb_rs485_read;
  gateway.mmbs_conf_rs485[0].write = mmb_rs485_write;
  gateway.mmbs_conf_rs485[0].transport = NMBS_TRANSPORT_RTU;
  gateway.mmbs_conf_rs485[0].arg = NULL;
  mmb_init(&gateway.mmb_rs485[0], &gateway.mbs_rs485[0], &gateway.mmbs_conf_rs485[0], RS485_0_PORT);

  nmbs_platform_conf_create(&gateway.mmbs_conf_rs485[1]);
  gateway.mmbs_conf_rs485[1].read = mmb_rs485_2_read;
  gateway.mmbs_conf_rs485[1].write = mmb_rs485_2_write;
  gateway.mmbs_conf_rs485[1].transport = NMBS_TRANSPORT_RTU;
  gateway.mmbs_conf_rs485[1].arg = NULL;
  mmb_init(&gateway.mmb_rs485[1], &gateway.mbs_rs485[1], &gateway.mmbs_conf_rs485[1], RS485_2_PORT);
#endif
#if ZIGBEE_APP_ENABLE
  xTaskCreate(RFApp, "RFApp", 1024 * 4, NULL, 10, &rf_app_task_handle);
  if (rf_app_task_handle == NULL)
  {
    log_error(TAG, "Create RF app task failed");
  }
#endif
}

void rf_app_poll(uint32_t time_stamp)
{
#if LORA_ENABLE
  lora_poll(&lora, 1);
#endif
#if ZIGBEE_ENABLE
  zigbee_poll(&zigbee, time_stamp);
#endif
  // lora_poll(&lora, timestamp);
}

bool rf_app_busy()
{
#if ZIGBEE_ENABLE
  return zigbee.event.event != ZB_EVENT_NONE ? true : false;
#endif
#if LORA_ENABLE
  return lora.event.event != LR_EVENT_NONE ? true : false;
#endif
}

#if ZIGBEE_APP_ENABLE

// SX_485_IO_t sx485io[NUM_SX485IO_DEV];
// SX_420mA_t sx420mA[NUM_SX420mA_DEV];
#if NUM_SXMETTER_DEV
SX_Metter_t sxmetter[NUM_SXMETTER_DEV];
#endif
#if NUM_SXTHS_DEV
SX_THS_t sx_ths[NUM_SXTHS_DEV];
#endif

static void RFApp(void *arg)
{
  // Create num device IO
  gateway.numDevIO = app_setting.setting.dev_io;
  if (gateway.numDevIO > 0)
  {
    log_info(TAG, "Create %d device IO", gateway.numDevIO);
    // gateway.devio = (SX_485_IO_t **)malloc(gateway.numDevIO * sizeof(SX_485_IO_t *));
    for (int i = 0; i < gateway.numDevIO; i++)
    {
      // gateway.devio[i] = (SX_485_IO_t *) malloc(sizeof(SX_485_IO_t));
      sx_485_io_init(&gateway.devio[i], &gateway.mmb_zigbee, &gateway.thingsboard, app_setting.setting.start_io_addr + i);
    }
  }
  // Create num device 4-20 mA
  gateway.numDev420mA = app_setting.setting.dev_ai;
  if (gateway.numDev420mA > 0)
  {
    log_info(TAG, "Create %d device 4-20mA", gateway.numDev420mA);
    // gateway.dev420mA = (SX_420mA_t **)malloc(gateway.numDev420mA * sizeof(SX_420mA_t *));
    for (int i = 0; i < gateway.numDev420mA; i++)
    {
      // gateway.dev420mA[i] = (SX_420mA_t*) malloc(sizeof(SX_420mA_t));
      sx_420mA_init(&gateway.dev420mA[i], &gateway.mmb_zigbee, &gateway.thingsboard, app_setting.setting.start_ai_addr + i);
    }
  }
#if NUM_SXMETTER_DEV
  sx_metter_init(&sxmetter[0], &gateway.mmb_rs485[0], &gateway.thingsboard, 1);
#endif
#if NUM_SXTHS_DEV
  sx_ths_init(&sx_ths[0], &gateway.mmb_rs485[1], &gateway.thingsboard, 2);
#endif
  uint8_t dev_io = 0;
  uint8_t dev_420 = 0;
  uint8_t dev_metter = 0;
  uint8_t dev_ths = 0;
  uint32_t time_poll = 0;
  while (1)
  {
    rf_app_poll(10);
    time_poll += 10;

    if (time_poll > 200)
    {
      if (!rf_app_busy())
      {
        if (dev_io < gateway.numDevIO)
          sx_485_io_poll(&gateway.devio[dev_io]);
        if (dev_420 < gateway.numDev420mA)
        {
          sx_420mA_poll(&gateway.dev420mA[dev_420]);
        }
      }
      (dev_io == gateway.numDevIO) ? dev_io = 0 : dev_io++;
      (dev_420 == gateway.numDev420mA) ? dev_420 = 0 : dev_420++;
#if NUM_SXMETTER_DEV
      sx_metter_poll(&sxmetter[dev_metter]);
#endif
#if NUM_SXTHS_DEV
      sx_ths_poll(&sx_ths[dev_ths]);
#endif
      time_poll = 0;
    }
    // sx_metter_poll(&sxmetter[0]);
    // sx_ths_poll(&sx_ths[0]);
    vTaskDelay(10);
  }
}

#endif