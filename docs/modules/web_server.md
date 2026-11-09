# Web Server Module

## Purpose

Provide a single-client Ethernet dashboard and a small HTTP API for network,
storage, and Zigbee radio status/configuration plus non-secret device settings.

## Responsibilities

- Serve the dashboard and favicon from firmware-resident constant arrays.
- Parse bounded HTTP requests with lwIP raw TCP/pbuf callbacks.
- Report network, radio, storage, FreeRTOS resource metrics, and asynchronous
  operation snapshots.
- Validate radio configuration requests and preserve security parameters.
- Validate and queue non-secret MQTT, Ethernet, and supported UART settings.
- Defer radio transactions to the application task through a static queue.
- Present monitor, settings, and file-system views as client-side views.
- Provide LittleFS capacity and directory metadata and bounded file downloads
  through an application-task handoff; never expose filesystem mutations.
- Apply the supplied Flip7-inspired teal, coral, gold, cream, and tactile
  card design consistently across all dashboard views.

## Public API

```c
bool web_server_init(void);
void web_server_process(void);
```

Call `web_server_init()` after `MX_LWIP_Init()` from task context. It queues
listener setup on the lwIP TCP/IP thread. Call `web_server_process()`
regularly from the task that owns `rf_app_process()`.

## Dependencies and Ownership

- `lwip/tcp.h` and `lwip/tcpip.h`: raw TCP transport; all PCB operations run
  on the lwIP TCP/IP thread.
- `rf_app`: radio state and asynchronous read/write operations; invoked only
  from the application task.
- `file_system_port`: mounted-state snapshot.
- `file_system`: logical capacity and directory metadata, accessed only from
  application-task context.
- Embedded assets: source files under `applications/web_server/`, generated
  into `web_server_assets.h`.

The web server owns one listener PCB, one connection context, fixed-size
request/response storage, and separate one-entry static queues for radio
commands and file-system directory scans. It creates no additional task and
performs no dynamic allocation.
The generated dashboard inlines its stylesheet, script, and favicon in the
HTML response to avoid concurrent browser asset requests against the
single-client connection limit. The embedded HTML, CSS, and JavaScript assets
are deterministic gzip streams and are served with `Content-Encoding: gzip`;
the browser decompresses them. The favicon remains PNG. API polling uses
sequential requests.
The `#monitor`, `#settings`, and `#filesystem` fragments switch views without
requesting a second page. Login is deferred and not implemented. The current
plain-HTTP listener has no authentication; do not use shared `admin` / `admin`
credentials or expose MQTT credentials or TLS trust material through it. The
proposed login uses
HTTPS with the browser-native Basic authentication prompt, but the TLS
transport and unique credential provisioning/recovery workflow are unresolved.
Do not create a standalone HTML login form for this design. See the
[login implementation plan](../../applications/web_server/webserver.md#login-and-web-access-control-deferred-not-implemented)
for prerequisite decisions, ordered stages, and release gates. Until these are
implemented and validated, keep the service restricted to a trusted isolated
LAN. The settings UI exposes only non-secret fields and displays this warning.

## Thread Safety and Data Flow

```text
Browser → raw TCP callback → radio queue ─────┐
Browser → raw TCP callback → file-system queue ├→ StartMainTask
                                               ├→ rf_app
                                               └→ file_system
Browser ← raw TCP callback ← protected snapshots
Browser → raw TCP callback → settings module → static persistence queue
```

TCP callbacks and `StartMainTask` share status and directory snapshots under FreeRTOS
critical sections. Radio operations and filesystem access never run in a TCP
callback. Directory scans and file reads are queued to `StartMainTask`; settings
persistence is owned by the settings module. File downloads expose file
contents to unauthenticated clients, so the service must remain on a trusted
isolated LAN. See the
[settings module contract](./settings.md) for record and reboot behavior.

## HTTP Surface

| Method | Route | Purpose |
|--------|-------|---------|
| `GET` | `/` | Dashboard HTML |
| `GET` | `/styles.css` | Dashboard styles |
| `GET` | `/app.js` | Dashboard behavior |
| `GET` | `/X-favicon.png` | Embedded logo/favicon |
| `GET` | `/api/v1/status` | Network, storage, heap, task, CPU, and operation state |
| `GET` | `/api/v1/files?path=<encoded>&requestId=<id>&offset=<n>` | LittleFS capacity and a paged, read-only directory listing |
| `GET` | `/api/v1/files/download?path=<encoded>&requestId=<id>&offset=<n>` | One bounded file-download chunk |
| `GET` | `/api/v1/radio/config` | Public radio config and radio state, including whether the configuration has been read successfully |
| `POST` | `/api/v1/radio/read` | Queue an asynchronous radio read |
| `PUT` | `/api/v1/radio/config` | Validate and queue a radio update |
| `GET` | `/api/v1/settings` | Read persisted non-secret settings and save state |
| `PUT` | `/api/v1/settings/mqtt` | Queue MQTT settings for persistence and reboot |
| `PUT` | `/api/v1/settings/ethernet` | Queue DHCP/static IPv4 settings |
| `PUT` | `/api/v1/settings/uart7`, `/usart3`, `/usart10` | Queue supported UART settings |
| `POST` | `/api/v1/settings/network/confirm` | Confirm active static networking during the startup rollback window |

Only one client is accepted at a time. Responses close the connection.
Headers are limited to 1024 bytes and bodies to 512 bytes. Requests with
chunked encoding, pipelining, unsupported routes, duplicate/unknown fields,
or invalid enum/range values are rejected. An incomplete request is closed
after 15 seconds without receive progress. The API does not expose security keys, filesystem mutations, or firmware
upload. Directory requests return `202 Accepted` while a scan runs in
application context, then
`200 OK` with logical volume capacity, allocated/unallocated block capacity,
current directory counts, and up to eight entries per response. Download
requests return `202 Accepted` while a file chunk is read in application
context, then `200 OK` with at most 1024 binary bytes and file-size/offset
headers. The browser requests chunks sequentially. File sizes and offsets are
limited to signed 32-bit range. Invalid paths, `.`/`..` traversal, unknown
query fields, and malformed percent encoding are rejected. Directory
metadata and downloadable file contents are unauthenticated information on
the trusted LAN.
`PUT /api/v1/radio/config` is unavailable until a successful module-parameter
read has established the current values, including the security fields that
must be preserved. Device settings are saved asynchronously in LittleFS and
apply after reboot. Static Ethernet settings must be confirmed within 60
seconds after boot or DHCP is restored. UART9 is managed by radio settings;
LPUART1 remains the console. Product variant currently returns `null` because
no provisioning source is defined.

## Runtime Metrics

The application task refreshes metrics at one-second intervals. Heap values
come from the FreeRTOS heap-4 allocator; the dashboard computes used heap from
capacity minus free bytes and shows the percentage. These values describe the
FreeRTOS heap pool, not total MCU SRAM. Task count is complete, while the
returned task list is reported only when the complete task set fits the fixed
16-entry buffer; otherwise task details are omitted and truncation is flagged.
Stack high-water marks are converted from stack words to bytes. CPU percentages
are sampled using the Cortex-M DWT cycle counter and the idle-task runtime;
they remain unavailable until a complete non-zero sample is collected.

The service is plain HTTP without authentication and must remain on a trusted,
isolated LAN until an access-control and transport-security policy is approved.
The file-system view exposes names, directory structure, and file sizes, and
allows read-only downloads. It does not permit writes, deletes, or formatting.
