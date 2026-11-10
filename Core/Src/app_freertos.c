/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
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
#include "app_freertos.h"
#include "stm32h5xx.h"
#include "dtu_board.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lwip.h"
#include "logger.h"
#include "main.h"
#include "file_system_port.h"
#include "mqtt_agent.h"
#include "rf_app.h"
#include "settings.h"
#include "web_server.h"
#include "button_app.h"
#include "indicator_app.h"
#include "indicator_service_if.h"
#include "splc_mqtt_port.h"
#include "splc_port.h"
#include "sx_uart.h"
#include "stm32h5xx_hal_uart.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "cli_shell.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MQTT_TASK_STACK_SIZE (4096U)
#define BUTTON_TASK_STACK_SIZE (512)
#define INDICATOR_TASK_STACK_SIZE (512)
#define INDICATOR_ERROR_QUEUE_LENGTH (8U)
#define NETWORK_CONFIRM_TIMEOUT_MS (60000U)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
static const char TAG[] = "SYS";


static const char mqtt_ca_pem[1500] =
"-----BEGIN CERTIFICATE-----\r\n"
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\r\n"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\r\n"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\r\n"
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\r\n"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\r\n"
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\r\n"
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\r\n"
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\r\n"
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\r\n"
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\r\n"
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\r\n"
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\r\n"
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\r\n"
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\r\n"
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\r\n"
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\r\n"
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\r\n"
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\r\n"
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\r\n"
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\r\n"
"-----END CERTIFICATE-----\r\n";

static MQTTAgentLibrary_t *mqtt_agent;
static StaticTask_t mqtt_task_control_block;
static StackType_t mqtt_task_stack[MQTT_TASK_STACK_SIZE];
static StaticTask_t button_task_control_block;
static StackType_t button_task_stack[BUTTON_TASK_STACK_SIZE];
static StaticTask_t indicator_task_control_block;
static StackType_t indicator_task_stack[INDICATOR_TASK_STACK_SIZE];
typedef struct
{
  indicator_error_t error;
  bool active;
} indicator_error_event_t;
static StaticQueue_t indicator_error_queue_control_block;
_Alignas(portBYTE_ALIGNMENT)
static uint8_t indicator_error_queue_storage[
    sizeof(indicator_error_event_t) * INDICATOR_ERROR_QUEUE_LENGTH];
static QueueHandle_t indicator_error_queue;
static TickType_t network_confirm_deadline;
static bool network_confirm_pending;
static bool network_fallback_save_pending;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

ShellContext_t serial_shell;

int shell_send_char(void *arg,char c);
int shell_send_str(void *arg,char *c);
int shell_recv_char(void *arg);
/* USER CODE END Variables */
/* Definitions for mainTask */
osThreadId_t mainTaskHandle;
uint32_t MainBufferTask[ 2048U ];
osStaticThreadDef_t MainControlBlocTask;
const osThreadAttr_t mainTask_attributes = {
  .name = "mainTask",
  .stack_mem = &MainBufferTask[0],
  .stack_size = sizeof(MainBufferTask),
  .cb_mem = &MainControlBlocTask,
  .cb_size = sizeof(MainControlBlocTask),
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

static void mqtt_task(void *arg);
static void button_task(void *arg);
static void indicator_task(void *arg);
static bool indicator_error_queue_init(void);
static void indicator_report_error(indicator_error_t error, bool active);
static bool settings_install_uarts(const settings_config_t *config);
static void settings_network_recovery_process(void);

/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/* USER CODE BEGIN 2 */
void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
   mqtt_agent_idle_process();
}
/* USER CODE END 2 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
  LOGE(TAG,"%s overflow %p",pcTaskName,xTask);
}
/* USER CODE END 4 */

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
void configureTimerForRunTimeStats(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  __DSB();
  __ISB();
}

unsigned long getRunTimeCounterValue(void)
{
  return (unsigned long)DWT->CYCCNT;
}
static void logger_puts(const char *s, size_t len)
{
  (void)sx_uart_write(SX_UART_PORT_LPUART1,
                      (const uint8_t *)s,
                      len,
                      portMAX_DELAY);
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of mainTask */
  mainTaskHandle = osThreadNew(StartMainTask, NULL, &mainTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartMainTask */
/**
* @brief Function implementing the mainTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartMainTask */
__weak void StartMainTask(void *argument)
{
  /* USER CODE BEGIN mainTask */
  logger_init(LOGGER_DEBUG, logger_puts);
  if (!indicator_error_queue_init()) {
    LOGE(TAG, "Failed to initialize indicator error queue");
    Error_Handler();
  }
  if (file_system_port_init() != 0) {
    LOGE(TAG, "File system mount failed errno=%d", errno);
    indicator_report_error(INDICATOR_ERROR_STORAGE, true);
  }
  else{
    LOGD(TAG, "File system mount succeeded");
    indicator_report_error(INDICATOR_ERROR_STORAGE, false);
  }
  {
    settings_config_t settings = {0};
    const settings_result_t settings_result = settings_init();

    if (settings_result != SETTINGS_RESULT_OK) {
      LOGW(TAG, "Settings storage unavailable; using firmware defaults");
      indicator_report_error(INDICATOR_ERROR_STORAGE, true);
    } else {
      indicator_report_error(INDICATOR_ERROR_STORAGE, false);
    }
    if (!settings_get_config(&settings)) {
      LOGE(TAG, "Could not read settings snapshot");
      indicator_report_error(INDICATOR_ERROR_CONFIGURATION, true);
      Error_Handler();
    }
    if (!settings_install_uarts(&settings)) {
      LOGE(TAG, "Could not apply saved UART configuration");
      indicator_report_error(INDICATOR_ERROR_UART, true);
      Error_Handler();
    }
    if (!MX_LWIP_ConfigureIPv4(
            settings.ethernet.dhcp_enabled,
            settings.ethernet.address,
            settings.ethernet.netmask,
            settings.ethernet.gateway)) {
      LOGE(TAG, "Could not configure saved Ethernet settings");
      indicator_report_error(INDICATOR_ERROR_NETWORK, true);
      Error_Handler();
    }
    network_confirm_pending = !settings.ethernet.dhcp_enabled;
    network_confirm_deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(NETWORK_CONFIRM_TIMEOUT_MS);
  }
  LOGI(TAG, "Initializing TCP/IP");
  MX_LWIP_Init();

  if (xTaskCreateStatic(mqtt_task, "mqtt-task", MQTT_TASK_STACK_SIZE, NULL,
                        tskIDLE_PRIORITY + 1U, mqtt_task_stack,
                        &mqtt_task_control_block) == NULL) {
    LOGE(TAG, "Failed to create MQTT task");
    indicator_report_error(INDICATOR_ERROR_MQTT, true);
  }
  if (xTaskCreateStatic(button_task, "button-task",
                        BUTTON_TASK_STACK_SIZE, NULL,
                        tskIDLE_PRIORITY + 1U, button_task_stack,
                        &button_task_control_block) == NULL) {
    LOGE(TAG, "Failed to create button task");
    indicator_report_error(INDICATOR_ERROR_INTERNAL, true);
  }
  if (!rf_app_init()) {
    LOGE(TAG, "Failed to initialize RF application");
    indicator_report_error(INDICATOR_ERROR_RADIO, true);
  } else {
    indicator_report_error(INDICATOR_ERROR_RADIO, false);
  }
  if (splc_port_start() != 0) {
  LOGE(TAG, "Failed to start SimplePLC engine");
  indicator_report_error(INDICATOR_ERROR_INTERNAL, true);
  }
  if (!web_server_init()) {
    LOGE(TAG, "Failed to queue web server startup");
    indicator_report_error(INDICATOR_ERROR_NETWORK, true);
  } else {
    indicator_report_error(INDICATOR_ERROR_NETWORK, false);
  }
  serial_shell.impl.send_char = shell_send_char;
  serial_shell.impl.send_str = shell_send_str;
  serial_shell.impl.receive_char = shell_recv_char;
  serial_shell.impl.arg = NULL;
  cli_shell_boot(&serial_shell);
  if (xTaskCreateStatic(indicator_task, "indicator-task",
                        INDICATOR_TASK_STACK_SIZE, NULL,
                        tskIDLE_PRIORITY + 1U, indicator_task_stack,
                        &indicator_task_control_block) == NULL) {
    LOGE(TAG, "Failed to create indicator task");
  }
  for(;;)
  {
    rf_app_process(1U);
    web_server_process();
    settings_network_recovery_process();
    int32_t len = sx_uart_get_rx_bytes_available(SX_UART_PORT_LPUART1);
    if(len > 0){
      uint8_t buff[128];
      sx_uart_read(SX_UART_PORT_LPUART1, buff, (len > 128 ) ? 128 : len,0);
      for(int i = 0;i < ((len > 128 ) ? 128 : len); i++)
      cli_shell_receive_char(&serial_shell,(char)buff[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(1U));
  }
  /* USER CODE END mainTask */
}

/* Private application code --------------------------------------------------*/
indicator_service_result_t indicator_service_set_error(
    indicator_error_t error,
    bool active)
{
  indicator_error_event_t event;
  uint16_t error_mask = (uint16_t)error;

  if (error_mask < (uint16_t)INDICATOR_ERROR_CONFIGURATION ||
      error_mask > (uint16_t)INDICATOR_ERROR_UNKNOWN ||
      (error_mask & (uint16_t)(error_mask - 1U)) != 0U) {
    return INDICATOR_SERVICE_RESULT_INVALID_ARGUMENT;
  }
  if (indicator_error_queue == NULL) {
    return INDICATOR_SERVICE_RESULT_NOT_READY;
  }

  event.error = error;
  event.active = active;
  if (xQueueSend(indicator_error_queue, &event, 100U) != pdPASS) {
    return INDICATOR_SERVICE_RESULT_QUEUE_FULL;
  }
  return INDICATOR_SERVICE_RESULT_OK;
}

static bool indicator_error_queue_init(void)
{
  indicator_error_queue = xQueueCreateStatic(
      INDICATOR_ERROR_QUEUE_LENGTH,
      (UBaseType_t)sizeof(indicator_error_event_t),
      indicator_error_queue_storage,
      &indicator_error_queue_control_block);
  return indicator_error_queue != NULL;
}

static void indicator_report_error(indicator_error_t error, bool active)
{
  const indicator_service_result_t result =
      indicator_service_set_error(error, active);

  if (result != INDICATOR_SERVICE_RESULT_OK) {
    LOGW(TAG, "Could not queue indicator error update=%u result=%u",
         (unsigned int)error, (unsigned int)result);
  }
}

static void button_task(void *arg)
{
  button_app_t button_app;
  const button_control_if_t *const button_control =
      dtu_board_button_control_if();
  TickType_t last_wake_time = xTaskGetTickCount();

  (void)arg;
  if (!button_app_init(&button_app) || button_control == NULL ||
      button_control->is_pressed == NULL ||
      button_control->request_reboot == NULL ||
      button_control->request_upgrade_mode == NULL) {
    LOGE(TAG, "Button application initialization failed");
    vTaskDelete(NULL);
    return;
  }

  for (;;) {
    const uint32_t now_ms =
        (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
    const button_app_action_t action = button_app_process(
        &button_app, button_control->is_pressed(button_control->context),
        now_ms);

    if (action == BUTTON_APP_ACTION_REBOOT) {
      LOGI(TAG, "Button double-click requested device reboot");
      button_control->request_reboot(button_control->context);
      LOGE(TAG, "Device reboot request returned unexpectedly");
      vTaskDelete(NULL);
      return;
    }
    if (action == BUTTON_APP_ACTION_UPGRADE_MODE) {
      LOGI(TAG, "Button long-press requested ROM bootloader mode");
      button_control->request_upgrade_mode(button_control->context);
      LOGE(TAG, "Bootloader mode request returned unexpectedly");
      vTaskDelete(NULL);
      return;
    }
    if (action == BUTTON_APP_ACTION_ERROR) {
      LOGE(TAG, "Button gesture processing failed");
      vTaskDelete(NULL);
      return;
    }

    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(BUTTON_APP_SCAN_PERIOD_MS));
  }
}

static void indicator_task(void *arg)
{
  indicator_app_t indicator_app;
  const indicator_led_if_t *const led_if =
      dtu_board_indicator_led_if();
  TickType_t last_wake_time = xTaskGetTickCount();

  (void)arg;
  if (indicator_app_init(&indicator_app) != INDICATOR_RESULT_OK ||
      led_if == NULL || led_if->apply_frame == NULL) {
    LOGE(TAG, "Indicator application initialization failed");
    vTaskDelete(NULL);
    return;
  }

  for (;;) {
    indicator_error_event_t event;
    indicator_led_frame_t frame;
    indicator_result_t result;

    while (xQueueReceive(indicator_error_queue, &event, 0U) == pdPASS) {
      result = indicator_app_set_error(&indicator_app, event.error,
                                       event.active);
      if (result != INDICATOR_RESULT_OK) {
        LOGE(TAG, "Invalid indicator error event=%u result=%u",
             (unsigned int)event.error, (unsigned int)result);
        vTaskDelete(NULL);
        return;
      }
    }

    result = indicator_app_process(
        &indicator_app, true, MX_LWIP_IsLinkUp(),
        (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()), &frame);
    if (result != INDICATOR_RESULT_OK) {
      LOGE(TAG, "Indicator state processing failed result=%u",
           (unsigned int)result);
      vTaskDelete(NULL);
      return;
    }
    if (!led_if->apply_frame(led_if->context, &frame)) {
      LOGE(TAG, "Board rejected indicator LED frame");
      vTaskDelete(NULL);
      return;
    }

    vTaskDelayUntil(&last_wake_time,
                    pdMS_TO_TICKS(INDICATOR_APP_UPDATE_PERIOD_MS));
  }
}
/* USER CODE BEGIN Application */
static bool settings_install_uarts(const settings_config_t *config)
{
  static const sx_uart_port_t ports[SETTINGS_UART_COUNT] = {
      SX_UART_PORT_UART7,
      SX_UART_PORT_USART3,
      SX_UART_PORT_USART10,
  };
  size_t index;

  if (config == NULL) {
    return false;
  }
  for (index = 0U; index < SETTINGS_UART_COUNT; index++) {
    const settings_uart_config_t *const saved = &config->uart[index];
    sx_uart_config_t uart = {
        .mode = SX_UART_MODE_INTERRUPT,
        .rx_buffer_size = SX_UART_RX_BUFFER_MAX,
        .Baudrate = saved->baud_rate,
        .WordLength = saved->data_bits == 7U
                          ? SX_UART_DATA_7_BITS
                          : SX_UART_DATA_8_BITS,
        .StopBits = saved->stop_bits == 2U
                        ? UART_STOPBITS_2
                        : UART_STOPBITS_1,
        .Parity = saved->parity == 1U
                      ? UART_PARITY_EVEN
                      : (saved->parity == 2U ? UART_PARITY_ODD
                                             : UART_PARITY_NONE),
    };

    if (sx_uart_is_installed(ports[index]) &&
        !sx_uart_uninstall(ports[index])) {
      return false;
    }
    if (!sx_uart_install(ports[index], &uart)) {
      return false;
    }
  }
  return true;
}

static void settings_network_recovery_process(void)
{
  settings_save_state_t save_state;
  settings_config_t config;
  const TickType_t now = xTaskGetTickCount();

  if (!network_confirm_pending || settings_network_is_confirmed() ||
      (int32_t)(now - network_confirm_deadline) < 0) {
    return;
  }
  if (!network_fallback_save_pending) {
    if (!settings_get_config(&config)) {
      LOGE(TAG, "Could not read settings for DHCP recovery");
      return;
    }
    config.ethernet.dhcp_enabled = true;
    if (settings_request_save(&config) != SETTINGS_RESULT_OK) {
      return;
    }
    network_fallback_save_pending = true;
  }
  if (!settings_get_save_state(&save_state)) {
    LOGE(TAG, "Could not read settings save state");
    network_fallback_save_pending = false;
    return;
  }
  if (save_state == SETTINGS_SAVE_FAILED) {
    network_fallback_save_pending = false;
    return;
  }
  if (save_state == SETTINGS_SAVE_SUCCEEDED) {
    if (MX_LWIP_ApplyIPv4(true, NULL, NULL, NULL)) {
      network_confirm_pending = false;
      network_fallback_save_pending = false;
      LOGW(TAG, "Static IPv4 was not confirmed; DHCP restored");
    }
  }
}

static void mqtt_report_status(uint32_t generation, bool connected,
                               bool tls_active, bool authenticated,
                               uint16_t last_error)
{
  splc_mqtt_status_t status = {0};
  status.generation = generation;
  status.last_error = last_error;
  if (connected) {
    status.flags |= SPLC_MQTT_STATUS_CONNECTED;
  }
  if (tls_active) {
    status.flags |= SPLC_MQTT_STATUS_TLS_ACTIVE;
  }
  if (authenticated) {
    status.flags |= SPLC_MQTT_STATUS_AUTH_OK;
  }
  splc_mqtt_port_report_status(&status);
}

static uint16_t mqtt_error_code(MQTTStatus_t status)
{
  if (status == MQTTServerRefused) {
    return SPLC_MQTT_ERR_AUTH_FAIL;
  }
  if (status == MQTTRecvFailed) {
    return SPLC_MQTT_ERR_TIMEOUT;
  }
  return SPLC_MQTT_ERR_TCP_RESET;
}

static void mqtt_agent_close(void)
{
  if (mqtt_agent == NULL) {
    return;
  }
  const MQTTStatus_t status = mqtt_agent_disconnect(mqtt_agent);
  if (status != MQTTSuccess) {
    LOGW(TAG, "MQTT disconnect cleanup returned: %s",
         MQTT_Status_strerror(status));
  }
  mqtt_agent_destroy(mqtt_agent);
  mqtt_agent = NULL;
}

static void mqtt_task(void *arg)
{
  settings_config_t settings = {0};
  splc_mqtt_port_config_t plc_config = {0};
  splc_mqtt_port_message_t pending_message;
  bool settings_ready;
  bool config_received = false;
  bool pending = false;
  bool connected = false;
  bool retry_waiting = false;
  bool reconnect = true;
  bool app_configured = false;
  bool current_tls = false;
  uint16_t last_error = SPLC_MQTT_ERR_NONE;
  uint32_t generation = 0U;
  TickType_t retry_at = 0U;
  char port_text[6];

  (void)arg;
  settings_ready = settings_get_config(&settings);
  if (!settings_ready) {
    LOGE(TAG, "MQTT settings are unavailable; waiting for SPLC broker config");
  }
  (void)snprintf(port_text, sizeof(port_text), "%u",
                 (unsigned int)settings.mqtt.port);

  while (!network_posix_is_ready()) {
    vTaskDelay(pdMS_TO_TICKS(250U));
  }

  for (;;) {
    splc_mqtt_port_config_t updated;
    while (splc_mqtt_port_take_config(&updated, 0U)) {
      if (!config_received || updated.generation != generation ||
          updated.app_configured != app_configured) {
        splc_mqtt_port_discard_publishes();
        pending = false;
        plc_config = updated;
        generation = updated.generation;
        app_configured = updated.app_configured;
        config_received = true;
        reconnect = true;
        retry_waiting = false;
      }
    }

    if (mqtt_agent != NULL && !mqtt_agent_is_running(mqtt_agent)) {
      LOGW(TAG, "MQTT command loop stopped; reconnecting");
      mqtt_agent_close();
      connected = false;
      reconnect = true;
      retry_waiting = true;
      retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(5000U);
      last_error = SPLC_MQTT_ERR_TCP_RESET;
      mqtt_report_status(generation, false, false, false, last_error);
      indicator_report_error(INDICATOR_ERROR_MQTT, true);
    }

    if (reconnect && (!retry_waiting ||
                      (int32_t)(xTaskGetTickCount() - retry_at) >= 0)) {
      MQTTAgentLibraryConfig_t mqtt_config = {0};
      const char *host = NULL;
      const char *port = port_text;
      const char *client_id = NULL;
      const char *username = NULL;
      const char *password = NULL;
      uint32_t keepalive = 60U;
      bool enabled = false;
      bool persistent_session = false;
      bool tls = false;
      bool unsupported_config = false;

      mqtt_agent_close();
      connected = false;
      mqtt_report_status(generation, false, false, false, SPLC_MQTT_ERR_NONE);

      if (app_configured) {
        host = plc_config.broker.host;
        client_id = plc_config.broker.client_id;
        username = plc_config.broker.username[0] != '\0'
                       ? plc_config.broker.username : NULL;
        password = plc_config.broker.password[0] != '\0'
                       ? plc_config.broker.password : NULL;
        tls = plc_config.broker.use_tls;
        keepalive = plc_config.broker.keepalive_sec;
        persistent_session = !plc_config.broker.clean_session;
        enabled = true;
        unsupported_config = plc_config.broker.version != 0U ||
                             plc_config.broker.use_mtls ||
                             plc_config.broker.ignore_hostname ||
                             plc_config.broker.allow_self_signed;
        (void)snprintf(port_text, sizeof(port_text), "%u",
                       (unsigned int)plc_config.broker.port);
      } else if (settings_ready) {
        host = settings.mqtt.host;
        client_id = settings.mqtt.client_id;
        tls = settings.mqtt.tls_enabled;
        keepalive = settings.mqtt.keep_alive_seconds;
        enabled = settings.mqtt.enabled;
        (void)snprintf(port_text, sizeof(port_text), "%u",
                       (unsigned int)settings.mqtt.port);
      }

      if (unsupported_config) {
        LOGE(TAG, "SPLC MQTT config requests unsupported TLS policy or MQTT version");
        last_error = SPLC_MQTT_ERR_AUTH_FAIL;
        mqtt_report_status(generation, false, false, false, last_error);
        indicator_report_error(INDICATOR_ERROR_MQTT, true);
        retry_waiting = true;
        retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(5000U);
        reconnect = false;
      } else if (!enabled) {
        LOGI(TAG, "MQTT is disabled by device settings");
        last_error = SPLC_MQTT_ERR_NONE;
        mqtt_report_status(generation, false, false, false, last_error);
        indicator_report_error(INDICATOR_ERROR_MQTT, false);
        reconnect = false;
        retry_waiting = false;
      } else if (host == NULL || host[0] == '\0' ||
                 client_id == NULL || client_id[0] == '\0') {
        LOGE(TAG, "MQTT broker host and client ID must be configured");
        last_error = SPLC_MQTT_ERR_AUTH_FAIL;
        mqtt_report_status(generation, false, tls, false, last_error);
        indicator_report_error(INDICATOR_ERROR_MQTT, true);
        retry_waiting = true;
        retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(5000U);
        reconnect = false;
      } else if (!network_posix_is_ready()) {
        retry_waiting = true;
        retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(1000U);
      } else {
        mqtt_config.host = host;
        mqtt_config.port = port;
        mqtt_config.ca = tls ? mqtt_ca_pem : NULL;
        mqtt_config.ca_len = tls ? (uint32_t)(strlen(mqtt_ca_pem) + 1U) : 0U;
        mqtt_config.client_id = client_id;
        mqtt_config.username = username;
        mqtt_config.password = password;
        mqtt_config.keepAlive = keepalive;
        mqtt_config.persistent_session = persistent_session;
        mqtt_config.will_message = "offline";
        mqtt_config.will_topic = "device/status";
        mqtt_config.will_qos = MQTTQoS0;
        mqtt_config.will_retain = false;
        mqtt_config.security_mode = tls ? NETWORK_SECURITY_TLS
                                        : NETWORK_SECURITY_PLAIN;
        mqtt_config.connect_timeout_ms = 10000U;
        mqtt_config.command_timeout_ms = 15000U;
        mqtt_config.incoming_publish_callback = NULL;
        mqtt_config.incoming_publish_context = NULL;

        LOGI(TAG, "Connecting MQTT agent to %s:%s", host, port);
        MQTTStatus_t status = mqtt_agent_connect(&mqtt_agent, &mqtt_config);
        if (status == MQTTSuccess) {
          status = mqtt_agent_start(mqtt_agent);
        }
        if (status != MQTTSuccess) {
          LOGE(TAG, "MQTT agent connection failed: %s",
               MQTT_Status_strerror(status));
          mqtt_agent_close();
          last_error = mqtt_error_code(status);
          mqtt_report_status(generation, false, tls, false, last_error);
          indicator_report_error(INDICATOR_ERROR_MQTT, true);
          retry_waiting = true;
          retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(5000U);
          reconnect = true;   /* keep trying: the retry_at gate above spaces the attempts */
        } else {
          connected = true;
          current_tls = tls;
          last_error = SPLC_MQTT_ERR_NONE;
          retry_waiting = false;
          reconnect = false;
          mqtt_report_status(generation, true, tls, username != NULL,
                             last_error);
          indicator_report_error(INDICATOR_ERROR_MQTT, false);
          LOGI(TAG, "MQTT agent connected");
        }
      }
    }

    if (connected) {
      if (!pending) {
        pending = splc_mqtt_port_take_publish(&pending_message, 0U);
      }
      if (pending) {
        const MQTTStatus_t status = mqtt_agent_publish_with_options(
            mqtt_agent, pending_message.topic, pending_message.payload,
            pending_message.payload_len, (MQTTQoS_t)pending_message.qos,
            pending_message.retain);
        if (status == MQTTSuccess) {
          pending = false;
        } else {
          LOGW(TAG, "SPLC MQTT publish failed: %s",
               MQTT_Status_strerror(status));
          last_error = mqtt_error_code(status);
          mqtt_report_status(generation, false, current_tls, false,
                             last_error);
          connected = false;
          reconnect = true;
          retry_waiting = true;
          retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(1000U);
          mqtt_agent_close();
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(25U));
  }
}
int shell_send_char(void *arg,char c){
    // uart_write_bytes(LOG_PORT,(uint8_t*)&c,1);
    sx_uart_write(SX_UART_PORT_LPUART1,(uint8_t*)&c,1,portMAX_DELAY);
    return c;
}
int shell_send_str(void *arg,char *c){
    // uart_write_bytes(LOG_PORT,(uint8_t*)&c,1);
    sx_uart_write(SX_UART_PORT_LPUART1,(uint8_t*)c,strlen(c),portMAX_DELAY);
    // bsp_delay(10);
    return strlen(c);
}
int shell_recv_char(void *arg){
    int c = 0;
    // uart_read_bytes(LOG_PORT,(uint8_t*)&c,1,portMAX_DELAY);
    sx_uart_read(SX_UART_PORT_LPUART1,(uint8_t*)&c,1,portMAX_DELAY);
    return c;
}
/* USER CODE END Application */