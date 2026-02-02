# BCM State Machines

This document describes the state machines implemented in the Body Control Module.

## BCM System State Machine

The main BCM system operates in the following states:

```
                    ┌─────────────────┐
                    │                 │
        ┌───────────│      INIT       │
        │           │                 │
        │           └────────┬────────┘
        │                    │ Initialization
        │                    │ Complete
        │                    ▼
        │           ┌─────────────────┐
        │    ┌──────│                 │◄─────────────┐
        │    │      │     NORMAL      │              │
        │    │      │                 │──────┐       │
        │    │      └────────┬────────┘      │       │
        │    │               │               │       │
        │    │ Sleep         │ Critical      │       │
        │    │ Request       │ Fault         │ Diag  │
        │    ▼               ▼               │ Mode  │
        │  ┌─────────┐  ┌─────────────┐      │       │
        │  │  SLEEP  │  │    FAULT    │◄─────┘       │
        │  └────┬────┘  └──────┬──────┘              │
        │       │              │                     │
        │  Wakeup│         Fault│                    │
        │  Event│        Cleared│                    │
        │       ▼              │                     │
        │  ┌─────────┐         │      ┌──────────────┤
        │  │ WAKEUP  │─────────┼──────│  DIAGNOSTIC  │
        │  └─────────┘         │      └──────────────┘
        │                      │
        └──────────────────────┘
```

### State Descriptions

| State | Description | Entry Condition | Exit Condition |
|-------|-------------|-----------------|----------------|
| INIT | System initialization | Power-on | Init complete |
| NORMAL | Normal operation | From INIT/WAKEUP/FAULT | Sleep/Fault/Diag request |
| SLEEP | Low-power sleep | Sleep request | Wake event |
| WAKEUP | Wake-up transition | From SLEEP | Wakeup complete |
| FAULT | Fault condition active | Critical fault detected | Fault cleared |
| DIAGNOSTIC | Diagnostic session active | Diag tool request | Session end |

### Transitions

```c
bcm_result_t bcm_request_state(bcm_state_t new_state);
```

Allowed transitions:
- INIT → NORMAL
- NORMAL → SLEEP, FAULT, DIAGNOSTIC
- SLEEP → WAKEUP
- WAKEUP → NORMAL
- FAULT → NORMAL, DIAGNOSTIC
- DIAGNOSTIC → NORMAL

---

## Door Lock State Machine

Each door has an independent lock state machine:

```
              ┌──────────────┐
              │              │
    ┌─────────│   UNKNOWN    │
    │         │              │
    │         └───────┬──────┘
    │                 │ Status Read
    │                 ▼
    │         ┌──────────────┐          ┌──────────────┐
    │         │              │  Lock    │              │
    │    ┌────│   UNLOCKED   │─────────►│    LOCKED    │────┐
    │    │    │              │◄─────────│              │    │
    │    │    └──────────────┘  Unlock  └──────────────┘    │
    │    │                                                   │
    │    │    ┌──────────────┐                              │
    │    │    │              │                              │
    │    └───►│    MOVING    │◄─────────────────────────────┘
    │         │              │
    │         └──────────────┘
    │                │
    │                │ Timeout/Error
    └────────────────┘
```

### Lock States

| State | Description |
|-------|-------------|
| UNKNOWN | Initial state, position not yet determined |
| UNLOCKED | Door is unlocked |
| LOCKED | Door is locked |
| MOVING | Lock actuator is in motion |

---

## Window State Machine

Each window operates independently:

```
                    ┌───────────────┐
                    │               │
        ┌───────────│    UNKNOWN    │
        │           │               │
        │           └───────┬───────┘
        │                   │ Position Read
        │                   ▼
        │           ┌───────────────┐
        │      ┌────│    CLOSED     │────┐
        │      │    │   (100%)      │    │
        │      │    └───────────────┘    │
        │      │            ▲            │
        │      │ Down       │ Up         │ Down
        │      │ Cmd        │ Reached    │ Cmd
        │      ▼            │            ▼
        │  ┌────────────────┴────────────────┐
        │  │                                 │
        │  │         MOVING_UP               │
        │  │              │                  │
        │  │              │ Stop/            │
        │  │              │ Anti-pinch       │
        │  │              ▼                  │
        │  │  ┌─────────────────────┐        │
        │  │  │      PARTIAL        │        │
        │  │  │     (1-99%)         │        │
        │  │  └──────────┬──────────┘        │
        │  │             │                   │
        │  │         MOVING_DOWN             │
        │  │             │                   │
        │  └─────────────┴───────────────────┘
        │                │
        │                ▼
        │        ┌───────────────┐
        │        │     OPEN      │
        │        │    (0%)       │
        │        └───────────────┘
        │                │
        │                │ Error/Blocked
        │                ▼
        │        ┌───────────────┐
        └───────►│   BLOCKED     │
                 │               │
                 └───────────────┘
```

### Window States

| State | Position | Description |
|-------|----------|-------------|
| UNKNOWN | - | Position not yet calibrated |
| CLOSED | 100% | Window fully closed |
| OPEN | 0% | Window fully open |
| PARTIAL | 1-99% | Window partially open |
| MOVING_UP | - | Window closing |
| MOVING_DOWN | - | Window opening |
| BLOCKED | - | Anti-pinch activated or motor fault |

### Anti-Pinch Logic

```
if (motor_current > ANTI_PINCH_THRESHOLD && moving_up) {
    stop_motor();
    reverse_motor_briefly();
    state = BLOCKED;
    report_fault(FAULT_DOOR_WINDOW_ANTI_PINCH);
}
```

---

## Turn Signal State Machine

```
                 ┌──────────────────┐
                 │                  │
    ┌────────────│       OFF        │◄──────────────────────────────┐
    │            │                  │                               │
    │            └────────┬─────────┘                               │
    │                     │                                         │
    │      ┌──────────────┼──────────────┐                         │
    │      │              │              │                         │
    │      │ Left Cmd     │ Right Cmd    │ Hazard Cmd              │
    │      ▼              ▼              ▼                         │
    │  ┌────────┐    ┌────────┐    ┌────────────┐                  │
    │  │  LEFT  │    │ RIGHT  │    │   HAZARD   │                  │
    │  │ NORMAL │    │ NORMAL │    │            │                  │
    │  └───┬────┘    └───┬────┘    └─────┬──────┘                  │
    │      │             │               │                         │
    │      │ Brief       │ Brief         │                         │
    │      │ Tap         │ Tap           │ Off Cmd                 │
    │      ▼             ▼               │                         │
    │  ┌────────┐    ┌────────┐          │                         │
    │  │ LANE   │    │ LANE   │          │                         │
    │  │ CHANGE │    │ CHANGE │          │                         │
    │  │ LEFT   │    │ RIGHT  │          │                         │
    │  └───┬────┘    └───┬────┘          │                         │
    │      │             │               │                         │
    │      │ 3 Blinks    │ 3 Blinks      │                         │
    │      │ Complete    │ Complete      │                         │
    │      └─────────────┴───────────────┴─────────────────────────┘
    │
    │                  Flash Timing Loop
    │                  ┌─────────────┐
    │             ┌───►│   ON (lit)  │
    │             │    └──────┬──────┘
    │             │           │ on_time elapsed
    │             │           ▼
    │             │    ┌─────────────┐
    │             └────│  OFF (dark) │
    │                  └─────────────┘
    │                        │ off_time elapsed
    │                        │ (loops back to ON)
    │
    └── Off/Cancel Cmd anywhere ──────────────────────────────────►
```

### Turn Signal Modes

| Mode | Description | Exit Condition |
|------|-------------|----------------|
| OFF | No signals active | Command received |
| NORMAL | Standard turn signal | Off command or auto-cancel |
| LANE_CHANGE | 3-blink comfort feature | Blink count complete |
| HAZARD | Both sides flashing | Off command |

### Timing Parameters

```c
#define TURN_SIGNAL_ON_TIME_MS   500   // Normal flash on
#define TURN_SIGNAL_OFF_TIME_MS  500   // Normal flash off
#define HAZARD_ON_TIME_MS        400   // Hazard on
#define HAZARD_OFF_TIME_MS       400   // Hazard off
#define TURN_FAST_FLASH_ON_MS    250   // Bulb failure on
#define TURN_FAST_FLASH_OFF_MS   250   // Bulb failure off
#define TURN_LANE_CHANGE_BLINKS  3     // Lane change count
```

---

## Lighting State Machine

### Headlight Mode State Machine

```
              ┌───────────────┐
              │               │
    ┌─────────│      OFF      │◄────────────────┐
    │         │               │                 │
    │         └───────┬───────┘                 │
    │                 │                         │
    │    ┌────────────┼────────────┐            │
    │    │            │            │            │
    │    ▼            ▼            ▼            │
    │ ┌──────┐   ┌─────────┐  ┌─────────┐      │
    │ │PARK- │   │  LOW    │  │  AUTO   │      │
    │ │ING   │   │  BEAM   │  │         │      │
    │ └──┬───┘   └────┬────┘  └────┬────┘      │
    │    │            │            │            │
    │    │            │    ┌───────┴────────┐  │
    │    │            │    │ Ambient Light  │  │
    │    │            │    │ Check          │  │
    │    │            │    └───────┬────────┘  │
    │    │            │            │            │
    │    │            ▼            ▼            │
    │    │       ┌─────────────────────┐       │
    │    │       │  High Beam Toggle   │       │
    │    │       └──────────┬──────────┘       │
    │    │                  │                  │
    │    │                  ▼                  │
    │    │       ┌─────────────────────┐       │
    │    │       │      HIGH BEAM      │       │
    │    │       └─────────────────────┘       │
    │    │                                     │
    │    └─────────────────────────────────────┘
    │
    └── Off Command ───────────────────────────►
```

### Interior Light State Machine

```
        ┌───────────────┐
        │               │
   ┌────│      OFF      │◄────────────────────────────────────┐
   │    │               │                                     │
   │    └───────┬───────┘                                     │
   │            │                                             │
   │   ┌────────┼────────┐                                    │
   │   │        │        │                                    │
   │   │ Manual │ Door   │ Ignition                           │
   │   │ On     │ Open   │ Off + Follow-Me-Home               │
   │   ▼        ▼        ▼                                    │
   │ ┌──────┐ ┌──────┐ ┌────────────────┐                    │
   │ │  ON  │ │ DOOR │ │ FOLLOW-ME-HOME │                    │
   │ │(255) │ │ MODE │ │    (timed)     │                    │
   │ └──┬───┘ └──┬───┘ └────────┬───────┘                    │
   │    │        │              │                             │
   │    │        │ Door Closed  │ Timeout                     │
   │    │        ▼              │                             │
   │    │   ┌─────────────┐     │                             │
   │    │   │   FADING    │     │                             │
   │    │   │   OUT       │◄────┘                             │
   │    │   └──────┬──────┘                                   │
   │    │          │                                          │
   │    │          │ Fade Complete                            │
   │    │          │                                          │
   │    └──────────┴──────────────────────────────────────────┘
```

---

## Fault State Machine

```
                    ┌───────────────────┐
                    │                   │
        ┌───────────│     INACTIVE      │◄─────────────────────────┐
        │           │                   │                          │
        │           └─────────┬─────────┘                          │
        │                     │                                    │
        │                     │ Fault Detected                     │
        │                     ▼                                    │
        │           ┌───────────────────┐                          │
        │           │                   │                          │
        │    ┌──────│     PENDING       │                          │
        │    │      │   (debouncing)    │                          │
        │    │      └─────────┬─────────┘                          │
        │    │                │                                    │
        │    │ Fault          │ Debounce                           │
        │    │ Cleared        │ Complete                           │
        │    │                ▼                                    │
        │    │      ┌───────────────────┐                          │
        │    │      │                   │                          │
        │    │      │      ACTIVE       │─────────────┐            │
        │    │      │                   │             │            │
        │    │      └─────────┬─────────┘             │            │
        │    │                │                       │            │
        │    │                │ Fault Condition       │ Recovery   │
        │    │                │ No Longer Present     │ Attempt    │
        │    │                ▼                       │            │
        │    │      ┌───────────────────┐             │            │
        │    │      │                   │◄────────────┘            │
        │    │      │      HEALED       │                          │
        │    │      │   (heal timer)    │                          │
        │    │      └─────────┬─────────┘                          │
        │    │                │                                    │
        │    │                │ Heal Timer                         │
        │    │                │ Complete                           │
        │    │                ▼                                    │
        │    │      ┌───────────────────┐                          │
        │    │      │                   │                          │
        │    └─────►│      STORED       │──────────────────────────┘
        │           │  (historical)     │         Clear Command
        │           └───────────────────┘
        │                     │
        │                     │ Fault Re-detected
        └─────────────────────┘
```

### Fault Timing Parameters

```c
#define FAULT_DEBOUNCE_TIME_MS     100    // Time before confirming fault
#define FAULT_HEALING_TIME_MS      1000   // Time before marking as stored
#define FAULT_MAX_OCCURRENCES      10     // Max before permanent fault
#define FAULT_MAX_RECOVERY_ATTEMPTS 3     // Recovery attempt limit
```

---

## CAN Bus State Machine

```
                    ┌───────────────────┐
                    │                   │
        ┌───────────│  NOT INITIALIZED  │
        │           │                   │
        │           └─────────┬─────────┘
        │                     │ can_interface_init()
        │                     ▼
        │           ┌───────────────────┐
        │           │                   │◄──────────────────────────┐
        │    ┌──────│        OK         │                          │
        │    │      │                   │                          │
        │    │      └─────────┬─────────┘                          │
        │    │                │                                    │
        │    │                │ TX Errors > 128                    │
        │    │                ▼                                    │
        │    │      ┌───────────────────┐                          │
        │    │      │                   │                          │
        │    │      │  ERROR PASSIVE    │──────────┐               │
        │    │      │                   │          │               │
        │    │      └─────────┬─────────┘          │               │
        │    │                │                    │               │
        │    │                │ TX Errors > 255    │ Errors < 128  │
        │    │                ▼                    │               │
        │    │      ┌───────────────────┐          │               │
        │    │      │                   │          │               │
        │    │      │     BUS OFF       │──────────┘               │
        │    │      │                   │                          │
        │    │      └─────────┬─────────┘                          │
        │    │                │                                    │
        │    │                │ Recovery Timer (500ms)             │
        │    │                └─────────────────────────────────────┘
        │    │
        │    │ can_interface_deinit()
        │    │
        └────┴───────────────────────────────────────────────────►
```

---

## State Machine Implementation Guidelines

1. **Single Entry Point**: Each state machine should have a single `_process()` function called periodically

2. **Atomic Transitions**: State changes should be atomic to prevent race conditions

3. **Timeout Handling**: All waiting states should have timeouts

4. **Error Recovery**: Each state machine should handle errors gracefully and return to a known state

5. **Logging**: State transitions should be logged for debugging

6. **Testing**: Each state and transition should have corresponding unit tests
