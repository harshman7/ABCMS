# BCM Testing Strategy

## Overview

This document describes the multi-level testing approach for the BCM software.

```
┌─────────────────────────────────────────────────────────────────┐
│            SIL Testing (Software-in-the-Loop)                    │
│                  vcan0 + can_simulator.py                        │
├─────────────────────────────────────────────────────────────────┤
│                    Unit Tests (CppUTest)                         │
│              Direct module testing, stub CAN                     │
├─────────────────────────────────────────────────────────────────┤
│                    Static Analysis                               │
│              Compiler warnings, code review                      │
└─────────────────────────────────────────────────────────────────┘
```

## Unit Testing

### Framework: CppUTest

CppUTest is used for unit testing because:
- Designed for embedded C/C++
- No dynamic memory in test framework core
- Mock support via CppUTestExt
- Runs on host for fast iteration

### Test Structure

```cpp
TEST_GROUP(ModuleName)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);  // Stub mode
        fault_manager_init();
        module_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(ModuleName, TestCase)
{
    // Arrange
    can_frame_t frame = build_cmd(...);
    
    // Act
    cmd_result_t result = module_handle_cmd(&frame);
    
    // Assert
    CHECK_EQUAL(CMD_RESULT_OK, result);
}
```

### Test Files

| File | Module | Test Count |
|------|--------|------------|
| test_door_control.cpp | Door Control | ~25 |
| test_lighting_control.cpp | Lighting | ~20 |
| test_fault_manager.cpp | Fault Manager | ~25 |

### Test Categories

#### 1. Initialization Tests

```cpp
TEST(DoorInit, AllDoorsUnlockedInitially)
{
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(i));
    }
}
```

#### 2. Command Validation Tests

```cpp
TEST(DoorCommandValidation, RejectsWrongDLC)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 0);
    frame.dlc = 3;  // Wrong length
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
    CHECK_TRUE(fault_manager_is_active(FAULT_CODE_INVALID_LENGTH));
}

TEST(DoorCommandValidation, RejectsInvalidChecksum)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 0);
    frame.data[DOOR_CMD_BYTE_CHECKSUM] = 0xFF;  // Bad checksum
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_CHECKSUM_ERROR, result);
}

TEST(DoorCommandValidation, RejectsInvalidCounter)
{
    can_frame_t frame1 = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 5);
    door_control_handle_cmd(&frame1);
    
    can_frame_t frame2 = build_door_cmd(DOOR_CMD_UNLOCK_ALL, DOOR_ID_ALL, 7);
    cmd_result_t result = door_control_handle_cmd(&frame2);
    CHECK_EQUAL(CMD_RESULT_COUNTER_ERROR, result);
}

TEST(DoorCommandValidation, AcceptsWrappingCounter)
{
    can_frame_t frame1 = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 15);
    door_control_handle_cmd(&frame1);
    
    can_frame_t frame2 = build_door_cmd(DOOR_CMD_UNLOCK_ALL, DOOR_ID_ALL, 0);
    cmd_result_t result = door_control_handle_cmd(&frame2);
    CHECK_EQUAL(CMD_RESULT_OK, result);  // Counter wrapped correctly
}
```

#### 3. State Machine Tests

```cpp
TEST(DoorStateMachine, UnlockedToLockingToLocked)
{
    CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(0));
    
    door_control_lock(0);
    CHECK_EQUAL(DOOR_STATE_LOCKING, door_control_get_lock_state(0));
    
    door_control_update(100);
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(0));
}
```

#### 4. Status Frame Tests

```cpp
TEST(DoorStatusFrame, ChecksumValid)
{
    can_frame_t frame;
    door_control_build_status_frame(&frame);
    
    uint8_t calc = can_calculate_checksum(frame.data, DOOR_STATUS_DLC - 1);
    CHECK_EQUAL(calc, frame.data[DOOR_STATUS_BYTE_CHECKSUM]);
}

TEST(DoorStatusFrame, CounterIncrements)
{
    can_frame_t frame1, frame2;
    
    door_control_build_status_frame(&frame1);
    uint8_t counter1 = CAN_GET_COUNTER(frame1.data[DOOR_STATUS_BYTE_VER_CTR]);
    
    door_control_build_status_frame(&frame2);
    uint8_t counter2 = CAN_GET_COUNTER(frame2.data[DOOR_STATUS_BYTE_VER_CTR]);
    
    CHECK_EQUAL((counter1 + 1) & CAN_COUNTER_MASK, counter2);
}
```

### Running Tests

```bash
# Build with tests
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run via CTest
ctest --output-on-failure

# Run directly with verbose output
./bcm_tests -v

# Run specific test group
./bcm_tests -g DoorLockCommands

# Run specific test
./bcm_tests -n "DoorCommandValidation::RejectsInvalidChecksum"
```

## SIL Testing

### Setup Virtual CAN

```bash
# Load vcan module
sudo modprobe vcan

# Create vcan0 interface
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Verify
ip link show vcan0
```

### Build with SocketCAN

```bash
mkdir build && cd build
cmake -DBCM_SIL=ON ..
cmake --build .
```

### Run BCM Application

```bash
# Terminal 1: Run BCM
./bcm_app -i vcan0

# Terminal 2: Run simulator
python3 ../tools/can_simulator.py -i vcan0 --interactive
```

### SIL Scenarios

#### Scenario 1: Basic Operation

```
1. Unlock all doors
2. Turn on headlights
3. Activate left turn signal
4. Deactivate turn signal
5. Turn off headlights
6. Lock all doors
```

**Expected BCM output:**
```
[DOOR] Door 0: UNLOCKING
[DOOR] Door 0: UNLOCKED
[LIGHT] Headlight mode: 0 -> 1
[TURN] LEFT ON
[TURN] OFF
[LIGHT] Headlight mode: 1 -> 0
[DOOR] Door 0: LOCKING
[DOOR] Door 0: LOCKED
```

#### Scenario 2: Hazard Lights

```
1. Activate hazard
2. Observe flashing (both L/R)
3. Deactivate hazard
```

**Expected BCM output:**
```
[TURN] HAZARD ON
[    ...ms] ... Turn:HAZ[LR] ...
[    ...ms] ... Turn:HAZ[--] ...
[    ...ms] ... Turn:HAZ[LR] ...
[TURN] OFF
```

#### Scenario 3: Fault Injection

```
1. Send frame with wrong DLC
2. Send frame with bad checksum
3. Send frame with invalid command
```

**Expected BCM output:**
```
[DOOR] Command error: 1   (INVALID_CMD - wrong DLC)
[FAULT] SET: 0x23
[LIGHT] Command error: 2  (CHECKSUM_ERROR)
[FAULT] SET: 0x20
[TURN] Command error: 1   (INVALID_CMD)
[FAULT] SET: 0x22
```

### CAN Monitor

```bash
# Monitor all CAN traffic
candump vcan0

# Filter for BCM TX only
candump vcan0,200:700
```

## Validation Matrix

### What's Tested

| Component | Unit Test | SIL Test |
|-----------|-----------|----------|
| Door lock/unlock | ✓ | ✓ |
| Door state transitions | ✓ | ✓ |
| Lighting on/off/auto | ✓ | ✓ |
| Turn signal modes | ✓ | ✓ |
| Flash timing | ✓ | ✓ |
| Checksum validation | ✓ | ✓ |
| Counter validation | ✓ | ✓ |
| Invalid command rejection | ✓ | ✓ |
| Fault recording | ✓ | ✓ |
| Status frame generation | ✓ | ✓ |
| Timeout handling | - | ✓ |
| Multi-frame sequences | - | ✓ |

### Edge Cases Covered

| Edge Case | Test |
|-----------|------|
| Counter wrap (15 -> 0) | Unit |
| Counter skip (5 -> 7) | Unit |
| Max active faults | Unit |
| Unknown fault code | Unit |
| Wrong CAN ID | Unit |
| Null frame pointer | Unit |
| Double lock/unlock | Unit |
| Invalid door ID | Unit |
| Invalid brightness | Unit |

## Code Coverage

Target coverage levels:

| Module | Target | Measured |
|--------|--------|----------|
| door_control.c | 90% | TBD |
| lighting_control.c | 90% | TBD |
| turn_signal.c | 90% | TBD |
| fault_manager.c | 95% | TBD |
| can_interface.c | 80% | TBD |

### Generating Coverage (GCC)

```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="--coverage" \
      -DCMAKE_CXX_FLAGS="--coverage" \
      ..
cmake --build .
ctest

# Generate report
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## Continuous Integration

```yaml
# .github/workflows/ci.yml
name: BCM CI

on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake cpputest
      
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DBUILD_TESTS=ON ..
          cmake --build .
      
      - name: Test
        run: |
          cd build
          ctest --output-on-failure
```

## Sample Test Output

```
$ ./bcm_tests

..............................................

OK (70 tests, 70 ran, 185 checks, 0 ignored, 0 filtered out, 12 ms)
```

Verbose output:

```
$ ./bcm_tests -v

TEST(FaultEdgeCases, UnknownFaultCodeNoFlag) - 0 ms
TEST(FaultEdgeCases, ClearAllAfterMax) - 0 ms
TEST(FaultEdgeCases, MaxFaultsHandled) - 0 ms
TEST(FaultStatusFrame, CounterIncrements) - 0 ms
TEST(FaultStatusFrame, VersionCorrect) - 0 ms
...

OK (70 tests, 70 ran, 185 checks, 0 ignored, 0 filtered out, 15 ms)
```
