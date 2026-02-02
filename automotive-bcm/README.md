# Automotive Body Control Module (BCM)

A production-grade Body Control Module implementation for automotive applications, written in C11. This module manages vehicle body functions including door control, lighting, turn signals, and fault management via CAN bus communication.

## Features

- **Door Control**: Lock/unlock, window control, child safety lock
- **Lighting Control**: Headlights, tail lights, interior lights, ambient lighting
- **Turn Signals**: Left/right indicators, hazard lights with proper timing
- **Fault Management**: Error detection, logging, and recovery mechanisms
- **CAN Interface**: Standard CAN 2.0B communication protocol

## Project Structure

```
automotive-bcm/
├── README.md                 # This file
├── CMakeLists.txt            # Top-level CMake configuration
├── .gitignore                # Git ignore rules
├── config/                   # Configuration headers
│   ├── can_ids.h             # CAN message ID definitions
│   └── bcm_config.h          # BCM configuration parameters
├── include/                  # Public header files
│   ├── bcm.h                 # Main BCM interface
│   ├── door_control.h        # Door control module
│   ├── lighting_control.h    # Lighting control module
│   ├── turn_signal.h         # Turn signal module
│   ├── fault_manager.h       # Fault management module
│   ├── can_interface.h       # CAN abstraction layer
│   └── system_state.h        # System state definitions
├── src/                      # Source files
│   ├── main.c                # Application entry point
│   ├── bcm.c                 # BCM core implementation
│   ├── door_control.c        # Door control implementation
│   ├── lighting_control.c    # Lighting control implementation
│   ├── turn_signal.c         # Turn signal implementation
│   ├── fault_manager.c       # Fault manager implementation
│   ├── can_rx.c              # CAN receive handler
│   └── can_tx.c              # CAN transmit handler
├── tests/                    # Unit tests (CppUTest)
│   ├── CMakeLists.txt        # Test build configuration
│   ├── test_door_control.cpp # Door control tests
│   ├── test_lighting_control.cpp # Lighting control tests
│   └── test_fault_manager.cpp    # Fault manager tests
├── tools/                    # Development tools
│   └── can_simulator.py      # CAN bus simulator for testing
└── docs/                     # Documentation
    ├── architecture.md       # System architecture
    ├── state_machines.md     # State machine documentation
    └── testing_strategy.md   # Testing approach
```

## Prerequisites

### Build Tools
- CMake 3.16 or higher
- C11-compatible compiler (GCC, Clang)
- C++11-compatible compiler (for tests)

### Testing Framework
- CppUTest (installed locally or fetched automatically)

## Build Instructions

### macOS

```bash
# Install prerequisites (using Homebrew)
brew install cmake

# Optional: Install CppUTest for testing
brew install cpputest

# Clone and build
git clone <repository-url>
cd automotive-bcm

# Create build directory
mkdir build && cd build

# Configure (Release build)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build application
cmake --build .

# Run the application
./bcm_app
```

### Linux (Ubuntu/Debian)

```bash
# Install prerequisites
sudo apt-get update
sudo apt-get install -y cmake build-essential

# Optional: Install CppUTest for testing
sudo apt-get install -y cpputest

# Clone and build
git clone <repository-url>
cd automotive-bcm

# Create build directory
mkdir build && cd build

# Configure (Release build)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build application
cmake --build .

# Run the application
./bcm_app
```

### Building Tests

```bash
# From the build directory
cmake -DBUILD_TESTS=ON ..
cmake --build .

# Run tests
ctest --output-on-failure

# Or run directly
./bcm_tests
```

### Debug Build

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

## CAN Simulator

A Python-based CAN simulator is provided for testing:

```bash
# Install Python dependencies
pip install python-can

# Run simulator (requires virtual CAN interface or hardware)
python tools/can_simulator.py
```

## Configuration

### CAN IDs
Edit `config/can_ids.h` to customize CAN message identifiers for your vehicle network.

### BCM Parameters
Edit `config/bcm_config.h` to adjust timing, thresholds, and feature enables.

## License

Copyright (c) 2026. All rights reserved.

## Contributing

1. Follow the coding standards in `docs/architecture.md`
2. Ensure all tests pass before submitting changes
3. Add tests for new functionality
4. Update documentation as needed
