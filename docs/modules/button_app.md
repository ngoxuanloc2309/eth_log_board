# Button Application Module

## Purpose

Recognize the device button's single-click, double-click, and long-hold
gestures without hardware dependencies.

## Responsibilities

- Debounce button input using a 30 ms stable-sample interval.
- Ignore one short click.
- Request reboot after a debounced second click that begins within 400 ms of
  the first debounced release.
- Request upgrade mode once after 10 seconds of stable pressed state.
- Use wrap-safe millisecond subtraction for gesture timing.

## Public API

Declared in `applications/button_app/button_app.h`:

```c
bool button_app_init(button_app_t *app);
button_app_action_t button_app_process(button_app_t *app,
                                       bool is_pressed,
                                       uint32_t now_ms);
```

The caller owns the state object and supplies one logical sample per scan.
The process function returns a one-shot action; the caller performs reboot or
bootloader entry in task context.

## Dependencies and Ownership

- Depends only on standard C types and no HAL, FreeRTOS, or MCU headers.
- `button_control_if_t` is the hardware-independent contract implemented by
  the board layer.
- A static `button-task` owns button sampling and dispatches returned actions.
- The board adapter maps GPIOD pin 3 and the existing reset/ROM-bootloader
  functions to the interface.

## Data Flow

```text
GPIOD button → board interface → button-task (10 ms fixed period)
                                  ↓
                           button_app state machine
                           ↙                 ↘
                      reboot request   upgrade-mode request
```

## State Machine

```text
IDLE → PRESSED → CLICK_PENDING → SECOND_PRESS
          │              │             │
          └─ 10 s ───────┴─────────────┴→ UPGRADE_MODE
                                         SECOND_PRESS release → REBOOT
```

Single clicks expire without action. Upgrade mode takes precedence over a
pending click and is reported only once. The existing upgrade action enters
the STM32 ROM bootloader; it is not OTA and requires an external programming
tool.

## RTOS Timing

`button-task` uses a fixed 10 ms period via `vTaskDelayUntil()`, priority
`tskIDLE_PRIORITY + 1`, and a 384-word static stack. It performs only one GPIO
sample, gesture-state processing, and action dispatch. It does not block on
communications or perform protocol work.

## Verification

Host tests cover no-op single clicks, double-click boundaries, bounce,
long-hold precedence, one-shot events, invalid state, and timer wraparound.
Target-board verification of electrical polarity, external bias, timing feel,
reset, and ROM bootloader entry remains required.
