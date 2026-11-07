import paho.mqtt.client as mqtt
import time
import json

file_name = "../build/RF_Gateway.bin"
file_log = "logs.txt"
try:
    firmware = open(file=file_log,mode="+a")
except Exception as e:
    print("error {e}")
finally:
    # firmware.write(f"deviceID,timestamp,motionState,latitude,longitude,temperature,humidity,pm10,pm2_5,so2,co,o3,h2s,no2\n")
    print("Open the firmware")
    firmware.close()


def on_connect(client,userdata,flags,rc):
    print("Connected with result code " + str(rc))
    client.subscribe("hanoi/air_quality/data/#")

def on_message(client, userdata, msg):
    print(f"Topic : {msg.topic}")
    print(f"Received message: {msg.payload}")
    file = open(file=file_log,mode="+a")
    # file.write(f"Topic : {msg.topic.decode('utf-8')}\r\n")
    try:
        message = msg.payload.decode('utf-8')
    except Exception as e:
        print(f"error decode message {e}")
        return

    # file.write(f"Received message: {msg.payload.decode('utf-8')}\r\n")
    try:
        root = json.loads(message)
    except Exception as e:
        print(f"error parse json {e}")
        return
    try:
        file.write(f"{root['deviceID']},{root['timestamp']},{root['motionState']},{root['location']['latitude']},{root['location']['longitude']},{root['environment']['temperature']},{root['environment']['humidity']},{root['sensors']['pm10']},{root['sensors']['pm2_5']},{root['sensors']['so2']},{root['sensors']['co']},{root['sensors']['o3']},{root['sensors']['h2s']},{root['sensors']['no2']}\n")
        file.close()
    except Exception as e:
        print(f"error {e}")
        return
try:
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect("103.226.248.21", 1993, 60)

    client.loop_forever()
except Exception as e:
    print(f"error {e}")