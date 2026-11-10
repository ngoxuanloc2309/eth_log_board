# SimplePLC DTU integration changes

This note records the integration work in this session. The firmware has not
been verified on a physical board.

## Files changed

### Build integration

- `CMakeLists.txt`
  - Added SimplePLC in library mode with product options from `port/splc`.
  - Added the product port target and linked it into the firmware.
  - Added the host logger include directory and FreeRTOS adapter object.
- `port/splc/CMakeLists.txt` (new)
  - Added an object target for the DTU-specific RS-485, Zigbee, MQTT, RTC,
    TCP, and USB port sources.
- `port/splc/splcopts.h`
  - Added DTU Zigbee product settings, including STM32H5, FreeRTOS, USB-CDC,
    Modbus MB, three RS-485 ports, MQTT, external RTC, and host-owned UARTs
    and logging.
  - Disabled the built-in ADC driver because this product has no configured
    ADC peripheral or AI channels.
- `libs/simple_plc/libs/CMakeLists.txt`
  - Added CMake targets for the vendored Modbus master and slave sources when
    the MB Modbus library is selected.
- `libs/simple_plc/port/CMakeLists.txt`
  - Added the MB adapter include paths.

### Host and product ports

- `Core/Src/app_freertos.c`
  - Changed the three RS-485 UARTs from polling to interrupt mode with RX
    buffers so the Modbus port can receive bytes.
  - Added SimplePLC startup after network and RF initialization.
  - Reworked the MQTT task to consume broker configuration and publish
    messages through the SPLC port queues, and report connection status.
- `applications/mqtt_client_posix/mqtt_agent.h`
  - Added clean/persistent session selection and a publish API that accepts
    the MQTT retain option.
- `applications/mqtt_client_posix/mqtt_agent.c`
  - Applied the session selection and retain options while preserving the
    existing publish API behavior.
- `port/splc/splc_host_rs485_port.c` and
  `port/splc/splc_host_rs485_port.h`
  - Added an RS-485 UART reconfiguration callback and documented the required
    interrupt-mode RX buffers.
- `port/splc/splc_host_zigbee_port.c` and
  `port/splc/splc_host_zigbee_port.h`
  - Registered Zigbee as Master-only without a byte pipe, because the host
    `rf_app` still owns and parses UART9.
- `port/splc/splc_mqtt_port.c` and `port/splc/splc_mqtt_port.h`
  - Added bounded queues for broker configuration, publishes, and status
    updates so PLC services remain in the engine task.
- `port/splc/splc_port.c`
  - Added the MQTT port pump to the engine loop.
- Existing DTU port files were organized under `port/splc/`; the USB port now
  defaults to CDC instance 0.

## Build and test status

- `cmake --preset Debug` — completed successfully.
- `cmake --build build\Debug --target splc_dtu_port -j 8` — completed
  successfully for the host-port objects at that point in the changes.
- `cmake --preset Release` — completed successfully.
- A Release build compiled the SimplePLC layers and product ports, but the
  full firmware link did not complete. Observed linker errors included a
  duplicate `USB_DRD_FS_IRQHandler` from the generated host interrupt file
  and the SimplePLC board file, plus unresolved platform symbols.
- Further CMake wiring was changed after that link attempt. A later full
  firmware build and the host MQTT tests have not been verified successfully.
- No board-level tests were run. ADC porting was out of scope.
