import serial
import time

try:
    ser = serial.Serial(
        port='COM13',        # Replace with your actual port name
        baudrate=115200,
        timeout=1          # Timeout ensures read operations don't block indefinitely
    )
    ser.isOpen()
    print(f"Serial port {ser.name} opened successfully.")

    while True:
        # Read a line of data, decode it from bytes to a string, and remove whitespace
        data_bytes = ser.readline()
        print(f"{data_bytes}")
        file = open(file="logger.log",mode="+a")
        file.write(f"{data_bytes}\n")
        file.close()
    ser.close()

except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
except Exception as e:
    print(f"An unexpected error occurred: {e}")