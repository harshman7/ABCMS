# BCM System Architecture

## Overview

The Body Control Module (BCM) is a central electronic control unit responsible for managing various vehicle body functions. This document describes the software architecture, module interactions, and design decisions.

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              BCM Application                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│   │    Door      │  │   Lighting   │  │ Turn Signal  │  │    Fault     │   │
│   │   Control    │  │   Control    │  │   Control    │  │   Manager    │   │
│   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│          │                 │                 │                 │            │
│   ┌──────┴─────────────────┴─────────────────┴─────────────────┴───────┐   │
│   │                         BCM Core (bcm.c)                            │   │
│   │              - System initialization & coordination                 │   │
│   │              - State machine management                             │   │
│   │              - Periodic processing                                  │   │
│   └──────────────────────────────┬──────────────────────────────────────┘   │
│                                  │                                          │
│   ┌──────────────────────────────┴──────────────────────────────────────┐   │
│   │                      CAN Interface Layer                             │   │
│   │    ┌─────────────┐                        ┌─────────────┐           │   │
│   │    │   CAN RX    │                        │   CAN TX    │           │   │
│   │    │  (can_rx.c) │                        │  (can_tx.c) │           │   │
│   │    └─────────────┘                        └─────────────┘           │   │
│   └──────────────────────────────┬──────────────────────────────────────┘   │
│                                  │                                          │
├──────────────────────────────────┼──────────────────────────────────────────┤
│                          Hardware Abstraction                               │
│                                  │                                          │
│                        ┌─────────┴─────────┐                               │
│                        │    CAN HAL        │                               │
│                        │  (Platform Specific)                              │
│                        └───────────────────┘                               │
└─────────────────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
                            ┌─────────────┐
                            │  CAN Bus    │
                            └─────────────┘
```

## Module Descriptions

### BCM Core (`bcm.c`)

The central coordinator responsible for:
- System initialization and shutdown
- State machine management
- Coordinating subsystem processing
- Periodic CAN message transmission
- Error handling and recovery

**Key Functions:**
- `bcm_init()` - Initialize all subsystems
- `bcm_process()` - Main processing loop
- `bcm_request_state()` - State transition management

### Door Control (`door_control.c`)

Manages all door-related functions:
- Central locking (lock/unlock all doors)
- Individual door control
- Window control (up/down/auto)
- Child safety locks
- Auto-lock at speed feature

**State Machines:**
- Lock actuator state per door
- Window motor state per door
- Anti-pinch detection

### Lighting Control (`lighting_control.c`)

Controls all vehicle lighting:
- Headlights (low beam, high beam, auto)
- Fog lights (front and rear)
- Interior lights with fade effects
- Welcome lights and follow-me-home

**Features:**
- Automatic headlight control based on ambient light
- Interior light door-triggered mode
- PWM dimming for interior lights

### Turn Signal (`turn_signal.c`)

Manages turn indicators and hazard lights:
- Left/right turn signal control
- Hazard light activation
- Lane change assist (3-blink feature)
- Bulb failure detection with fast flash

**Timing:**
- 500ms on/500ms off for normal flash
- 400ms on/400ms off for hazard
- 250ms on/250ms off for fast flash (bulb failure)

### Fault Manager (`fault_manager.c`)

Handles fault detection and diagnostics:
- Fault detection with debouncing
- Fault logging with freeze frame
- Fault healing with timeout
- Recovery action management
- UDS diagnostic interface

**Fault States:**
1. Inactive - No fault present
2. Pending - Fault detected, debouncing
3. Active - Fault confirmed
4. Healed - Fault condition cleared, in healing period
5. Stored - Historical fault

### CAN Interface (`can_rx.c`, `can_tx.c`)

Provides CAN communication:
- Message reception and dispatch
- Message transmission with queuing
- Error handling and bus-off recovery
- Message filtering

## Data Flow

### Incoming CAN Messages

```
CAN Bus → CAN HAL → can_rx.c → Message Dispatch → Module Handlers
                                      │
                                      ├─→ door_control_handle_can_cmd()
                                      ├─→ lighting_control_handle_can_cmd()
                                      └─→ turn_signal_handle_can_cmd()
```

### Outgoing CAN Messages

```
Modules → Module Status Functions → bcm_process() → can_tx.c → CAN HAL → CAN Bus
```

## Configuration

### Compile-Time Configuration (`bcm_config.h`)

- Feature enables/disables
- Timing parameters
- Threshold values
- Hardware pin assignments

### CAN Message IDs (`can_ids.h`)

- BCM transmit message IDs
- BCM receive message IDs
- External ECU message IDs
- Command byte definitions

## Memory Map

| Component | Estimated RAM | Notes |
|-----------|---------------|-------|
| System State | ~200 bytes | Global state structure |
| Door Control | ~100 bytes | Per-door state |
| Lighting | ~80 bytes | Mode and timing data |
| Turn Signal | ~60 bytes | Flash timing state |
| Fault Manager | ~2KB | Fault log storage |
| CAN Queues | ~1KB | TX/RX message buffers |
| **Total** | **~3.5KB** | |

## Timing Requirements

| Operation | Max Time | Priority |
|-----------|----------|----------|
| Main cycle | 10ms | Normal |
| CAN TX | 1ms | High |
| Turn signal toggle | ±10ms | High |
| Fault debounce | 100ms | Normal |

## Coding Standards

### C11 Compliance

All code follows C11 standard:
- No compiler-specific extensions
- Standard fixed-width integers (`uint8_t`, etc.)
- No dynamic memory allocation
- All functions have return value checking

### Naming Conventions

- Module prefix for all functions: `door_`, `lighting_`, `turn_signal_`, `fault_`
- Uppercase for constants and macros
- Snake_case for variables and functions
- CamelCase for type definitions

### Error Handling

All functions return `bcm_result_t`:
```c
typedef enum {
    BCM_OK = 0,
    BCM_ERROR,
    BCM_ERROR_INVALID_PARAM,
    BCM_ERROR_TIMEOUT,
    BCM_ERROR_BUSY,
    BCM_ERROR_NOT_READY,
    BCM_ERROR_HW_FAULT,
    BCM_ERROR_COMM,
    BCM_ERROR_BUFFER_FULL,
    BCM_ERROR_NOT_SUPPORTED
} bcm_result_t;
```

## Safety Considerations

### Watchdog

- External watchdog timer expected
- 100ms timeout configuration
- Kick in main loop

### Fail-Safe States

- Doors: Unlock on power loss
- Lights: Off on fault
- Turn signals: Hazard activation on critical fault

### Critical Sections

- CAN queue access (atomic)
- Fault log modification (atomic)
- State transitions (validated)

## Future Enhancements

1. **AUTOSAR Compliance** - Align with AUTOSAR BSW modules
2. **CAN FD Support** - Higher bandwidth for diagnostics
3. **Secure Boot** - Software integrity verification
4. **OTA Updates** - Over-the-air update capability
5. **Power Management** - Advanced sleep/wake strategies
