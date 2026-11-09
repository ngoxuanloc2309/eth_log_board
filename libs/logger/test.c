#include <stdio.h>
#include "logger.h"

static const char* TAG = "Test";

int main(){
    logger_init(LOGGER_DEBUG,NULL);
    LOGD(TAG,"This is %s ","DEBUG");
    LOGI(TAG,"This is %s ","INFOR");
    LOGW(TAG,"This is %s ","WARNING");
    LOGE(TAG,"This is %s ","ERROR");
    uint8_t buff[] = {0x01,0x02,0x03};
    log_print_hex(LOGGER_DEBUG,TAG,buff,3);
    return 0;
}