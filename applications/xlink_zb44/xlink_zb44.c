#include "xlink_zb44.h"

void Xlink_ZB44_Init(XLink_ZB44_t *dev) {
    if(dev == NULL) return;
    memset(dev->counter,0,sizeof(uint32_t) * XLINK_NUMBER_COUNTER);
    memset(dev->input,0,sizeof(bool) * XLINK_NUMBER_INPUT);
    memset(dev->output,0,sizeof(bool) * XLINK_NUMBER_OUTPUT);
    dev->isConnect = false;
    dev->timeout = 0;
}

void Xlink_ZB44_Poll(XLink_ZB44_t *dev, uint32_t elapsed_ms) {
  if (dev == NULL)
    return;

  dev->timeout += elapsed_ms;

  if (dev->timeout >= XLINK_DEV_TIMEOUT_CONNECT_mS) {
    dev->isConnect = false;
  }
}