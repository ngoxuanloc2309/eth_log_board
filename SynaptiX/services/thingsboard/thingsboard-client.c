/*
 * thingsboard-client.c
 *
 *  Created on: Jun 25, 2025
 *      Author: talon
 */

#include "thingsboard-client.h"
#include "logger.h"
#include "string.h"
#include "dns.h"
#include "app_settings.h"

#define THINGSBOARD_STACK_SIZE (1024*12)
#define THINGSBOARD_PRIORITY (configMAX_PRIORITIES - 1)
#define MQTT_PUBLISH_SUCCESS_BIT 1 << 0
#define MQTT_PUBLISH_FAIL_BIT 1 << 1
#define MQTT_CONNECTED_BIT 1 << 2
#define MQTT_CONNECTION_FAIL_BIT 1<<3
#define p_client(x) ((mqtt_client_t *)(x))
#define DNS_TIMEOUT_mS 5000

#define tb_pub_lock(x) xSemaphoreTake(x, portMAX_DELAY)
#define tb_pub_unlock(x) xSemaphoreGive(x)
static const char *TAG = "Things Board";
static void ThingsBoardProcess(void *arg);
static err_t mqtt_do_connect(ThingsBoard_t *client);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
							   mqtt_connection_status_t status);
static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic,
									 u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
								  u8_t flags);
static void mqtt_pub_request_cb(void *arg, err_t result);
static void mqtt_subscribe_handle(ThingsBoard_t *client);
static void mqtt_reconnect_task(void *arg);
void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg)
{
	LOGD(TAG, "Host %s have ipv4 : %s", name, ip_ntoa(ipaddr));
	ThingsBoard_t *client = (ThingsBoard_t *)arg;
	client->ip.addr = ipaddr->addr;
}

void ThingsBoard_Init(ThingsBoard_t *client, struct ThingsBoard_cfg *_cfg, struct ThingsboardAPI *_api)
{
	client->configs = _cfg;
	memset(client->message.topic, 0, TOPIC_DEFAULT_SIZE);
	memset(client->message.payload, 0, PAYLOAD_DEFAULT_SIZE);
	client->message.payload_len = 0;
	client->message.topic_len = 0;
	client->api = _api;
	client->pub_mutex = xSemaphoreCreateMutex();
	if(_cfg->publish_time_S == 0)
		client->configs->publish_time_S = MQTT_PUSHLISH_TIME;
	else{
		client->configs->publish_time_S = _cfg->publish_time_S;
	}
	if(pdPASS != xTaskCreate(ThingsBoardProcess, "things-board", THINGSBOARD_STACK_SIZE,
				client, THINGSBOARD_PRIORITY, (TaskHandle_t *)&client->task)){
					LOGE(TAG,"Can't create mqtt process task");
				}
}

int tb_publish(ThingsBoard_t *client, const char *topic, const char *payload, size_t len_payload, int qos, int retain)
{
	if (client == NULL || !mqtt_client_is_connected(p_client(client)))
		return -1;
	tb_pub_lock(client->pub_mutex);
	LOGD(TAG,"Publish topic : %s",topic);
	LOGD(TAG,"Payload : %s",payload);
	err_t err = mqtt_publish(&client->base, topic, payload, len_payload, qos, retain, mqtt_pub_request_cb, client);
	EventBits_t events = MQTT_PUBLISH_FAIL_BIT;
	events = xEventGroupWaitBits(client->events,MQTT_PUBLISH_SUCCESS_BIT | MQTT_PUBLISH_FAIL_BIT,pdTRUE,pdFAIL,pdMS_TO_TICKS(10000));
	if(events & MQTT_PUBLISH_SUCCESS_BIT){
		LOGD(TAG,"Publish success");
	}
	else{
		LOGE(TAG,"Publish false");
	}
	tb_pub_unlock(client->pub_mutex);
	return err;
}

const char *payload_static =
	"{\"temperature\":30,\"humidity\" : 50, \"lat\" : 20.129039 , \"lon\" : 32.80989,\"time\": 21093821934,\"status\" : 1,\"error\" : 0}";

struct mqtt_connect_client_info_t ci;

static void ThingsBoardProcess(void *arg)
{
	ThingsBoard_t *client = (ThingsBoard_t *)arg;
	struct ThingsBoard_cfg *_cfg = client->configs;
	LOGD(TAG, "Things Board Initialize");
	EventBits_t mqtt_publish_event;
	client->events = xEventGroupCreate();
	LOGD(TAG, "Connecting to %s , port %d", client->configs->host, client->configs->port);
	client->ip.addr = 0;
	err_t err = ERR_OK;
	do
	{
		err = dns_gethostbyname(client->configs->host, &client->ip,
								dns_found_cb, client);
		vTaskDelay(pdMS_TO_TICKS(DNS_TIMEOUT_mS));
	} while (err != ERR_OK);

	LOGD(TAG, "dns_gethostbyname return %d", err);
	while (client->ip.addr == 0)
	{
		vTaskDelay(1);
	}
	
	uint32_t reconnect_times = 1000;
	
	memset(&ci, 0, sizeof(ci));
	memset(&client->base, 0, sizeof(mqtt_client_t));
	ci.client_id = client->configs->clientID;
	ci.client_user = client->configs->username;
	ci.client_pass = client->configs->password;
	ci.keep_alive = THINGSBOARD_KEEP_ALIVE_S;
	if (_cfg->tls == true)
	{
		ci.tls_config = altcp_tls_create_config_client_2wayauth(_cfg->ca_buff, _cfg->ca_size, _cfg->client_key_buff, _cfg->client_key_size,
																NULL, 0,
																_cfg->client_crt_buff, _cfg->client_crt_size);
	}
	else{
		ci.tls_config = NULL;
	}
	int num_connection = 0;
connection : 
	mqtt_do_connect(client);
	if (reconnect_times < MAX_TIME_WAIT_RECONNECT_mS){
		reconnect_times *= 2;
		num_connection++;
		if(num_connection >= 10){
			HAL_NVIC_SystemReset();
		}
	}
	mqtt_publish_event = MQTT_CONNECTION_FAIL_BIT;
	mqtt_publish_event = xEventGroupWaitBits(client->events,
											 MQTT_PUBLISH_SUCCESS_BIT | MQTT_PUBLISH_FAIL_BIT | MQTT_CONNECTED_BIT | MQTT_CONNECTION_FAIL_BIT, pdFALSE, pdFALSE,
											 pdMS_TO_TICKS(reconnect_times));
	if (mqtt_publish_event & MQTT_CONNECTED_BIT)
	{
		xEventGroupClearBits(client->events, mqtt_publish_event);
		reconnect_times = 1000;
		if (client->api->update_attribute != NULL)
		{
			char *payload = client->api->update_attribute();
			char topic[128];
			sprintf(topic,ATTRIBUTE_UPDATE_API,app_setting.setting.name);
			if (0 != tb_publish(client, topic, payload, strlen(payload), 1, 0))
			{
				LOGE(TAG, "Can't publish attribute topic");
			}
		}
	}
	if(mqtt_publish_event & MQTT_CONNECTION_FAIL_BIT){
		xEventGroupClearBits(client->events, mqtt_publish_event);
		LOGE(TAG,"Can't connecting to %s",client->configs->host);
		vTaskDelay(reconnect_times);
		goto connection;
	}
	while (1)
	{
		if (mqtt_client_is_connected(p_client(client)))
		{
			mqtt_publish_event = xEventGroupWaitBits(client->events,
													  MQTT_PUBLISH_FAIL_BIT , pdFALSE, pdFALSE,
													 1);
			if (mqtt_publish_event & MQTT_PUBLISH_FAIL_BIT)
			{
				xEventGroupClearBits(client->events, mqtt_publish_event);
				LOGE(TAG, "Disconnect form host %s", client->configs->host);
				mqtt_disconnect(p_client(client));
				vTaskDelay(pdMS_TO_TICKS(2000));
				num_connection = 0;
				goto connection;
			}
			if (client->api->get_telemetry != NULL)
			{
				int res = client->api->get_telemetry();
				if (res != 0)
				{
					LOGE(TAG, "Publish false");
					mqtt_disconnect(p_client(client));
					vTaskDelay(pdMS_TO_TICKS(2000));
					num_connection = 0;
					goto connection;
				}
			}
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		else{
			mqtt_disconnect(p_client(client));
			vTaskDelay(pdMS_TO_TICKS(2000));
			num_connection = 0;
			goto connection;
		}
			
	}

	// DELETE_TASK:
	//	vTaskDelete((TaskHandle_t)client->task);
}

static err_t mqtt_do_connect(ThingsBoard_t *client)
{
	
	err_t err;
	LOGD(TAG, "Connect to host : %s", ip_ntoa(&client->ip));


	err = mqtt_client_connect(&client->base, &client->ip,
							  client->configs->port, mqtt_connection_cb, client, &ci);
	if (err != ERR_OK)
	{
		LOGE(TAG, "mqtt_connect return %d", err);
		mqtt_disconnect(&client->base);
		xEventGroupSetBits(client->events, MQTT_CONNECTION_FAIL_BIT);
	}
	return err;
}
static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
							   mqtt_connection_status_t status)
{
	ThingsBoard_t *thingsboard = (ThingsBoard_t *)arg;
	if (status == MQTT_CONNECT_ACCEPTED)
	{
		LOGD(TAG, "mqtt_connection_cb: Successfully connected");

		char topic[128];
		sprintf(topic,RPC_REQUEST_API,app_setting.setting.name);

		mqtt_subscribe(client, topic, 1,
					   mqtt_sub_request_cb, arg);

		sprintf(topic,ATTRIBUTE_REQUEST_API,app_setting.setting.name);			   
		mqtt_subscribe(client, topic, 1,
					   mqtt_sub_request_cb, arg);
		// mqtt_subscribe(client,ATTRIBUTE_UPDATE_API,1,mqtt_sub_request_cb,arg);
		sprintf(topic,ATTRIBUTE_FW_REQUEST_API"+",app_setting.setting.name);
		mqtt_subscribe(client, topic, 1,
					   mqtt_sub_request_cb, arg);
		/* Setup callback for incoming publish requests */
		mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb,
								mqtt_incoming_data_cb, thingsboard);
		xEventGroupSetBits(thingsboard->events, MQTT_CONNECTED_BIT);
	}
	else
	{
		LOGE(TAG, "mqtt_connection_cb: Disconnected, reason: %d",
				  status);
		/* Its more nice to be connected, so try to reconnect */
		// xTaskCreate(mqtt_reconnect_task, "mqtt reconnect", 4096, thingsboard, 10, NULL);
		xEventGroupSetBits(thingsboard->events, MQTT_CONNECTION_FAIL_BIT);
	}
}
static void mqtt_sub_request_cb(void *arg, err_t result)
{
	/* Just print the result code here for simplicity,
	 normal behaviour would be to take some action if subscribe fails like
	 notifying user, retry subscribe or disconnect from server */
	LOGD(TAG, "Subscribe result: %d", result);
}
static void mqtt_incoming_publish_cb(void *arg, const char *topic,
									 u32_t tot_len)
{

	ThingsBoard_t *client = (ThingsBoard_t *)arg;
	log_debug(TAG,"Incomming topic : %s, len : %lu",topic,tot_len);
	memset(client->message.topic, 0, TOPIC_DEFAULT_SIZE);
	memset(client->message.payload, 0, PAYLOAD_DEFAULT_SIZE);
	client->message.offset = 0;
	client->message.payload_len = tot_len;
	strcpy(client->message.topic,topic);
	client->message.topic_len = strlen(topic);
}
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
								  u8_t flags)
{

	ThingsBoard_t *client = (ThingsBoard_t *)arg;
	log_debug(TAG,"Incomming data : ");
	log_print_hex(LOGGER_DEBUG,TAG,data,len);
	memcpy(client->message.payload + client->message.offset, data, len);
	client->message.offset += len;
	if(flags & MQTT_DATA_FLAG_LAST)
	{
		log_debug(TAG,"Incomming data complete");
		client->message.payload_len = client->message.offset;
		mqtt_subscribe_handle(client);
	}
}
static void mqtt_pub_request_cb(void *arg, err_t result)
{
	ThingsBoard_t *client = (ThingsBoard_t *)arg;
	client->pub_release = 1;
	if (result == 0)
	{
		LOGD(TAG, "Publish success %d", result);

		xEventGroupSetBits(client->events, MQTT_PUBLISH_SUCCESS_BIT);
		return;
	}
	LOGE(TAG, "Publish fail with result: %d\r\n", result);
	xEventGroupSetBits(client->events, MQTT_PUBLISH_FAIL_BIT);
}
static void mqtt_subscribe_handle(ThingsBoard_t *client)
{
	log_debug(TAG, "Incoming publish at topic %s with total length %u",
			 client->message.topic, client->message.topic_len);
	log_debug(TAG, "Incoming publish payload \"%s\" with length %d",
			 client->message.payload, client->message.payload_len);
	log_debug(TAG, "Process RPC");
	char topic[256];
	sprintf(topic,RPC_REQUEST_API,app_setting.setting.name);
	if (strstr(client->message.topic, topic)!=NULL)
	{
		if (client->api->rpc_handle != NULL)
		{
			client->api->rpc_handle(client, client->message.payload, client->message.topic);
		}
		else
		{
			LOGE(TAG, "RPCHandle NULL");
		}
		return;
	}
	sprintf(topic,ATTRIBUTE_FW_REQUEST_API,app_setting.setting.name);
	if(strstr(client->message.topic, topic) !=NULL){
		if(client->api->firmware_handle != NULL){
			client->api->firmware_handle(client,client->message.payload,client->message.payload_len,client->message.topic);
		}
		return;
	}

}
static void mqtt_reconnect_task(void *arg)
{
	ThingsBoard_t *thingsboard = (ThingsBoard_t *)arg;
	while (1)
	{
		err_t err = mqtt_do_connect(thingsboard);
		if (err == ERR_OK)
			break;
		vTaskDelay(10000);
	}
	vTaskDelete(NULL);
}
