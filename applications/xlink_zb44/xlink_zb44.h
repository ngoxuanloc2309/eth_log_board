#ifndef __XLINK_ZB44_H__
#define __XLINK_ZB44_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define XLINK_NUMBER_INPUT 4
#define XLINK_NUMBER_OUTPUT 4
#define XLINK_NUMBER_COUNTER 4
#define XLINK_DEV_TIMEOUT_CONNECT_mS 10000
typedef struct XLink_ZB44
{
    /* data */
    bool input[XLINK_NUMBER_INPUT];
    bool output[XLINK_NUMBER_OUTPUT];
    uint32_t counter[XLINK_NUMBER_COUNTER];
    uint8_t id;
    bool isConnect;
    uint32_t timeout;
}XLink_ZB44_t;

void Xlink_ZB44_Init(XLink_ZB44_t *dev);
void Xlink_ZB44_Poll(XLink_ZB44_t *dev,uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif //__XLINK_ZB44_H__