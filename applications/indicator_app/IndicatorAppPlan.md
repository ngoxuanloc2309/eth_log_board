# Device Indicator Application Plan

## Status

Implemented as a hardware-independent LED-pattern module, logical LED
interface, board adapter, and statically allocated service task. The LED
polarity, enclosure visibility, and board-level behavior still require
hardware verification.

## Goal

Define an application-owned behavior for the device's three LEDs:

- **Power**: show that firmware startup completed and the application is
  running.
- **Link**: show Ethernet physical-link state.
- **Status**: show normal operation or a coded, active application error.

Keep LED behavior deterministic and non-blocking. The indicator module owns
pattern timing and LED intent, but does not detect subsystem faults or access
GPIO/HAL directly.

## Existing Hardware and Unknowns

- CubeMX configures three output pins on GPIOC: `LED0` PC8, `LED1` PC7, and
  `LED2` PC6. `MX_GPIO_Init()` initializes all three outputs low.
- The required logical-to-physical mapping is:

  | Logical function | Board LED | MCU pin |
  |---|---|---|
  | Power | `LED0` | PC8 |
  | Link | `LED1` | PC7 |
  | Status | `LED2` | PC6 |

- Confirm each LED's active electrical polarity from the schematic or board
  measurements. Also confirm whether the power LED is software-controlled or
  tied directly to a power rail.
- Ethernet link state is available from the lwIP netif, but it must be
  published to the indicator owner through an application-level state update;
  the indicator module must not query lwIP directly.
- No shared project-wide error enum was found. Define an indicator-facing
  system fault enum rather than a generic repository-wide `error_t`.

## Proposed LED Behavior

| LED | Normal behavior | Fault behavior |
|---|---|---|
| Power | Steady on after application initialization succeeds | Off before initialization; steady on while firmware is alive |
| Link | Steady on when Ethernet physical link is up; off when down | No separate fault pattern; link state remains independently visible |
| Status | One short heartbeat pulse every 2 seconds when no error is active | Repeating blink-count code for the highest-priority active error |

Proposed heartbeat timing is 100 ms on / 1900 ms off. Error codes use 200 ms
on pulses with 200 ms off between pulses. The repeat period is 1 second per
blink in the error code: a one-blink error repeats every second, a two-blink
error every two seconds, up to an eight-blink error every eight seconds. The
remaining time in each period is the off interval before the next code group.
These values should be checked for visibility and usability on the target
enclosure.

During startup, keep status off until the indicator service is initialized.
Do not flash the power LED for faults: the status LED owns fault diagnostics,
while the link LED continues to report physical link independently.

## Enum-Driven LED Display

The indicator application accepts typed logical state and produces one
complete LED frame. The board layer only applies that frame; it does not
interpret error codes or create blink patterns.

| Input enum/state | Power LED (`LED0`) | Link LED (`LED1`) | Status LED (`LED2`) |
|---|---|---|---|
| `application_ready == false` | Off | Reflect current physical link if available; otherwise off | Off |
| `application_ready == true`, `INDICATOR_ERROR_NONE`, link down | On | Off | 100 ms heartbeat pulse every 2 seconds |
| `application_ready == true`, `INDICATOR_ERROR_NONE`, link up | On | On | 100 ms heartbeat pulse every 2 seconds |
| `application_ready == true`, any active `indicator_error_t` | On | Reflect current physical link | Blink count mapped from the error enum |

`INDICATOR_ERROR_NONE` selects the heartbeat pattern; every active error enum
selects its documented code on LED2. Thus power and link reporting continue
independently while LED2 presents the current error. The frame uses logical
on/off values, so active-low electrical polarity is handled only by the board
adapter.

The planned public types should include an enum identifying each logical LED
(`INDICATOR_LED_POWER`, `INDICATOR_LED_LINK`, `INDICATOR_LED_STATUS`) and an
enum for logical output (`INDICATOR_LED_OFF`, `INDICATOR_LED_ON`). A logical
frame contains one output value per LED. Time-varying blink/heartbeat output
is computed by the application module on each update and represented in that
frame as ON or OFF; do not encode GPIO levels or pin numbers in the enums.

## Proposed Error Enum

`indicator_error_t` represents a currently active subsystem fault to display.
It is not the return status for indicator API calls and must not be inferred
from LED pattern state.

| Enum value | Blink count | Repeat period | Meaning |
|---|---:|---:|---|
| `INDICATOR_ERROR_NONE` | Heartbeat | 2 s | No reportable error is active |
| `INDICATOR_ERROR_CONFIGURATION` | 1 | 1 s | Invalid or unavailable required configuration |
| `INDICATOR_ERROR_STORAGE` | 2 | 2 s | Settings or required storage operation failed |
| `INDICATOR_ERROR_NETWORK` | 3 | 3 s | Network initialization or address configuration failed |
| `INDICATOR_ERROR_MQTT` | 4 | 4 s | MQTT connection/service failure |
| `INDICATOR_ERROR_RADIO` | 5 | 5 s | Radio subsystem failure |
| `INDICATOR_ERROR_UART` | 6 | 6 s | Required UART initialization or operation failed |
| `INDICATOR_ERROR_INTERNAL` | 7 | 7 s | Internal application invariant or unrecoverable service failure |
| `INDICATOR_ERROR_UNKNOWN` | 8 | 8 s | Unclassified error report |

The implementation should use explicit enum names in application code and a
separate mapping function for blink counts; enum bit values must not determine
the user-visible pulse count. Invalid enum input must be reported explicitly
and must not silently become `INDICATOR_ERROR_NONE`.

The error bit values and blink-count/repeat-period mapping are stable
user-visible contracts: for example, `INDICATOR_ERROR_STORAGE` is always the
`0x0004` error bit and means two LED2 pulses with a 2-second repeat period.
The implementation should test every mapping. For an active code, LED2 is on
for 200 ms and off for 200 ms between pulses. The remaining portion of that
code's repeat period is off before the group starts again; the final pulse
does not add an extra inter-pulse off interval.

### Multiple active errors and clearing

Subsystems report and clear their own error condition through the application
owner. The owner selects the highest-priority active error for display using
this proposed order:

1. Internal
2. Configuration
3. Storage
4. Network
5. Radio
6. UART
7. MQTT
8. Unknown

Errors remain visible while active and clear only when the reporting owner
explicitly clears or resolves them. A transient event should be logged by its
owning subsystem; it should not become a permanent LED fault unless that
subsystem reports an active failure. The priority and latching policy must be
confirmed before implementation, especially for retrying network/MQTT
services.

## Proposed Architecture

```text
Subsystem owners
  ├── report/clear typed errors ──→ application indicator state
  └── publish Ethernet link state ─→ application indicator state
                                      ↓
                              indicator-task
                                      ↓
                           indicator_app_process()
                                      ↓ desired LED frame
                               indicator_control_if
                                      ↓
                             board GPIO adapter
```

- Keep pure pattern selection and elapsed-time handling in
  `applications/indicator_app`; it must not include STM32 HAL, lwIP, or
  FreeRTOS headers.
- Define a narrow board-owned LED interface in `interface/`. It should accept
  logical on/off states for the three named LEDs, not GPIO pin numbers.
- Map logical LED names and active electrical levels in `board/`.
- The application integration owns the FreeRTOS task, event/state handoff,
  static task storage, and interaction with subsystem owners.
- Perform GPIO writes in task context. No logging, pattern calculations, or
  GPIO access from an ISR.
- Give the indicator state a single application-level owner or protect
  updates with a documented thread-safe mechanism. Avoid unsynchronized writes
  from multiple subsystem tasks.

## Proposed Module Contract

The implementation should provide a small, hardware-independent API along
these lines:

```c
typedef enum
{
    INDICATOR_ERROR_NONE = 0x0000,
    INDICATOR_ERROR_CONFIGURATION = (0x0001U << 1U),
    INDICATOR_ERROR_STORAGE = (0x0001U << 2U),
    INDICATOR_ERROR_NETWORK = (0x0001U << 3U),
    INDICATOR_ERROR_MQTT = (0x0001U << 4U),
    INDICATOR_ERROR_RADIO = (0x0001U << 5U),
    INDICATOR_ERROR_UART = (0x0001U << 6U),
    INDICATOR_ERROR_INTERNAL = (0x0001U << 7U),
    INDICATOR_ERROR_UNKNOWN = (0x0001U << 8U),
    INDICATOR_ERROR_COUNT = (0x0001U << 9U)
} indicator_error_t;

typedef enum
{
    INDICATOR_RESULT_OK = 0,
    INDICATOR_RESULT_INVALID_ARGUMENT,
    INDICATOR_RESULT_NOT_INITIALIZED
} indicator_result_t;
```

The exact public API and state structure should be settled during
implementation. It should accept startup-ready state, Ethernet link state,
and the selected active error, then return a logical LED frame for the
provided monotonic time. The pure module must return an explicit result for
invalid state or invalid enum values; board write failures must also be
reported by the board adapter and application integration.

The module contract must keep enum roles separate:

- `indicator_error_t` selects the LED2 diagnostic code.
- A logical LED identifier enum selects which named output is being addressed.
- A logical LED output enum carries only `OFF` or `ON` in the computed frame.
- API result enums report call validity and initialization errors; they are
  not LED display states or subsystem faults.

## Timing and RTOS Plan

- Run as a statically allocated, low-priority service task at an initial
  50 ms update period. Confirm that period against chosen pulse timings and
  task-latency budget.
- Use `vTaskDelayUntil()` or an equivalent event-driven wait; avoid drift from
  repeated relative delays.
- Use unsigned elapsed-time subtraction for wrap-safe timing.
- Compute all LED outputs in one logical frame and apply it through the board
  interface. Do not scatter GPIO writes among subsystem tasks.
- Coalesce state updates so a burst of network/error events does not queue
  unbounded work.

## State and Pattern Precedence

```text
UNINITIALIZED
  └─ service initialized → STARTUP
       └─ application ready → NORMAL
            ├─ link changes → NORMAL (link LED updated independently)
            └─ active error → ERROR_CODE
                 └─ error cleared → NORMAL
```

Power readiness and Ethernet link are independent of the status pattern.
An active error overrides the status heartbeat. If the selected error changes
while a code is blinking, restart the code at its next safe pattern boundary
to avoid presenting a mixed code. All transitions and output states must be
deterministic for identical input state and timestamps.

## Error Handling and Observability

- Validate the interface and state during startup; report initialization or
  GPIO write failures through repository-standard logging.
- Do not silently convert missing LED hardware callbacks, an invalid error
  enum, or board write failure into a successful frame.
- Log fault transitions when owners report/clear errors, not every task tick.
- The LED is diagnostic feedback, not a substitute for subsystem logs,
  telemetry, or recovery. It must not change subsystem behavior.
- Keep error detection and recovery owned by the affected subsystem. The
  indicator only presents the current selected fault.

## Validation Plan

### Host tests

- Every named error maps to the documented pulse count; `NONE` maps to the
  heartbeat.
- Higher-priority errors take precedence when multiple faults are active.
- Active errors remain visible until explicitly cleared.
- Startup-ready, link-up/link-down, and error transitions generate the
  expected independent LED outputs.
- Pattern phase, code restart behavior, and exact timing boundaries are
  deterministic, including timestamp wraparound.
- Invalid/uninitialized state and invalid enum values return explicit errors.

### Firmware and board checks

- Build the firmware and verify static task/stack usage and GPIO writes.
- Confirm each LED label-to-pin mapping, polarity, and power-rail relationship
  against the specified mapping and schematic or measurements before enabling
  outputs.
- Observe startup, steady-state heartbeat, Ethernet cable insertion/removal,
  each supported error code, simultaneous errors, error clearing, and reset.
- Check visibility in the final enclosure and verify the LED update task does
  not interfere with network, radio, or control timing.

## Decisions Required Before Implementation

1. Confirm each LED's active polarity. The board adapter currently assumes
   active-high outputs.
2. Confirm whether the power LED is GPIO-controlled or directly tied to the
   supply rail; a software indicator can only show firmware readiness.
3. Confirm the proposed heartbeat and per-error repeat-period timings on the
   target hardware.
4. Confirm which subsystem conditions count as active errors, especially
   recoverable/retrying MQTT and network disconnections, and approve the
   multi-error priority and clear policy.