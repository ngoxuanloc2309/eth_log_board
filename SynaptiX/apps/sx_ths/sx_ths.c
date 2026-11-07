#include "sx_ths.h"
#include "logger.h"
#include "cJSON.h"

static const char *TAG = "SX-THS";
#define p_dev(dev) (&dev->dev)

static SXDevConfig_t dev_config = {
    .coil_reg_num = 0,
    .coil_reg_start_addr = 0,
    .discrete_reg_num = 0,
    .discrete_reg_start_addr = 0,
    .holding_reg_num = 4,
    .holding_reg_start_addr = 0,
    .input_reg_num = 0,
    .input_reg_start_addr = 0
};

void sx_ths_init(SX_THS_t *dev,MMBCLient_t *mmb, ThingsBoard_t *net,uint8_t id){
    sx_dev_init(p_dev(dev), mmb,&dev_config, id);
    dev->net = net;
    dev->temp.temp = 0;
    dev->humi.humi = 0;
}
void sx_ths_poll(SX_THS_t *dev){
    sx_dev_poll(p_dev(dev));
    if(p_dev(dev)->isConnect){
        dev->temp.data[0] = p_dev(dev)->holding_reg[1];
        dev->temp.data[1] = p_dev(dev)->holding_reg[0];
        dev->humi.data[0] = p_dev(dev)->holding_reg[3];
        dev->humi.data[1] = p_dev(dev)->holding_reg[2];
    }
}