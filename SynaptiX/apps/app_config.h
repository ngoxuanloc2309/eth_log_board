#pragma once

/*
 * File: /d:/SynaptiX/Firmware/H5RTC_Test/SynaptiX/apps/app_config.h
 *
 * Purpose:
 *   Configuration header containing build-time feature flags, device counts,
 *   and service port assignments for the application. These macros control
 *   which subsystems are enabled and set default sizes/intervals used
 *   elsewhere in the firmware.
 *
 * Notes and conventions:
 *   - Most macros are boolean feature flags (0 = disabled, 1 = enabled).
 *   - Device count macros must be non-negative integers.
 *   - Port macros must be valid TCP port numbers (1..65535), avoid collisions
 *     with reserved/privileged ports if running without special privileges.
 *   - Times are expressed in milliseconds unless otherwise noted.
 *
 * Macros (summary and intent):
 *   - MQTT_PUSHLISH_TIME
 *       Interval (ms) used by the MQTT client to publish telemetry/status.
 *       NOTE: macro name contains a likely typo ("PUSHLISH" vs "PUBLISH").
 *             Treat as the publish interval; adjust or rename carefully to
 *             avoid breaking code that references the existing identifier.
 *
 *   - NUM_SX485IO_DEV
 *       Number of SX485 I/O devices supported/compiled into the build.
 *
 *   - NUM_SX420mA_DEV
 *       Number of SX 4-20mA input devices supported.
 *
 *   - NUM_SXMETTER_DEV
 *       Number of SX meter devices. (Name appears to contain a typo:
 *       "METTER" vs "METER".)
 *
 *   - NUM_SXTHS_DEV
 *       Number of SX temperature/humidity (THS) devices supported.
 *
 *   - NUM_DEVICES
 *       Aggregate device count used by the build. Currently calculated as the
 *       sum of NUM_SX485IO_DEV + NUM_SX420mA_DEV. Review this macro if you
 *       expect NUM_DEVICES to include SXMETTER or SXTHS counts.
 *
 * Feature enable flags:
 *   - ZIGBEE_APP_ENABLE
 *   - MODBUS_APP_ENABLE
 *   - BUTTON_APP_ENABLE
 *   - USB_DEVICE_FS_ENABLE
 *   - FATFS_ENABLE
 *
 *   Use these flags to include/exclude subsystems from the final firmware.
 *   Disabling unused features reduces code size and power usage on constrained
 *   devices. Ensure dependent modules are not compiled-in when a feature is 0.
 *
 * Service ports:
 *   - SHELL_TCP_PORT
 *       TCP port for the remote shell (telnet/console).
 *
 *   - MODBUS_TCP_PORT
 *       TCP port for Modbus TCP (default 502 — if running as non-root on POSIX
 *       systems, consider a non-privileged port or use a port redirect).
 *
 *   - HTTP_SERVER_PORT
 *       Port for the built-in HTTP server.
 *
 * Guidelines for modifying this file:
 *   - Keep macro names stable to avoid breaking compile-time references.
 *   - If renaming macros to fix typos, update all dependent files and add
 *     backward-compatible aliases where practical (e.g., define old name to
 *     new name) to ease migration.
 *   - Validate aggregated macros (e.g., NUM_DEVICES) if you add new device
 *     types to avoid mismatch.
 *   - Add inline comments near macros with non-obvious constraints (e.g.,
 *     runtime limits, maximums for arrays).
 *
 * Deployment / administrative note:
 *   - Enable Raptor mini (Preview) for all clients
 *
 * Change history / TODO:
 *   - Consider renaming MQTT_PUSHLISH_TIME -> MQTT_PUBLISH_INTERVAL_MS.
 *   - Review NUM_DEVICES definition to include all device types if required.
 *   - Normalize naming typos (SXMETTER -> SXMETER, etc.) and provide defines
 *     for backward compatibility.
 */

#define MQTT_ID "SX-ETH-GW-V2"

#define SHELL_PROMPT ""

// #define MQTT_HOST "broker.emqx.io"
// // #define MQTT_HOST "demo.thingsboard.io"
// #define MQTT_CLIENT_ID "czrxg6oilrjrygneqiij"
// #define MQTT_CLIENT_USER "d2n5nu51zj0ri1kddxql"
// #define MQTT_CLIENT_PASS "te1na6korx7mm2dy3ysj"

#define MQTT_HOST "192.168.110.79"
#define MQTT_CLIENT_ID "synaptix-demo01"
#define MQTT_CLIENT_USER "synaptix-demo"
#define MQTT_CLIENT_PASS "synaptix-demo"

#define USER_MQTT_PORT 1993

#define MQTT_PUSHLISH_TIME 5000


#define ZIGBEE_APP_ENABLE 1
#define MODBUS_APP_ENABLE 1

#if ZIGBEE_APP_ENABLE || MODBUS_APP_ENABLE

#define NUM_SX485IO_DEV 40
#define NUM_SX420mA_DEV 20
#define NUM_SXMETTER_DEV 0
#define NUM_SXTHS_DEV 0
#define NUM_DEVICES (NUM_SX485IO_DEV + NUM_SX420mA_DEV)

#endif

#define MONITOR_HEAP_RTOS 1

#define BUTTON_APP_ENABLE 1
#define USB_DEVICE_FS_ENABLE 0
#define FATFS_ENABLE 0
#define SHELL_TCP_PORT 8000
#define MODBUS_TCP_PORT 502
#define HTTP_SERVER_PORT 80
#define MODBUS_RTU_ENABLE 1
// #if USB_DEVICE_FS_ENABLE && MODBUS_RTU_ENABLE
#define MBS_USB_ID 1
// #endif
#define STORAGE_SIZE 100*1024
#define LED_INDICATOR_ENABLE 1

#define S_DISCRETE_INPUT_START        0
#define S_DISCRETE_INPUT_NDISCRETES   80

#define S_COIL_START                  0
#define S_COIL_NCOILS                 80

#define S_REG_INPUT_START             0
#define S_REG_INPUT_NREGS             100

#define S_REG_HOLDING_START           0
#define S_REG_HOLDING_NREGS           100

#ifndef ETH_MAC_ADDR0
#define ETH_MAC_ADDR0 0x00
#endif
#ifndef ETH_MAC_ADDR1
#define ETH_MAC_ADDR1 0x80
#endif
#ifndef ETH_MAC_ADDR2
#define ETH_MAC_ADDR2 0xE1
#endif
#ifndef ETH_MAC_ADDR3
#define ETH_MAC_ADDR3 0x00
#endif
#ifndef ETH_MAC_ADDR4
#define ETH_MAC_ADDR4 0x01
#endif
#ifndef ETH_MAC_ADDR5
#define ETH_MAC_ADDR5 0x02
#endif

#define USB_SERIAL_ID_1 1
#define USB_SERIAL_ID_2 2
#define USB_SERIAL_ID_3 3

#define MMB_SERIAL_PORT RS485_0_PORT