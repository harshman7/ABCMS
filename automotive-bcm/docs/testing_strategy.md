# BCM Testing Strategy

This document outlines the comprehensive testing approach for the Body Control Module software.

## Testing Levels

```
┌─────────────────────────────────────────────────────────────────┐
│                    System Integration Tests                      │
│              (HIL, Vehicle-level, End-to-end)                   │
├─────────────────────────────────────────────────────────────────┤
│                    Integration Tests                             │
│              (Module interactions, CAN traffic)                  │
├─────────────────────────────────────────────────────────────────┤
│                    Unit Tests                                    │
│              (Individual functions, state machines)              │
├─────────────────────────────────────────────────────────────────┤
│                    Static Analysis                               │
│              (Code quality, MISRA compliance)                    │
└─────────────────────────────────────────────────────────────────┘
```

## Unit Testing

### Framework: CppUTest

We use CppUTest for unit testing because:
- Designed for embedded C/C++
- Supports memory leak detection
- Mock object support
- Runs on host and target
- No dynamic memory in test runners

### Test Structure

```cpp
TEST_GROUP(ModuleName)
{
    void setup() override
    {
        // Initialize before each test
        system_state_init();
        module_init();
    }

    void teardown() override
    {
        // Cleanup after each test
        module_deinit();
    }
};

TEST(ModuleName, TestCase)
{
    // Arrange
    // Act
    // Assert
}
```

### Test Coverage Goals

| Module | Target Coverage | Priority |
|--------|-----------------|----------|
| Door Control | 90% | High |
| Lighting Control | 90% | High |
| Turn Signal | 95% | Critical |
| Fault Manager | 95% | Critical |
| CAN Interface | 85% | High |
| BCM Core | 80% | Medium |

### Unit Test Categories

#### 1. Initialization Tests
```cpp
TEST(DoorControlInit, InitializesSuccessfully)
{
    bcm_result_t result = door_control_init();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DoorControlInit, RejectsDoubleInit)
{
    door_control_init();
    bcm_result_t result = door_control_init();
    CHECK_EQUAL(BCM_ERROR, result);
}
```

#### 2. State Transition Tests
```cpp
TEST(DoorLock, TransitionsFromUnlockedToLocked)
{
    door_unlock(DOOR_FRONT_LEFT);
    CHECK_EQUAL(LOCK_STATE_UNLOCKED, door_get_lock_state(DOOR_FRONT_LEFT));
    
    door_lock(DOOR_FRONT_LEFT);
    CHECK_EQUAL(LOCK_STATE_LOCKED, door_get_lock_state(DOOR_FRONT_LEFT));
}
```

#### 3. Boundary Tests
```cpp
TEST(WindowControl, RejectsInvalidPosition)
{
    bcm_result_t result = door_window_set_position(DOOR_FRONT_LEFT, 150);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(WindowControl, AcceptsMaxPosition)
{
    bcm_result_t result = door_window_set_position(DOOR_FRONT_LEFT, 100);
    CHECK_EQUAL(BCM_OK, result);
}
```

#### 4. Error Handling Tests
```cpp
TEST(FaultManager, HandlesFullFaultLog)
{
    // Fill fault log
    for (int i = 0; i < BCM_MAX_FAULT_COUNT; i++) {
        fault_report((fault_code_t)(0x1000 + i), FAULT_SEVERITY_WARNING);
    }
    
    // One more should fail
    bcm_result_t result = fault_report(0x2000, FAULT_SEVERITY_WARNING);
    CHECK_EQUAL(BCM_ERROR_BUFFER_FULL, result);
}
```

#### 5. Timing Tests
```cpp
TEST(TurnSignal, FlashTimingCorrect)
{
    turn_signal_left_on();
    
    // Check initial state is ON
    CHECK_TRUE(turn_signal_is_illuminated(TURN_DIRECTION_LEFT));
    
    // Process for ON time
    turn_signal_process(TURN_SIGNAL_ON_TIME_MS);
    
    // Should now be OFF
    CHECK_FALSE(turn_signal_is_illuminated(TURN_DIRECTION_LEFT));
}
```

### Running Unit Tests

```bash
# Build with tests
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run all tests
ctest --output-on-failure

# Run with verbose output
./bcm_tests -v

# Run specific test group
./bcm_tests -g DoorControl

# Run specific test
./bcm_tests -n DoorLockControl::LockAllSetsAllDoorsLocked
```

---

## Integration Testing

### CAN Integration Tests

Test interactions between modules via CAN messages:

```python
# Python test using CAN simulator
def test_keyfob_unlock_triggers_welcome_lights():
    """Verify welcome lights activate when doors unlock via key fob"""
    
    # Send key fob unlock command
    send_can(CAN_ID_KEYFOB_CMD, [KEYFOB_UNLOCK])
    
    # Wait for processing
    time.sleep(0.1)
    
    # Verify door status shows unlocked
    door_status = receive_can(CAN_ID_DOOR_STATUS, timeout=1.0)
    assert (door_status.data[0] & 0x0F) == 0x00, "Doors should be unlocked"
    
    # Verify lighting status shows welcome lights
    light_status = receive_can(CAN_ID_LIGHTING_STATUS, timeout=1.0)
    assert (light_status.data[5] & 0x04) != 0, "Welcome lights should be active"
```

### Module Integration Test Matrix

| Test Case | Modules Involved | CAN Messages |
|-----------|------------------|--------------|
| Key fob lock | Door, Lighting | KEYFOB_CMD → DOOR_STATUS |
| Auto-lock at speed | Door, Vehicle Speed | VEHICLE_SPEED → DOOR_STATUS |
| Auto headlights | Lighting, Ambient Sensor | AMBIENT_LIGHT → LIGHTING_STATUS |
| Bulb failure indicator | Turn Signal, Fault | TURN_STATUS, BCM_FAULT |
| Critical fault handling | Fault, BCM Core | BCM_STATUS, BCM_FAULT |

---

## System Testing

### Hardware-in-the-Loop (HIL) Testing

```
┌─────────────────────────────────────────────────────────────┐
│                        HIL System                           │
│                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    │
│  │   BCM ECU   │◄──►│    CAN      │◄──►│    HIL      │    │
│  │   (Target)  │    │   Interface │    │  Simulator  │    │
│  └─────────────┘    └─────────────┘    └─────────────┘    │
│                                              │             │
│                                              ▼             │
│                                        ┌─────────────┐    │
│                                        │   Test      │    │
│                                        │   Scripts   │    │
│                                        └─────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### System Test Scenarios

#### Scenario 1: Normal Driving Cycle
1. Unlock doors (key fob)
2. Open driver door → Interior light on
3. Close door → Interior light fade
4. Ignition ON
5. Engine start
6. Accelerate → Auto-lock at 15 km/h
7. Turn signal left → 3 blinks (lane change)
8. Turn signal right → Continuous until cancelled
9. Decelerate to stop
10. Ignition OFF
11. Lock doors (key fob)

#### Scenario 2: Night Driving
1. Unlock doors → Welcome lights
2. Ignition ON
3. Auto headlights activate (low ambient)
4. High beam toggle
5. Fog lights on/off
6. Ignition OFF → Follow-me-home lights
7. Timeout → Lights off

#### Scenario 3: Fault Conditions
1. Simulate bulb failure → Fast flash
2. Simulate CAN bus-off → Recovery
3. Simulate over-voltage → System response
4. Clear faults via diagnostic tool

---

## Static Analysis

### MISRA C:2012 Compliance

Key rules enforced:

| Rule | Description | Enforcement |
|------|-------------|-------------|
| 8.13 | Pointer to const when possible | Warning |
| 11.3 | No casts between pointer and integer | Error |
| 14.4 | Controlling expression shall be boolean | Error |
| 15.7 | All if...else chains shall end with else | Warning |
| 17.7 | Return value shall be used | Warning |
| 21.3 | No <stdlib.h> memory functions | Error |

### Compiler Warnings

Build with maximum warnings:

```cmake
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wshadow
    -Wundef
    -Wcast-align
    -Wstrict-prototypes
)
```

### Static Analysis Tools

- **cppcheck**: General static analysis
- **clang-tidy**: Clang-based checks
- **PC-lint**: Commercial MISRA checker

```bash
# Run cppcheck
cppcheck --enable=all --std=c11 src/ include/

# Run clang-tidy
clang-tidy src/*.c -- -Iinclude -Iconfig
```

---

## Code Coverage

### Coverage Tools

- **gcov/lcov** for GCC
- **llvm-cov** for Clang

### Generating Coverage Report

```bash
# Build with coverage
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
cmake --build .

# Run tests
ctest

# Generate report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

### Coverage Requirements

| Level | Minimum | Target |
|-------|---------|--------|
| Statement | 80% | 90% |
| Branch | 75% | 85% |
| Function | 95% | 100% |

---

## Regression Testing

### Continuous Integration

```yaml
# Example CI pipeline
stages:
  - build
  - test
  - analyze
  - report

build:
  script:
    - mkdir build && cd build
    - cmake -DBUILD_TESTS=ON ..
    - cmake --build .

test:
  script:
    - cd build
    - ctest --output-on-failure
  artifacts:
    reports:
      junit: build/test_results.xml

analyze:
  script:
    - cppcheck --enable=all --xml src/ 2> cppcheck.xml
  artifacts:
    reports:
      codequality: cppcheck.xml

coverage:
  script:
    - cd build
    - cmake -DENABLE_COVERAGE=ON ..
    - cmake --build .
    - ctest
    - lcov --capture --directory . --output-file coverage.info
  artifacts:
    paths:
      - coverage.info
```

### Test Selection for Regression

After code changes:

| Change Type | Tests Required |
|-------------|----------------|
| Single function change | Unit tests for function + callers |
| Module change | All module tests + integration tests |
| Interface change | All dependent module tests |
| Configuration change | Full test suite |
| Critical bug fix | Full test suite + specific regression test |

---

## Test Data Management

### Test Vectors

Store test vectors in structured format:

```c
/* test_vectors.h */
typedef struct {
    uint8_t can_data[8];
    uint8_t can_len;
    bcm_result_t expected_result;
    door_position_t expected_door;
    lock_state_t expected_state;
} door_cmd_test_vector_t;

static const door_cmd_test_vector_t DOOR_CMD_VECTORS[] = {
    { {0x01}, 1, BCM_OK, DOOR_FRONT_LEFT, LOCK_STATE_LOCKED },
    { {0x02}, 1, BCM_OK, DOOR_FRONT_LEFT, LOCK_STATE_UNLOCKED },
    // ... more vectors
};
```

---

## Defect Tracking

### Defect Categories

| Category | Description | Example |
|----------|-------------|---------|
| Functional | Feature not working | Lock command ignored |
| Timing | Timing out of spec | Flash rate too slow |
| Interface | CAN/API issue | Wrong message ID |
| Safety | Safety-related | Anti-pinch failure |
| Performance | Resource issue | Stack overflow |

### Defect Lifecycle

```
Found → Confirmed → Assigned → Fixed → Verified → Closed
```

---

## Test Deliverables

1. **Test Plan**: Overall testing approach
2. **Test Cases**: Detailed test procedures
3. **Test Reports**: Execution results
4. **Coverage Reports**: Code coverage analysis
5. **Defect Reports**: Issues found
6. **Traceability Matrix**: Requirements to tests mapping

---

## Best Practices

1. **Test Early**: Write tests alongside code
2. **Test Often**: Run tests on every commit
3. **Test Thoroughly**: Cover edge cases and error paths
4. **Test Independently**: Tests should not depend on each other
5. **Test Repeatably**: Same inputs should give same outputs
6. **Document Tests**: Explain what and why you're testing
7. **Maintain Tests**: Update tests when requirements change
8. **Review Tests**: Code review includes test code
