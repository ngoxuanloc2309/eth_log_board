#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_MQTT_HOST_MAX_LENGTH (128U)
#define SETTINGS_MQTT_CLIENT_ID_MAX_LENGTH (64U)
#define SETTINGS_UART_COUNT (3U)

typedef enum
{
    SETTINGS_UART7 = 0,
    SETTINGS_USART3,
    SETTINGS_USART10
} settings_uart_port_t;

typedef struct
{
    uint32_t baud_rate;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;
} settings_uart_config_t;

typedef struct
{
    bool dhcp_enabled;
    uint8_t address[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
} settings_ethernet_config_t;

typedef struct
{
    bool enabled;
    bool tls_enabled;
    char host[SETTINGS_MQTT_HOST_MAX_LENGTH];
    uint16_t port;
    char client_id[SETTINGS_MQTT_CLIENT_ID_MAX_LENGTH];
    uint16_t keep_alive_seconds;
} settings_mqtt_config_t;

typedef struct
{
    settings_ethernet_config_t ethernet;
    settings_mqtt_config_t mqtt;
    settings_uart_config_t uart[SETTINGS_UART_COUNT];
} settings_config_t;

typedef enum
{
    SETTINGS_RESULT_OK = 0,
    SETTINGS_RESULT_INVALID,
    SETTINGS_RESULT_BUSY,
    SETTINGS_RESULT_STORAGE_ERROR,
    SETTINGS_RESULT_NOT_INITIALIZED
} settings_result_t;

typedef enum
{
    SETTINGS_SAVE_IDLE = 0,
    SETTINGS_SAVE_PENDING,
    SETTINGS_SAVE_SUCCEEDED,
    SETTINGS_SAVE_FAILED
} settings_save_state_t;

/**
 * @brief Load persistent settings or initialize firmware defaults.
 *
 * This function must be called from task context after the filesystem has
 * been mounted and before settings are consumed by other subsystems.
 *
 * @return SETTINGS_RESULT_OK when initialized; storage errors are reported
 *         while leaving safe defaults available.
 */
settings_result_t settings_init(void);

/**
 * @brief Get a consistent snapshot of the current non-secret settings.
 *
 * @param config Destination for the settings snapshot.
 * @return true when settings have been initialized and config is non-null.
 */
bool settings_get_config(settings_config_t *config);

/**
 * @brief Validate and queue non-secret settings for persistent saving.
 *
 * Values are staged for application after reboot. MQTT credentials and TLS
 * trust material are deliberately not part of this structure.
 *
 * @param config Complete configuration to validate and save.
 * @return SETTINGS_RESULT_OK when queued; otherwise an explicit error.
 */
settings_result_t settings_request_save(const settings_config_t *config);

/**
 * @brief Get the latest settings save operation state.
 *
 * @param state Destination for the state.
 * @return true when settings have been initialized and state is non-null.
 */
bool settings_get_save_state(settings_save_state_t *state);

/**
 * @brief Mark the pending static Ethernet configuration as reachable.
 *
 * This cancels the startup DHCP rollback for the current boot only.
 */
void settings_confirm_network(void);

/**
 * @brief Query whether the current static Ethernet configuration was confirmed.
 *
 * @return true after settings_confirm_network() is called.
 */
bool settings_network_is_confirmed(void);

/**
 * @brief Validate a complete non-secret settings configuration.
 *
 * @param config Configuration to validate.
 * @return true when every field is valid.
 */
bool settings_validate(const settings_config_t *config);

#ifdef __cplusplus
}
#endif

#endif