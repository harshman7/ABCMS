# BCM System Architecture

## Overview

The Body Control Module (BCM) is an embedded control unit that manages vehicle body functions through CAN bus communication. This implementation follows a message-driven architecture with explicit state machines for each subsystem.

## System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              BCM Application                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│   │    Door      │  │   Lighting   │  │ Turn Signal  │  │    Fault     │   │
│   │   Control    │  │   Control    │  │   Control    │  │   Manager    │   │
│   │  State M/C   │  │  State M/C   │  │  State M/C   │  │              │   │
│   └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│          │                 │                 │                 │            │
│   ┌──────┴─────────────────┴─────────────────┴─────────────────┴───────┐   │
│   │                         BCM Core (bcm.c)                            │   │
│   │              - bcm_init() / bcm_process()                           │   │
│   │              - Message routing                                      │   │
│   │              - Periodic scheduler (10ms/100ms/1000ms)               │   │
│   └──────────────────────────────┬──────────────────────────────────────┘   │
│                                  │                                          │
│   ┌──────────────────────────────┴──────────────────────────────────────┐   │
│   │                      CAN Interface Layer                             │   │
│   │    ┌─────────────────────────────────────────────────────────┐      │   │
│   │    │  can_init() / can_send() / can_recv()                   │      │   │
│   │    │  BCM_SIL=1: SocketCAN | BCM_SIL=0: Stub Queue           │      │   │
│   │    └─────────────────────────────────────────────────────────┘      │   │
│   └──────────────────────────────┬──────────────────────────────────────┘   │
│                                  │                                          │
├──────────────────────────────────┼──────────────────────────────────────────┤
│                           System State                                      │
│                   (Centralized, No Dynamic Allocation)                      │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   │
                                   ▼
                            ┌─────────────┐
                            │  CAN Bus    │
                            │  (vcan0)    │
                            └─────────────┘
```

## CAN Message Schema

### Message IDs (11-bit Standard)

| Direction | ID      | Name               | DLC | Period    |
|-----------|---------|-------------------|-----|-----------|
| RX        | 0x100   | DOOR_CMD          | 4   | On demand |
| RX        | 0x110   | LIGHTING_CMD      | 4   | On demand |
| RX        | 0x120   | TURN_SIGNAL_CMD   | 4   | On demand |
| TX        | 0x200   | DOOR_STATUS       | 6   | 100ms     |
| TX        | 0x210   | LIGHTING_STATUS   | 6   | 100ms     |
| TX        | 0x220   | TURN_SIGNAL_STATUS| 6   | 100ms     |
| TX        | 0x230   | FAULT_STATUS      | 8   | 500ms     |
| TX        | 0x240   | BCM_HEARTBEAT     | 4   | 1000ms    |

### Command Frame Format

All command frames (RX) follow this structure:

```
Byte 0: Command code
Byte 1: Parameter / Door ID
Byte 2: [7:4] Version, [3:0] Rolling Counter (0-15)
Byte 3: Checksum (XOR of bytes 0-2 with seed 0xAA)
```

### Status Frame Format

All status frames (TX) follow this structure:

```
Bytes 0-N: Status data (varies by message)
Byte N+1:  [7:4] Version, [3:0] Rolling Counter
Byte N+2:  Checksum (XOR of previous bytes with seed 0xAA)
```

### DOOR_CMD (0x100)

| Byte | Content | Values |
|------|---------|--------|
| 0 | Command | 0x01=Lock all, 0x02=Unlock all, 0x03=Lock single, 0x04=Unlock single |
| 1 | Door ID | 0x00=FL, 0x01=FR, 0x02=RL, 0x03=RR, 0xFF=All |
| 2 | Ver/Ctr | [7:4]=Version, [3:0]=Counter |
| 3 | Checksum | XOR(bytes 0-2) ^ 0xAA |

### LIGHTING_CMD (0x110)

| Byte | Content | Values |
|------|---------|--------|
| 0 | Headlight | 0x00=Off, 0x01=On, 0x02=Auto, 0x03=High on, 0x04=High off |
| 1 | Interior | [1:0]=Mode (0=Off,1=On,2=Auto), [7:4]=Brightness |
| 2 | Ver/Ctr | [7:4]=Version, [3:0]=Counter |
| 3 | Checksum | XOR(bytes 0-2) ^ 0xAA |

### TURN_SIGNAL_CMD (0x120)

| Byte | Content | Values |
|------|---------|--------|
| 0 | Command | 0x00=Off, 0x01=Left, 0x02=Right, 0x03=Hazard on, 0x04=Hazard off |
| 1 | Reserved | 0x00 |
| 2 | Ver/Ctr | [7:4]=Version, [3:0]=Counter |
| 3 | Checksum | XOR(bytes 0-2) ^ 0xAA |

### DOOR_STATUS (0x200)

| Byte | Content |
|------|---------|
| 0 | Lock states (bit 0-3 = FL,FR,RL,RR locked) |
| 1 | Open states (bit 0-3 = FL,FR,RL,RR open) |
| 2 | Last command result |
| 3 | Active fault count |
| 4 | Ver/Ctr |
| 5 | Checksum |

### FAULT_STATUS (0x230)

| Byte | Content |
|------|---------|
| 0 | Fault flags 1 (bit 0=Door, 1=Headlight, 2=Turn, 3=CAN, 4=Checksum, 5=Counter, 6=Timeout) |
| 1 | Fault flags 2 (reserved) |
| 2 | Total fault count |
| 3 | Most recent fault code |
| 4 | Timestamp high (seconds) |
| 5 | Timestamp low |
| 6 | Ver/Ctr |
| 7 | Checksum |

## Data Flow

### Command Processing

```
CAN Bus
   │
   ▼
can_recv()
   │
   ▼
bcm_process() ──► route_can_frame()
                       │
       ┌───────────────┼───────────────┐
       ▼               ▼               ▼
door_control_    lighting_control_ turn_signal_
handle_cmd()     handle_cmd()      handle_cmd()
       │               │               │
       ▼               ▼               ▼
   Validate:       Validate:       Validate:
   - DLC           - DLC           - DLC
   - Checksum      - Checksum      - Checksum
   - Counter       - Counter       - Counter
   - Command       - Command       - Command
       │               │               │
       ▼               ▼               ▼
   Update          Update          Update
   State Machine   State Machine   State Machine
```

### Periodic Processing

```
┌─────────────────────────────────────────────────────┐
│                   Main Loop                          │
│                                                      │
│   while (running) {                                  │
│       bcm_process(current_ms);                       │
│       sleep(1ms);                                    │
│   }                                                  │
└─────────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────┐
│                 bcm_process()                        │
│                                                      │
│   1. Update system time                              │
│   2. Poll CAN RX, route to handlers                  │
│   3. Every 10ms: bcm_process_10ms()                  │
│   4. Every 100ms: bcm_process_100ms()                │
│   5. Every 1000ms: bcm_process_1000ms()              │
└─────────────────────────────────────────────────────┘

┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ bcm_process_    │  │ bcm_process_    │  │ bcm_process_    │
│ 10ms()          │  │ 100ms()         │  │ 1000ms()        │
│                 │  │                 │  │                 │
│ - Door update   │  │ - TX door       │  │ - TX heartbeat  │
│ - Light update  │  │   status        │  │ - TX fault      │
│ - Turn signal   │  │ - TX lighting   │  │   status        │
│   flash timing  │  │   status        │  │ - Timeout check │
│                 │  │ - TX turn       │  │                 │
│                 │  │   status        │  │                 │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

## Fault Strategy

### Fault Detection

| Fault | Detection | Flag |
|-------|-----------|------|
| Invalid DLC | Frame length check | FAULT_CODE_INVALID_LENGTH |
| Bad checksum | XOR verification | FAULT_CODE_INVALID_CHECKSUM |
| Counter error | Sequence check | FAULT_CODE_INVALID_COUNTER |
| Invalid command | Range check | FAULT_CODE_INVALID_CMD |
| Timeout | No command for 30s | FAULT_CODE_TIMEOUT |

### Fault Recording

- Maximum 8 active faults tracked
- Each fault records: code, timestamp
- Total historical count maintained
- Most recent fault code stored
- Fault flags provide quick status check

### Fault Recovery

- Faults cleared manually or via timeout
- Turn signals auto-off after 30s timeout
- AUTO lighting faults on sensor timeout

## Memory Layout

| Component | Approximate Size | Notes |
|-----------|-----------------|-------|
| System State | ~400 bytes | Static allocation |
| Event Log | ~520 bytes | 32 entries × 16 bytes |
| CAN Queues | ~640 bytes | RX:32 + TX:16 frames |
| **Total** | **~1.6KB RAM** | No malloc |

## Module Interface Summary

```c
/* BCM Core */
int bcm_init(const char *can_ifname);
int bcm_process(uint32_t current_ms);

/* Door Control */
void door_control_init(void);
cmd_result_t door_control_handle_cmd(const can_frame_t *frame);
void door_control_update(uint32_t current_ms);
void door_control_build_status_frame(can_frame_t *frame);

/* Lighting Control */
void lighting_control_init(void);
cmd_result_t lighting_control_handle_cmd(const can_frame_t *frame);
void lighting_control_update(uint32_t current_ms);
void lighting_control_build_status_frame(can_frame_t *frame);

/* Turn Signal */
void turn_signal_init(void);
cmd_result_t turn_signal_handle_cmd(const can_frame_t *frame);
void turn_signal_update(uint32_t current_ms);
void turn_signal_build_status_frame(can_frame_t *frame);

/* Fault Manager */
void fault_manager_init(void);
void fault_manager_set(fault_code_t code);
void fault_manager_clear(fault_code_t code);
void fault_manager_build_status_frame(can_frame_t *frame);

/* CAN Interface */
can_status_t can_init(const char *ifname);
can_status_t can_send(const can_frame_t *frame);
can_status_t can_recv(can_frame_t *frame);
```

## Build Configurations

| Flag | Effect |
|------|--------|
| `BCM_SIL=1` | Enable Linux SocketCAN |
| `BCM_SIL=0` | Use stub in-memory queue |
| `BUILD_TESTS=ON` | Build CppUTest unit tests |
| `CMAKE_BUILD_TYPE=Debug` | Debug symbols, -O0 |
| `CMAKE_BUILD_TYPE=Release` | Optimized, -O2, -Werror |
