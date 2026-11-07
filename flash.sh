STM32_Programmer_CLI \
-c port=SWD mode=UR reset=HWrst \
-w build/Release/RF_Gateway.hex 0x8010000 \
-v \
-rst