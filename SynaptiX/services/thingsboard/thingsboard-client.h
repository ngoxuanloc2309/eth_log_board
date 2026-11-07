/*
 * thingsboard-client.h
 *
 *  Created on: Jun 25, 2025
 *      Author: talon
 */

#ifndef APPS_THINGSBOARD_DEVICE_THINGSBOARD_CLIENT_H_
#define APPS_THINGSBOARD_DEVICE_THINGSBOARD_CLIENT_H_

#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_opts.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/altcp_tls.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"
#include "timers.h"
#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"
#include "lwipopts.h"

#if 0
#define ATTRIBUTE_UPDATE_API "v1/devices/me/attributes"
#define ATTRIBUTE_REQUEST_API "v1/devices/me/attributes/response/+"
#define ATTRIBUTE_FW_REQUEST_API "v2/fw/request/%d/chunk/%d" // v2/fw/request/${requestId}/chunk/${chunkIndex}
#define TELEMETRY_API "v1/devices/me/telemetry"
#define RPC_REQUEST_API "v1/devices/me/rpc/request/+"
#endif

#ifndef MQTT_ID
#define MQTT_ID "1234"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#define ATTRIBUTE_UPDATE_API "synaptix/v2/attributes/%s"
#define ATTRIBUTE_REQUEST_API "synaptix/v2/attributes/request/%s/+"
#define ATTRIBUTE_FW_REQUEST_API "synaptix/v2/fw/request/%s/chunk/" // v2/fw/request/${requestId}/chunk/${chunkIndex}
#define ATTRIBUTE_FW_RESPONSE_API "synaptix/v2/fw/response/%s/chunk/"
#define TELEMETRY_API "synaptix/v2/telemetry/%s"

#define TELEMETRY_IO_API "synaptix/v2/telemetry/%s/IO/%d"
#define TELEMETRY_4_20mA_API "synaptix/v2/telemetry/%s/4_20mA/%d"
#define TELEMETRY_THS_API "synaptix/v2/telemetry/%s/THS/%d"

#define RPC_REQUEST_API "synaptix/v2/rpc/request/%s"
#define RPC_RESPONSE_API "synaptix/v2/rpc/response/%s"

#define THINGSBOARD_USER_NAME_SIZE 128
#define THINGSBOARD_PASSWORD_SIZE 128
#define THINGSBOARD_CLIENTID_SIZE 128
#define THINGSBOARD_HOST_SIZE 256
#define TOPIC_DEFAULT_SIZE MQTT_TOPIC_LENGTH
#define PAYLOAD_DEFAULT_SIZE MQTT_PAYLOAD_LENGTH

#ifndef MQTT_PUSHLISH_TIME
#define MQTT_PUSHLISH_TIME 10000
#endif
#ifndef THINGSBOARD_KEEP_ALIVE_S
#define THINGSBOARD_KEEP_ALIVE_S 60
#endif
#ifndef MAX_TIME_WAIT_RECONNECT_mS 
#define MAX_TIME_WAIT_RECONNECT_mS 20000
#endif

#ifndef MQTT_TLS_CA_MAX_BUFF_SIZE 
#define MQTT_TLS_CA_MAX_BUFF_SIZE 2048
#endif

#ifndef MQTT_TLS_CLIENT_CRT_MAX_BUFF_SIZE
#define MQTT_TLS_CLIENT_CRT_MAX_BUFF_SIZE 2048
#endif

#ifndef MQTT_TLS_CLIENT_KEY_MAX_BUFF_SIZE
#define MQTT_TLS_CLIENT_KEY_MAX_BUFF_SIZE 2048
#endif

typedef int(*get_last_telemetry_json)(void);
typedef char* (*update_attribute_t)(void);


typedef struct ThingsBoard{
	mqtt_client_t base;
	struct ThingsBoard_cfg {
		char username[THINGSBOARD_USER_NAME_SIZE];
		char password[THINGSBOARD_PASSWORD_SIZE];
		char clientID[THINGSBOARD_CLIENTID_SIZE];
		char host[THINGSBOARD_HOST_SIZE];
		int port;
		size_t publish_time_S;
		bool tls;
		uint8_t ca_buff[MQTT_TLS_CA_MAX_BUFF_SIZE];
		size_t ca_size;
		uint8_t client_crt_buff[MQTT_TLS_CLIENT_CRT_MAX_BUFF_SIZE];
		size_t client_crt_size;
		uint8_t client_key_buff[MQTT_TLS_CLIENT_KEY_MAX_BUFF_SIZE];
		size_t client_key_size;
	}*configs;
	struct ThingsBoard_Attributes{
		char fw_version[6];
		char hw_version[6];
		char name[64];
		double latitude;
		double longitude;
		double temperature;
	}*attributes;
	struct ThingsboardAPI{
		get_last_telemetry_json get_telemetry;
		update_attribute_t update_attribute;
		int (*rpc_handle)(struct ThingsBoard *client, const char *json_str,const char *topic);
		int (*firmware_handle)(struct ThingsBoard *client, const uint8_t *payload,size_t len,const char *topic);
	} *api;
	struct ThingsBoard_Message{
		char topic[TOPIC_DEFAULT_SIZE];
		char payload[PAYLOAD_DEFAULT_SIZE];
		int topic_len;
		int payload_len;
		int offset;
	}message;
	TaskHandle_t task;
	EventGroupHandle_t events;
	SemaphoreHandle_t pub_mutex;
	uint8_t pub_release;
	ip_addr_t ip;
}ThingsBoard_t;

void ThingsBoard_Init(ThingsBoard_t *client,struct ThingsBoard_cfg *_cfg,struct ThingsboardAPI *_api);

int tb_publish(ThingsBoard_t *client,const char *topic,const char *payload,size_t len_payload,int qos,int retain);
#endif /* APPS_THINGSBOARD_DEVICE_THINGSBOARD_CLIENT_H_ */
