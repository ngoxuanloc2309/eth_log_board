# Web Server: Configuration and Device Monitoring Plan

## Status

The first production implementation is in `applications/web_server` and is
wired into `StartMainTask`. It uses lwIP raw TCP/pbuf callbacks, serves
firmware-resident dashboard assets, and exposes bounded status, radio, and
non-secret device-settings APIs. Settings are persisted and applied on reboot.
The host POSIX server in `tests` remains a separate mock fixture.

## Goal

Provide a browser-based interface on the board's Ethernet network to:

- Monitor network, radio, and storage health.
- View the currently reported radio configuration.
- Request supported radio reads and configuration updates.
- Provide a standalone monitoring view separate from device settings.
- Configure non-secret MQTT connection settings, Ethernet IPv4, and supported
  UART parameters.
- Plan a secure firmware-upgrade workflow, subject to bootloader and
  authentication requirements.
- Show operation progress and report failures without blocking firmware tasks.

Keep the web feature bounded in memory and responsive to the existing
FreeRTOS, lwIP, MQTT, and radio workloads.

## Current Repository Baseline

- `MX_LWIP_Init()` starts lwIP in RTOS mode, brings Ethernet up, and starts
  DHCP. The `gnetif` status and link callbacks currently log network changes.
- lwIP raw TCP and pbuf APIs are available; the lwIP HTTPD component is not
  configured. The existing `applications/tcp` service uses the socket API and
  is not the planned web-server transport.
- `rf_app` exposes radio initialization, busy state, current parameters, and
  asynchronous read/write/connect operations. Its protocol polling runs from
  `StartMainTask`.
- The shell `upgrade` command enters the STM32 system bootloader directly.
  There is no application-level OTA staging, image authentication, or rollback
  flow in the current repository.
- The filesystem is mounted separately and may be unavailable when flash is
  blank or invalid. Startup deliberately does not format storage.

## Recommended Architecture

```text
Browser
  ↓ HTTP
applications/web_server
  ├─ raw lwIP TCP callbacks and bounded pbuf/HTTP handling
  ├─ one active browser connection at a time
  ├─ bounded routes and response encoding
  ├─ dashboard asset from index.html, embedded in firmware
  ├─ read-only status snapshot
  └─ validated commands queued for application context
       ↓
applications/rf_app / network status API / file_system_port
       ↓
lwIP raw TCP API (tcp_pcb) and pbuf chains
       ↓
Ethernet interface and device drivers
```

The web server must not call STM32 HAL or manipulate lwIP's internal state
directly. Keep transport handling, HTTP routing, and device configuration
behind separate APIs. Do not perform radio transactions, flash writes, or
blocking work in the TCP server callback.

### Transport and HTTP handling

- Use lwIP's raw TCP API directly (`tcp_new`, `tcp_bind`, `tcp_listen`,
  `tcp_accept`, `tcp_recv`, `tcp_sent`, `tcp_err`, `tcp_poll`, and
  `tcp_write`/`tcp_output`) instead of BSD sockets, netconn, or
  `applications/tcp`. Do not enable a second HTTP stack.
- Initialize and bind the listener after lwIP startup. The listener must
  remain valid when Ethernet has no link or DHCP has not yet assigned an
  address so it can serve the device once the interface becomes available.
- Support exactly one active web client connection. Keep a single statically
  allocated connection context; while it is occupied, abort any newly
  accepted PCB and return `ERR_ABRT` from the accept callback. Do not allocate
  a per-client pool or queue additional clients.
- Use pbuf chains as receive input. Consume every pbuf exactly once, including
  error and disconnect paths; copy only bounded request bytes into the
  connection's fixed-size parser buffer, call `tcp_recved()` for consumed
  bytes, and release pbuf ownership with `pbuf_free()` according to lwIP raw
  API callback rules. Never retain a pbuf after returning from its callback.
- Handle requests split across pbufs and TCP callbacks, and handle response
  writes split across available TCP send-buffer/window space. Use
  `TCP_WRITE_FLAG_COPY` for response data whose source buffer may be reused,
  check all raw API return codes, and retry queued response data from sent/poll
  callbacks when the TCP window permits. Do not assume a callback contains a
  complete HTTP request or that a whole response fits in one `tcp_write()`.
- Raw TCP callbacks execute in lwIP's TCP/IP core context. Keep them bounded
  and non-blocking: parse and snapshot only, then hand configuration requests
  to application context through a bounded queue/mailbox. Do not call radio
  operations, LittleFS, FreeRTOS blocking APIs, or wait for TCP output from a
  raw callback.
- On normal completion, unregister callbacks and close the PCB with the raw
  API, handling `tcp_close()` failure without losing ownership of the PCB.
  On abort/error paths, clear the static connection state and never reuse a
  PCB after it has been aborted or freed.
- Limit request/header bytes and JSON body bytes. Reject oversized, malformed,
  unsupported, or invalid requests with a bounded HTTP error response when
  possible, then close the connection cleanly.
- Support only the methods and routes needed by the first release. Reject
  unsupported methods, unknown paths, duplicate/invalid fields, and bodies
  exceeding configured limits.
- Serve `index.html` from firmware resources so the dashboard works even when
  LittleFS cannot mount. Do not expose arbitrary filesystem paths or serve
  uploaded files.

### Implemented firmware module

`web_server_init()` creates its one-entry static FreeRTOS command queue and
queues listener creation with `tcpip_callback()` after `MX_LWIP_Init()`.
`StartMainTask` calls `web_server_process()` beside `rf_app_process()`;
that function refreshes the application-owned radio/storage snapshot and
dispatches accepted radio operations. Raw TCP callbacks only validate requests,
copy bounded commands to the queue, format responses, and transmit through
`tcp_write()` / `tcp_output()`.

The listener is bound to `gnetif` with `tcp_bind_netif()`. It allows one
connection, closes each response connection, and aborts additional accepted
clients while that connection is active. Request headers are limited to
1024 bytes and JSON bodies to 512 bytes. Chunked transfer encoding, keep-alive,
request pipelining, unknown JSON fields, duplicate fields, and partial
configuration payloads are rejected. Incomplete requests are closed after
15 seconds without receive progress. The radio configuration endpoint accepts
the ten public dashboard fields only; security flags and keys are preserved
from the current parameter snapshot.

The source assets are `index.html`, `styles.css`, `app.js`, and
`X-favicon.png`. `web_server_assets.h` contains their generated `static const
uint8_t` arrays. To prevent browsers opening competing connections against the
single-client TCP server, generated HTML inlines its stylesheet, script, and
favicon into one response. HTML, CSS, and JavaScript are stored as deterministic
gzip streams and served with `Content-Encoding: gzip`; the favicon remains PNG.
This keeps generated text assets compact in flash while allowing browsers to
decompress them transparently. API polling then uses sequential requests.
Regenerate the arrays after changing a source asset with:

```sh
python3 applications/web_server/tools/generate_assets.py \
  applications/web_server applications/web_server/web_server_assets.h
```

`file_system_port_is_mounted()` reports storage state without mounting or
formatting. The filesystem view obtains capacity and directory metadata from
the application task; the TCP callback only queues a bounded directory scan
and formats its completed snapshot. Settings persistence is handled by the
separate settings module, not by the web server. The server does not persist
radio settings or expose firmware upload. HTTP is unauthenticated and
unencrypted; use only on a trusted isolated LAN. The settings API deliberately
excludes MQTT credentials and TLS trust material. The filesystem view exposes
directory names, types, and sizes and allows downloading selected files. It
does not support filesystem mutations. Downloaded files may contain sensitive
information, so the unauthenticated service must remain on a trusted isolated
LAN.

### API surface

| Method and path | Purpose |
|-----------------|---------|
| `GET /` | Serve the dashboard. |
| `GET /styles.css` | Serve the dashboard stylesheet for sequential loading. |
| `GET /app.js` | Serve dashboard behavior. |
| `GET /X-favicon.png` | Serve the embedded logo asset directly. |
| `GET /api/v1/status` | Return Ethernet/storage status, FreeRTOS heap and task metrics, sampled CPU utilization, and the current operation state. |
| `GET /api/v1/files?path=<encoded>&requestId=<id>&offset=<n>` | Return LittleFS capacity and a bounded page of read-only directory metadata. |
| `GET /api/v1/files/download?path=<encoded>&requestId=<id>&offset=<n>` | Return one bounded binary chunk from a selected regular file; repeat with the next offset to download it. |
| `GET /api/v1/radio/config` | Return the currently known radio parameters, or an explicit unavailable/busy result. |
| `POST /api/v1/radio/read` | Request an asynchronous read of radio configuration. |
| `PUT /api/v1/radio/config` | Validate and request an asynchronous update using the existing `rf_app` API. |
| `GET /api/v1/settings` | Return persisted non-secret MQTT, Ethernet, and supported UART settings, save state, and the unprovisioned product variant (`null`). |
| `PUT /api/v1/settings/mqtt` | Validate and queue MQTT non-secret settings for persistence and reboot-time application. |
| `PUT /api/v1/settings/ethernet` | Validate and queue DHCP/static IPv4 settings. |
| `PUT /api/v1/settings/uart7`, `/usart3`, `/usart10` | Validate and queue a supported physical UART configuration for reboot-time application. |
| `POST /api/v1/settings/network/confirm` | Confirm the active static IPv4 configuration during the 60-second startup window. |

Settings PUT requests require complete group objects; partial, duplicate, or
unknown fields are rejected. A successful response means the update was queued,
not yet persisted. The shared `saveState` in the GET response reports `pending`,
`saved`, or `failed`. MQTT username/password and CA/private-key material are
never part of the settings schema. UART9 is owned by the Zigbee radio
configuration; LPUART1 is reserved for the console. Static network
configuration must be confirmed after reboot within 60 seconds or the
application restores DHCP.

### Runtime monitoring

The status response reports FreeRTOS heap capacity, currently free bytes, and
the minimum-ever free bytes. The dashboard shows current heap use as
`capacity - free`, both in KiB and as a percentage; this is the FreeRTOS
heap-4 pool, not total MCU SRAM. Task state, priority, and stack high-water
values are sampled into a fixed 16-task buffer; stack values are reported in bytes.
`taskCount` is the total number of tasks and `tasksTruncated` indicates whether
the response omits task details because the total exceeds that fixed limit.

Runtime statistics use the Cortex-M DWT cycle counter. Overall CPU utilization
is derived from the idle-task share over approximately one second; it is
reported in tenths of a percent with `cpuAvailable: false` until a complete
sample exists. Each listed task also includes its sampled CPU share. CPU values
are not available until a non-zero runtime sample exists.

Use JSON with stable field names and explicit error responses. Return
appropriate HTTP status codes (for example 400 for invalid input, 409 when a
radio operation is already in progress, and 503 when a required subsystem is
unavailable). Do not report a configuration update as successful until the
radio completion callback confirms it.

The dashboard assets in `index.html`, `styles.css`, and `app.js` expect this
initial response contract:

```json
{
  "network": {
    "linkUp": true,
    "interfaceUp": true,
    "dhcp": true,
    "ip": "192.0.2.10",
    "netmask": "255.255.255.0",
    "gateway": "192.0.2.1"
  },
  "storage": { "mounted": true },
  "system": {
    "heapTotalBytes": 65536,
    "heapFreeBytes": 42100,
    "heapMinimumFreeBytes": 38000,
    "cpuAvailable": true,
    "cpuTenthsPercent": 123,
    "taskCount": 1,
    "taskReported": 1,
    "tasksTruncated": false,
    "tasks": [{
      "name": "mainTask",
      "state": "running",
      "priority": 2,
      "stackHighWaterBytes": 1024,
      "cpuTenthsPercent": 150
    }]
  },
  "operation": { "kind": "none", "state": "idle", "error": 0 }
}
```

`GET /api/v1/radio/config` returns `initialized`, `busy`, `configAvailable`,
and a `config` object with `panId`, `channel`, `userAddress`, `pointType`, `transferModel`,
`baudRate`, `dataBits`, `stopBits`, `parity`, and `antennaSelect`. Numeric
values use the firmware's existing enum/range values. `POST
/api/v1/radio/read` accepts an empty JSON object. `PUT /api/v1/radio/config`
accepts those same fields, excluding all security flags and keys. The firmware
implements these contracts and preserves current device security settings;
updates are refused until a successful parameter read establishes those
settings; security fields are never parsed from HTTP or overwritten by a
browser update.
Successful read/update requests should return `202 Accepted` with an
`operation` object in `pending` state; the status endpoint should keep
reporting that operation until the radio callback reports completion.
The dashboard uses the Flip7-inspired light palette across its responsive
Monitor, Settings, and File system views. It polls status every four seconds
(backing off after request failures) and pauses polling while its tab is
hidden. The file-system view reports LittleFS capacity, browses directory
names and metadata, and downloads files in bounded chunks. It does not expose
filesystem mutations, firmware upload, or security-key editing.

The radio fields and shared validation ranges are defined by
`ZigbeeParameter_t` and the Zigbee parameter constraints used by the CLI.
Avoid exposing security codes or other credentials in status responses or
logs.

### Device status model

The application-owned snapshot avoids having HTTP handlers read mutable module
internals. It includes:

- Ethernet: link up/down, interface up/down, IPv4 address, netmask, gateway,
  DHCP state.
- Radio: initialized/busy state and the latest successfully read parameter
  set.
- Storage: mounted/available state, without triggering mount or format from a
  web request.
- System: FreeRTOS heap capacity, free/minimum-free bytes, total task count,
  bounded task details, stack high-water values, and sampled CPU utilization.
- Last operation: kind, pending/success/failure state, and a safe error code.

The task-context producer updates the radio/storage snapshot; network fields
are read from `gnetif` inside the lwIP callback context. Snapshot copies are
protected briefly, before formatting or sending HTTP responses. The server
does not claim topology, sensor, or signal metrics that are not exposed
through stable APIs.

## Dashboard Scope

`index.html` should provide a responsive single-page dashboard with:

1. **Network card** — link and DHCP state, IP address, netmask, gateway.
2. **Radio card** — initialized/busy state and last known radio configuration.
3. **Storage card** — mounted/available state.
4. **System card and task table** — heap and sampled CPU usage, task states,
   priorities, stack high-water values, and task CPU share.
5. **Configuration form** — PAN ID, channel, user address, role, transfer
   mode, UART baud rate/parity, and antenna selection; security keys are not
   displayed or changed by the dashboard.
6. **Operation feedback** — pending state, completion result, and retry guidance.

Poll the status endpoint at a conservative interval (initial target: 2–5
seconds), avoid overlapping requests, and stop or back off when the page is
hidden or the device is unreachable. The page must remain usable without
external fonts, scripts, CDNs, or internet access.

## Dashboard Views

The dashboard provides separate Monitor, Settings, and File system views.
Monitor and File system are read-only; Settings contains radio and supported
device settings. This is a UI separation only: it does not create another web
server task or bypass existing application-context handoffs.

### Monitor view (implemented)

- Keep the network, radio, storage, CPU, heap, task, and operation summaries on
  the Monitor view.
- Use the `#monitor` entry point for direct links to the standalone view.
- Preserve bounded polling, hidden-tab pause, and retry backoff. Do not start
  parallel status requests when switching views.
- Clearly mark stale/unavailable samples and retain the distinction between
  the FreeRTOS heap pool and total MCU SRAM.
- Keep the view navigation client-side so it does not create extra requests
  that conflict with the single-client connection limit.

### File system view (implemented)

- Use the `#filesystem` entry point to display LittleFS total, allocated, and
  unallocated logical capacity plus counts for the current directory.
- Browse directory names, types, and file sizes in pages of at most eight
  entries. The API path is root-relative, rejects traversal components, and
  accepts only read-only GET requests.
- Use Back to navigate to the parent directory.
- Download regular files through `GET
  /api/v1/files/download?path=<encoded>&requestId=<id>&offset=<n>`. Each
  response contains at most 1024 bytes; the browser requests sequential
  chunks and assembles the file. File size and offsets are limited to signed
  32-bit range.
- Queue directory enumeration, LittleFS usage scans, and file reads to
  application-task context; never invoke filesystem APIs from a raw TCP
  callback.
- Scan only when opening/navigating the page or using its Refresh button; do
  not run full-volume scans on the dashboard's periodic status poll.
- File contents are now downloadable; do not expose create, update, delete,
  rename, unmount, or format operations.
- The service remains unauthenticated HTTP. Directory names, sizes, and file
  contents may disclose sensitive information, so keep the feature on a
  trusted isolated LAN until the access-control and TLS gate is complete.

### Radio settings (basic operations implemented)

- Keep the current read and update flows in the Settings view. They use the
  existing `rf_app` API, validate the public fields, preserve radio security
  parameters, and report asynchronous completion.
- Do not display or accept radio security keys through the dashboard.
- Radio settings remain outside the device-settings persistence store. Any
  future radio persistence requires a separate storage, validation, and
  recovery decision; never format storage automatically.
- Keep radio operations serialized and reject unavailable, busy, malformed,
  or out-of-range requests with explicit status.
- Non-secret MQTT, UART7/USART3/USART10, Ethernet IPv4, and read-only product
  variant settings are tracked in
  [`applications/settings/settings_plan.md`](../settings/settings_plan.md)
  and implemented through the settings module.

### Login and web access control (deferred; not implemented)

- Login is explicitly deferred. The current server remains plain HTTP with no
  authentication; do not describe the dashboard as protected.
- **Selected user experience:** use the browser-native HTTP Basic
  authentication prompt over HTTPS. Do not build a custom HTML login page:
  Basic authentication is enforced by the HTTP server and does not create an
  application-managed login form or session.
- Do not add the proposed `admin` / `admin` credentials or any shared default
  password. They are guessable and would provide false security, especially
  for MQTT secrets or firmware updates.
- The previously selected TLS plus HTTP Basic design is not ready to
  implement: this web server uses lwIP raw-TCP callbacks and has no HTTPS
  transport adapter, and the device-specific credential provisioning and
  recovery workflow is still undecided. The proposed certificate paths
  `/web/tls/server.crt` and `/web/tls/server.key` are not currently loaded by
  the server and do not by themselves provide a login credential.
- Resume implementation only after an HTTPS transport and unique
  per-device/physically provisioned admin credential workflow are approved.
  Until then, keep the current service restricted to a trusted isolated LAN
  and do not expose MQTT secrets or firmware update controls.

- Use HTTPS with HTTP Basic authentication, as selected for the device console.
  The browser's native username/password prompt is the login experience; do not
  build a separate HTML login form or imply that Basic authentication provides
  a server-managed login session or reliable logout.
- Require authentication for every HTTP route, including dashboard assets,
  status, radio configuration, MQTT settings, and any future update route.
  Return `401 Unauthorized` with a fixed realm for missing or invalid
  credentials. Do not serve protected data over plain HTTP or silently
  downgrade/redirect an HTTP request to an unauthenticated route.
- Integrate an approved TLS implementation into the web transport before
  parsing credentials or serving protected responses. The currently
  configured mbedTLS server support alone is not sufficient; the raw TCP ↔ TLS
  record path, bounded buffers, handshake timeouts, certificate loading, and
  connection cleanup must be implemented and validated. Fail closed if
  certificate/key files are absent, invalid, expired, mismatched, or
  unsupported.
- Provision the server certificate and private key through a controlled
  manufacturing/service workflow. Do not embed a shared production private
  key in firmware, log or serve private-key material, or accept replacement
  keys from an unauthenticated web request. Define certificate rotation and
  recovery for invalid or expired material.
- Provision a unique administrative credential per device or require a
  physically controlled first-boot setup procedure. Never ship a shared
  default password. Store only a password verifier using a reviewed,
  salt-based password-hashing scheme with a work factor suitable for the MCU;
  document entropy, verifier storage, reset, and recovery requirements before
  choosing the implementation.
- Bound credential/header sizes and authentication attempts. Rate-limit
  repeated failures without allowing a remote client to permanently lock out
  service, compare verifiers without avoidable timing leakage, and ensure
  credentials never appear in logs, status snapshots, or error bodies.
- Keep state-changing requests same-origin only, reject cross-origin requests,
  and do not enable permissive CORS. Test browser behavior with cached Basic
  credentials; do not claim a logout action unless a separately designed
  session mechanism is adopted.
- Until HTTPS and authentication are fully implemented and tested, keep the
  current plain-HTTP service restricted to a trusted isolated LAN and do not
  expose MQTT secrets or firmware update controls.

#### Implementation plan and release gates

Implement login only after the following decisions and stages are complete.
Do not expose MQTT secrets, firmware update operations, or other privileged
routes while any prerequisite is unresolved.

1. **Approve device provisioning and recovery.** Define how each device gets
   its unique TLS certificate/private key and administrator credential,
   whether first-boot physical setup is needed, how certificate trust is
   established by the browser, and how credentials/certificates are rotated
   and recovered. Document where private material and the password verifier
   are stored, how power loss affects setup, and who can reset access. Do not
   proceed with a shared production identity or password.
2. **Implement and validate HTTPS transport.** Select and integrate a
   supported TLS server adapter for lwIP raw TCP. Bound handshake time,
   record sizes, and static buffers; handle disconnect, timeout, malformed
   handshakes, and exhausted resources without leaking connection state.
   Load provisioned certificate/key material through a controlled source and
   fail closed if it is missing, invalid, expired, mismatched, or unsupported.
   Plain HTTP must not serve the console or accept credentials after HTTPS is
   enabled; do not silently downgrade.
3. **Implement credential verification.** Add a bounded Basic-auth parser
   and verifier that runs only after TLS is established. Use a reviewed
   salt-based password-hashing scheme with an MCU-appropriate work factor;
   never store or log plaintext passwords. Bound credential sizes, compare
   verifiers without avoidable timing leakage, and rate-limit repeated
   failures without creating a remotely triggerable permanent lockout.
4. **Protect the complete route surface.** Challenge unauthenticated requests
   with `401 Unauthorized` and a fixed realm before returning dashboard HTML,
   embedded assets, API data, or error details. Apply the same policy to all
   future MQTT and firmware-update routes. Reject cross-origin state-changing
   requests and keep CORS disabled unless a separately reviewed requirement
   justifies it.
5. **Validate browser behavior and failure cases.** Test successful and
   failed credentials, missing/invalid provisioning, expired or mismatched
   certificates, handshake and request timeouts, retry limits, and bounded
   memory use. Verify that cached browser credentials behave as expected.
   Do not present a logout button: browsers may retain Basic credentials, so
   reliable logout requires a separately designed session mechanism.
6. **Enable dependent features only after the security gate passes.** Confirm
   on-device HTTPS, credential provisioning/recovery, complete route coverage,
   and failure handling before exposing MQTT credentials or firmware update
   controls. Keep those features unavailable if any check fails.

### Device settings (non-secret settings implemented)

- The settings module persists validated MQTT connection fields, Ethernet
  IPv4 settings, and UART7/USART3/USART10 settings in versioned, checksummed
  LittleFS records. It applies these values during startup; web updates are
  saved asynchronously and require reboot.
- MQTT enabled state, host, port, client ID, keep-alive, and TLS mode are
  exposed by the API. MQTT username/password and CA/trust material are
  excluded from the settings model and must never appear in status, logs, or
  error responses.
- Per user selection, the current unauthenticated HTTP interface may change
  non-secret settings only. It must remain on a trusted isolated LAN. Login
  and HTTPS remain prerequisites before credentials or other privileged
  operations are exposed.
- Static Ethernet configuration reverts to DHCP unless confirmed within
  60 seconds after reboot. UART9 remains owned by radio settings and LPUART1
  remains the console. Product variant is read-only and currently unprovisioned.
- Storage format recovery, validation boundaries, MQTT TLS mode, startup
  application, and on-board network rollback require focused testing.

### Firmware upgrade (design gate; upload not implemented)

- Keep the upgrade section below as a prerequisite gate. The existing shell
  `upgrade` command enters the STM32 system bootloader; it is not an HTTP upload
  mechanism.
- First decide between operator guidance for external programming and true
  authenticated OTA. Do not add an upload control or update route until flash
  layout, staging, protected image verification, activation, rollback, and
  recovery behavior are approved and implemented.
- If OTA is approved, transfer and verification must be bounded and
  asynchronous, with signed-image checks and explicit progress/error state.
  Never activate an incomplete, unauthenticated, incompatible, or invalid
  image, and do not reboot before the final HTTP response has been sent.

## Security and Safety Requirements

- Bind to Ethernet only; never expose the service on a debug UART or unrelated
  network interface.
- Treat all HTTP input as untrusted. Enforce strict route, method, content
  length, field, and numeric range checks with fixed-size storage.
- Do not add a generic command endpoint, filesystem mutation, firmware
  upload, or unauthenticated destructive operation. The approved bounded
  file-download endpoint is read-only and restricts access to validated
  paths under the LittleFS root.
- The initial HTTP endpoint is plain HTTP and provides no transport encryption.
  Until authentication/TLS policy is selected, restrict use to a trusted,
  isolated LAN; do not expose it to the public internet.
- Configuration writes affect the radio module. Require explicit form
  submission, allow only one operation at a time, and report asynchronous
  completion. Never format flash from the web server.
- Firmware installation is a separate, destructive operation. Do not expose
  an upload or reboot action until a verified update/rollback design and an
  access-control policy are approved.
- Device-level non-secret settings are persisted by the settings module using
  versioned records and a static save task. Radio settings remain outside that
  store. Do not add secrets or firmware-update state to this storage path
  without a separate security and recovery design.

### Firmware upgrade planning

The web upgrade feature must not simply call the existing `upgrade` shell
command: that command transfers control to the STM32 system bootloader, which
expects its supported external programming protocol and does not implement a
browser upload or validate an application image.

Before enabling web-based firmware installation:

- Decide whether the web UI should only instruct an operator to use the
  existing external bootloader/programmer workflow, or whether the product
  requires true in-application OTA.
- For OTA, first establish the STM32H5 flash layout, application and staging
  capacity, boot-selection mechanism, and whether a protected resident
  bootloader can verify and activate a staged image. Do not overwrite the
  currently running image in place.
- Require authenticated and authorized update requests. Plain HTTP on an
  isolated LAN is not sufficient protection for a firmware-writing endpoint;
  select TLS/authentication or a separately approved physically controlled
  provisioning boundary before implementation.
- Require a signed image and verify its signature, target hardware,
  firmware/version policy, image length, and integrity before activation.
  A checksum alone is not authenticity verification.
- Design power-loss behavior, interrupted uploads, staging cleanup, recovery
  or rollback, anti-downgrade policy, and a post-boot health confirmation.
  Invalid, partial, incompatible, or unauthenticated images must never be
  activated.
- Do not assume LittleFS is suitable as the OTA staging area. Prove its
  capacity, erase/write endurance, concurrency, and recovery behavior against
  the flash map and bootloader requirements first.
- Keep transfer handling bounded and asynchronous. Report upload/verification/
  activation state and safe error codes; do not block the TCP callback or
  reboot while a response is still being sent.

If OTA is approved, a future API may use an authenticated
`POST /api/v1/firmware/update` for a bounded/chunked upload and
`GET /api/v1/firmware/update/status` for progress and result. These routes are
proposals only and must remain disabled until the security and bootloader
design is implemented and independently tested.

## Implementation Phases

### Phase 1 — Status and transport (implemented)

- Add `applications/web_server` as a separate CMake target.
- Define one static raw-TCP listener, one active-connection context, bounded
  pbuf parsing state, and HTTP request/response limits.
- Integrate the raw-TCP listener after `MX_LWIP_Init()` and report listener
  setup failures.
- Add radio/storage snapshots and report network state from `gnetif` in the
  lwIP callback context.
- Serve the embedded dashboard and implement `GET /api/v1/status`.
- Firmware build passes; production raw-TCP protocol tests and board verification
  remain follow-up validation.
- Keep firmware installation out of scope until the upgrade prerequisites
  below have been resolved.

### Phase 2 — Read and configure radio (implemented)

- Define the browser/API data model from the existing radio parameter type.
- Reuse or extract existing CLI validation so web and shell inputs have the same
  accepted ranges.
- Add a bounded command handoff to application context; keep the TCP callback
  non-blocking.
- Implement asynchronous read/update routes and completion status.
- Host mock endpoint smoke tests pass. Production board testing of busy,
  unavailable, invalid, failed, and successful operation paths remains.

### Phase 3 — Separate Monitor and Settings views (implemented)

- Organize existing dashboard monitoring as a standalone Monitor view and
  radio controls under Settings without changing the existing single-client
  transport model.
- Preserve serialized polling and sequential asset/API requests.
- Provide direct `#monitor` and `#settings` links and client-side view changes.
- Browser verification of narrow screens, offline state, and operation
  feedback remains.

### Phase 4 — Non-secret device settings (implemented; validation pending)

- Add versioned, checksummed LittleFS records with alternating slots and a
  static save queue/task. The settings module does not format storage.
- Load persisted MQTT, Ethernet, and UART settings before starting lwIP/MQTT;
  use defaults when both slots are absent and report storage/corruption errors
  when the filesystem or an existing record is invalid.
- Expose group-specific GET/PUT APIs and a settings UI. Values are applied
  after reboot; credentials and TLS trust material remain excluded.
- Preserve DHCP as the default. A static configuration rolls back to DHCP
  unless confirmed within 60 seconds after boot.
- Validate record corruption/power-loss recovery, all API boundaries, startup
  application, and board-level network rollback behavior.

### Phase 6 — Firmware upgrade design gate

- Document the MCU flash map, current system-bootloader behavior, application
  image format, boot selection, and available staging capacity.
- Choose operator-assisted system-bootloader programming or a protected
  bootloader-based OTA architecture.
- Approve authentication/TLS, signing-key ownership, image compatibility and
  anti-downgrade rules, interrupted-update recovery, rollback, and post-boot
  confirmation before adding upload routes or UI controls.
- If OTA is not supportable safely on the current hardware/boot chain, expose
  only operator guidance for the existing external programming process.

### Phase 7 — Board and browser verification

- Build the firmware and verify link-up, DHCP, and web-server startup on the
  STM32H5 board.
- The current settings API is intentionally non-secret but remains
  unauthenticated HTTP. Login/TLS validation is a separate deferred gate
  before any secret or privileged operation is exposed.
- Open the dashboard from a browser using the DHCP-assigned address.
- Verify monitor refresh, radio read/update, non-secret settings changes,
  repeated connections, disconnects, malformed requests, oversized input,
  and radio/network outages.
- If OTA is approved and implemented, also test invalid signatures, wrong
  hardware/version, interrupted uploads, power loss during activation,
  rollback, and successful post-boot confirmation on hardware.
- Measure TCP/IP task stack high-water marks and heap usage; verify that the
  single-connection state, pbuf processing, and response path remain bounded.
- Update hardware, task, and module documentation with final port, task, memory,
  and security settings.

## Acceptance Criteria

- A browser on the local Ethernet network can load the dashboard after DHCP
  assigns an address.
- Status values agree with the network interface and application-owned
  snapshots; unavailable values are clearly marked rather than fabricated.
- Invalid or oversized requests are rejected without out-of-bounds access,
  excessive allocation, or task stalls.
- Only one web client is active at a time; concurrent connection attempts are
  aborted with the documented raw-API callback result, without leaking PCBs
  or pbufs. Sequential reconnects must continue to work after disconnect.
- Raw TCP callbacks do not block and all received pbufs are released exactly
  once.
- Radio operations remain asynchronous and cannot overlap.
- Settings are validated, queued outside hardware mutation paths, persisted
  without automatic formatting, and applied after reboot. MQTT secrets and TLS
  trust material never appear in settings/status responses or logs.
- Static network settings revert to DHCP unless confirmed within the startup
  window.
- Firmware update remains unavailable unless the approved authentication,
  signed-image verification, staging, activation, and recovery requirements
  are implemented and tested.
- Existing TCP, MQTT, shell, radio, and filesystem functions continue to work.
- The firmware does not require a mounted filesystem to serve the dashboard
  and never formats flash automatically.

## Decisions Needed for Follow-on Features

1. Login is deferred; the proposed HTTPS/Basic design still needs an approved
   TLS transport and a per-device credential provisioning/recovery workflow.
2. Which additional device measurements are required beyond Ethernet, radio,
   and storage health?
3. Should firmware upgrade remain operator-assisted through the existing
   system-bootloader workflow, or is true in-application OTA required?
4. If OTA is required, what signing authority, bootloader/rollback guarantees,
   version policy, and authenticated update boundary are mandated?

## Local C Mock API

The host-only C mock server is under `applications/web_server/tests`. It serves
the HTML, CSS, JavaScript, and favicon assets, and implements the dashboard's
status, radio, and settings API routes with simulated values. Radio reads and
updates return a pending operation and transition to success after a short
delay; radio configuration writes are validated against the firmware's public
field ranges. The settings fixture accepts the supported group routes and
returns the same reboot-apply contract without touching hardware or storage.
This host fixture uses POSIX sockets for browser-based testing; production
firmware transport must use lwIP's raw TCP/pbuf API described above.

Build and run it from the repository root:

```sh
cmake -S applications/web_server/tests -B build/web-server-mock
cmake --build build/web-server-mock
./build/web-server-mock/web_server_mock_api
```

Open `http://127.0.0.1:8080/` in a browser. An optional port can be passed as
the executable's first argument. The mock binds to loopback only, does not
modify hardware, and does not replace the production STM32 HTTP server.