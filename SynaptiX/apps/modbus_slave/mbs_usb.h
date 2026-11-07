#ifndef MBS_USB_H
#define MBS_USB_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the USB interface for the Modbus slave.
 *
 * This function sets up all necessary configurations and resources
 * required for the Modbus slave to communicate over USB.
 *
 * Call this function before using any USB-related Modbus slave operations.
 */
void mbs_usb_init();

#ifdef __cplusplus
}
#endif

#endif