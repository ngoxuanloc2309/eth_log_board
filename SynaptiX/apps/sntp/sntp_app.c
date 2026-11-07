#include "sntp_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/apps/sntp.h"
#include "logger.h"
#include "board.h"

static const char *TAG = "SNTP";


static const char *sntp_server[] = {SNTP_SERVER_ADDRESS,SNTP_GOOGLE_NTP,SNTP_GOOGLE_2_NTP,SNTP_GOOGLE_3_NTP};

static ip_addr_t ntp_ip[4];

TaskHandle_t sntp_task_handle = NULL;
// uint32_t sntp_tick = 1767674883;

void sntp_set_time(uint32_t sntp_time){
    // if(sntp_time < 1767674883) return;
    // sntp_tick = sntp_time;
    LOGD(TAG, "Setting SNTP time: %u", sntp_time);
    struct tm *time;
    time_t sTime = sntp_time +(7*3600);
    time = localtime(&sTime);
    LOGD(TAG,"Time is : %d:%d:%d",time->tm_hour,time->tm_min,time->tm_sec);
    LOGD(TAG,"Day is : %d/%d/%d",time->tm_mday,time->tm_mon+1,time->tm_year + 1900);
    struct timeval tv = {
    .tv_sec = sntp_time,
    .tv_usec = 0
    };
    bsp_set_time(&tv,NULL);
}

static void sntp_task(void *arg) {
    // sntp_tick = 0;
    LOGD(TAG, "SNTP task started");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
    IP4_ADDR(&ntp_ip[0],162,159,200,123);
    IP4_ADDR(&ntp_ip[1],216,239,35,0);
    IP4_ADDR(&ntp_ip[2],216,239,35,4);
    IP4_ADDR(&ntp_ip[3],216,239,35,8);
    for(uint8_t i = 0;i<4;i++){
      sntp_setserver(i, &ntp_ip[i]);
      LOGD(TAG, "SNTP server set to %s", sntp_server[i]);
    }
    sntp_enabled();
    LOGD(TAG, "SNTP enabled");
    
    // while(1){
    //   sntp_tick++;
    //   vTaskDelay(pdMS_TO_TICKS(1000));
    // }
    vTaskDelete(sntp_task_handle);
    sntp_task_handle = NULL;
}
void sntp_app_init(void){
    xTaskCreate(sntp_task, "SNTP Task", 256, NULL, 10, &sntp_task_handle);
    LOGD(TAG, "SNTP task created");
}

uint32_t sntp_get_timestamp()
{   
    return bsp_get_timestamp();
}