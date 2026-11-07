#include "sx_metter.h"
#include "logger.h"

static const char *TAG = "SX_Metter";

#define p_dev(dev) (&dev->dev)

static SXDevConfig_t dev_config = {
    .coil_reg_num = 0,
    .coil_reg_start_addr = 0,
    .discrete_reg_num = 0,
    .discrete_reg_start_addr = 0,
    .holding_reg_num = 0,
    .holding_reg_start_addr = 0,
    .input_reg_num = 64,
    .input_reg_start_addr = 0
};

void sx_metter_init(SX_Metter_t *dev,MMBCLient_t *mmb, ThingsBoard_t *net,uint8_t id){
    log_info(TAG,"ID : %d",id);
    log_info(TAG,"Modbus Phy : %p",mmb);
    log_info(TAG,"Net Phy : %p",net);
    sx_dev_init(p_dev(dev), mmb,&dev_config ,id);
    dev->net = net;
    dev->voltageVN[0] = (float *)&p_dev(dev)->input_reg[0];
    dev->voltageVN[1] = (float *)&p_dev(dev)->input_reg[2];
    dev->voltageVN[2] = (float *)&p_dev(dev)->input_reg[4];
    dev->averageVoltageLN = (float *)&p_dev(dev)->input_reg[6];
    dev->voltageV12 = (float *)&p_dev(dev)->input_reg[8];
    dev->voltageV23 = (float *)&p_dev(dev)->input_reg[10];
    dev->voltageV31 = (float *)&p_dev(dev)->input_reg[12];
    dev->averageVoltageLL = (float *)&p_dev(dev)->input_reg[14];
    dev->currentI[0] = (float*)&p_dev(dev)->input_reg[16];
    dev->currentI[1] = (float*)&p_dev(dev)->input_reg[18];
    dev->currentI[2] = (float*)&p_dev(dev)->input_reg[20];
    dev->currentAverage = (float*)&p_dev(dev)->input_reg[22];
    dev->kW[0] = (float*)&p_dev(dev)->input_reg[24];
    dev->kW[1] = (float*)&p_dev(dev)->input_reg[26];
    dev->kW[2] = (float*)&p_dev(dev)->input_reg[28];
    dev->kVA[0] = (float*)&p_dev(dev)->input_reg[30];
    dev->kVA[1] = (float*)&p_dev(dev)->input_reg[32];
    dev->kVA[2] = (float*)&p_dev(dev)->input_reg[34];
    dev->kVAr[0] = (float*)&p_dev(dev)->input_reg[36];
    dev->kVAr[1] = (float*)&p_dev(dev)->input_reg[38];
    dev->kVAr[2] = (float*)&p_dev(dev)->input_reg[40];
    dev->totalkW = (float*)&p_dev(dev)->input_reg[42];
    dev->totalkVA = (float*)&p_dev(dev)->input_reg[44];
    dev->totalkVAr = (float*)&p_dev(dev)->input_reg[46];
    dev->PF[0] = (float*)&p_dev(dev)->input_reg[48];
    dev->PF[1] = (float*)&p_dev(dev)->input_reg[50];
    dev->PF[2] = (float*)&p_dev(dev)->input_reg[52];
    dev->PFAverage = (float*)&p_dev(dev)->input_reg[54];
    dev->frequency = (float*)&p_dev(dev)->input_reg[56];
    dev->kWh = (float*)&p_dev(dev)->input_reg[58];
    dev->kVAh = (float*)&p_dev(dev)->input_reg[60];
    dev->kVArh = (float*)&p_dev(dev)->input_reg[62];
    // dev->pass = (uint16_t*) &p_dev(dev)->input_reg[0];
    // dev->slave_id = (uint16_t*) &p_dev(dev)->input_reg[7];
    // dev->parity = (uint16_t*) &p_dev(dev)->input_reg[9];
    // dev->baudrate = (uint16_t*) &p_dev(dev)->input_reg[8];
    // dev->stopbit = (uint16_t*) &p_dev(dev)->input_reg[10];
}
void sx_metter_poll(SX_Metter_t *dev){
    sx_dev_poll(p_dev(dev));
}