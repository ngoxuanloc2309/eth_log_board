#ifndef SX_485_IO_H
#define SX_485_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mmb_serial.h"
#include <stdbool.h>
#include <thingsboard-client.h>
#include "sx_dev.h"

#define IO_OUTPUT_NUM 4
#define IO_INPUT_NUM 4

#define RS485_IO_TYPE "/485_IO"

typedef struct SX_485_IO {
    SXDev_t dev;
    ThingsBoard_t *net;
    uint8_t output[IO_OUTPUT_NUM]; //holding reg-address 0-3 , 0-off, 1-on, 2-toggle
    enum InputEvent{
        INPUT_EVENT_RELEASE = 0,
        INPUT_EVENT_PRESS = 1,
        INPUT_EVENT_CLICK = 2
    } *input_event[IO_INPUT_NUM]; //holding reg-address 4-7 0-release, 1-press , 2-click
    uint16_t *counter[IO_INPUT_NUM]; //input reg-address 0-3
    uint8_t reset_couter[IO_INPUT_NUM]; //coil-address 0-4 , 0-no action, 1-reset
    uint8_t input[IO_INPUT_NUM];
    uint8_t last_input[IO_INPUT_NUM];
    uint16_t last_counter[IO_INPUT_NUM];
}SX_485_IO_t;

/**
 * @brief Initializes the SX_485_IO device.
 *
 * This function sets up the SX_485_IO device with the provided parameters,
 * including the device structure, MMB client, ThingsBoard network, and device ID.
 *
 * @param dev Pointer to the SX_485_IO_t device structure to initialize.
 * @param mmb Pointer to the MMBCLient_t instance for communication.
 * @param net Pointer to the ThingsBoard_t network instance.
 * @param id Unique identifier for the device.
 */
void sx_485_io_init(SX_485_IO_t *dev,MMBCLient_t *mmb, ThingsBoard_t *net,uint8_t id);
/**
 * @brief Polls the SX_485_IO device for new data or events.
 *
 * This function checks the specified SX_485_IO device for any incoming data,
 * status changes, or events that require processing. It should be called
 * periodically to ensure timely handling of device communication.
 *
 * @param dev Pointer to the SX_485_IO_t device structure to poll.
 */
void sx_485_io_poll(SX_485_IO_t *dev);

/**
 * @brief Retrieves the input event for a specified input channel of the SX_485_IO device.
 *
 * @param dev Pointer to the SX_485_IO_t device structure.
 * @param input Index of the input channel to query.
 * @return enum InputEvent The event associated with the specified input channel.
 */
enum InputEvent SX485IO_GetInputEV(SX_485_IO_t *dev,int input);
/**
 * @brief Sets the state of a specified output on the SX_485_IO device.
 *
 * This function allows you to turn a specific output on or off for the given SX_485_IO device.
 *
 * @param dev Pointer to the SX_485_IO_t device structure.
 * @param output The index or identifier of the output to be controlled.
 * @param onOff Boolean value indicating the desired state:
 *              - true to turn the output ON
 *              - false to turn the output OFF
 * @return int Status code indicating success or failure of the operation.
 */
int SX485IO_SetOutput(SX_485_IO_t *dev,int output,bool onOff);
/**
 * @brief Sets the state of multiple output channels on the SX_485_IO device.
 *
 * This function updates the output states for a range of outputs starting from
 * 'start_output' and affecting 'num_output' consecutive outputs. The desired
 * states are provided in the 'state' array.
 *
 * @param dev           Pointer to the SX_485_IO device structure.
 * @param start_output  Index of the first output to set.
 * @param num_output    Number of outputs to set starting from 'start_output'.
 * @param state         Array of boolean values representing the desired state for each output.
 *                      The array must have at least 'num_output' elements.
 * @return int          Returns 0 on success, or a negative error code on failure.
 */
int SX485IO_SetOutputArr(SX_485_IO_t *dev,uint8_t start_output, uint8_t num_output, bool *state);
/**
 * @brief Sets the clean input event value for the specified SX_485_IO device.
 *
 * This function updates the clean input event state for a given input channel
 * on the SX_485_IO device.
 *
 * @param dev   Pointer to the SX_485_IO device structure.
 * @param input Input channel or value to set the clean event for.
 * @return      0 on success, negative value on error.
 */
int SX485IO_SetCleanInputEV(SX_485_IO_t *dev,int input);
/**
 * @brief Resets the specified counter in the SX_485_IO device.
 *
 * This function resets the counter identified by counter_num in the given SX_485_IO_t device structure.
 *
 * @param dev Pointer to the SX_485_IO_t device instance.
 * @param counter_num The index of the counter to reset.
 * @return 0 on success, or a negative error code on failure.
 */
int SX485IO_ResetCounter(SX_485_IO_t *dev,int counter_num);
#ifdef __cplusplus
}
#endif
#endif // SX_485_IO_H