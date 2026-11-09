# Firmware Development Plan: Data Transfer Unit (DTU)

## 1. System Overview & Hardware Architecture

### 1.1 Hardware Specifications
* **Microcontroller:** STM32H563VIT6 (ARM Cortex-M33, up to 250 MHz, 2 MB Dual-Bank Flash, 640 KB SRAM, MPU, Hardware Encryption/HASH engine).
* **Network Interface:** 1x 10/100 Mbps Ethernet (RMII interface connected to an external PHY like LAN8742A / DP83848).
* **Serial Interfaces:** 3x Isolated RS485 Ports driven by hardware USART/UART peripherals with dedicated GPIOs for DE/RE (Driver Enable / Receiver Enable) flow control.
* **Configuration Interface:** 1x USB Type-C Port operating in USB 2.0 Full-Speed/High-Speed Device mode (CDC Class / Virtual COM Port).
* **Non-Volatile Storage:** Internal Dual-Bank Flash reserved for NVS (Non-Volatile Storage configuration parameters) and Bootloader/Dual-Image OTA updates.

### 1.2 Software Stack Architecture
```
+-----------------------------------------------------------------------+
|                         APPLICATION LAYER                             |
|  [Modbus Engine]   [Data Router Matrix]  [CLI/Config Server]  [OTA Engine] |
+-----------------------------------------------------------------------+
|                         MIDDLEWARE LAYER                              |
|   FreeRTOS   |   LwIP Stack (TCP/UDP/MQTT/Modbus TCP)  |  FreeMODBUS    |
+-----------------------------------------------------------------------+
|                         HAL / LL / DRIVERS                            |
|  STM32CubeH5 HAL/LL | Ethernet MAC | UART (DMA) | USB Device (CDC) | Flash |
+-----------------------------------------------------------------------+
```

---

## 2. Operating System & Task Architecture (FreeRTOS)

To ensure real-time responsiveness, deterministic RS485 timing, and reliable high-throughput Ethernet operation, tasks are partitioned with designated priorities and stack allocations:

| Task Name | Priority | Stack Size | Description |
| :--- | :--- | :--- | :--- |
| **`vTask_EthNetif`** | High (5) | 1024 words | Handles LwIP stack processing, Ethernet RX/TX interrupts, and network status management. |
| **`vTask_RS485_Rx`** (x3) | High (4) | 512 words | Processes incoming RS485 data frames triggered by UART DMA + Idle Line detection. |
| **`vTask_ModbusMaster`**| Medium-High (3) | 768 words | Handles scheduled polling of external Modbus RTU slaves based on configured register lists. |
| **`vTask_ModbusSlave`** | Medium-High (3) | 768 words | Serves requests from external Modbus Masters directed to local data registers. |
| **`vTask_DataRouter`** | Medium (2) | 1024 words | Routes data between RS485 channels and Ethernet sockets/MQTT topics/Modbus TCP. |
| **`vTask_USB_Config`** | Low-Medium (2) | 768 words | Parses CLI commands / JSON payload via USB Type-C CDC for system configuration. |
| **`vTask_SystemMon`** | Low (1) | 256 words | Manages Independent Watchdog (IWDG), CPU/RAM statistics, and status LEDs. |

---

## 3. Module Design Specifications

### 3.1 Tri-Port RS485 Driver Engine
Each RS485 port operates independently with **DMA Circular/Normal Mode + UART IDLE Line Detection** to handle variable-length frames efficiently without CPU overhead.

#### Port Modes:
1. **Base Mode (Transparent Bridge):** Raw serial payload is encapsulated into TCP/UDP frames or MQTT messages and forwarded directly to the network (and vice versa).
2. **Modbus RTU Slave Mode:** Responds to requests from an external Modbus Master using internal holding/input register tables.
3. **Modbus RTU Master Mode:** Periodically queries connected external Modbus RTU slaves according to user-defined polling schedules and forwards aggregated data to the server.

```c
typedef enum {
    RS485_MODE_BASE = 0,
    RS485_MODE_MODBUS_SLAVE,
    RS485_MODE_MODBUS_MASTER
} RS485_Mode_t;

typedef struct {
    uint32_t baudrate;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
    RS485_Mode_t mode;
    uint8_t slave_address;     // Used in Slave mode
    uint32_t poll_interval_ms; // Used in Master mode
} RS485_PortConfig_t;
```

### 3.2 Network Layer (Ethernet & LwIP Stack)
* **IP Configuration:** Static IP and Dynamic IP (DHCP Client).
* **Protocols Supported:**
  * **Modbus TCP Gateway:** Bidirectional conversion between Modbus TCP and Modbus RTU.
  * **Raw TCP Server/Client:** Dedicated sockets for transparent pass-through data in Base Mode.
  * **MQTT Client:** Secure/Unsecure telemetry transmission (optional MbedTLS integration).

### 3.3 USB Type-C Configuration Interface
* Configured as a **USB CDC (Virtual COM Port)**.
* Interactive **Command Line Interface (CLI)** or **JSON API** over USB.
* Commands enable configuring RS485 operating modes, network settings, and polling tables.
* Settings are saved to an internal **Flash NVS Emulation Partition**.

---

## 4. Development Roadmap

### Phase 1: Hardware Bring-up & Peripheral Drivers (Weeks 1 - 2)
* Initialize STM32H563VIT6 project using STM32CubeMX (250 MHz Clock, MPU, ICache/DCache).
* Configure GPIOs for status LEDs, DE/RE RS485 direction pins.
* Implement UART drivers with **DMA + IDLE Line Interrupt** for all 3 RS485 ports.
* Implement USB Device CDC stack and test USB Type-C host communication.
* Configure Ethernet RMII interface and verify PHY communication.

### Phase 2: Middleware & OS Integration (Weeks 3 - 4)
* Integrate **FreeRTOS** kernel and set up queues, semaphores, and task structures.
* Integrate **LwIP stack** (OS mode with `NO_SYS = 0`).
* Verify network stability (DHCP, Static IP, Ping response).
* Implement Flash NVS Manager for non-volatile configuration storage.
* Build CLI Parser Engine over USB CDC for system setup and parameter management.

### Phase 3: Modbus Engine & Data Routing Matrix (Weeks 5 - 6)
* Port and integrate Modbus RTU stack (Master and Slave modes) for the 3 RS485 interfaces.
* Implement **Data Router Engine** to route data packets between RS485 channels and Ethernet endpoints.
* Implement Modbus RTU <-> Modbus TCP translation gateway.

### Phase 4: Stability, OTA & Testing (Weeks 7 - 8)
* Implement Independent Watchdog (IWDG) and automatic reconnection logic for Ethernet and sockets.
* Integrate **Dual-Bank Bootloader** for Firmware Over-The-Air (FOTA) updates via HTTP or USB.
* Perform high-load stress testing across all 3 RS485 ports and Ethernet simultaneously.
* Optimize memory usage (RAM/Flash footprint analysis) and finalize documentation.