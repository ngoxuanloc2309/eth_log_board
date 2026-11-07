#ifndef RF_APP_H
#define RF_APP_H

#ifdef __cplusplus
extern "C" {
#endif
// #include "app_config.h"
#include "app.h"
#if LORA_ENABLE
#include "lora.h"
#endif
#if ZIGBEE_ENABLE
#include "zigbee.h"
#endif
#include <stdbool.h>

/**
 * @brief Initializes the RF application.
 *
 * This function sets up all necessary resources and configurations
 * required for the RF application to operate.
 */
void rf_app_init();
/**
 * @brief Polls the RF application for events or data.
 *
 * This function should be called periodically to handle RF-related tasks,
 * such as processing incoming data, managing state machines, or handling
 * communication events.
 */
void rf_app_poll(uint32_t time_stamp);
/**
 * @brief Checks if the RF application is currently busy.
 *
 * This function returns true if the RF application is engaged in an operation
 * and cannot accept new tasks or commands at the moment.
 *
 * @return true if the RF application is busy, false otherwise.
 */
bool rf_app_busy();
#if LORA_ENABLE
extern LoRaMesh_t lora;
#endif
#if ZIGBEE_ENABLE
extern ZigbeeMesh_t zigbee;
#endif
#ifdef __cplusplus
}
#endif  
#endif // LORA_APP_H