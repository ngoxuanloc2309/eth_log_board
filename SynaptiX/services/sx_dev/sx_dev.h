#ifndef SX_DEV_H
#define SX_DEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nanomodbus.h"
#include "mmb_serial.h"
#include <stdbool.h>

#define HOLDING_REG_NUM 10
#define INPUT_REG_NUM 10
#define COIL_REG_NUM 8
#define DISCRETE_REG_NUM 8
#define TIMEOUT_DISCONNECT_mS 5000
typedef struct SXDevConfig{
    uint16_t holding_reg_start_addr;
    uint16_t holding_reg_num;
    uint16_t input_reg_start_addr;
    uint16_t input_reg_num;
    uint16_t coil_reg_start_addr;
    uint16_t coil_reg_num;
    uint16_t discrete_reg_start_addr;
    uint16_t discrete_reg_num;
}SXDevConfig_t;

typedef struct SXDev
{
    /* data */
    bool isConnect;
    uint8_t id;
    uint32_t timeout_disconnect;
    uint16_t *holding_reg;
    uint16_t *input_reg;
    uint8_t *coil_reg;
    uint8_t *discrete_reg;
    SXDevConfig_t *config;
    MMBCLient_t *mmb;
    
}SXDev_t;

void sx_dev_init(SXDev_t *dev,MMBCLient_t *mmb,SXDevConfig_t *config,uint8_t id);
void sx_dev_poll(SXDev_t *dev);
#ifdef __cplusplus
}
#endif
#endif
