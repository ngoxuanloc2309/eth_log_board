# Application libraries

The `button_app` target implements the device's single-click no-op,
double-click reboot, and 10-second hold to enter the existing STM32 ROM
bootloader. See [`docs/modules/button_app.md`](../docs/modules/button_app.md)
for the module contract and [`button_app/ButtonAppPlan.md`](./button_app/ButtonAppPlan.md)
for design and hardware validation details.

The `indicator_app` target computes power, Ethernet-link, and status LED
outputs from logical state and typed error enums. `indicator-task` applies
complete LED frames through the board adapter and reports active faults with
priority-based blink codes. See [`docs/modules/indicator_app.md`](../docs/modules/indicator_app.md)
and [`indicator_app/IndicatorAppPlan.md`](./indicator_app/IndicatorAppPlan.md).

The `file_system` target in [`file_system/`](./file_system/) provides bounded
POSIX-style file operations on LittleFS using an initialized W25Q flash device.
Mounting requires caller-supplied task serialization callbacks; format is
explicit and never occurs automatically. It supports eight open file
descriptors and Newlib file syscalls. See
[`docs/modules/file_system.md`](../docs/modules/file_system.md) for the API,
limits, and initialization requirements.
The startup task calls `file_system_port_init()` to connect SPI1, the board
W25Q flash, and LittleFS. Mount failure is reported; flash is never formatted
automatically.

The board's `sx_spi` adapter exposes initialized STM32H5 SPI1 through the
hardware-independent `sx_spi_if_t` contract. It serializes transfers with
static FreeRTOS objects and defers asynchronous completion callbacks to its
service task. See [`docs/modules/sx_spi.md`](../docs/modules/sx_spi.md) and
[`docs/TaskDesign.md`](../docs/TaskDesign.md).

The `mqtt_client` target in [`mqtt-client/`](./mqtt-client/) builds the
coreMQTT-Agent client for the repository's lwIP, Mbed TLS, and FreeRTOS setup.
It uses coreMQTT-Agent v1.3.1 with coreMQTT v2.3.1 for MQTT 3.1.1. Its socket
adapter supports plain TCP or TLS, and the Agent command pool and task stack
use static storage.

Call `ApplicationMqttClient_Init()` to validate and retain the client
configuration, then `ApplicationMqttClient_Start()` to launch its native
FreeRTOS supervisor. The supervisor establishes the socket, optionally
completes TLS verification with the configured CA certificate, initializes
the Agent, and queues CONNECT. It retries initial connections and reconnects
after transport or MQTT failures, so it can be started before lwIP has a
usable interface. Publish, subscribe, and disconnect requests are submitted
through the corresponding `ApplicationMqttClient_*` functions.

See [`mqtt-client/README.md`](./mqtt-client/README.md) for the library build
and host protocol-test commands. Device settings supply the non-secret broker
host, port, client ID, keep-alive, and TLS-mode defaults; trust store,
credentials, and topics remain application policy.

The `settings` target in [`settings/`](./settings/) owns validated, non-secret
MQTT, Ethernet, and UART configuration, persists it in LittleFS, and applies
it during startup. Credentials and TLS trust material are excluded. See
[`docs/modules/settings.md`](../docs/modules/settings.md) and
[`settings/settings_plan.md`](./settings/settings_plan.md) for the storage
contract, supported ports, and recovery behavior.

The `tcp_application` target in [`tcp/`](./tcp/) provides a non-blocking lwIP
TCP server with a hard maximum of four clients. Start it from a FreeRTOS task
after the scheduler starts using `tcp_server_start()`, then stop it with
`tcp_server_stop()`. The server uses static task, stack, semaphore, and state
storage, so only one instance may be active at a time. Its event callback runs
in the server task and must not
block. Client RX/TX data is exposed as contiguous ring-buffer spans through
`tcp_client_read_ptr()` / `tcp_client_read_consume()` and
`tcp_client_send_ptr()` / `tcp_client_send_commit()`. Each client has a
512-byte RX buffer and a 512-byte TX buffer; serialize application access to
each client's pointer API and do not retain pointers across consume, commit,
or disconnect.

The `web_server` target serves the dark-mode dashboard and exposes status and
radio and non-secret settings endpoints over lwIP raw TCP on the Ethernet
interface. It accepts one client at a time and defers radio transactions to
`mainTask`.
The firmware assets are generated from [`web_server/`](./web_server/) source
files; see [`docs/modules/web_server.md`](../docs/modules/web_server.md) and
[`web_server/webserver.md`](./web_server/webserver.md) for the API, limits,
and security constraints.
