#include "app_settings.h"
#include "logger.h"
#include "usart.h"

static const char *TAG = "Settings";

void app_setting_load(APP_Settings_t *setting)
{
    bsp_flash_read(APP_SETTING_ADDRESS, setting, sizeof(APP_Settings_t));
    if (setting->mg_number != MG_NUMBER)
    {
        log_error(TAG, "Setting area error, overwrite setting default");

        app_setting_default(setting);

        app_setting_save(setting);
    }
    app_setting_printf(setting);
}

void app_setting_save(APP_Settings_t *setting)
{
    bsp_flash_unlock();
    bsp_flash_erase(APP_SETTING_ADDRESS, 1);
    bsp_flash_program(APP_SETTING_ADDRESS, (uint8_t *)setting, sizeof(APP_Settings_t));
    bsp_flash_lock();
    log_info(TAG, "Save success");
}

void app_setting_printf(APP_Settings_t *setting)
{
    log_info(TAG, "RS485 :");
    log_info(TAG, "Baudrate : %lu", setting->rs485.baudrate);
    log_info(TAG, "Databits : %d", setting->rs485.databits == DATA8 ? 8 : 9);
    log_info(TAG, "Stopbits : %d", setting->rs485.stopbits == STOPBITS_1 ? 1 : 2);
    switch (setting->rs485.parity)
    {
    case PARITY_NONE:
        /* code */
        log_info(TAG, "Parity   : None");
        break;
    case PARITY_EVEN:
        log_info(TAG, "Parity   : Even");
        break;
    case PARITY_ODD:
        log_info(TAG, "Parity   : Odd");
        break;
    default:
        break;
    }

    log_info(TAG, "RF :");
    log_info(TAG, "Baudrate : %lu", setting->rf.baudrate);
    log_info(TAG, "Databits : %d", setting->rf.databits == DATA8 ? 8 : 9);
    log_info(TAG, "Stopbits : %d", setting->rf.stopbits == STOPBITS_1 ? 1 : 2);
    switch (setting->rf.parity)
    {
    case PARITY_NONE:
        /* code */
        log_info(TAG, "Parity   : None");
        break;
    case PARITY_EVEN:
        log_info(TAG, "Parity   : Even");
        break;
    case PARITY_ODD:
        log_info(TAG, "Parity   : Odd");
        break;
    default:
        break;
    }

    log_info(TAG, "USB :");
    log_info(TAG, "Baudrate : %lu", setting->com.baudrate);
    log_info(TAG, "Databits : %d", setting->com.databits == DATA8 ? 8 : 9);
    log_info(TAG, "Stopbits : %d", setting->com.stopbits == STOPBITS_1 ? 1 : 2);
    switch (setting->com.parity)
    {
    case PARITY_NONE:
        /* code */
        log_info(TAG, "Parity   : None");
        break;
    case PARITY_EVEN:
        log_info(TAG, "Parity   : Even");
        break;
    case PARITY_ODD:
        log_info(TAG, "Parity   : Odd");
        break;
    default:
        break;
    }

    log_info(TAG, "Mode : %d", setting->app_mode);

    log_info(TAG, "Logger level : %s", setting->setting.log_level);
    log_info(TAG, "Name : %s", setting->setting.name);
    log_info(TAG, "Number of device IO : %d", setting->setting.dev_io);
    log_info(TAG, "Start device io address : ", setting->setting.start_io_addr);

    log_info(TAG, "DHCP : %s", setting->network.dhcp == true ? "true" : "false");
    log_info(TAG, "IP address : %s", setting->network.ip_address);
    log_info(TAG, "Subnet mask : %s", setting->network.netmask);
    log_info(TAG, "Default gateway : %s", setting->network.gw_address);
    log_info(TAG, "MAC : %s", setting->network.MAC);

    log_info(TAG, "MQTT host : %s", setting->mqtt_setting.host);
    log_info(TAG, "MQTT port : %d", setting->mqtt_setting.port);
    log_info(TAG, "MQTT ID : %s", setting->mqtt_setting.cli_id);
    log_info(TAG, "MQTT user name : %s", setting->mqtt_setting.user_name);
    log_info(TAG, "MQTT password : %s", setting->mqtt_setting.password);
    log_info(TAG, "MQTT Publish time (s) : %d", setting->mqtt_setting.publish_time);
    log_info(TAG, "MQTT TLS : %s", setting->mqtt_setting.tls == true ? "true" : "false");
}
extern UART_HandleTypeDef *hal_uart[BSP_UART_PORT_NUM];
void app_setup_serial(int serial, Serial_t *config)
{
    if (serial < 0 && serial > BSP_UART_PORT_NUM)
        return;

    hal_uart[serial]->Instance = USART1;
    hal_uart[serial]->Init.BaudRate = config->baudrate;
    hal_uart[serial]->Init.WordLength = config->databits;
    hal_uart[serial]->Init.StopBits = config->stopbits;
    hal_uart[serial]->Init.Parity = config->parity;
    hal_uart[serial]->Init.Mode = UART_MODE_TX_RX;
    hal_uart[serial]->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    hal_uart[serial]->Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(hal_uart[serial]) != HAL_OK)
    {
        Error_Handler();
    }
}

void app_setting_default(APP_Settings_t *setting)
{
    setting->com.baudrate = 115200;
    setting->com.databits = DATA8;
    setting->com.parity = PARITY_NONE;
    setting->com.stopbits = STOPBITS_1;

    setting->rf.baudrate = 115200;
    setting->rf.databits = DATA8;
    setting->rf.parity = PARITY_NONE;
    setting->rf.stopbits = STOPBITS_1;

    setting->rs485.baudrate = 115200;
    setting->rs485.databits = DATA8;
    setting->rs485.parity = PARITY_NONE;
    setting->rs485.stopbits = STOPBITS_1;
    setting->log_level = LOGGER_DEBUG;
    setting->app_mode = Parency_Transfer;
    setting->mg_number = MG_NUMBER;
    setting->num_dev_io = 10;
    setting->setting.dev_ai = 0;
    setting->setting.dev_io = 10;
    strcpy(setting->setting.log_level, "debug");
    strcpy(setting->setting.name, BOARD_NAME);
    setting->setting.start_io_addr = 1;
    setting->setting.start_ai_addr = setting->setting.start_io_addr + setting->setting.dev_ai;

    strcpy(setting->secure.admin_password, "admin");
    strcpy(setting->secure.user_password, "user");

    strcpy(setting->network.ip_address, "192.168.1.2");
    strcpy(setting->network.netmask, "255.255.255.0");
    strcpy(setting->network.gw_address, "192.168.1.1");
    strcpy(setting->network.MAC,"00:08:E1:00:01:02");
    setting->network.dhcp = true;

    strcpy(setting->wifi.ip_addr, "192.168.1.2");
    strcpy(setting->wifi.mask, "255.255.255.0");
    strcpy(setting->wifi.gw, "192.168.1.1");
    strcpy(setting->wifi.mac,"00:08:E1:00:01:02");
    strcpy(setting->wifi.ssid,"HVC_wifi6");
    strcpy(setting->wifi.pass,"havicom2025@");
    setting->wifi.dhcp = true;

    strcpy(setting->mqtt_setting.cli_id, MQTT_CLIENT_ID);
    strcpy(setting->mqtt_setting.password, MQTT_CLIENT_PASS);
    strcpy(setting->mqtt_setting.user_name, MQTT_CLIENT_USER);
    strcpy(setting->mqtt_setting.host, MQTT_HOST);
    setting->mqtt_setting.port = USER_MQTT_PORT;
    setting->mqtt_setting.publish_time = 5;
    setting->mqtt_setting.tls = false;
    memset(setting->ca_crt_path,0,128);
    memset(setting->client_crt_path,0,128);
    memset(setting->client_key_path,0,128);
    strcpy(setting->ca_crt_path,"ca.crt");
    strcpy(setting->client_crt_path,"client.crt");
    strcpy(setting->client_key_path,"client.key");
    setting->rs485_setting.enable = true;
    setting->rs485_setting.baud1 = 115200;
    setting->rs485_setting.baud2 = 115200;
    setting->rs485_setting.baud3 = 115200;
    setting->rs485_setting.port1 = 601;
    setting->rs485_setting.port2 = 602;
    setting->rs485_setting.port3 = 603;
}

APP_Settings_t app_setting;