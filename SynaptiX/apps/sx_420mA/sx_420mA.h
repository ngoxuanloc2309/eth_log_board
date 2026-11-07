#ifndef SX_420mA_H
#define SX_420mA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mmb_serial.h"
#include <stdbool.h>
#include <thingsboard-client.h>
#include "sx_dev.h"

#define AI_NUM 4
#define RS485_4_20mA_TYPE "/485_4_20"
typedef struct SX_420mA {
    SXDev_t dev;
    ThingsBoard_t *net;
    uint16_t *adc[AI_NUM];
}SX_420mA_t;

/**
 * @brief Initializes the SX_420mA device.
 *
 * This function sets up the SX_420mA device with the provided parameters,
 * including the device structure, MMB client, ThingsBoard network, and device ID.
 *
 * @param dev Pointer to the SX_420mA device structure to initialize.
 * @param mmb Pointer to the MMBCLient structure for communication.
 * @param net Pointer to the ThingsBoard network structure.
 * @param id Unique identifier for the device.
 */
void sx_420mA_init(SX_420mA_t *dev,MMBCLient_t *mmb, ThingsBoard_t *net,uint8_t id);
/**
 * @brief Polls the SX_420mA device for new data or status updates.
 *
 * This function should be called periodically to check the current state
 * of the SX_420mA device and handle any necessary updates or processing.
 *
 * @param dev Pointer to the SX_420mA_t device structure.
 */
void sx_420mA_poll(SX_420mA_t *dev);

#ifdef __cplusplus
}
#endif
#endif // SX_485_IO_H