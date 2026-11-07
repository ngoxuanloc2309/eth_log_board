#include "button.h"
#include "logger.h"
#include "ota.h"
#include "app_settings.h"

extern Bootloader_t boot_patition;

#define TIME_DEBOUND_CLICK_mS 100
#define TIME_DEBOUND_HOLD_mS 2000

static const char *TAG = "Button";

typedef struct UserButton
{
     /* data */
     uint32_t tick;
     uint32_t time_holde;
     BT_Handlle_t handle;
     void *arg;
     uint8_t click;
}UserButton_t;

UserButton_t user_bt;

void button_init(BT_Handlle_t handle, void *arg)
{
     log_info(TAG,"Button init...");
     user_bt.handle = handle;
     user_bt.arg = arg;
     user_bt.tick = 0;
     user_bt.click = 0;
     user_bt.time_holde = 0;

}
void button_poll(int time_stamp)
{
     int res = bsp_button_status();
     static uint32_t time_events = 0;
     if(user_bt.tick > 0 && res == BT_ST_OFF){
          user_bt.tick -= time_stamp;
     }
     if(res == BT_ST_ON && user_bt.tick == 0){
          user_bt.tick = TIME_DEBOUND_CLICK_mS;
          log_info(TAG,"Clicked");
          user_bt.click++;
          time_events = 0;
     }
     if(res == BT_ST_ON){
          user_bt.time_holde+= time_stamp;
     }
     else{
          user_bt.time_holde = 0;
     }
     if(user_bt.time_holde == 5000){
          log_info(TAG,"Holded!!! Restore default settings");
          app_setting_default(&app_setting);
          app_setting_save(&app_setting);
          bsp_restart();
     }
     time_events += time_stamp;

     if (time_events >= 500)
     {
          switch (user_bt.click)
          {
          case 0:
          break;
          case 1:
               /* code */
               log_info(TAG,"Clicked");
               bsp_restart();
               break;
          case 2:
               /* code */
               log_info(TAG,"Double Clicked");
               // bsp_restart();
               break;
          case 3:
               /* code */
               log_info(TAG,"Tripble Clicked");
               boot_patition.isNewFw = 2;
               boot_save(&boot_patition);
               bsp_restart();
               break;
          default:
               // log_info(TAG,"Holded");
               break;
          }
          user_bt.click = 0;
     }
}