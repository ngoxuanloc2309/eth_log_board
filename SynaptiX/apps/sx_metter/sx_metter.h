#ifndef SX_METTER_H
#define SX_METTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sx_dev.h"
#include "thingsboard-client.h"

#define METTER_PHASE 3

#define METTER_TYPE "/electricity"

typedef struct SX_Metter
{
    /* data */
    SXDev_t dev;
    ThingsBoard_t *net;
    uint16_t *pass;
    uint16_t *slave_id;
    uint16_t *baudrate;
    uint16_t *parity;
    uint16_t *stopbit;
    float *voltageVN[METTER_PHASE];
    float *averageVoltageLN;
    float *voltageV12;
    float *voltageV23;
    float *voltageV31;
    float *averageVoltageLL;
    float *currentI[METTER_PHASE];
    float *currentAverage;
    float *kW[METTER_PHASE];
    float *kVA[METTER_PHASE];
    float *kVAr[METTER_PHASE];
    float *totalkW;
    float *totalkVA;
    float *totalkVAr;
    float *PF[METTER_PHASE];
    float *PFAverage;
    float *frequency;
    float *kWh;
    float *kVAh;
    float *kVArh;
}SX_Metter_t;

/**
 * @brief Initializes the SX_Metter device.
 *
 * This function sets up the SX_Metter device with the provided device structure,
 * Modbus client, ThingsBoard network, and device ID.
 *
 * @param dev Pointer to the SX_Metter_t device structure to initialize.
 * @param mmb Pointer to the MMBCLient_t Modbus client instance.
 * @param net Pointer to the ThingsBoard_t network instance.
 * @param id Unique identifier for the SX_Metter device.
 */
void sx_metter_init(SX_Metter_t *dev,MMBCLient_t *mmb, ThingsBoard_t *net,uint8_t id);
/**
 * @brief Polls the SX_Metter device for new data or status updates.
 *
 * This function should be called periodically to retrieve the latest
 * measurements or status from the specified SX_Metter device.
 *
 * @param dev Pointer to the SX_Metter_t device instance to poll.
 */
void sx_metter_poll(SX_Metter_t *dev);

#ifdef __cplusplus
}
#endif

#endif //SX_METTER_H