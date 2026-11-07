#include "sx_420mA.h"

#define p_dev(dev) &dev->dev

static SXDevConfig_t dev_config = {
    .coil_reg_num = 0,
    .coil_reg_start_addr = 0,
    .discrete_reg_num = 0,
    .discrete_reg_start_addr = 0,
    .holding_reg_num = 0,
    .holding_reg_start_addr = 0,
    .input_reg_num = 10,
    .input_reg_start_addr = 0
};

void sx_420mA_init(SX_420mA_t *dev, MMBCLient_t *mmb, ThingsBoard_t *net, uint8_t id)
{
    sx_dev_init(p_dev(dev),mmb,&dev_config,id);
    for (int i = 0; i < AI_NUM; i++)
    {
        dev->adc[i] = &dev->dev.input_reg[i];
    }
    dev->net = net;
}

void sx_420mA_poll(SX_420mA_t *dev)
{
    sx_dev_poll(p_dev(dev));
}
