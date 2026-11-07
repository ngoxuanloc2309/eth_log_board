#ifndef MQTT_APP_H
#define MQTT_APP_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the MQTT application.
 *
 * This function sets up the necessary resources and configurations
 * required for the MQTT application to operate.
 */
void mqtt_app_init();

int get_num_dev_online();

#ifdef __cplusplus
}
#endif
#endif // MQTT_APP_H