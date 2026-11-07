#include "function.h"
#include "sx_485_io.h"
#include <string.h>
#include <stdlib.h>
#include "logger.h"
#include "app.h"
#include "FreeRTOS.h"
#include "timers.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static cJSON * set_output(void *arg,cJSON *params);
static cJSON * set_all_output(void *arg,cJSON *params);
static cJSON * reset_counter(void *arg,cJSON *params);
static cJSON * reboot(void *arg,cJSON *params);

static const RPC_t rpc_funcs_commands[] = {
    {.name = "setOutput",.arg = NULL,.func = set_output},
    {.name = "setAllOutput",.arg = NULL,.func = set_all_output},
    {.name = "resetCounter",.arg = NULL,.func = reset_counter},
    {.name = "reboot",.arg = NULL,.func = reboot}
};

static const char *TAG = "Function";

// extern SX_485_IO_t sx485io[NUM_SX485IO_DEV];

SX_485_IO_t *getSX485IO_dev(uint8_t id){
   SX_485_IO_t *dev = NULL;
    for(int i = 0;i<gateway.numDevIO;i++){
        if(gateway.devio[i].dev.id == id){
            dev = &gateway.devio[i];
            break;
        }
    } 
    return dev;
}

cJSON * set_output(void *arg, cJSON *params)
{
    cJSON *id = cJSON_GetObjectItem(params,"id");
    cJSON *output = cJSON_GetObjectItem(params,"output");
    cJSON *state = cJSON_GetObjectItem(params,"state");

    if(id == NULL || output == NULL || state == NULL) return cJSON_CreateString("id or output or state is null");

    if(cJSON_IsNumber(id) && cJSON_IsNumber(output) && cJSON_IsBool(state)){
        SX_485_IO_t *dev = getSX485IO_dev(cJSON_GetNumberValue(id));
        if(dev != NULL){
           bool st = (cJSON_IsTrue(state) ? true : false);
           int res = SX485IO_SetOutput(dev,cJSON_GetNumberValue(output),st);
           if(res != 0) {
            char message[128];
            sprintf(message,"set output : %d, id : %d, error : %d",(uint8_t)cJSON_GetNumberValue(output),(uint8_t)cJSON_GetNumberValue(id),res);
            return cJSON_CreateString(message);
           }
           return cJSON_CreateString("set output success"); 
        }
    }

    return cJSON_CreateString("Invalid id or number or state");
}

cJSON * set_all_output(void *arg, cJSON *params)
{
    return 0;
}

static const char *mb_err_2_str(int err){
    
    switch (err)
    {
    case NMBS_ERROR_INVALID_REQUEST:
        /* code */
        return "ERROR INVALID REQUEST";
        break;
    case NMBS_ERROR_INVALID_UNIT_ID:
        /* code */
        return "ERROR INVALID UNIT ID";
        break;
    case NMBS_ERROR_INVALID_TCP_MBAP:
        /* code */
        return "ERROR INVALID TCP MBAP";
        break;
    case NMBS_ERROR_CRC:
        /* code */
        return "ERROR CRC";
        break;
    case NMBS_ERROR_TRANSPORT:
        /* code */
        return "ERROR TRANSPORT";
        break;
    case NMBS_ERROR_TIMEOUT:
        /* code */
        return "ERROR TIMEOUT";
        break;
    case NMBS_ERROR_INVALID_RESPONSE:
        /* code */
        return "ERROR INVALID RESPONSE";
        break;
    case NMBS_ERROR_INVALID_ARGUMENT:
        /* code */
        return "ERROR INVALID ARGUMENT";
        break;
    case NMBS_ERROR_NONE:
        return "SUCCESS";
        /* code */
        break;
    case NMBS_EXCEPTION_ILLEGAL_FUNCTION:
        /* code */
        return "EXCEPTION ILLEGAL FUNCTION";
        break;
    case NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
        /* code */
        return "EXCEPTION ILLEGAL DATA ADDRESS";
        break;
    case NMBS_EXCEPTION_ILLEGAL_DATA_VALUE:
        /* code */
        return "EXCEPTION ILLEGAL DATA VALUE";
        break;
    case NMBS_EXCEPTION_SERVER_DEVICE_FAILURE:
        /* code */
        return "EXCEPTION SERVER DEVICE FAILURE";
        break;
    default:
        return "ERROR OTHER";
        break;
    }
}

cJSON * reset_counter(void *arg, cJSON *params)
{
    LOGD(TAG,"Reset counter");
    cJSON *id = cJSON_GetObjectItem(params,"id");
    cJSON *counter = cJSON_GetObjectItem(params,"counter");

    if(id == NULL || counter == NULL) {
        LOGE(TAG,"id or counter is NULL");
        return cJSON_CreateString("id or counter id null");
    }
    if(cJSON_IsNumber(id) && cJSON_IsNumber(counter)){
        uint8_t s_id = cJSON_GetNumberValue(id);
        uint8_t s_counter = cJSON_GetNumberValue(counter);
        LOGD(TAG,"Reset counter %d, id : %d",s_counter,s_id);
        SX_485_IO_t *dev = getSX485IO_dev(s_id);
        if(dev != NULL){
           int res = SX485IO_ResetCounter(dev,s_counter);
           LOGD(TAG,"resetCounter result : %d", res);
           if(res != 0) {
            char message[128];
            sprintf(message,"%s",mb_err_2_str(res));
            return cJSON_CreateString(message);
           }
           return cJSON_CreateString("Reset counter success");
        }
    }
    return cJSON_CreateString("Invalid id or couter");
}
static void vRebootCallback(TimerHandle_t xTimer){
    LOGD(TAG,"Reboot device");
    HAL_NVIC_SystemReset();
}
static cJSON * reboot(void *arg,cJSON *params){
    LOGD(TAG,"Reboot after 10 seconds ...");
    TimerHandle_t reboot_timer = NULL;
    
    reboot_timer = xTimerCreate("reboot",pdMS_TO_TICKS(10000),pdFALSE,NULL,vRebootCallback);
    if(reboot_timer == NULL){
        return cJSON_CreateString("Can't reboot device");
    }
    xTimerStart(reboot_timer,0);
    return cJSON_CreateString("reboot device success");
}

const RPC_t  *const rpc_funcs = rpc_funcs_commands;
const size_t rpc_funcs_nums = ARRAY_SIZE(rpc_funcs_commands);
        
cJSON * RPC_Call(char *name, cJSON *param)
{
    for(size_t i = 0; i < rpc_funcs_nums; i++){
        if(strcmp(rpc_funcs[i].name,name) == 0){
            return rpc_funcs[i].func(rpc_funcs[i].arg,param);
        }
    }
    return cJSON_CreateString("Invalid function");;
}