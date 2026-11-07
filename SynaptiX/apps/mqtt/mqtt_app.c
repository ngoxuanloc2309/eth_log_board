#include "mqtt_app.h"
#include "app_config.h"
#include "thingsboard-client.h"
#include "cJSON.h"
#include "app.h"
#include "logger.h"
#include "function.h"
#include "app_settings.h"
#include "file_io.h"
#include "ota.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "str2hex.h"
#define PAYLOAD_BUFF_SIZE 4096

#if 1
// extern SX_485_IO_t sx485io[NUM_SX485IO_DEV];
// extern SX_420mA_t sx420mA[NUM_SX420mA_DEV];
extern SX_Metter_t sxmetter[NUM_SXMETTER_DEV];
extern SX_THS_t sx_ths[NUM_SXTHS_DEV];
#endif

static const char *TAG = "MQTT APP";

struct ThingsBoard_cfg thingsboard_cfg;
struct ThingsboardAPI thingsboard_api;

static char payload[PAYLOAD_BUFF_SIZE];
#if NUM_SXMETTER_DEV > 0
static char *createMetterPayload()
{
	cJSON *root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "timestamp", sntp_get_timestamp());
	cJSON *dev_arr = cJSON_CreateArray();
	cJSON *dev = cJSON_CreateObject();
	cJSON_AddBoolToObject(dev, "isConnect", sxmetter[0].dev.isConnect);
	cJSON_AddNumberToObject(dev, "id", sxmetter[0].dev.id);
	cJSON_AddStringToObject(dev, "type", "metter");
	if (sxmetter[0].dev.isConnect)
	{
		cJSON_AddNumberToObject(dev, "V1N", *sxmetter[0].voltageVN[0]);
		cJSON_AddNumberToObject(dev, "V2N", *sxmetter[0].voltageVN[1]);
		cJSON_AddNumberToObject(dev, "V3N", *sxmetter[0].voltageVN[2]);
		cJSON_AddNumberToObject(dev, "AVLN", *sxmetter[0].averageVoltageLN);
		cJSON_AddNumberToObject(dev, "V12", *sxmetter[0].voltageV12);
		cJSON_AddNumberToObject(dev, "V23", *sxmetter[0].voltageV23);
		cJSON_AddNumberToObject(dev, "V31", *sxmetter[0].voltageV31);
		cJSON_AddNumberToObject(dev, "AVLL", *sxmetter[0].averageVoltageLL);
		cJSON_AddNumberToObject(dev, "I1", *sxmetter[0].currentI[0]);
		cJSON_AddNumberToObject(dev, "I2", *sxmetter[0].currentI[1]);
		cJSON_AddNumberToObject(dev, "I3", *sxmetter[0].currentI[2]);
		cJSON_AddNumberToObject(dev, "AI", *sxmetter[0].currentAverage);
		cJSON_AddNumberToObject(dev, "kW1", *sxmetter[0].kW[0]);
		cJSON_AddNumberToObject(dev, "kW2", *sxmetter[0].kW[1]);
		cJSON_AddNumberToObject(dev, "kW3", *sxmetter[0].kW[2]);
		cJSON_AddNumberToObject(dev, "kVA1", *sxmetter[0].kVA[0]);
		cJSON_AddNumberToObject(dev, "kVA2", *sxmetter[0].kVA[1]);
		cJSON_AddNumberToObject(dev, "kVA3", *sxmetter[0].kVA[2]);
		cJSON_AddNumberToObject(dev, "kVAr1", *sxmetter[0].kVAr[0]);
		cJSON_AddNumberToObject(dev, "kVAr2", *sxmetter[0].kVAr[1]);
		cJSON_AddNumberToObject(dev, "kVAr3", *sxmetter[0].kVAr[2]);
		cJSON_AddNumberToObject(dev, "toltal_kW", *sxmetter[0].totalkW);
		cJSON_AddNumberToObject(dev, "toltal_kVA", *sxmetter[0].totalkVA);
		cJSON_AddNumberToObject(dev, "toltal_kVAr", *sxmetter[0].totalkVAr);
		cJSON_AddNumberToObject(dev, "PF1", *sxmetter[0].PF[0]);
		cJSON_AddNumberToObject(dev, "PF2", *sxmetter[0].PF[1]);
		cJSON_AddNumberToObject(dev, "PF3", *sxmetter[0].PF[2]);
		cJSON_AddNumberToObject(dev, "APF", *sxmetter[0].PFAverage);
		cJSON_AddNumberToObject(dev, "Frequency", *sxmetter[0].frequency);
		cJSON_AddNumberToObject(dev, "kWh", *sxmetter[0].kWh);
		cJSON_AddNumberToObject(dev, "kVAh", *sxmetter[0].kVAh);
		cJSON_AddNumberToObject(dev, "kVArh", *sxmetter[0].kVArh);
	}
	cJSON_AddItemToArray(dev_arr,dev);
	cJSON_AddItemToObject(root,"devices",dev_arr);
	memset(payload, 0, PAYLOAD_BUFF_SIZE);

	cJSON_PrintPreallocated(root, payload, PAYLOAD_BUFF_SIZE, false);

	cJSON_Delete(root);
	// LOGD(TAG, "Json payload : %s", payload);
	return payload;
}
#endif

#if NUM_SXTHS_DEV > 0
static char *createTHSPayload(){
	cJSON *root = cJSON_CreateObject();
	cJSON *dev_arr = cJSON_CreateArray();
	cJSON_AddNumberToObject(root, "timestamp", sntp_get_timestamp());
	for (int i = 0; i < NUM_SXTHS_DEV; i++)
	{
		cJSON *dev = cJSON_CreateObject();
		cJSON_AddBoolToObject(dev, "isConnect", sx_ths[i].dev.isConnect);
		cJSON_AddNumberToObject(dev, "id", sx_ths[i].dev.id);
		cJSON_AddStringToObject(dev, "type", "THS");
		cJSON_AddNumberToObject(dev, "temperature",sx_ths[i].temp.temp);
		cJSON_AddNumberToObject(dev, "humidity",sx_ths[i].humi.humi);
		cJSON_AddItemToArray(dev_arr, dev);
	}
	cJSON_AddItemToObject(root, "devices", dev_arr);
	memset(payload, 0, PAYLOAD_BUFF_SIZE);
	cJSON_PrintPreallocated(root, payload, PAYLOAD_BUFF_SIZE, false);
	cJSON_Delete(root);
	// LOGD(TAG, "Json payload : %s", payload);
	return payload;
}
#endif
char *createIODevicePayload(int dev_start, int dev_end)
{
	if(dev_start < 0 || dev_end < 0 || dev_start > dev_end ){
		LOGE(TAG, "Invalid dev");
		return NULL;
	}
	cJSON *root = cJSON_CreateObject();
	cJSON *dev_arr = cJSON_CreateArray();
	cJSON_AddNumberToObject(root, "timestamp", sntp_get_timestamp());
	cJSON_AddStringToObject(root, "UTC", "+07:00");
	for (int i = dev_start; i < dev_end; i++)
	{
		cJSON *dev = cJSON_CreateObject();
		cJSON_AddBoolToObject(dev, "isConnect", gateway.devio[i].dev.isConnect);
		cJSON_AddNumberToObject(dev, "id", gateway.devio[i].dev.id);
		cJSON_AddStringToObject(dev, "type", "IO");
		// if (gateway.devio[i].dev.isConnect)
		// {
			// cJSON *output = cJSON_CreateArray();
			// for (int j = 0; j < IO_OUTPUT_NUM; j++)
			// 	cJSON_AddItemToArray(output, cJSON_CreateNumber(gateway.devio[i].output[j]));
			cJSON *inputs = cJSON_CreateArray();
			cJSON *counters = cJSON_CreateArray();
			for (int j = 0; j < IO_INPUT_NUM; j++){
				cJSON_AddItemToArray(inputs, cJSON_CreateNumber(gateway.devio[i].input[j]));
				cJSON_AddItemToArray(counters, cJSON_CreateNumber(*gateway.devio[i].counter[j]));
			}
			// // cJSON_AddItemToObject(dev, "outputs", output);
			cJSON_AddItemToObject(dev, "inputs", inputs);
			cJSON_AddItemToObject(dev, "counters", counters);
		// }
		cJSON_AddItemToArray(dev_arr, dev);
	}
	cJSON_AddItemToObject(root, "devices", dev_arr);
	memset(payload, 0, PAYLOAD_BUFF_SIZE);
	cJSON_PrintPreallocated(root, payload, PAYLOAD_BUFF_SIZE, false);
	cJSON_Delete(root);
	// LOGD(TAG, "Json payload : %s", payload);
	return payload;
}

static char *create420mADevicePayload()
{
	cJSON *root = cJSON_CreateObject();
	cJSON *dev_arr = cJSON_CreateArray();
	cJSON_AddNumberToObject(root, "timestamp", sntp_get_timestamp());
	for (int i = 0; i < app_setting.setting.dev_ai; i++)
	{
		cJSON *dev = cJSON_CreateObject();
		cJSON_AddBoolToObject(dev, "isConnect", gateway.dev420mA[i].dev.isConnect);
		cJSON_AddNumberToObject(dev, "id", gateway.dev420mA[i].dev.id);
		cJSON_AddStringToObject(dev, "type", "4-20mA");
		if (gateway.dev420mA[i].dev.isConnect)
		{
			cJSON *value = cJSON_CreateArray();
			cJSON_AddItemToArray(value, cJSON_CreateNumber(*gateway.dev420mA[i].adc[0]));
			cJSON_AddItemToArray(value, cJSON_CreateNumber(*gateway.dev420mA[i].adc[1]));
			cJSON_AddItemToArray(value, cJSON_CreateNumber(*gateway.dev420mA[i].adc[2]));
			cJSON_AddItemToArray(value, cJSON_CreateNumber(*gateway.dev420mA[i].adc[3]));
			cJSON_AddItemToObject(dev, "value", value);
		}
		cJSON_AddItemToArray(dev_arr, dev);
	}
	cJSON_AddItemToObject(root, "devices", dev_arr);
	memset(payload, 0, PAYLOAD_BUFF_SIZE);
	cJSON_PrintPreallocated(root, payload, PAYLOAD_BUFF_SIZE, false);
	cJSON_Delete(root);
	// LOGD(TAG, "Json payload : %s", payload);
	return payload;
}

int push_last_telemetry_json(void)
{
	

	static uint32_t _time_publish = 0;

	_time_publish++;

	if (_time_publish >= app_setting.mqtt_setting.publish_time)
	{
		_time_publish = 0;
		char topic[128];
		char *l_payload = NULL;
		#if NUM_SXMETTER_DEV > 0
		sprintf(topic, TELEMETRY_API METTER_TYPE, app_setting.setting.name);
		
		l_payload = createMetterPayload();

		tb_publish(&gateway.thingsboard,topic, l_payload, strlen(l_payload), 1, 0);
		#endif

		#if NUM_SXTHS_DEV > 0
		sprintf(topic, TELEMETRY_API THS_TYPE, app_setting.setting.name);

		l_payload = createTHSPayload();
		tb_publish(&gateway.thingsboard,topic,l_payload,strlen(l_payload),1,0);
		#endif

		uint8_t devio_interger = 0;
		uint8_t devio_decimal = 0;
		LOGD(TAG,"Number of IO Device : %d",app_setting.setting.dev_io);
		devio_interger = app_setting.setting.dev_io / 5 ;
		devio_decimal = app_setting.setting.dev_io % 5;
		LOGD(TAG,"devio interger : %d" , devio_interger);
		LOGD(TAG,"devio decimal : %d",devio_decimal);
		int res = 0;
		if(devio_interger > 0){
			for(int i = 0;i<devio_interger*5;i += 5){
				LOGD(TAG,"Dev range : %d-%d",i+1,i+5);
				l_payload = createIODevicePayload(i,i+5);
				sprintf(topic, TELEMETRY_API RS485_IO_TYPE, app_setting.setting.name);
				res = tb_publish(&gateway.thingsboard, topic, l_payload, strlen(l_payload), 1, 0);
			}
		}
		if(devio_decimal != 0){
			LOGD(TAG,"Dev range : %d-%d",devio_interger*5+1,devio_interger*5 + devio_decimal);
			l_payload = createIODevicePayload(devio_interger*5,devio_interger*5 + devio_decimal);
			sprintf(topic, TELEMETRY_API RS485_IO_TYPE, app_setting.setting.name);
			res = tb_publish(&gateway.thingsboard, topic, l_payload, strlen(l_payload), 1, 0);
		}
		if (0 != res)
			return -1;
	}
	// payload = create420mADevicePayload();
	// if (0 != tb_publish(&thingsboard, TELEMETRY_API RS485_4_20mA_TYPE, payload, strlen(payload), 1, 0))
	// 	return -1;

	lfs_dir_t dir;
    struct lfs_info info;
    int err;
	char *path = "/data";
	err = lfs_dir_open(CONFIG_LITTLEFS, &dir, "/data");
    if (err < 0)
    {
        LOGE(TAG, "Failed to open dir %s, err=%d", path, err);
        return 0;
    }
	LOGD(TAG, "Listing directory: %s", path);

	while (true)
	{
		err = lfs_dir_read(CONFIG_LITTLEFS, &dir, &info);
		if (err < 0)
		{
			LOGE(TAG,"Read dir error %d", err);
            break;
		}
		if (err == 0) {
            // end of directory
            break;
        }
		if (info.type == LFS_TYPE_REG)
		{
			LOGD(TAG, "[FILE] %s (%ld bytes)", info.name, info.size);
			char file_name[270];
			sprintf(file_name,"/data/%s",info.name);
			FILE *file = fopen(file_name, "r");
			if (file == NULL)
			{
				LOGE(TAG, "Can't open file %s", file_name);
				break;
			}
			size_t size = fread(payload, sizeof(uint8_t), PAYLOAD_BUFF_SIZE, file);

			if (size > 0)
			{
				cJSON *root = cJSON_Parse(payload);
				if (root == NULL)
				{
					fclose(file);
					remove(file_name);
					break;
				}
				cJSON *m_topic = cJSON_GetObjectItem(root, "topic");
				cJSON *m_payload = cJSON_GetObjectItem(root, "payload");

				if (cJSON_IsString(m_topic) && cJSON_IsString(m_payload))
				{
					if (0 != tb_publish(&gateway.thingsboard, cJSON_GetStringValue(m_topic), cJSON_GetStringValue(m_payload), strlen(cJSON_GetStringValue(m_payload)), 1, 0))
					{
						LOGD(TAG, "Can't republish file %s", file_name);
						cJSON_Delete(root);
						fclose(file);
						remove(file_name);
						lfs_dir_close(CONFIG_LITTLEFS, &dir);
						return -1;
						break;
					}
					LOGD(TAG, "Publish file %s success", file_name);
				}
				cJSON_Delete(root);
			}
			fclose(file);
			remove(file_name);
			break;
		}
	}

	lfs_dir_close(CONFIG_LITTLEFS, &dir);
	return 0;
}
char *push_update_attribute(void)
{
	memset(payload, 0, 4096);

	cJSON *json = cJSON_CreateObject();
	cJSON_AddNumberToObject(json, "timestamp", sntp_get_timestamp());
	cJSON_AddBoolToObject(json, "active", true);
	cJSON_AddNumberToObject(json, "error", 0);
	cJSON_AddNumberToObject(json, "uuid", bsp_get_chip_id());
	cJSON_AddStringToObject(json, "vendor", BOARD_VENDOR);
	cJSON_AddStringToObject(json, "name", BOARD_NAME);
	cJSON_AddStringToObject(json, "url", BOARD_URL);
	cJSON_AddStringToObject(json, "hw version", bsp_get_hw_version());
	cJSON_AddStringToObject(json, "sw version", bsp_get_sw_version());
	cJSON_PrintPreallocated(json, payload, 4096, false);

	cJSON_Delete(json);

	LOGD(TAG, "Json payload : %s", payload);

	return payload;
}

typedef struct {
	const char *topic;
	const uint8_t *payload;
	size_t len;
	ThingsBoard_t *client;
} MQTTHandleMap_t;

static void rpc_task(void *param){
	MQTTHandleMap_t *rpc_handle = (MQTTHandleMap_t *)param;
	ThingsBoard_t *client = rpc_handle->client;
	const char *json_str = (const char *)rpc_handle->payload;
	// const char *topic = (const char *)rpc_handle->topic;
	cJSON *root = cJSON_Parse(json_str);
	if (root == NULL)
	{
		goto end_rpc;
	}

	cJSON *method = cJSON_GetObjectItem(root, "method");
	cJSON *params = cJSON_GetObjectItem(root, "params");

	if (method == NULL || params == NULL)
	{
		cJSON_Delete(root);
		goto end_rpc;
	}

	if (cJSON_IsString(method))
	{
		cJSON *res = RPC_Call(cJSON_GetStringValue(method),params);
		cJSON *message_return = cJSON_CreateObject();
		cJSON_AddStringToObject(message_return,"method",cJSON_GetStringValue(method));
		cJSON *param_copy = cJSON_Duplicate(params,1);
		cJSON_AddItemToObject(message_return,"params",param_copy);
		cJSON_AddItemToObject(message_return,"result",res);
		char response[256];
		memset(response,0,256);
		cJSON_PrintPreallocated(message_return,response,256,false);
		char topic[256];
		sprintf(topic,RPC_RESPONSE_API,app_setting.setting.name);
		tb_publish(client, topic,response, strlen(response), 1, 0);
		cJSON_Delete(message_return);
	}
	cJSON_Delete(root);
end_rpc:
	vTaskDelete(NULL);
}

int RPCHandle(struct ThingsBoard *client, const char *json_str, const char *topic)
{
	MQTTHandleMap_t rpc_handle = {
		.topic = topic,
		.payload = (const uint8_t *)json_str,
		.len = strlen(json_str),
		.client = client
	};
	// if(rpc_task_handle != NULL){
	// 	LOGW(TAG,"RPC inprocess");
	// 	return -1;
	// }
	xTaskCreate(rpc_task, "RPC_Task", 1024 , &rpc_handle, configMAX_PRIORITIES - 1,NULL);
	return 0;
}



TaskHandle_t fw_task_handle = NULL;

static void firmware_handle_task(void *param){
	MQTTHandleMap_t *fw_handle = (MQTTHandleMap_t *)param;
	ThingsBoard_t *client = fw_handle->client;
	uint8_t *payload = (uint8_t*)fw_handle->payload;
	size_t len = fw_handle->len;
	payload[len] = '\0';
	const char *topic = fw_handle->topic;
	uint8_t topic_buff[256];
	sprintf((char*)topic_buff,(char*)ATTRIBUTE_FW_REQUEST_API"begin",app_setting.setting.name);
	static uint32_t _crc = 0;
	static uint32_t _fw_length = 0;
	static int _segment = -1;
	if(strcmp(topic,(char*)topic_buff) == 0){
		LOGD(TAG,"Begin OTA");
		sprintf((char*)topic_buff,(char*)ATTRIBUTE_FW_RESPONSE_API"begin",app_setting.setting.name);
		// get new firmware
		cJSON *root = cJSON_Parse((char*)payload);

		if(root == NULL) {
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"json message error -1\"}", strlen("{\"status\" : \"json message error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
		cJSON *fw_version = cJSON_GetObjectItem(root,"version"); 
		if(fw_version == NULL || !cJSON_IsString(fw_version)) {
			cJSON_Delete(root);
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"firmware version error -1\"}", strlen("{\"status\" : \"firmware version error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}

		cJSON *fw_size = cJSON_GetObjectItem(root,"size");
		if(fw_size == NULL || !cJSON_IsNumber(fw_size)){
			cJSON_Delete(root);
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"firmware size error -1\"}", strlen("{\"status\" : \"firmware size error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}

		cJSON *fw_crc = cJSON_GetObjectItem(root,"crc");
		if(fw_crc == NULL || !cJSON_IsNumber(fw_crc)){
			cJSON_Delete(root);
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"firmware crc error -1\"}", strlen("{\"status\" : \"firmware crc error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
		_crc = cJSON_GetNumberValue(fw_crc);
		_fw_length = cJSON_GetNumberValue(fw_size);
		LOGD(TAG,"New firmware version : %s",cJSON_GetStringValue(fw_version));
		LOGD(TAG,"New firmware size : %lu",_fw_length);
		LOGD(TAG,"Firmware CRC : %lu",_crc);
		bool res = mg_ota_begin((size_t)_fw_length);
		// bool res = true;
		cJSON_Delete(root);
		// if new firmware
			// erase old firmware
		if(res){
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"ota ready\"}", strlen("{\"status\" : \"ota ready\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
		else{
			tb_publish(client, (char*)topic_buff,(char*)"{\"status\" : \"ota begin error -1\"}", strlen("{\"status\" : \"ota begin error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
		// response
	}

	sprintf((char*)topic_buff,(char*)ATTRIBUTE_FW_REQUEST_API"end",app_setting.setting.name);

	if(strcmp(topic,(char*)topic_buff) == 0){
		LOGD(TAG,"End OTA");
		sprintf((char*)topic_buff,(char*)ATTRIBUTE_FW_RESPONSE_API"end",app_setting.setting.name);
		// Create new timer go to bootloader
		// Check CRC
		bool res = mg_ota_end();
		// publish result
		if(res){
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"ota end\"}", strlen("{\"status\" : \"ota end\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
		else{
			_segment = -1;
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"ota end error -1\"}", strlen("{\"status\" : \"ota end error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
	}

	sprintf((char*)topic_buff,(char*)ATTRIBUTE_FW_REQUEST_API,app_setting.setting.name);

	if(strstr(topic,(char*)topic_buff) != NULL){
		int segment = 0;
		sscanf(topic + strlen((char*)topic_buff),"%d",&segment);
		LOGD(TAG,"New segment : %d\nSize : %lu",segment,len);
		if(_segment >= segment){
			LOGW(TAG,"Duplicate segment : %d",segment);
			vTaskDelete(fw_task_handle);
			return;
		}
		_segment = segment;
		// LOGD(TAG,"Payload : %s",payload);
		// write firmware
		// publish result
		sprintf((char*)topic_buff,(char*)ATTRIBUTE_FW_RESPONSE_API"%d",app_setting.setting.name,segment);
		uint8_t fw_data[512];
		size_t fw_data_len = 0;
		memset(fw_data,0,512);
		hex_string_to_bytes((const char*)payload,fw_data,(size_t*)&fw_data_len);
		LOGD(TAG,"Firmware data len : %lu",fw_data_len);
		bool res = false;
		if(fw_data_len == strlen((char*)payload)/2){
			LOGD(TAG,"Firmware data converted");
			res = mg_ota_write(fw_data,fw_data_len);
		}
		else{
			LOGE(TAG,"Firmware data converted error");
		}
		// bool 
		if(res){
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"ota write success\"}", strlen("{\"status\" : \"ota write success\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
		else{
			_segment = -1;
			tb_publish(client,(char*) topic_buff,(char*)"{\"status\" : \"ota write error -1\"}", strlen("{\"status\" : \"ota write error -1\"}"), 1, 0);
			vTaskDelete(fw_task_handle);
			return;
		}
	}

	vTaskDelete(fw_task_handle);
	return;
}
MQTTHandleMap_t fw_handle;
static int FirmwareHandle(struct ThingsBoard *client, const uint8_t *payload,size_t len,const char *topic){
	fw_handle.client = client;
	fw_handle.payload = payload;
	fw_handle.len = len;
	fw_handle.topic = topic;
	// Create task to handle firmware

	xTaskCreate(
		firmware_handle_task,
		"firmware_handle_task",
		1024,
		&fw_handle,
		configMAX_PRIORITIES - 1,
		&fw_task_handle);
	return 0;
}

#include "app_settings.h"
void mqtt_app_init()
{
	log_debug(TAG,"MQTT App Init...");
	memset(&thingsboard_cfg, 0, sizeof(struct ThingsBoard_cfg));
	strcpy(thingsboard_cfg.clientID, app_setting.mqtt_setting.cli_id);
	strcpy(thingsboard_cfg.host, app_setting.mqtt_setting.host);
	strcpy(thingsboard_cfg.username, app_setting.mqtt_setting.user_name);
	strcpy(thingsboard_cfg.password, app_setting.mqtt_setting.password);
	thingsboard_cfg.port = app_setting.mqtt_setting.port;
	thingsboard_cfg.publish_time_S = app_setting.mqtt_setting.publish_time;
#if ZIGBEE_APP_ENABLE && MODBUS_APP_ENABLE
	thingsboard_api.get_telemetry = push_last_telemetry_json;
	thingsboard_api.update_attribute = push_update_attribute;
	thingsboard_api.rpc_handle = RPCHandle;
	thingsboard_api.firmware_handle = FirmwareHandle;
#else
	thingsboard_api.get_telemetry = NULL;
	thingsboard_api.update_attribute = NULL;
	thingsboard_api.rpc_handle = NULL;
#endif
	if (app_setting.mqtt_setting.tls == true)
	{
		thingsboard_cfg.tls = true;
		thingsboard_cfg.ca_size = 0;
		thingsboard_cfg.client_crt_size = 0;
		thingsboard_cfg.client_key_size = 0;
		// LOGD(TAG,"Open CA Cerfiticate file : %s",app_setting.ca_crt_path);
		FILE *ca_file = fopen(app_setting.ca_crt_path, "r");
		if (ca_file != NULL)
		{
			size_t ca_size = fread(thingsboard_cfg.ca_buff, sizeof(uint8_t), MQTT_TLS_CA_MAX_BUFF_SIZE, ca_file);
			thingsboard_cfg.ca_size = ca_size + 1;
			fclose(ca_file);
			thingsboard_cfg.ca_buff[ca_size] = '\0';
			LOGD(TAG, "%s file loaded, size : %lu bytes",app_setting.ca_crt_path, thingsboard_cfg.ca_size);
			log_debug(TAG, "%s : \n%s",app_setting.ca_crt_path, thingsboard_cfg.ca_buff);
		}
		FILE *client_crt_file = fopen(app_setting.client_crt_path, "r");
		if (client_crt_file != NULL)
		{
			size_t client_crt_size = fread(thingsboard_cfg.client_crt_buff, sizeof(uint8_t), MQTT_TLS_CLIENT_CRT_MAX_BUFF_SIZE, client_crt_file);
			thingsboard_cfg.client_crt_size = client_crt_size + 1;
			fclose(client_crt_file);
			thingsboard_cfg.client_crt_buff[client_crt_size] = '\0';
			LOGD(TAG, "%s file loaded, size : %lu bytes",app_setting.client_crt_path, thingsboard_cfg.client_crt_size);
			log_debug(TAG, "%s : \n%s",app_setting.client_crt_path, thingsboard_cfg.client_crt_buff);
		}
		else
			LOGW(TAG, "client.crt file not found");
		FILE *client_key_file = fopen(app_setting.client_key_path, "r");
		if (client_key_file != NULL)
		{
			size_t client_key_size = fread(thingsboard_cfg.client_key_buff, sizeof(uint8_t), MQTT_TLS_CLIENT_KEY_MAX_BUFF_SIZE, client_key_file);
			thingsboard_cfg.client_key_size = client_key_size + 1;
			fclose(client_key_file);
			thingsboard_cfg.client_key_buff[client_key_size] = '\0';
			LOGD(TAG, "%s file loaded, size : %lu bytes",app_setting.client_key_path, thingsboard_cfg.client_key_size);
			log_debug(TAG, "%s : \n%s",app_setting.client_key_path, thingsboard_cfg.client_key_buff);
		}
		else
			LOGW(TAG, "client.key file not found");

		if (thingsboard_cfg.ca_size == 0 ||
			thingsboard_cfg.client_crt_size == 0 ||
			thingsboard_cfg.client_key_size == 0)
		{
			LOGW(TAG, "TLS files not found or invalid size, TLS connection will not be established");
			return;
		}
		else
			LOGD(TAG, "TLS files loaded, TLS connection will be established");
	}
	else
	{
		thingsboard_cfg.tls = false;
	}
	ThingsBoard_Init(&gateway.thingsboard, &thingsboard_cfg, &thingsboard_api);
}

int get_num_dev_online()
{
	int is_online = 0;
	#if NUM_SXMETTER_DEV
	if (sxmetter[0].dev.isConnect)
	{
		is_online++;
	}
	#endif
	#if NUM_SXTHS_DEV
	if (sx_ths[0].dev.isConnect)
	{
		is_online++;
	}
	#endif
	for (int i = 0; i < app_setting.setting.dev_io; i++)
	{
		if (gateway.devio[i].dev.isConnect)
		{
			is_online++;
		}
	}
	for (int i = 0; i < app_setting.setting.dev_io; i++)
	{
		if (gateway.dev420mA[i].dev.isConnect)
		{
			is_online++;
		}
	}
	gateway.num_dev_online = is_online;
	return gateway.num_dev_online;
}
