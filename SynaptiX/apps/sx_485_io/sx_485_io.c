#include "sx_485_io.h"
#include "logger.h"
#include "cJSON.h"
#include "app_settings.h"
#include "file_io.h"


static const char *TAG = "SX-485-IO";

#define p_dev(dev) (&dev->dev)

extern uint32_t sntp_get_timestamp();

static SXDevConfig_t dev_config = {
    .coil_reg_num = 0,
    .coil_reg_start_addr = 0,
    .discrete_reg_num = 4,
    .discrete_reg_start_addr = 0,
    .holding_reg_num = 0,
    .holding_reg_start_addr = 0,
    .input_reg_num = 4,
    .input_reg_start_addr = 16
};

void sx_485_io_init(SX_485_IO_t *dev, MMBCLient_t *mmb, ThingsBoard_t *net, uint8_t id)
{
    sx_dev_init(p_dev(dev), mmb,&dev_config ,id);
    for (int i = 0; i < IO_INPUT_NUM; i++)
    {
        dev->counter[i] = &p_dev(dev)->input_reg[i];
        dev->last_input[i] = 0;
        dev->last_counter[i] = 0;
        dev->input[i] = 0;
    }
    dev->net = net;
}

void sx_485_io_poll(SX_485_IO_t *dev)
{
    sx_dev_poll(p_dev(dev));
    // uint16_t reset_value[4] = {INPUT_EVENT_RELEASE, INPUT_EVENT_RELEASE, INPUT_EVENT_RELEASE, INPUT_EVENT_RELEASE};
    for(int i = 0;i<IO_INPUT_NUM;i++){
        dev->input[i] = (p_dev(dev)->discrete_reg[0] >> i) & 0x01;
        // dev->output[i] = (p_dev(dev)->coil_reg[0] >> i) & 0x01;
    }
    uint8_t flag = 0;
    for (int i = 0; i < IO_INPUT_NUM; i++)
    {
        if (dev->last_input[i] != dev->input[i] || dev->last_counter[i] != *dev->counter[i])
        {
            // tb_publish(dev->net,TELEMETRY_API,)
            dev->last_input[i] = dev->input[i];
            dev->last_counter[i] = *dev->counter[i];
            flag++;
        }
    }
    if ((flag > 0) && (p_dev(dev)->isConnect == true))
    {
        cJSON *device = cJSON_CreateObject();
        uint32_t timestamp = sntp_get_timestamp();
        cJSON_AddNumberToObject(device,"timestamp",timestamp);
        // cJSON *output = cJSON_CreateArray();
        // for(int i = 0; i<IO_OUTPUT_NUM;i++)
        //     cJSON_AddItemToArray(output, cJSON_CreateNumber(dev->output[i]));
        // cJSON_AddItemToObject(device, "outputs", output);
        cJSON *input = cJSON_CreateArray();
        cJSON *counter = cJSON_CreateArray();
        for(int i = 0; i<IO_INPUT_NUM;i++){
            cJSON_AddItemToArray(input, cJSON_CreateNumber(dev->input[i]));
            cJSON_AddItemToArray(counter, cJSON_CreateNumber(*dev->counter[i]));
        }
        cJSON_AddItemToObject(device, "inputs", input);
        cJSON_AddItemToObject(device,"counters",counter);
        cJSON_AddBoolToObject(device, "isConnect", p_dev(dev)->isConnect);
        cJSON_AddNumberToObject(device, "id", p_dev(dev)->id);
        static char json_buff[4096];
        memset(json_buff, 0, 4096);
        cJSON_PrintPreallocated(device, json_buff, 4096, false);
        cJSON_Delete(device);
        log_info(TAG, "Json payload : %s", json_buff);
        char topic[128];

        sprintf(topic, TELEMETRY_API"%s/events/%d",app_setting.setting.name,RS485_IO_TYPE, p_dev(dev)->id);
        if (-1 == tb_publish(dev->net,topic, json_buff, strlen(json_buff), 2, 0))
        {
            log_error(TAG, "Net is NULL or not connect");
            char file_name[128];
            sprintf(file_name,"/data/%lu.json",timestamp);
            FILE *file = fopen(file_name,"w");
            if(file == NULL) {
                log_error(TAG,"Can't create data file %s",file_name);
                return;
            }

            cJSON *root = cJSON_CreateObject();

            cJSON_AddStringToObject(root,"topic",topic);
            cJSON_AddStringToObject(root,"payload",json_buff);

            if(fputs(cJSON_Print(root),file) <= 0){
                log_info(TAG,"Can't write file %s",file_name);
                fclose(file);
                remove(file_name);
                cJSON_Delete(root);
                return;
            }
            log_info(TAG,"Writed to file %s ",file_name);
            cJSON_Delete(root);
            fclose(file);
            return;
        }
        flag = 0;
    }
}

enum InputEvent SX485IO_GetInputEV(SX_485_IO_t *dev, int input)
{
    if (input < 0 || input >= IO_INPUT_NUM)
        return INPUT_EVENT_RELEASE;
    return *dev->input_event[input];
}
int SX485IO_SetOutput(SX_485_IO_t *dev, int output, bool state)
{
    if (output < 0 || output >= IO_OUTPUT_NUM)
        return -1;

    int res = mmb_write_single_coil(p_dev(dev)->mmb, p_dev(dev)->id, output, state);
    res == 0 ? log_info(TAG, "Set output %d device %d %s success", output, p_dev(dev)->id, state == false ? "OFF" : "ON") : log_error(TAG, "Set output %d device %d %s fail", output, p_dev(dev)->id, state == false ? "OFF" : "ON");
    return res;
}

int SX485IO_SetOutputArr(SX_485_IO_t *dev, uint8_t start_output, uint8_t num_output, bool *state)
{
    uint8_t output_st = p_dev(dev)->coil_reg[0];
    if(start_output >= IO_OUTPUT_NUM) return -1;
    if(num_output > (IO_INPUT_NUM - start_output)) return -2;

    for(int i = start_output;i < num_output ;i++){
        output_st |= ((0x01 & state[i-start_output]) << i);
    }
    return mmb_write_multiple_coils(p_dev(dev)->mmb,p_dev(dev)->id,start_output,num_output,&output_st);;
}

int SX485IO_SetCleanInputEV(SX_485_IO_t *dev, int input)
{
    if (input < 0 || input >= IO_INPUT_NUM)
        return -1;
    int res = mmb_write_single_register(p_dev(dev)->mmb, p_dev(dev)->id, input + 4, INPUT_EVENT_RELEASE);
    res == 0 ? log_info(TAG, "Clean input %d event success", input) : log_error(TAG, "Clean input %d event fail", input);
    return res;
}

int SX485IO_ResetCounter(SX_485_IO_t *dev, int counter_num)
{
    if (counter_num < 0 || counter_num > +IO_INPUT_NUM)
    {
        return -1;
    }
    int res = mmb_write_single_coil(p_dev(dev)->mmb, p_dev(dev)->id,8 + counter_num, true);
    res == 0 ? log_info(TAG, "Reset counter %d device %d success", counter_num, p_dev(dev)->id) : log_error(TAG, "Reset counter %d device %d fail", counter_num, p_dev(dev)->id);
    return res;
}
