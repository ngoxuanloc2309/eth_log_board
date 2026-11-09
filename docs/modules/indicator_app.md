# Indicator Application Module

## Purpose

Compute logical output patterns for the device's power, Ethernet-link, and
status LEDs, including a heartbeat and typed subsystem fault codes.

## Responsibilities

- Keep LED state and pattern timing independent of STM32 HAL, lwIP, and
  FreeRTOS.
- Show application readiness on LED0, physical Ethernet link on LED1, and
  heartbeat/error codes on LED2.
- Select the highest-priority active error and expose stable blink counts.
- Return one complete logical LED frame for the application task to apply.

## Public API

Declared in `applications/indicator_app/indicator_app.h`:

```c
indicator_result_t indicator_app_init(indicator_app_t *app);
indicator_result_t indicator_app_set_error(indicator_app_t *app,
                                           indicator_error_t error,
                                           bool active);
indicator_result_t indicator_app_process(indicator_app_t *app,
                                         bool application_ready,
                                         bool link_up,
                                         uint32_t now_ms,
                                         indicator_led_frame_t *frame);
```

`indicator_service_set_error()` in `indicator_service_if.h` queues active or
clear events from task context. It returns explicit invalid-argument,
not-ready, and queue-full results; callers should log failures instead of
assuming the displayed state changed.

## LED Mapping and Patterns

| Logical LED | Board LED | MCU pin | Behavior |
|---|---|---|---|
| Power | LED0 | PC8 | On after application startup |
| Link | LED1 | PC7 | On while Ethernet physical link is up |
| Status | LED2 | PC6 | 100 ms pulse each 2 s when healthy; coded active-error pulses otherwise |

Error flags use stable bit values. Blink counts are mapped explicitly and do
not derive from those bit values:

| Error | Bit value | LED2 pulses | Repeat period |
|---|---:|---:|---:|
| `INDICATOR_ERROR_CONFIGURATION` | `0x0002` | 1 | 1 s |
| `INDICATOR_ERROR_STORAGE` | `0x0004` | 2 | 2 s |
| `INDICATOR_ERROR_NETWORK` | `0x0008` | 3 | 3 s |
| `INDICATOR_ERROR_MQTT` | `0x0010` | 4 | 4 s |
| `INDICATOR_ERROR_RADIO` | `0x0020` | 5 | 5 s |
| `INDICATOR_ERROR_UART` | `0x0040` | 6 | 6 s |
| `INDICATOR_ERROR_INTERNAL` | `0x0080` | 7 | 7 s |
| `INDICATOR_ERROR_UNKNOWN` | `0x0100` | 8 | 8 s |

Each pulse is 200 ms on, with 200 ms off between pulses. The complete code
repeats once per second per pulse in that code; the remainder of that period is
the off interval before the next code group. `INDICATOR_ERROR_NONE` selects the
healthy heartbeat. Each nonzero error value is a distinct bit flag. When
several faults are active, priority is internal,
configuration, storage, network, radio, UART, MQTT, then unknown.

## Dependencies and Ownership

- The pure indicator module depends only on standard C types and
  `indicator_led_if.h`.
- The application integration owns the static error queue and
  `indicator-task`.
- The board layer implements `indicator_led_if_t`, mapping logical LED0/1/2
  to GPIOC pins PC8/PC7/PC6.
- The board adapter currently assumes active-high LED outputs. Confirm
  polarity and whether the power LED is software-controlled against the
  schematic or on hardware before release.
- `MX_LWIP_IsLinkUp()` publishes physical-link state to application
  integration; the pure module never reads lwIP state directly.

## Timing and Concurrency

`indicator-task` runs at 20 Hz with a 384-word static stack and
`tskIDLE_PRIORITY + 1`. Error events are transferred through a static
eight-entry queue; the indicator task alone mutates pattern state and writes
the LED frame. Time comparisons use unsigned millisecond subtraction for
wraparound safety.

## Verification

Host tests cover startup/readiness, independent link output, heartbeat timing,
wraparound, every error code, storage pulse boundaries, priority and clearing,
and invalid inputs. Physical LED polarity, visible timing, and board mapping
still require hardware verification.
