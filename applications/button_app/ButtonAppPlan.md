# Device Button Application Plan

## Status

Implemented as a hardware-independent gesture state machine, a statically
created FreeRTOS task, and a board-owned input/system-control interface.
Physical button polarity and gesture timings still require target-board
verification.

## Goal

Define deterministic button gestures for the device:

- One click: no action.
- Double click: reboot the device.
- Continuous 10-second press: enter firmware upgrade mode.

The button handler must not perform hardware access or system transitions from
an interrupt callback. It must distinguish short and long presses reliably,
including switch bounce and ambiguous click sequences.

## Existing Hardware and System Behavior

- CubeMX currently configures `BUTTON_Pin` on GPIOD pin 3 as a GPIO input with
  no internal pull resistor. The board adapter currently assumes active-low;
  verify the electrical active level and external bias against the board
  schematic before release.
- `enter_bootloader_mode()` in `board/dtu_board.c` transfers execution to the
  STM32H563 system ROM bootloader. The existing shell `upgrade` command calls
  this routine; it does not perform an in-application OTA update.
- `bsp_restart` currently maps to `NVIC_SystemReset`. Application logic should
  invoke reboot and bootloader actions through an explicit board/system-control
  interface rather than calling HAL or registers directly.

## Gesture Contract

| Gesture | Recognition | Action |
|---|---|---|
| Single click | One debounced press and release with no second press before the double-click window expires | None |
| Double click | Two debounced press-release cycles, with the second press beginning within the configured inter-click window | Request a system reboot after the second release |
| Upgrade hold | Button remains continuously active for 10 seconds after debounce | Enter the STM32 system ROM bootloader once |

Initial timing values for implementation and verification:

- Button scan period: 10 ms in the existing application task.
- Debounce interval: 30 ms of stable input before accepting a transition.
- Double-click window: 400 ms from the first debounced release to the
  beginning of the second debounced press.
- Upgrade hold: 10 seconds of continuously debounced active input.

These are starting values and must be checked against the physical switch,
human interaction, and board feedback. Use wrap-safe RTOS tick/time arithmetic.
Do not count debounce time as part of the 10-second stable hold.

## Gesture Precedence and Safety

1. A stable press starts long-hold timing; do not trigger reboot on an initial
   click or on the first release.
2. A first short release starts a pending-click window. If that window expires
   without a second press, discard the click without action.
3. If a second press begins before the window expires, cancel the pending
   single click. Request reboot only after the second press is debounced and
   released, provided it did not become a long hold.
4. The 10-second hold has precedence over click recognition. Once the stable
   hold reaches the threshold, cancel any pending click and request upgrade
   mode exactly once.
5. After requesting reboot or bootloader entry, ignore further button events
   until reset. Do not retry a system transition from a repeated scan.
6. A reset or bootloader transition must only happen in task context. Upgrade
   mode means entering the existing ROM bootloader; firmware transfer requires
   an external programming tool and supported bootloader transport.

## Proposed Architecture

```text
button-task (static, 10 ms fixed period)
  ├── button_control_if.is_pressed()
  └── button_app_process(state, sample, now_ms)
        └── one-shot action
              ├── button_control_if.request_reboot()
              └── button_control_if.request_upgrade_mode()
                    ↓
              board implementation
                    ├── NVIC_SystemReset
                    └── enter_bootloader_mode()
```

- Keep gesture recognition and timing in `applications/button_app`; it should
  not include STM32 HAL, GPIO registers, or FreeRTOS internals.
- Read the logical button state through a small board GPIO/button interface.
  The BSP maps the physical GPIOD pin and active polarity to that interface.
- Route reset and ROM-bootloader actions through a narrow system-control
  interface with board-specific implementations.
- Run button sampling in its own statically allocated, low-priority service
  task with a fixed 10 ms period. This keeps 10 ms polling independent of
  `mainTask` loop work and uses `vTaskDelayUntil()` to avoid timing drift.
- Keep all callbacks and ISR work short. An EXTI-based implementation, if
  adopted later, may only notify task context; it must not reboot or transfer
  to the bootloader in the ISR.

## State Machine

```text
IDLE
  └─ debounced press → PRESSED
       ├─ 10 s stable hold → UPGRADE_REQUESTED
       └─ debounced release → CLICK_PENDING
            ├─ second press within 400 ms → SECOND_PRESS
            │    ├─ 10 s stable hold → UPGRADE_REQUESTED
            │    └─ debounced release → REBOOT_REQUESTED
            └─ window expires → IDLE (no action)

REBOOT_REQUESTED / UPGRADE_REQUESTED
  └─ terminal until reset
```

The state machine must handle a press that begins before the double-click
deadline and remains held beyond it: the second press is in progress, so a
10-second stable hold still selects upgrade mode. Release bounce must not
produce extra clicks or cancel a valid hold.

## Public Interface Proposal

Keep the application-facing contract independent of board pin numbers:

```c
bool button_app_init(button_app_t *app);
button_app_action_t button_app_process(button_app_t *app,
                                       bool is_pressed,
                                       uint32_t now_ms);
```

The `button_control_if_t` hardware interface provides the logical button
sample and task-context reboot/bootloader requests. Gesture processing is
independent of HAL and FreeRTOS; it returns a one-shot action for the task to
dispatch. Invalid state returns an explicit error action.

## Error Handling and Observability

- Fail initialization explicitly if either required interface is unavailable.
- Log a single event when a reboot or bootloader action is requested; do not
  log on each polling iteration.
- If transition callbacks can fail, retain a terminal error state and report
  it through the project-standard logging or status mechanism.
- Do not use a success-shaped fallback for invalid button input or missing
  hardware configuration.
- Optional LED feedback during the long hold may be considered only after the
  board LED ownership and user feedback requirements are agreed; it is not
  required for the initial behavior.

## Validation Plan

### Unit tests

- A short press and release produces no action after the click window expires.
- Two valid clicks request exactly one reboot, after the second release.
- A second press beginning before the deadline is accepted even if its release
  occurs after the deadline.
- A press shorter than 10 seconds never requests upgrade mode.
- A stable hold of 10 seconds requests upgrade mode exactly once.
- Upgrade mode takes precedence over a pending first click or a second click.
- Contact bounce around press/release edges does not create phantom clicks or
  reset the hold timer.
- A continuously held button across tick-counter wrap still reaches the
  correct timeout.
- Invalid/missing interfaces and failed system-control calls are surfaced.

### Hardware verification

- Confirm button pin, electrical polarity, external pull, and switch behavior
  from the board schematic and with measurements.
- Measure effective debounce and gesture windows on the target board.
- Verify single click has no side effect, double click resets once, and a
  10-second hold enters the ROM bootloader.
- Verify the expected external upgrade/programming workflow can connect after
  bootloader entry and that a normal reset remains available if a hold is
  accidental.

## Acceptance Criteria

- One click never reboots or enters upgrade mode.
- Double click reboots exactly once and only after the gesture is complete.
- A continuous stable 10-second press enters the existing ROM bootloader
  exactly once; shorter holds do not.
- Long press wins over pending click/double-click interpretation.
- Button timing is debounced, wrap-safe, deterministic, and testable without
  hardware.
- GPIO, reset, and bootloader operations remain behind BSP/interface
  boundaries; no HAL, register, or bootloader handoff executes in an ISR.
- Documentation clearly states that upgrade mode requires an external
  programming tool and is not an OTA update.

## Remaining Hardware Checks

1. Confirm the button active polarity and external electrical bias from the
   board schematic.
2. Confirm that a 400 ms double-click window and 30 ms debounce interval feel
   correct on the device.
3. Decide whether a visual indication is required while the 10-second hold is
   timing.