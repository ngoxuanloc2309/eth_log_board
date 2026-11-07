#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdint.h>
#include "board.h"
#include "logger.h"
#include "mongoose_glue.h"
#include "app_config.h"

#ifndef APP_SETTING_ADDRESS
#define APP_SETTING_ADDRESS 0x080FC000
#endif
#ifndef APP_SETTING_SECTOR
#define APP_SETTING_SECTOR  126
#endif
#define MG_NUMBER           0x0812FC21
typedef struct Serial
{
    /* data */
    uint32_t baudrate;
    enum Databits
    {
        DATA8 = SERIAL_WORDLENGTH_8B,
        DATA9 = SERIAL_WORDLENGTH_9B
    } databits;
    enum Stopbits{
        STOPBITS_1 = SERIAL_STOPBITS_1,
        STOPBITS_2 = SERIAL_STOPBITS_2
    }stopbits;
    enum Parity{
        PARITY_NONE = SERIAL_PARITY_NONE,
        PARITY_ODD = SERIAL_PARITY_ODD,
        PARITY_EVEN = SERIAL_PARITY_EVEN
    } parity;
} Serial_t;

typedef enum{
    Parency_Transfer = 0,
    Parency_Modbus
}APP_MODE_t;

typedef struct APP_Settings
{
    /* data */
    Serial_t rs485;
    Serial_t rf;
    Serial_t com;
    LOGGING_LEVELS log_level;
    APP_MODE_t app_mode;
    uint8_t num_dev_io;
    struct settings setting;
    struct security secure;
    struct network_settings network;
    struct wifi wifi;
    struct mqtt mqtt_setting;
    char ca_crt_path[128];
    char client_crt_path[128];
    char client_key_path[128];
    struct rs485 rs485_setting;
    uint32_t mg_number;
} APP_Settings_t;

/**
 * @brief Loads application settings into the provided APP_Settings_t structure.
 *
 * This function initializes or updates the given APP_Settings_t structure
 * with the current application settings, typically from non-volatile storage
 * or default values.
 *
 * @param[in,out] setting Pointer to an APP_Settings_t structure where the loaded
 *                        settings will be stored.
 */
void app_setting_load(APP_Settings_t *setting);
/**
 * @brief Saves the provided application settings.
 *
 * This function stores the given APP_Settings_t structure to non-volatile memory
 * or another persistent storage medium, ensuring that the application settings
 * are retained across resets or power cycles.
 *
 * @param setting Pointer to the APP_Settings_t structure containing the settings to save.
 */
void app_setting_save(APP_Settings_t *setting);
/**
 * @brief Prints the contents of the given APP_Settings_t structure.
 *
 * This function outputs the current settings stored in the provided
 * APP_Settings_t pointer, typically for debugging or logging purposes.
 *
 * @param setting Pointer to an APP_Settings_t structure containing the settings to print.
 */
void app_setting_printf(APP_Settings_t *setting);
/**
 * @brief Configures the serial interface with the specified settings.
 *
 * This function sets up the serial interface identified by the given serial number
 * using the provided configuration parameters.
 *
 * @param serial The identifier or index of the serial interface to configure.
 * @param config Pointer to a Serial_t structure containing the desired serial configuration.
 */
void app_setup_serial(int serial,Serial_t *config);
/**
 * @brief Initializes the given APP_Settings_t structure with default values.
 *
 * This function sets all fields of the provided settings structure to their
 * default configuration values. It should be called to ensure the settings
 * are properly initialized before use.
 *
 * @param setting Pointer to an APP_Settings_t structure to be initialized.
 */
void app_setting_default(APP_Settings_t *setting);
extern APP_Settings_t app_setting;

#endif // APP_SETTING_H