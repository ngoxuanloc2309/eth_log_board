# Device Settings Module

## Purpose

Own validated, non-secret device configuration and persist it for application
after reboot.

## Responsibilities

- Provide defaults and validate MQTT, Ethernet IPv4, and supported UART values.
- Load and save versioned configuration records in LittleFS.
- Preserve the previous valid record if a write is interrupted by alternating
  between two record files.
- Queue persistent writes on a statically allocated FreeRTOS task and report
  idle, pending, succeeded, or failed state.
- Accept only one save at a time; concurrent requests return a busy result
  rather than replacing an in-progress settings snapshot.
- Track confirmation of a static Ethernet configuration during the 60-second
  DHCP rollback window.

## Public API

The interface is declared in `applications/settings/settings.h`:

- `settings_init()` loads valid persistent data or initializes defaults.
- `settings_get_config()` returns a consistent configuration snapshot.
- `settings_validate()` validates a complete non-secret configuration.
- `settings_request_save()` validates and queues a complete configuration.
- `settings_get_save_state()` reports the latest save state.
- `settings_confirm_network()` and `settings_network_is_confirmed()` manage
  the current boot's static-network confirmation.

Initialize the module after LittleFS is mounted and before settings are
consumed by MQTT, lwIP, or UART startup. Storage failures are returned
explicitly; defaults remain available and no automatic format is attempted.

## Dependencies and Ownership

- LittleFS through `file_system_port` for persistent records.
- FreeRTOS static queue/task primitives for deferred writes and synchronization.
- Called by startup integration and the web-server settings API.
- Does not call HAL, control MQTT directly, or manipulate lwIP internals.

## Data Flow

```text
Web settings API → validation → static save queue → settings task → LittleFS
                                      ↓
                         boot-time settings snapshot
                            ↙       ↓       ↘
                          MQTT    lwIP     UART
```

## Configuration and Security

Persisted MQTT settings contain only enabled state, host, port, client ID,
keep-alive, and TLS mode. Username/password and TLS trust material are not
stored in this module and are not exposed by the settings API. The web API
uses unauthenticated HTTP and is limited to non-secret settings; restrict it
to a trusted isolated LAN until the separate HTTPS/authentication work is
completed.

Generic UART settings cover UART7, USART3, and USART10. LPUART1 is reserved
for the console, and UART9 remains owned by the Zigbee radio configuration.
The product variant is currently unprovisioned and returned as `null`.

## Persistence Format

The record includes a schema version, generation counter, and CRC32. Two
alternating LittleFS slots are used so an incomplete write does not replace
the other valid record. Defaults are used when no valid record is found.
Power-loss, corruption, and migration behavior require focused host and
hardware tests before release.
