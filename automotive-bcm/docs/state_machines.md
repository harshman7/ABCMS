# BCM State Machines

This document describes the state machines implemented in each BCM module.

## Door Control State Machine

Each door has an independent lock state machine.

### States

| State | Value | Description |
|-------|-------|-------------|
| UNLOCKED | 0 | Door is unlocked |
| LOCKED | 1 | Door is locked |
| LOCKING | 2 | Lock actuator moving to lock |
| UNLOCKING | 3 | Lock actuator moving to unlock |

### State Diagram

```
              ┌────────────────────────────────────────┐
              │                                        │
              ▼                                        │
       ┌─────────────┐   Lock Cmd    ┌─────────────┐  │
       │             │──────────────►│             │  │
       │  UNLOCKED   │               │   LOCKING   │  │
       │             │◄──────────────│             │  │
       └─────────────┘  Update tick  └──────┬──────┘  │
              ▲                             │         │
              │                             │ Update  │
              │ Unlock Cmd                  │ tick    │
              │                             ▼         │
       ┌──────┴──────┐               ┌─────────────┐  │
       │             │               │             │  │
       │  UNLOCKING  │◄──────────────│   LOCKED    │──┘
       │             │  Unlock Cmd   │             │
       └─────────────┘               └─────────────┘
```

### Transitions

| From | Event | To | Action |
|------|-------|----|----|
| UNLOCKED | Lock command | LOCKING | Start lock actuator |
| LOCKING | Update tick | LOCKED | Actuator complete |
| LOCKED | Unlock command | UNLOCKING | Start unlock actuator |
| UNLOCKING | Update tick | UNLOCKED | Actuator complete |

### Code Implementation

```c
void door_control_update(uint32_t current_ms) {
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        switch (state->door.lock_state[i]) {
            case DOOR_STATE_LOCKING:
                state->door.lock_state[i] = DOOR_STATE_LOCKED;
                break;
            case DOOR_STATE_UNLOCKING:
                state->door.lock_state[i] = DOOR_STATE_UNLOCKED;
                break;
            default:
                break;
        }
    }
}
```

---

## Lighting Control State Machines

### Headlight Mode States

| State | Value | Description |
|-------|-------|-------------|
| OFF | 0 | Headlights off |
| ON | 1 | Low beam on |
| AUTO | 2 | Automatic (sensor controlled) |

### Headlight Output States

| State | Value | Description |
|-------|-------|-------------|
| OFF | 0 | Lights physically off |
| ON | 1 | Low beam active |
| AUTO | 2 | Auto mode, lights on due to low ambient |
| HIGH_BEAM | 3 | High beam active |

### State Diagram (Headlight Mode)

```
                   Off Cmd
       ┌─────────────────────────────────┐
       │                                 │
       ▼         On Cmd                  │
┌─────────────┐─────────►┌─────────────┐ │
│             │          │             │ │
│     OFF     │◄─────────│     ON      │ │
│             │  Off Cmd │             │ │
└──────┬──────┘          └──────┬──────┘ │
       │                        │        │
       │ Auto Cmd               │ Auto   │
       │                        │ Cmd    │
       ▼                        ▼        │
┌─────────────────────────────────────┐  │
│                                     │  │
│               AUTO                  │──┘
│    (Output based on ambient)        │ Off Cmd
│                                     │
└─────────────────────────────────────┘
```

### Auto Mode Logic

```c
void update_headlight_output(void) {
    if (mode == LIGHTING_STATE_AUTO) {
        if (ambient_light < AUTO_ON_THRESHOLD) {
            output = HEADLIGHT_STATE_AUTO;  // Lights on
        } else if (ambient_light > AUTO_OFF_THRESHOLD) {
            output = HEADLIGHT_STATE_OFF;   // Lights off
        }
    }
}
```

### Thresholds

| Parameter | Value | Description |
|-----------|-------|-------------|
| AUTO_ON_THRESHOLD | 80 | Turn on below this (0-255 scale) |
| AUTO_OFF_THRESHOLD | 120 | Turn off above this |

---

## Turn Signal State Machine

### States

| State | Value | Description |
|-------|-------|-------------|
| OFF | 0 | No signals active |
| LEFT | 1 | Left turn signal flashing |
| RIGHT | 2 | Right turn signal flashing |
| HAZARD | 3 | Both signals flashing |

### State Diagram

```
                            Off Cmd
              ┌─────────────────────────────────┐
              │                                 │
              ▼          Left Cmd               │
       ┌─────────────┐─────────►┌─────────────┐ │
       │             │          │             │ │
       │     OFF     │◄─────────│    LEFT     │ │
       │             │  Off/    │             │ │
       └──────┬──────┘  Timeout └─────────────┘ │
              │                                 │
              │ Right Cmd                       │ Hazard Off
              ▼                                 │
       ┌─────────────┐                          │
       │             │──────────────────────────┘
       │    RIGHT    │  Off/Timeout
       │             │
       └──────┬──────┘
              │
              │ Hazard Cmd (from any state)
              ▼
       ┌─────────────┐
       │             │
       │   HAZARD    │ (No timeout)
       │             │
       └─────────────┘
```

### Transitions

| From | Event | To | Action |
|------|-------|----|----|
| OFF | Left command | LEFT | Start flashing left |
| OFF | Right command | RIGHT | Start flashing right |
| OFF | Hazard on | HAZARD | Start flashing both |
| LEFT | Off command | OFF | Stop flashing |
| LEFT | Timeout (30s) | OFF | Auto stop |
| RIGHT | Off command | OFF | Stop flashing |
| RIGHT | Timeout (30s) | OFF | Auto stop |
| HAZARD | Hazard off | OFF | Stop flashing |
| Any | Hazard on | HAZARD | Override current |

### Flash Timing

```
        ON_TIME           OFF_TIME
       ├────────┤        ├────────┤
       │████████│        │        │████████│        │
Time:  0       500      500     1000     1500     2000
                    (milliseconds)
```

| Signal | On Time | Off Time |
|--------|---------|----------|
| Turn (L/R) | 500ms | 500ms |
| Hazard | 400ms | 400ms |

### Flash State Update

```c
void turn_signal_update(uint32_t current_ms) {
    if (mode == TURN_SIG_STATE_OFF) return;
    
    uint32_t elapsed = current_ms - last_toggle_ms;
    uint16_t phase_duration = currently_on ? on_time : off_time;
    
    if (elapsed >= phase_duration) {
        currently_on = !currently_on;
        last_toggle_ms = current_ms;
        
        // Update outputs based on mode
        switch (mode) {
            case TURN_SIG_STATE_LEFT:
                left_output = currently_on;
                break;
            case TURN_SIG_STATE_HAZARD:
                left_output = right_output = currently_on;
                break;
            // ...
        }
        
        if (currently_on) flash_count++;
    }
}
```

### Timeout Behavior

- Turn signals (LEFT/RIGHT) auto-off after 30 seconds
- Hazard signals do NOT auto-off (safety feature)
- Timeout sets FAULT_CODE_TIMEOUT

---

## BCM System State Machine

### States

| State | Value | Description |
|-------|-------|-------------|
| INIT | 0 | System initializing |
| NORMAL | 1 | Normal operation |
| FAULT | 2 | Critical fault active |
| DIAGNOSTIC | 3 | Diagnostic mode |

### State Diagram

```
              ┌───────────────┐
              │               │
   Reset ────►│     INIT      │
              │               │
              └───────┬───────┘
                      │ Init complete
                      ▼
              ┌───────────────┐
              │               │◄─────────────┐
    ┌────────►│    NORMAL     │              │
    │         │               │──────┐       │
    │         └───────┬───────┘      │       │
    │                 │              │       │
    │  Fault          │ Critical    │       │
    │  cleared        │ fault       │ Diag  │
    │                 ▼              │ exit  │
    │         ┌───────────────┐      │       │
    │         │               │◄─────┘       │
    └─────────│    FAULT      │              │
              │               │──────────────┤
              └───────────────┘    Diag      │
                                   enter     │
                                             │
              ┌───────────────┐              │
              │               │◄─────────────┘
              │  DIAGNOSTIC   │
              │               │
              └───────────────┘
```

---

## Event Log

State transitions are logged to a ring buffer for debugging.

### Event Types

| Type | Value | Data Content |
|------|-------|--------------|
| EVENT_NONE | 0 | - |
| EVENT_DOOR_LOCK_CHANGE | 1 | [door_id, new_state, 0, 0] |
| EVENT_HEADLIGHT_CHANGE | 3 | [type, old_state, new_state, 0] |
| EVENT_TURN_SIGNAL_CHANGE | 5 | [old_mode, new_mode, 0, 0] |
| EVENT_FAULT_SET | 6 | [1, fault_code, 0, 0] |
| EVENT_FAULT_CLEAR | 7 | [0, fault_code, 0, 0] |
| EVENT_CMD_RECEIVED | 8 | [cmd, param, 0, 0] |
| EVENT_CMD_ERROR | 9 | [result, cmd, 0, 0] |

### Log Entry Format

```c
typedef struct {
    uint32_t        timestamp_ms;   // Event timestamp
    event_type_t    type;           // Event type
    uint8_t         data[4];        // Event-specific data
} event_log_entry_t;
```

### Ring Buffer

- Size: 32 entries
- Oldest entries overwritten when full
- Preserved across normal operation
- Cleared only on explicit request or reset
