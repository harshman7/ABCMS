# Automotive Body Control Module (BCM)

[![C11](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()

## About

A production-grade **Body Control Module (BCM)** implementation for automotive systems, written in **C11** with a focus on embedded best practices. This project demonstrates expertise in:

- **Embedded Systems Design** — Message-driven architecture with deterministic timing
- **Automotive Protocols** — CAN bus communication with proper frame validation
- **State Machine Implementation** — Explicit FSMs for door, lighting, and turn signal control
- **Defensive Programming** — Input validation, checksums, rolling counters, fault management
- **Testing Methodology** — Unit tests (CppUTest), Software-in-the-Loop (SIL) simulation
- **Zero Dynamic Allocation** — All memory statically allocated (~1.6KB RAM footprint)

> *Built as a portfolio project showcasing automotive embedded software development skills.*

---

## Features

- **Door Control**: Lock/unlock with state machine transitions
- **Lighting Control**: Headlights (off/on/auto), interior lights, high beam
- **Turn Signals**: Left/right/hazard with proper flash timing, auto-off timeout
- **Fault Management**: Checksum/counter validation, fault logging, status reporting
- **CAN Interface**: 11-bit standard IDs, rolling counter, XOR checksum
- **Event Log**: Ring buffer for state transition history

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Door Control    Lighting    Turn Signal   Fault    │
│  State Machine   State M/C   State M/C     Manager  │
├─────────────────────────────────────────────────────┤
│                    BCM Core                         │
│         Message routing, periodic scheduler         │
├─────────────────────────────────────────────────────┤
│              CAN Interface Layer                    │
│     BCM_SIL=1: SocketCAN | BCM_SIL=0: Stub          │
└─────────────────────────────────────────────────────┘
```

## Project Structure

```
automotive-bcm/
├── config/
│   ├── can_ids.h           # CAN message schema with byte layouts
│   └── bcm_config.h        # BCM configuration parameters
├── include/
│   ├── bcm.h               # BCM core interface
│   ├── door_control.h      # Door control module
│   ├── lighting_control.h  # Lighting control module
│   ├── turn_signal.h       # Turn signal module
│   ├── fault_manager.h     # Fault management
│   ├── can_interface.h     # CAN abstraction layer
│   └── system_state.h      # Centralized state
├── src/
│   ├── main.c              # Application entry point
│   ├── bcm.c               # BCM core implementation
│   ├── door_control.c      # Door state machine
│   ├── lighting_control.c  # Lighting state machine
│   ├── turn_signal.c       # Turn signal state machine
│   ├── fault_manager.c     # Fault recording/reporting
│   ├── system_state.c      # State management
│   └── can_interface.c     # SocketCAN/stub implementation
├── tests/                  # CppUTest unit tests
├── tools/
│   └── can_simulator.py    # Python CAN test tool
└── docs/                   # Architecture documentation
```

## Build Instructions

### Prerequisites

- CMake 3.16+
- C11-compatible compiler (GCC, Clang)
- CppUTest (for testing, optional - auto-fetched if not found)

### macOS / Linux (Stub Mode)

```bash
# Clone and build
cd automotive-bcm
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run
./bcm_app
```

### Linux with SocketCAN (SIL Mode)

```bash
# Create virtual CAN interface
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Build with SocketCAN support
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBCM_SIL=ON ..
cmake --build .

# Run
./bcm_app -i vcan0
```

### Building Tests

```bash
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run tests
ctest --output-on-failure

# Or run directly with verbose output
./bcm_tests -v
```

## Running

### Stub Mode (Default)

```bash
./bcm_app

# Output:
# ========================================
#   BCM - Body Control Module
#   Version: 1.0.0
# ========================================
# 
# [CAN] Initialized (stub mode)
# [DOOR] Initialized
# [LIGHT] Initialized
# [TURN] Initialized
# [FAULT] Initialized
# [BCM] Initialized successfully
# 
# [MAIN] BCM running. Press Ctrl+C to exit.
# [     1.000s] Doors:UUUU | Head:OFF | Turn:OFF[--] | Faults:0
```

### SIL Mode with Simulator

Terminal 1 (BCM):
```bash
./bcm_app -i vcan0
```

Terminal 2 (Simulator):
```bash
python3 tools/can_simulator.py -i vcan0 --interactive

# Commands:
> door unlock       # Unlock all doors
> light on          # Headlights on
> turn left         # Left turn signal
> hazard on         # Hazard lights
> scenario 1        # Run predefined scenario
```

## CAN Message Format

### Command Frames (RX)

All commands use 4-byte format:
- Byte 0: Command code
- Byte 1: Parameter
- Byte 2: [7:4] Version, [3:0] Counter (0-15)
- Byte 3: Checksum (XOR with 0xAA seed)

| ID | Name | Commands |
|----|------|----------|
| 0x100 | DOOR_CMD | 0x01=Lock all, 0x02=Unlock all, 0x03/0x04=Single |
| 0x110 | LIGHTING_CMD | 0x00=Off, 0x01=On, 0x02=Auto, 0x03/0x04=High beam |
| 0x120 | TURN_SIGNAL_CMD | 0x00=Off, 0x01=Left, 0x02=Right, 0x03/0x04=Hazard |

### Status Frames (TX)

| ID | Name | Period | DLC |
|----|------|--------|-----|
| 0x200 | DOOR_STATUS | 100ms | 6 |
| 0x210 | LIGHTING_STATUS | 100ms | 6 |
| 0x220 | TURN_SIGNAL_STATUS | 100ms | 6 |
| 0x230 | FAULT_STATUS | 500ms | 8 |
| 0x240 | BCM_HEARTBEAT | 1000ms | 4 |

## Sample Output

### Normal Operation

```
[DOOR] Door 0: UNLOCKING
[DOOR] Door 0: UNLOCKED
[DOOR] Door 1: UNLOCKING
[DOOR] Door 1: UNLOCKED
[LIGHT] Headlight mode: 0 -> 1
[TURN] LEFT ON
[     5.000s] Doors:UUUU | Head:ON  | Turn:LEFT[L-] | Faults:0
[     6.000s] Doors:UUUU | Head:ON  | Turn:LEFT[--] | Faults:0
```

### Fault Injection

```
[DOOR] Command error: 1
[FAULT] SET: 0x23
[     8.000s] Doors:UUUU | Head:OFF | Turn:OFF[--] | Faults:1
```

### Event Log (on exit)

```
[MAIN] Event Log (8 entries):
  [    1000 ms] Type=1 Data=[00 00 00 00]
  [    1500 ms] Type=3 Data=[00 00 01 00]
  [    2000 ms] Type=5 Data=[00 01 00 00]
  [    3000 ms] Type=9 Data=[01 FF 00 00]
```

## Testing

### Unit Tests

```bash
# Run all tests
./bcm_tests

# Verbose output
./bcm_tests -v

# Specific test group
./bcm_tests -g DoorLockCommands

# Specific test
./bcm_tests -n "DoorCommandValidation::RejectsInvalidChecksum"
```

### Test Coverage

- Door control: Lock/unlock, state transitions, validation
- Lighting: Mode changes, auto logic, high beam
- Turn signals: Flash timing, hazard, timeout
- Fault manager: Set/clear, flags, status frame
- Edge cases: Counter wrap, max faults, invalid inputs

### SIL Scenarios

```bash
# Run predefined scenarios
python3 tools/can_simulator.py -s 1    # Basic operation
python3 tools/can_simulator.py -s 2    # Hazard lights
python3 tools/can_simulator.py -s 3    # Fault injection
python3 tools/can_simulator.py -s all  # All scenarios
```

## Documentation

- [automotive-bcm/docs/architecture.md](automotive-bcm/docs/architecture.md) - System design, CAN schema
- [automotive-bcm/docs/state_machines.md](automotive-bcm/docs/state_machines.md) - State diagrams, transitions
- [automotive-bcm/docs/testing_strategy.md](automotive-bcm/docs/testing_strategy.md) - Test approach, coverage

## Design Constraints

- **No dynamic allocation** - All memory statically allocated
- **Defensive coding** - All inputs validated
- **C11 standard** - No compiler extensions
- **Embedded-friendly** - ~1.6KB RAM footprint

## Technologies

| Category | Technologies |
|----------|--------------|
| Language | C11 |
| Build | CMake 3.16+ |
| Testing | CppUTest, CTest |
| Protocol | CAN 2.0A (11-bit IDs) |
| SIL | Linux SocketCAN, vcan |
| Tools | Python 3, python-can |

## Author

Developed as a demonstration of automotive embedded software engineering skills.

## License

Copyright (c) 2026. All rights reserved.
