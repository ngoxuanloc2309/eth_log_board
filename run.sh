STM32_Programmer_CLI \
-c port=SWD mode=UR reset=HWrst \
-w build/Release/DTU_Firmware.hex 0x8000000 \
-v \
-rst