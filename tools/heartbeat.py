import paho.mqtt.client as mqtt
import time

file_name = "../build/RF_Gateway.bin"
file_log = "heartbeat.txt"
try:
    firmware = open(file=file_name,mode="rb")
except Exception as e:
    print("error {e}")
finally:
    print("Open the firmware")


def on_connect(client,userdata,flags,rc):
    print("Connected with result code " + str(rc))
    client.subscribe("hanoi/air_quality/#")

def on_message(client, userdata, msg):
    print(f"Topic : {msg.topic}")
    print(f"Received message: {msg.payload}")
    file = open(file=file_log,mode="a+")
    file.write(f"Topic : {msg.topic}\r\n")
    file.write(f"Received message: {msg.payload}\r\n")
    file.close()

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
client.on_connect = on_connect
client.on_message = on_message

client.connect("103.226.248.21", 1993, 60)

client.loop_forever()