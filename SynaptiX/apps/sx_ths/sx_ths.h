#ifndef SX_THS_H
#define SX_THS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sx_dev.h"
#include "thingsboard-client.h"


#define THS_TYPE "/THS"
typedef struct SX_THS
{
    /* data */
    SXDev_t dev;
    ThingsBoard_t *net;
    union temp_t{
        float temp;
        uint16_t data[2];
    }temp;
    union humi_t{
        float humi;
        uint16_t data[2];
    }humi;
}SX_THS_t;

void sx_ths_init(SX_THS_t *dev,MMBCLient_t *mmb, ThingsBoard_t *net,uint8_t id);
void sx_ths_poll(SX_THS_t *dev);
#ifdef __cplusplus
}
#endif

#endif // SX_THS_H