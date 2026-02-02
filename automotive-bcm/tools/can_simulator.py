#!/usr/bin/env python3
"""
BCM CAN Simulator

Sends CAN frames to vcan0 to test the Body Control Module.
Includes predefined scenarios and manual command mode.

Requirements:
    pip install python-can

Usage:
    python can_simulator.py [options]
    
Options:
    --interface, -i    CAN interface (default: vcan0)
    --scenario, -s     Run scenario: 1, 2, 3, or 'all'
    --interactive      Interactive command mode
    
Examples:
    python can_simulator.py -s 1              # Run scenario 1
    python can_simulator.py -s all            # Run all scenarios
    python can_simulator.py --interactive     # Interactive mode
"""

import argparse
import time
import struct
import sys

# Try to import python-can, fallback to socket
try:
    import can
    CAN_AVAILABLE = True
except ImportError:
    CAN_AVAILABLE = False
    print("Warning: python-can not installed. Using socket fallback.")
    import socket

# =============================================================================
# CAN IDs and Constants (must match can_ids.h)
# =============================================================================

CAN_ID_DOOR_CMD         = 0x100
CAN_ID_LIGHTING_CMD     = 0x110
CAN_ID_TURN_SIGNAL_CMD  = 0x120

CAN_ID_DOOR_STATUS      = 0x200
CAN_ID_LIGHTING_STATUS  = 0x210
CAN_ID_TURN_STATUS      = 0x220
CAN_ID_FAULT_STATUS     = 0x230
CAN_ID_BCM_HEARTBEAT    = 0x240

CAN_SCHEMA_VERSION      = 0x01
CAN_CHECKSUM_SEED       = 0xAA

# Door commands
DOOR_CMD_LOCK_ALL       = 0x01
DOOR_CMD_UNLOCK_ALL     = 0x02
DOOR_CMD_LOCK_SINGLE    = 0x03
DOOR_CMD_UNLOCK_SINGLE  = 0x04

# Lighting commands
HEADLIGHT_CMD_OFF       = 0x00
HEADLIGHT_CMD_ON        = 0x01
HEADLIGHT_CMD_AUTO      = 0x02
HEADLIGHT_CMD_HIGH_ON   = 0x03
HEADLIGHT_CMD_HIGH_OFF  = 0x04

INTERIOR_CMD_OFF        = 0x00
INTERIOR_CMD_ON         = 0x01
INTERIOR_CMD_AUTO       = 0x02

# Turn signal commands
TURN_CMD_OFF            = 0x00
TURN_CMD_LEFT_ON        = 0x01
TURN_CMD_RIGHT_ON       = 0x02
TURN_CMD_HAZARD_ON      = 0x03
TURN_CMD_HAZARD_OFF     = 0x04

# =============================================================================
# Helper Functions
# =============================================================================

def calculate_checksum(data):
    """Calculate XOR checksum with seed"""
    checksum = CAN_CHECKSUM_SEED
    for byte in data:
        checksum ^= byte
    return checksum

def build_ver_ctr(version, counter):
    """Build version/counter byte"""
    return ((version & 0x0F) << 4) | (counter & 0x0F)

class Counter:
    """Rolling counter manager"""
    def __init__(self):
        self.counters = {}
    
    def get(self, can_id):
        if can_id not in self.counters:
            self.counters[can_id] = 0
        val = self.counters[can_id]
        self.counters[can_id] = (val + 1) & 0x0F
        return val

counter = Counter()

# =============================================================================
# Frame Builders
# =============================================================================

def build_door_cmd(cmd, door_id=0xFF):
    """Build door command frame"""
    ctr = counter.get(CAN_ID_DOOR_CMD)
    data = [cmd, door_id, build_ver_ctr(CAN_SCHEMA_VERSION, ctr), 0]
    data[3] = calculate_checksum(data[:3])
    return (CAN_ID_DOOR_CMD, bytes(data))

def build_lighting_cmd(headlight, interior=0):
    """Build lighting command frame"""
    ctr = counter.get(CAN_ID_LIGHTING_CMD)
    data = [headlight, interior, build_ver_ctr(CAN_SCHEMA_VERSION, ctr), 0]
    data[3] = calculate_checksum(data[:3])
    return (CAN_ID_LIGHTING_CMD, bytes(data))

def build_turn_cmd(cmd):
    """Build turn signal command frame"""
    ctr = counter.get(CAN_ID_TURN_SIGNAL_CMD)
    data = [cmd, 0, build_ver_ctr(CAN_SCHEMA_VERSION, ctr), 0]
    data[3] = calculate_checksum(data[:3])
    return (CAN_ID_TURN_SIGNAL_CMD, bytes(data))

def build_malformed_frame(can_id, error_type):
    """Build intentionally malformed frame for testing"""
    if error_type == "wrong_dlc":
        return (can_id, bytes([0x01, 0x02]))  # Too short
    elif error_type == "bad_checksum":
        data = [0x01, 0x00, build_ver_ctr(CAN_SCHEMA_VERSION, 0), 0xFF]
        return (can_id, bytes(data))
    elif error_type == "bad_command":
        ctr = counter.get(can_id)
        data = [0xFF, 0x00, build_ver_ctr(CAN_SCHEMA_VERSION, ctr), 0]
        data[3] = calculate_checksum(data[:3])
        return (can_id, bytes(data))
    return (can_id, bytes([0]))

# =============================================================================
# CAN Interface
# =============================================================================

class CANInterface:
    def __init__(self, interface='vcan0'):
        self.interface = interface
        self.bus = None
        
    def connect(self):
        if CAN_AVAILABLE:
            try:
                self.bus = can.interface.Bus(channel=self.interface, 
                                             bustype='socketcan')
                print(f"[CAN] Connected to {self.interface}")
                return True
            except Exception as e:
                print(f"[CAN] Connection failed: {e}")
                return False
        else:
            print(f"[CAN] Simulating connection to {self.interface}")
            return True
    
    def disconnect(self):
        if self.bus:
            self.bus.shutdown()
            self.bus = None
    
    def send(self, can_id, data):
        if CAN_AVAILABLE and self.bus:
            msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=False)
            try:
                self.bus.send(msg)
                return True
            except Exception as e:
                print(f"[CAN] Send failed: {e}")
                return False
        else:
            # Simulate
            return True
    
    def receive(self, timeout=0.1):
        if CAN_AVAILABLE and self.bus:
            return self.bus.recv(timeout)
        return None

def print_frame(direction, can_id, data):
    """Pretty print a CAN frame"""
    data_hex = ' '.join(f'{b:02X}' for b in data)
    print(f"  [{direction}] ID=0x{can_id:03X} [{len(data)}] {data_hex}")

# =============================================================================
# Scenarios
# =============================================================================

def scenario_1(can_if):
    """
    Scenario 1: Basic operation
    - Unlock doors
    - Turn on headlights
    - Left turn signal
    """
    print("\n" + "="*60)
    print("SCENARIO 1: Basic Operation")
    print("="*60)
    
    steps = [
        ("Unlock all doors", build_door_cmd(DOOR_CMD_UNLOCK_ALL)),
        ("Turn on headlights", build_lighting_cmd(HEADLIGHT_CMD_ON)),
        ("Left turn signal ON", build_turn_cmd(TURN_CMD_LEFT_ON)),
        ("Wait 2s for flashing...", None),
        ("Left turn signal OFF", build_turn_cmd(TURN_CMD_OFF)),
        ("Turn off headlights", build_lighting_cmd(HEADLIGHT_CMD_OFF)),
        ("Lock all doors", build_door_cmd(DOOR_CMD_LOCK_ALL)),
    ]
    
    for desc, frame in steps:
        print(f"\n-> {desc}")
        if frame:
            can_id, data = frame
            print_frame("TX", can_id, data)
            can_if.send(can_id, data)
        time.sleep(0.5 if frame else 2.0)
    
    print("\nScenario 1 complete.\n")

def scenario_2(can_if):
    """
    Scenario 2: Hazard lights
    - Hazard on
    - Wait for flashing
    - Hazard off
    """
    print("\n" + "="*60)
    print("SCENARIO 2: Hazard Lights")
    print("="*60)
    
    steps = [
        ("Hazard lights ON", build_turn_cmd(TURN_CMD_HAZARD_ON)),
        ("Wait 3s for flashing...", None),
        ("Hazard lights OFF", build_turn_cmd(TURN_CMD_HAZARD_OFF)),
    ]
    
    for desc, frame in steps:
        print(f"\n-> {desc}")
        if frame:
            can_id, data = frame
            print_frame("TX", can_id, data)
            can_if.send(can_id, data)
        time.sleep(0.5 if frame else 3.0)
    
    print("\nScenario 2 complete.\n")

def scenario_3(can_if):
    """
    Scenario 3: Malformed frames (fault injection)
    - Wrong DLC
    - Bad checksum
    - Invalid command
    """
    print("\n" + "="*60)
    print("SCENARIO 3: Malformed Frames (Fault Injection)")
    print("="*60)
    
    print("\n-> Sending frame with wrong DLC (should trigger fault)")
    can_id, data = build_malformed_frame(CAN_ID_DOOR_CMD, "wrong_dlc")
    print_frame("TX", can_id, data)
    can_if.send(can_id, data)
    time.sleep(0.5)
    
    print("\n-> Sending frame with bad checksum (should trigger fault)")
    can_id, data = build_malformed_frame(CAN_ID_LIGHTING_CMD, "bad_checksum")
    print_frame("TX", can_id, data)
    can_if.send(can_id, data)
    time.sleep(0.5)
    
    print("\n-> Sending frame with invalid command (should trigger fault)")
    can_id, data = build_malformed_frame(CAN_ID_TURN_SIGNAL_CMD, "bad_command")
    print_frame("TX", can_id, data)
    can_if.send(can_id, data)
    time.sleep(0.5)
    
    print("\n[Expected: BCM should log checksum/counter/invalid command faults]")
    print("\nScenario 3 complete.\n")

def run_all_scenarios(can_if):
    """Run all scenarios"""
    scenario_1(can_if)
    time.sleep(1)
    scenario_2(can_if)
    time.sleep(1)
    scenario_3(can_if)

# =============================================================================
# Interactive Mode
# =============================================================================

def print_help():
    print("""
Commands:
  door lock             - Lock all doors
  door unlock           - Unlock all doors
  door lock <0-3>       - Lock single door
  door unlock <0-3>     - Unlock single door
  
  light on              - Headlights on
  light off             - Headlights off
  light auto            - Auto headlights
  light high            - High beam on
  light low             - High beam off
  
  turn left             - Left signal on
  turn right            - Right signal on
  turn off              - Turn signals off
  hazard on             - Hazard on
  hazard off            - Hazard off
  
  scenario <1|2|3|all>  - Run scenario
  help                  - Show this help
  quit                  - Exit
""")

def interactive_mode(can_if):
    print("\nInteractive mode. Type 'help' for commands, 'quit' to exit.\n")
    
    while True:
        try:
            cmd = input("> ").strip().lower()
            parts = cmd.split()
            
            if not parts:
                continue
            
            if parts[0] == 'quit' or parts[0] == 'exit':
                break
            elif parts[0] == 'help':
                print_help()
            elif parts[0] == 'scenario' and len(parts) > 1:
                if parts[1] == '1':
                    scenario_1(can_if)
                elif parts[1] == '2':
                    scenario_2(can_if)
                elif parts[1] == '3':
                    scenario_3(can_if)
                elif parts[1] == 'all':
                    run_all_scenarios(can_if)
            elif parts[0] == 'door' and len(parts) > 1:
                if parts[1] == 'lock':
                    if len(parts) > 2:
                        frame = build_door_cmd(DOOR_CMD_LOCK_SINGLE, int(parts[2]))
                    else:
                        frame = build_door_cmd(DOOR_CMD_LOCK_ALL)
                    print_frame("TX", frame[0], frame[1])
                    can_if.send(*frame)
                elif parts[1] == 'unlock':
                    if len(parts) > 2:
                        frame = build_door_cmd(DOOR_CMD_UNLOCK_SINGLE, int(parts[2]))
                    else:
                        frame = build_door_cmd(DOOR_CMD_UNLOCK_ALL)
                    print_frame("TX", frame[0], frame[1])
                    can_if.send(*frame)
            elif parts[0] == 'light' and len(parts) > 1:
                cmd_map = {
                    'on': HEADLIGHT_CMD_ON,
                    'off': HEADLIGHT_CMD_OFF,
                    'auto': HEADLIGHT_CMD_AUTO,
                    'high': HEADLIGHT_CMD_HIGH_ON,
                    'low': HEADLIGHT_CMD_HIGH_OFF
                }
                if parts[1] in cmd_map:
                    frame = build_lighting_cmd(cmd_map[parts[1]])
                    print_frame("TX", frame[0], frame[1])
                    can_if.send(*frame)
            elif parts[0] == 'turn' and len(parts) > 1:
                cmd_map = {
                    'left': TURN_CMD_LEFT_ON,
                    'right': TURN_CMD_RIGHT_ON,
                    'off': TURN_CMD_OFF
                }
                if parts[1] in cmd_map:
                    frame = build_turn_cmd(cmd_map[parts[1]])
                    print_frame("TX", frame[0], frame[1])
                    can_if.send(*frame)
            elif parts[0] == 'hazard' and len(parts) > 1:
                if parts[1] == 'on':
                    frame = build_turn_cmd(TURN_CMD_HAZARD_ON)
                else:
                    frame = build_turn_cmd(TURN_CMD_HAZARD_OFF)
                print_frame("TX", frame[0], frame[1])
                can_if.send(*frame)
            else:
                print("Unknown command. Type 'help' for available commands.")
                
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")

# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='BCM CAN Simulator')
    parser.add_argument('-i', '--interface', default='vcan0',
                        help='CAN interface (default: vcan0)')
    parser.add_argument('-s', '--scenario', choices=['1', '2', '3', 'all'],
                        help='Run scenario')
    parser.add_argument('--interactive', action='store_true',
                        help='Interactive mode')
    
    args = parser.parse_args()
    
    print("="*60)
    print("BCM CAN Simulator")
    print("="*60)
    
    can_if = CANInterface(args.interface)
    
    if not can_if.connect():
        print("Failed to connect to CAN interface")
        print("\nTo create a virtual CAN interface:")
        print("  sudo modprobe vcan")
        print("  sudo ip link add dev vcan0 type vcan")
        print("  sudo ip link set up vcan0")
        sys.exit(1)
    
    try:
        if args.scenario:
            if args.scenario == '1':
                scenario_1(can_if)
            elif args.scenario == '2':
                scenario_2(can_if)
            elif args.scenario == '3':
                scenario_3(can_if)
            elif args.scenario == 'all':
                run_all_scenarios(can_if)
        elif args.interactive:
            interactive_mode(can_if)
        else:
            print("\nNo action specified. Use --scenario or --interactive")
            print("Use --help for options\n")
    finally:
        can_if.disconnect()
    
    print("Goodbye!")

if __name__ == '__main__':
    main()
