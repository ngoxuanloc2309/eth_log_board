#include "sx_dev.h"
#include <stdlib.h>
#include <assert.h>
#include "logger.h"

static const char *TAG = "SX-DEV";

#define DELAY_POLLING_mS 10

void sx_dev_init(SXDev_t *dev, MMBCLient_t *mmb, SXDevConfig_t *config, uint8_t id)
{
    assert(config != NULL);
    assert(mmb != NULL);
    assert(id > 0);

    dev->config = config;
    if (config->holding_reg_num > 0){
        dev->holding_reg = (uint16_t *)malloc(dev->config->holding_reg_num * sizeof(uint16_t));
        memset(dev->holding_reg,0,dev->config->holding_reg_num * sizeof(uint16_t));
    }
    if (config->input_reg_num > 0){
        dev->input_reg = (uint16_t *)malloc(dev->config->input_reg_num * sizeof(uint16_t));
        memset(dev->input_reg,0,dev->config->input_reg_num * sizeof(uint16_t));
    }
    if (config->coil_reg_num > 0)
    {
        dev->coil_reg = (uint8_t *)malloc(dev->config->coil_reg_num);
        memset(dev->coil_reg,0,dev->config->coil_reg_num);
    }
    if (config->discrete_reg_num > 0)
    {
        dev->discrete_reg = (uint8_t *)malloc(dev->config->discrete_reg_num);
        memset(dev->discrete_reg,0,dev->config->discrete_reg_num);
    }
    dev->id = id;
    dev->isConnect = false;
    dev->mmb = mmb;
    dev->timeout_disconnect = 0;
}

void sx_dev_poll(SXDev_t *dev)
{
    if (dev->config->holding_reg_num > 0){
        if(0 == mmb_read_holding_registers(dev->mmb, dev->id, dev->config->holding_reg_start_addr, dev->config->holding_reg_num, dev->holding_reg)) dev->timeout_disconnect = bsp_get_tick();
        bsp_delay(pdMS_TO_TICKS(DELAY_POLLING_mS));
    }
    if (dev->config->input_reg_num > 0){
        if(0 == mmb_read_input_registers(dev->mmb, dev->id, dev->config->input_reg_start_addr, dev->config->input_reg_num, dev->input_reg)) dev->timeout_disconnect = bsp_get_tick();
        bsp_delay(pdMS_TO_TICKS(DELAY_POLLING_mS));
    }
    if (dev->config->coil_reg_num > 0){
        if(0 == mmb_read_coils(dev->mmb, dev->id, dev->config->coil_reg_start_addr, dev->config->coil_reg_num, dev->coil_reg)) dev->timeout_disconnect = bsp_get_tick();
        bsp_delay(pdMS_TO_TICKS(DELAY_POLLING_mS));
    }
    if (dev->config->discrete_reg_num > 0){
        if(0 == mmb_read_discrete_inputs(dev->mmb, dev->id, dev->config->discrete_reg_start_addr, dev->config->discrete_reg_num, &dev->discrete_reg[0]))
            dev->timeout_disconnect = bsp_get_tick();
        bsp_delay(pdMS_TO_TICKS(DELAY_POLLING_mS));
    }
    if(bsp_get_tick() - dev->timeout_disconnect > TIMEOUT_DISCONNECT_mS){
        dev->isConnect = false;
    }
    else dev->isConnect = true;
}
