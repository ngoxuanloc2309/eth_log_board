# Device Settings Plan

## Status

The non-secret settings store, boot-time integration, bounded HTTP API, and
dashboard forms are implemented. Settings are persisted and applied after
reboot; firmware, host API, browser, and board-level validation remain
follow-up work. Existing radio controls remain separate and continue to use
`rf_app`.

## Goal

Provide a safe, bounded way to view and configure device-level settings:

- MQTT broker connection settings.
- UART port settings.
- Product variant identifier.
- Ethernet IPv4 settings: DHCP, IP address, netmask, and default gateway.

Settings must be owned by the subsystem that applies them. The settings layer
must not become a hardware driver or a monolithic owner of every subsystem.

## Current Constraints

- MQTT broker host, port, client ID, and keep-alive are currently configured
  in `Core/Src/app_freertos.c`. The MQTT agent accepts username and password
  values, and the startup configuration includes a broker CA certificate.
- The web server is currently plain HTTP without authentication. Per the
  selected scope, only non-secret settings are exposed over this interface;
  MQTT credentials and TLS trust material are excluded. Use only on a trusted,
  isolated LAN. Login remains deferred; see the
  [web login plan](../web_server/webserver.md#login-and-web-access-control-deferred-not-implemented).
- lwIP initializes Ethernet with DHCP enabled. The web server reports the
  current IP, netmask, gateway, and DHCP state, but has no configuration
  endpoint for changing them.
- `sx_uart_if_t` defines UART/USART read and write operations but does not
  define a way to configure a UART. The radio configuration already contains
  baud rate, parity, and fixed data/stop-bit values for the radio module's
  serial link; this is distinct from reconfiguring an MCU UART peripheral.
- No product path identifier, owner, or provisioning source is currently
  defined in the inspected application and board APIs.
- The selected policy persists non-secret settings in LittleFS and stages
  updates for reboot. Storage is never formatted automatically; if settings
  cannot be loaded, firmware defaults remain available and an error is
  reported.

## Architecture and Ownership

```text
Settings UI / CLI
        ↓ validated request
Application orchestration (task context)
        ↓
Settings model and validation
        ├── MQTT settings API → MQTT agent
        ├── Ethernet settings API → lwIP/network owner
        ├── UART settings API → UART BSP/interface
        └── Product path identifier owner (to be selected)
```

- Keep configuration types, field validation, and settings orchestration in
  the settings module under `applications/settings`.
- Give each domain one owner: MQTT agent for broker configuration, the
  network/lwIP integration for Ethernet configuration, and the UART BSP
  adapter for physical UART configuration.
- Keep MCU/HAL calls out of the settings module. Add or extend hardware
  interfaces where a supported operation is missing; do not make application
  configuration code call HAL directly.
- Perform apply/reconfigure operations in task context. Web-server raw TCP
  callbacks may validate bounded input and enqueue a command only; they must
  not reconfigure a peripheral, restart DHCP, write flash, or block.
- Use typed settings structures and explicit status results. Do not pass
  arbitrary JSON or strings into hardware/subsystem APIs.

## Settings Scope

| Group | Fields | Access and apply | Owner / notes |
|---|---|---|---|
| MQTT | Enabled, broker host, port, client ID, keep-alive, TLS toggle | Read/write over the current HTTP API; persisted for reboot | Credentials and TLS trust material are not returned or editable. TLS uses existing firmware-provisioned trust material. |
| UART | Baud rate, data bits, parity, stop bits for UART7, USART3, USART10 | Read/write, persisted for reboot | LPUART1 console is excluded; UART9 remains owned by `rf_app` radio settings. Supported baud rates: 9600, 19200, 38400, 57600, 115200. |
| Product variant | Read-only product variant identifier | Read-only; currently unprovisioned (`null`) | No source of truth exists yet; do not treat as a filesystem path or URL. |
| Ethernet IPv4 | DHCP enabled, static IPv4 address, netmask, default gateway | Read/write, persisted for reboot | Static configuration must be confirmed within 60 seconds after boot or DHCP is restored. |

### Product variant: definition required

The requested "product path number" is treated as a product variant. It is
read-only and returns `null` until a provisioning owner and source of truth
are defined. It must not be interpreted as a filesystem path or used to
construct an unrestricted file/network path.

## Per-Feature Plan

### MQTT settings

1. The typed non-secret model is implemented and initialized from persisted
   settings or existing firmware defaults.
2. Web updates are staged for reboot; live MQTT reconfiguration is not
   performed by the HTTP callback.
3. Keep credentials and trust material out of status, settings responses,
   logs, and errors. The UI clearly warns that the current HTTP interface is
   unauthenticated and should be restricted to a trusted LAN.

### UART settings

- Implemented for UART7, USART3, and USART10 with supported baud, data-bit,
  parity, and stop-bit choices; changes apply during startup.
- LPUART1 is reserved for the console, and UART9 remains owned by `rf_app`.
- Verify peripheral ownership, polling mode, and hardware behavior on the
  target board before release.

### Ethernet IPv4 settings

- Implemented with DHCP as the default and bounded static address, netmask,
  gateway, and subnet validation.
- `lwIP` changes are applied in TCP/IP thread context. Static values are
  persisted but are accepted for the current boot only after explicit
  confirmation; otherwise `mainTask` queues DHCP fallback after 60 seconds.
- Verify link loss, address changes, confirmation, and rollback on hardware.

### Product variant

- Read-only and currently unprovisioned (`null` in the API and UI).
- Define a provisioning source and product owner before adding a value or
  making the field editable.

## Apply, Persistence, and Recovery Policy

- Expose current values and pending/applied/failure state separately. A
  request is not successful merely because it was parsed or queued.
- Validate the entire requested update before applying any field. Avoid
  partially applied multi-group updates; if atomic cross-subsystem changes
  cannot be guaranteed, apply one group at a time and report each result.
- Non-secret settings are persisted in alternating LittleFS records with
  schema version, generation counter, and CRC. A queued save reports pending
  before the worker completes; storage errors remain explicit.
- Firmware defaults are used if no valid record is available. The module
  never formats storage. Power-loss recovery, schema migration, and flash
  endurance still require focused tests.
- Separate ordinary operational settings from secrets and provisioned
  identity. Apply least privilege and do not include secret values in
  diagnostic snapshots.

## UI and API Plan

- Organize settings by group: Ethernet, UART, MQTT, and Product variant.
- The UI displays persisted configuration and save state; settings are
  applied on reboot, not immediately.
- Validate in the UI for usability and repeat validation in firmware as the
  trust boundary.
- Use explicit read/apply operations and display pending, success, and
  failure states. Do not report an apply as successful before the owning
  subsystem confirms it.
- Keep network changes and UART changes clearly warned if they can disconnect
  the browser or console.
- Never place passwords, private keys, or trust blobs in the settings UI, API,
  URLs, or logs.

## Implementation Sequence

1. **Implemented:** typed configuration, validation, persistent records,
   startup application, API routes, and dashboard forms for supported
   non-secret settings.
2. **Remaining validation:** test storage recovery and API boundaries, verify
   reboot-time MQTT/UART/network application, and exercise static-network
   confirmation/rollback on the target board.
3. **Follow-on:** define and provision a product variant value; implement
   HTTPS/authentication before exposing any secret or privileged operation.

## Acceptance Criteria

- Invalid, oversized, unsupported, or unauthorized input is rejected without
  changing live settings.
- Every setting has one documented owner and one authoritative current value.
- Apply results identify success, failure, and whether a value is merely
  pending or persisted.
- DHCP/static transitions and loss of the current management connection have
  a documented recovery path.
- UART settings cover only UART7, USART3, and USART10; the console and radio
  UART remain excluded from generic settings.
- MQTT secrets are never returned or logged; the documented HTTP exposure is
  limited to non-secret settings and restricted to a trusted isolated LAN.
- Storage absence, corruption, and interrupted updates do not cause automatic
  formatting or unsafe defaults; focused tests are still required.
- The settings layer remains hardware-independent and does not call HAL,
  manipulate registers, or directly access lwIP internal state.

## Related Documentation

- [Settings module design](../../docs/modules/settings.md)
- [Web server and API roadmap](../web_server/webserver.md)
- [Web server module design](../../docs/modules/web_server.md)
- [Bus interface contracts](../../docs/bus-interfaces.md)