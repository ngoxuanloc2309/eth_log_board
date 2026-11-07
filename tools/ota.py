import paho.mqtt.client as mqtt
import time
import json

_file_name = "../build/Release/RF_Gateway.bin"
_dev_id = "SX-ZB-ETH-V2.0"
_version = "2.0.0"
_crc = 0x1A2B3C4D
_size = 0
_file_buff = None
_chunk_id = 0

ATTRIBUTE_FW_REQUEST_API = "synaptix/fw/request/" + _dev_id + "/chunk/"
ATTRIBUTE_FW_RESPONSE_API = "synaptix/fw/response/" + _dev_id + "/chunk/+"
try:
    _firmware = open(file=_file_name,mode="rb")
except Exception as e:
    print("error {e}")
finally:
    print("Open the firmware")
    _file_buff = _firmware.read()
    
    _firmware.close()
    _size = len(_file_buff)

def send_chunk(client,id):
    offset = id * 128
    chunk_size = 128
    if((offset + chunk_size) > _size):
        chunk_size = _size - offset
    chunk_data = _file_buff[offset:offset+chunk_size]
    print(f"Sending chunk id {id} size {chunk_size}")
    topic = ATTRIBUTE_FW_REQUEST_API + str(id)
    client.publish(topic, payload=chunk_data.hex().upper(), qos=1)

def on_connect(client,userdata,flags,rc):
    print("Connected with result code " + str(rc))
    client.subscribe(ATTRIBUTE_FW_RESPONSE_API)
    root = {
        "version": _version,
        "crc": _crc,
        "size" : _size
    }
    message = json.dumps(root)
    topic = ATTRIBUTE_FW_REQUEST_API+"begin"
    client.publish(topic, payload=message, qos=1)

def on_message(client, userdata, msg):
    global _chunk_id
    print(f"Topic : {msg.topic}")
    print(f"Received message: {msg.payload}")

    if(msg.topic == "synaptix/fw/response/" + _dev_id + "/chunk/begin"):
        message = msg.payload.decode('utf-8')
        root = json.loads(message)
        if(root["status"] == "ota ready"):
            print("Start sending firmware")
            _chunk_id = 0
            send_chunk(client,id = _chunk_id)
        else:
            print("OTA not ready")
    elif(msg.topic == "synaptix/fw/response/" + _dev_id + "/chunk/end"):
        message = msg.payload.decode('utf-8')
        root = json.loads(message)
        if(root["status"] == "ota end"):
            print("OTA update complete")
        else:
            print("OTA end failed")
    elif (msg.topic.startswith("synaptix/fw/response/" + _dev_id + "/chunk/")):
        chunk_str = msg.topic.split("/")[-1]
        chunk_id = int(chunk_str)
        if(chunk_id != _chunk_id):
            print(f"Unexpected chunk id {chunk_id}, expected {_chunk_id}")
            return
        message = msg.payload.decode('utf-8')
        root = json.loads(message)
        if(root["status"] == "ota write success"):
            _chunk_id += 1
            if((_chunk_id * 128) < _size):
                send_chunk(client,id = _chunk_id)
            else:
                print("All chunks sent")
                topic = ATTRIBUTE_FW_REQUEST_API + "end"
                root = {
                    "version": _version,
                    "crc": _crc,
                    "size" : _size
                }
                message = json.dumps(root)
                client.publish(topic, payload=message, qos=1)
        else:
            print("OTA write failed")



client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("broker.emqx.io", 1883, 60)

client.loop_forever()