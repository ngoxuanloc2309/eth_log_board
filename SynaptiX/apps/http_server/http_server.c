#include "http_server.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "mongoose.h"
#include "mongoose_glue.h"
#include "logger.h"
#include "tcpip.h"
static const char *TAG = "HTTP_SERVER";

static TaskHandle_t http_server_task_handle = NULL;

extern RNG_HandleTypeDef hrng;

bool mg_random(void *buf, size_t len)
{ // Use on-board RNG or custom PRNG
  for (size_t n = 0; n < len; n += sizeof(uint32_t))
  {
    uint32_t r = 0;
    HAL_RNG_GenerateRandomNumber(&hrng, &r);
    memcpy((char *)buf + n, &r, n + sizeof(r) > len ? len - n : sizeof(r));
  }
  return true; // TODO(): change rng_read() sig: check chip RNG, return false
}

extern SemaphoreHandle_t usb_netif_sem;
static void http_server_task(void *arg)
{
  // while(usb_netif_sem == NULL){
  //   vTaskDelay(1);
  // }
  mongoose_init();
  mg_log_set(MG_LL_NONE);
  mongoose_add_ws_reporter(100, "state");
  mongoose_add_ws_reporter(2000,"ssids");
  /* Infinite loop */
  for (;;)
  {
    // xSemaphoreTake(usb_netif_sem,portMAX_DELAY);
    // LOCK_TCPIP_CORE();
    mongoose_poll();
    // UNLOCK_TCPIP_CORE();
    // xSemaphoreGive(usb_netif_sem);
  }
}

void http_server_init(void)
{
  xTaskCreate(http_server_task, "http server", 1024 * 8, NULL, 10, &http_server_task_handle);
  if (http_server_task_handle == NULL)
  {
    log_error(TAG, "Create http server task failed");
  }
}