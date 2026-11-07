#include "app.h"
#include "logger.h"
#include "rf_app.h"
#include "zigbee.h"
#include "mongoose_glue.h"
#include "rng.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "event_groups.h"
#include "file_io.h"
// #include "usb_device.h"
#include "shell_tcp.h"
#include "shell_serial.h"
#include "mb.h"
#include "port.h"
#include "mbs_tcp.h"
#include "mbs_usb.h"
// #include "usb_device.h"
#include "fatfs.h"
#include "app_settings.h"
#include "lwip.h"
#include "spif.h"
#include "mbedtls.h"
#include "sha1_alt.h"
#include "sha256_alt.h"
#include "md5_alt.h"
#include "aes_alt.h"
#include "http_server.h"
#include "usb_netif.h"
#include "sntp.h"

static const char *TAG = "APP";

#define NETWORK_CONNECTED (1 << 0)
#define NETWORK_DISCONNECTED (1 << 1)

#define FS_BLOCK_BASE (0)

// Define the block size, count, read size, program size, cache size, and look
#define FS_BLOCK_TOTAL 64   // Total blocks
#define FS_BLOCK_COUNT (64) // Number of blocks is less than (FS_BLOCK_TOTAL - FS_BLOCK_BASE)
#define FS_BLOCK_SIZE 512   // 512B sectors

ETH_ZBGateWay_t gateway;

int fs_read(const struct lfs_config *c, lfs_size_t block,
            lfs_size_t off, void *buffer, lfs_size_t size);
int fs_prog(const struct lfs_config *c, lfs_size_t block,
            lfs_size_t off, const void *buffer, lfs_size_t size);
int fs_erase(const struct lfs_config *c, lfs_size_t block);

static SemaphoreHandle_t xFSMutex = NULL;

file_system_config_t fs_config = {
    .block_size = FS_BLOCK_SIZE, // 4KB
    .block_count = FS_BLOCK_COUNT,   // Define the number of blocks
    .file_function = {
        .erase = fs_erase,
        .read = fs_read,
        .write = fs_prog}};

void file_system_app_init(void) {
    xFSMutex = xSemaphoreCreateMutex();
    // Initialize the W25Q flash memory
    if(!SPIF_Init(&bsp_spif,BSP_SPI_HANDLE,BSP_SPI_CS_Port,BSP_SPI_CS_Pin)){
      log_error(TAG,"Can't init external Flash");
      return;
    }
    fs_config.block_count = bsp_spif.SectorCnt;
    fs_config.block_size = SPIF_SECTOR_SIZE;
    // Initialize the file system
    if (file_system_init(&fs_config) != 0) {
        log_error(TAG, "Failed to initialize file system");
        return;
    }

    int res = mkdir("/logs");
    if(res != 0)
        log_error(TAG,"Dir is exist /logs");
    res = mkdir("/data");
        if(res != 0)
        log_error(TAG,"Dir is exist /data");
    fs_list_dir("/");
    // Load device configuration
    // load_config(&device_config);
}

TaskHandle_t button_handle = NULL;
EventGroupHandle_t network_evens = NULL;

#if BUTTON_APP_ENABLE
static void button_task(void *arg)
{
  button_init(NULL,NULL);
  while (1)
  {
    button_poll(10);
    vTaskDelay(10);
  }
}
#endif

#if LED_INDICATOR_ENABLE
static void led_indicator_task(void *arg)
{
  uint32_t time_count = 0;
  while (1)
  {
    time_count += 100;
    if (mqtt_client_is_connected(&gateway.thingsboard.base))
    {
      bsp_led_error_off();
      if(time_count % 1000 == 0){
        bsp_led_activity_toggle();
      }
    }
    else
    {
      bsp_led_activity_off();
      if (time_count % 500 == 0)
      {
        bsp_led_error_toggle();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#endif

#if MONITOR_HEAP_RTOS
uint32_t ram_usage = 0;
static char monitor_buff[4096-1024];
static char taskListBuff[512];
static char runtimeStats[512];
void bsp_usb_cdc_puts(int ch,char *s){
    bsp_usb_cdc_transmit(ch,(uint8_t*) s,strlen(s));
}
uint32_t cpuUsage;
static int get_idle_cpu_percent()
{
  TaskHandle_t idle_task = xTaskGetHandle("IDLE");
  if(idle_task != NULL){
    int percent = ulTaskGetRunTimePercent(idle_task);
    return percent;
  }
  return 0;  // không tìm thấy
}

uint32_t app_get_cpu_usage(void)
{
    return cpuUsage;
}

uint32_t app_get_ram_usage(void)
{
    return ram_usage;
}

static void RTOS_Monitor(void *arg){
  while(1){
    vTaskList(taskListBuff);
    // printf("\033[2J");  
    size_t freeHeapSize = xPortGetFreeHeapSize();
    ram_usage = (configTOTAL_HEAP_SIZE - freeHeapSize) / 1024;
    vTaskGetRunTimeStats(runtimeStats);
    cpuUsage = 100 - get_idle_cpu_percent();
    sprintf(monitor_buff,"\033[2J\x1B[32mTask\t\tState\tPrio\tStack\tNum\r\n%s\r\nMemory Usaged %0.2f %%, Free %u bytes\r\nCPU Usaged\r\nTask\t\tTick\t\tCPU\r\n%s\r\n\r\n",taskListBuff,(float) ((configTOTAL_HEAP_SIZE-freeHeapSize)*100)/ (float) configTOTAL_HEAP_SIZE,freeHeapSize,runtimeStats);
    bsp_usb_cdc_puts(USB_RPC,monitor_buff);
    gateway.uptime++;
    // bsp_usb_cdc_puts(USB_RPC,buff);
    bsp_delay(1000);
  }
}
#endif
#include "ota.h"
TimerHandle_t ota_check_timer = NULL;
static void sx_check_new_fw(TimerHandle_t xTimer){
  if(gateway.boot.isNewFw == 0){
    log_info(TAG,"No new firmware");
    return;
  }
  else if (gateway.boot.isNewFw == 3)
  {
    log_info(TAG, "New firmware detected, clear flag");
    gateway.boot.isNewFw = 0;
    boot_save(&gateway.boot);
  }
}
int __io_putchar(int ch){
  uart_write_bytes(LOG_PORT,(uint8_t*)&ch,1);
  return ch;
}

static void log_puts(const char *s)
{
  // printf(s);
  uart_write_bytes(LOG_PORT,(uint8_t*)s,strlen(s));
  // CDC_Transmit(0,(uint8_t*)s,strlen(s));
}
static const char *log_str[5] = {"disabled","error","info","debug","verbose"};
static const LOGGING_LEVELS log_level[5] = {LOGGER_OFF,LOGGER_ERROR,LOGGER_INFO,LOGGER_DEBUG,LOGGER_DEBUG};

void app_init()
{
  // Create event group for network events
  network_evens = xEventGroupCreate();
  // Initialize logging
  logger_init(LOGGER_DEBUG, log_puts);

  log_info(TAG, "App init");
  log_info(TAG, "Vendor : %s", BOARD_VENDOR);
  log_info(TAG, "URL : %s", BOARD_URL);
  log_info(TAG, "Board name: %s", BOARD_NAME);
  log_info(TAG, "Version : V%d.%d.%d.rc%d", BSP_VERSION_MAJOR, BSP_VERSION_MINOR, BSP_VERSION_PATCH, BSP_VERSION_RC);
  log_info(TAG, "UUID : 0x%08X", bsp_get_chip_id());
  
  // Load bootloader information
  boot_load(&gateway.boot);
  ota_check_timer = xTimerCreate("OTA_Check_Timer", pdMS_TO_TICKS(60000*5), pdFALSE, ( void * ) 0, sx_check_new_fw);
  xTimerStart( ota_check_timer, 0 );
  // Initialize file system
  file_system_app_init();
  // Load application settings
  app_setting_load(&app_setting);
  // Configure logging level
  for(int i = 0;i<5;i++){
    if(strcmp(app_setting.setting.log_level,log_str[i]) == 0){
      log_info(TAG,"Reconfig logging level");
      logger_set_level(log_level[i]);
      break;
    }
  }
  // Initialize FATFS
  #if FATFS_ENABLE
  if(FR_OK == MX_FATFS_Init()){

  }
  #endif
  // Initialize mbedTLS
  MX_MBEDTLS_Init();
  // Initialize lwIP stack
  MX_LWIP_Init(&app_setting);
  // Initialize USB device

  #if USB_DEVICE_FS_ENABLE
  MX_USB_DEVICE_Init();
  #else
  usb_netif_init();
  #endif
  // Initialize RTOS Monitor Heap and CPU usage
  #if MONITOR_HEAP_RTOS
  xTaskCreate(RTOS_Monitor,"rtos monitor",256,NULL,1,NULL);
  #endif
  // Initialize Modbus USB slave
  #if USB_DEVICE_FS_ENABLE && MODBUS_RTU_ENABLE
  mbs_usb_init();
  #endif
  // Initialize RF application
  rf_app_init();
  #if BUTTON_APP_ENABLE
  xTaskCreate(button_task, "button",1024*2, NULL, 5, &button_handle);
  if (button_handle == NULL)
  {
    log_error(TAG, "Create Button task failed");
  }
  #endif
  // Initialize serial shell application services
  shell_serial_init();
  // Initialize led indicator task
  #if LED_INDICATOR_ENABLE
  if(pdPASS != xTaskCreate(led_indicator_task,"led",256,NULL,5,NULL)){
    log_error(TAG,"Can't create led indicator task");
  }
  #endif
  http_server_init();
  // Wait for network connection
  xEventGroupWaitBits(network_evens,
                      NETWORK_CONNECTED, pdFALSE, pdFALSE,
                      app_setting.network.dhcp ? portMAX_DELAY : 1000);
  // Initialize shell command line application services
  // shell_tcp_init(&gateway.tcp_shell);
  // Initialize HTTP server
  
  // Initialize SNTP client
  sntp_app_init();
  // Initialize MQTT client
  mqtt_app_init();
  // Initialize Modbus TCP server
  // modbus_tcp_server_init(&gateway.mb_tcp,MODBUS_TCP_PORT);
}

/**
 * ip_status_callback - Called when the network interface status or IP configuration changes.
 * @netif: Pointer to the lwIP network interface whose status changed.
 *
 * This callback is intended to be registered with lwIP (for example via
 * netif_set_status_callback()) and will be invoked whenever the interface's
 * IP address, netmask, gateway or "up/down" state changes (e.g. DHCP lease,
 * manual reconfiguration, link up/down).
 *
 * Typical responsibilities:
 * - Inspect netif flags and addresses to determine new state (up/down, valid IP).
 * - Start or stop application services that depend on network connectivity.
 * - Notify other tasks/threads or signal application-level components of the change.
 * - Trigger any required reconfiguration (DNS, sockets, routes, etc.).
 *
 * Threading / safety:
 * - Executed in the lwIP TCP/IP context; avoid long-blocking operations or calls
 *   that are not safe from this context. If necessary, defer work to application
 *   threads using a message/queue mechanism.
 *
 * Side effects:
 * - No return value. All effects should be performed through side-effecting calls
 *   (logging, state updates, service control, inter-thread notifications).
 *
 * Example:
 *   netif_set_status_callback(&my_netif, ip_status_callback);
 */
void ip_status_callback(struct netif *netif)
{
  if (netif_is_up(netif))
  {
    log_info(TAG, "Ethernet Interface UP");
    log_info(TAG, "IP : %s", ipaddr_ntoa(&netif->ip_addr));
    log_info(TAG, "MASK : %s", ipaddr_ntoa(&netif->netmask));
    log_info(TAG, "GW : %s", ipaddr_ntoa(&netif->gw));
    bsp_led_network_on();
    if(app_setting.network.dhcp){
      strcpy(app_setting.network.ip_address,ipaddr_ntoa(&netif->ip_addr));
      strcpy(app_setting.network.netmask,ipaddr_ntoa(&netif->netmask));
      strcpy(app_setting.network.gw_address,ipaddr_ntoa(&netif->gw));
    }
    // sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // sntp_setservername(0, "pool.ntp.org");
    // sntp_init();
    xEventGroupSetBits(network_evens, NETWORK_CONNECTED);
    #if FATFS_ENABLE
    char path[20];
    sprintf(path, "%s/network.cfg", USERPath);
    FRESULT fret = f_open(&USERFile, path, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
    if (fret != FR_OK)
    {
      log_error(TAG, "Can't open file %s", path);
      f_close(&USERFile);
      return;
    }
    // UINT bytesWrote;
    // fret = f_write(&USERFile, "This is SX-ETH-ZB Gateway\r\n", strlen("This is SX-ETH-ZB Gateway\r\n"), &bytesWrote);
    f_printf(&USERFile, "IP Address : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
    f_printf(&USERFile, "SUBNET MASK : %s\r\n", ipaddr_ntoa(&netif->netmask));
    f_printf(&USERFile, "DEFAULT GATEWAY: %s\r\n", ipaddr_ntoa(&netif->gw));
    f_printf(&USERFile, "MAC : %02X:%02X:%02X:%02X:%02X:%02X\r\n", ETH_MAC_ADDR0, ETH_MAC_ADDR1, ETH_MAC_ADDR2, ETH_MAC_ADDR3,ETH_MAC_ADDR4,ETH_MAC_ADDR5);
    f_printf(&USERFile, "Visit me : http://%s\r\n",ipaddr_ntoa(&netif->ip_addr));
    f_close(&USERFile);
    #endif
  }
  else
  {
    log_warn(TAG, "Ethernet Interface DOWN");
    // sntp_stop();
    bsp_led_network_off();
  }
}

// Block device operations implementation
#if 1
/**
 * @brief Read data from flash storage at a specified block.
 * 
 * @param c Pointer to the LittleFS configuration structure containing
 *          filesystem parameters and function pointers.
 * @param block The block number to read from in the flash storage.
 * 
 * @return Returns the status of the read operation (typically 0 on success,
 *         negative error code on failure).
 * 
 * @note This function is part of the LittleFS block device interface and
 *       must be implemented according to the lfs_config specifications.
 */
int fs_read(const struct lfs_config *c, lfs_size_t block,
            lfs_size_t off, void *buffer, lfs_size_t size)
{
  (void)c; // Unused parameter
  // uint8_t *buf = (uint8_t*)buffer;
  // Calculate raw address
  // uint32_t raw_addr = ((block + FS_BLOCK_BASE) * FS_BLOCK_SIZE) + off;

  // Read from W25Q flash
  // xSemaphoreTake(xFSMutex, portMAX_DELAY);
  SPIF_ReadSector(&bsp_spif,block,buffer,size,off);
  // xSemaphoreGive(xFSMutex);

  return LFS_ERR_OK;
}

/**
 * @brief Programs a block in the LittleFS filesystem.
 * 
 * @param c Pointer to the LittleFS configuration structure containing filesystem parameters.
 * @param block The block number to be programmed.
 * 
 * @return LFS_ERR_OK on success, or a negative error code on failure.
 */
int fs_prog(const struct lfs_config *c, lfs_size_t block,
            lfs_size_t off, const void *buffer, lfs_size_t size)
{
  (void)c; // Unused parameter
  // uint8_t *buf = (uint8_t*)buffer;
  // Calculate raw address
  // uint32_t raw_addr = ((block + FS_BLOCK_BASE) * FS_BLOCK_SIZE) + off;

  // Program to W25Q flash
  // xSemaphoreTake(xFSMutex, portMAX_DELAY);
  SPIF_WriteSector(&bsp_spif,block,(uint8_t*)buffer,size,off);
  // xSemaphoreGive(xFSMutex);
  return LFS_ERR_OK;
}

/**
 * @brief Erase a block in the file system.
 * 
 * Erases a single block in the LittleFS file system at the specified block address.
 * This function is typically called by the LittleFS driver to clear storage space
 * for new file operations.
 * 
 * @param c Pointer to the LittleFS configuration structure containing file system parameters.
 * @param block The block number to be erased.
 * 
 * @return On success, returns 0. On error, returns a negative error code as defined
 *         by the LittleFS error codes.
 */
int fs_erase(const struct lfs_config *c, lfs_size_t block)
{
  // (void)c; // Unused parameter
  // xSemaphoreTake(xFSMutex, portMAX_DELAY);
  SPIF_EraseSector(&bsp_spif,block);
  // xSemaphoreGive(xFSMutex);
  return LFS_ERR_OK;
}
#endif