# FreeRTOS Task Design

## Tasks

| Task | Period | Priority | Stack | Responsibility | WCET |
|------|--------|----------|-------|----------------|------|
| `sx_spi` | Event-driven | `tskIDLE_PRIORITY + 1` | `2 * configMINIMAL_STACK_SIZE` words (512 words in this build) | Wait for SPI1 interrupt completion or timeout, release bus ownership, invoke completion callback | Callback-dependent; callbacks must be short and non-blocking |
| `settings` | Event-driven | `tskIDLE_PRIORITY + 1` | 384 words | Persist queued device settings to LittleFS and publish save state | Storage-dependent; may block on filesystem operations |
| `button-task` | 100 Hz | `tskIDLE_PRIORITY + 1` | 384 words | Sample button and dispatch one-shot reboot/ROM-bootloader gestures | Bounded GPIO sample and state update |
| `indicator-task` | 20 Hz | `tskIDLE_PRIORITY + 1` | 384 words | Apply power/link/status LED frame and display active error pattern | Bounded state update and three GPIO writes |

The task is created statically by `sx_spi_init()` before scheduler startup. It
does not poll; it blocks on a static one-entry request queue until an
asynchronous transfer is accepted.

The existing `mainTask` performs one-time filesystem initialization at startup
by calling `file_system_port_init()`. That call initializes the W25Q device
and mounts LittleFS; it does not format storage. W25Q busy polling delays in
task context. Normal filesystem operations use the port's static mutex to
serialize access to the shared W25Q context.

`settings_init()` runs after the filesystem mount attempt and before network
and MQTT startup. It creates a static one-entry queue and statically allocated
`settings` task; settings PUT handlers only enqueue validated records. The
task serializes LittleFS writes using the filesystem port's mutex and updates
the current settings snapshot only after a successful synchronized write.

The Ethernet web service does not add a task. Its raw TCP callbacks execute in
lwIP's TCP/IP thread and only parse bounded requests, update/copy a small
snapshot, queue radio commands, and send TCP data. `mainTask` calls
`web_server_process()` alongside `rf_app_process()` to refresh the snapshot
and dispatch queued radio operations. The web module's command queue is a
static one-entry queue; it never blocks the lwIP thread and it does not access
LittleFS from a network callback.
The same call refreshes fixed-capacity heap and task snapshots once per second.
FreeRTOS runtime statistics use the Cortex-M DWT cycle counter; the browser
reports CPU utilization from the idle task's share and marks the first sample
unavailable until a complete interval has elapsed.

`button-task` is statically created from `mainTask` after scheduler startup.
It samples the board button interface every 10 ms with `vTaskDelayUntil()`,
runs the hardware-independent button gesture state machine, and dispatches
reboot or ROM-bootloader requests from task context. It does not use an ISR or
share mutable gesture state with other tasks.

`indicator-task` is statically created from `mainTask` after system service
startup. It samples Ethernet physical-link state through the lwIP application
API, consumes typed error updates from a static queue, computes the complete
logical LED frame, and applies it through the board LED interface every 50 ms.
Only the indicator task owns the pattern state and performs LED writes.

Network-interface creation and configuration, DHCP startup, and Ethernet PHY
link-task up/down changes run through lwIP's `netifapi` functions because
TCP/IP core locking is disabled. The link callback publishes a cached atomic
link-state snapshot for application tasks; they must not inspect mutable
`gnetif` fields directly.

## Synchronization and ISR Handoff

| Resource | Type | Purpose |
|----------|------|---------|
| SPI bus | Static mutex | Serialize bus operations and provide priority inheritance |
| Async request | Static one-entry queue | Hand transfer buffers and callback to the service task |
| Async completion | Static binary semaphore | Transfer completion/error signal from SPI ISR to the task |
| Filesystem | Static mutex | Serialize LittleFS calls and shared W25Q device buffers |
| Settings save | Static one-entry queue | Hand complete validated snapshots to the settings persistence task |
| Indicator errors | Static eight-entry queue | Hand typed fault activation/clear events to the indicator task |

`SPI1_IRQHandler` remains in the HAL IRQ layer. HAL completion/error callbacks
only update transfer status and signal the task from ISR context. User
callbacks execute only in the `sx_spi` task. The SPI1 NVIC priority is
configured as 5 and must not be changed to a numerically smaller priority
than `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`.
