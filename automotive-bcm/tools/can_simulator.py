#!/usr/bin/env python3
"""
CAN Bus Simulator for BCM Testing

This tool simulates CAN bus traffic for testing the Body Control Module.
It can send commands and simulate vehicle sensor data.

Requirements:
    pip install python-can

Usage:
    python can_simulator.py [--interface vcan0] [--mode interactive]

For virtual CAN on Linux:
    sudo modprobe vcan
    sudo ip link add dev vcan0 type vcan
    sudo ip link set up vcan0
"""

import argparse
import sys
import time
import threading
import struct
from typing import Optional, Dict, Callable
from dataclasses import dataclass
from enum import IntEnum

try:
    import can
    CAN_AVAILABLE = True
except ImportError:
    CAN_AVAILABLE = False
    print("Warning: python-can not installed. Running in simulation mode.")


# =============================================================================
# CAN ID Definitions (must match can_ids.h)
# =============================================================================

class CanIds(IntEnum):
    """CAN Message IDs"""
    # BCM Status (TX from BCM)
    BCM_STATUS = 0x200
    DOOR_STATUS = 0x210
    LIGHTING_STATUS = 0x220
    TURN_SIGNAL_STATUS = 0x230
    BCM_FAULT = 0x240
    BCM_HEARTBEAT = 0x250
    
    # BCM Commands (RX to BCM)
    DOOR_CMD = 0x300
    LIGHTING_CMD = 0x310
    TURN_SIGNAL_CMD = 0x320
    BCM_CONFIG_CMD = 0x330
    
    # External ECU Messages
    IGNITION_STATUS = 0x100
    VEHICLE_SPEED = 0x110
    ENGINE_STATUS = 0x120
    KEYFOB_CMD = 0x130
    AMBIENT_LIGHT = 0x140
    RAIN_SENSOR = 0x150


class DoorCommands(IntEnum):
    """Door control commands"""
    LOCK_ALL = 0x01
    UNLOCK_ALL = 0x02
    LOCK_SINGLE = 0x03
    UNLOCK_SINGLE = 0x04
    WINDOW_UP = 0x10
    WINDOW_DOWN = 0x11
    WINDOW_STOP = 0x12
    CHILD_LOCK_ON = 0x20
    CHILD_LOCK_OFF = 0x21


class LightingCommands(IntEnum):
    """Lighting control commands"""
    HEADLIGHTS_ON = 0x01
    HEADLIGHTS_OFF = 0x02
    HEADLIGHTS_AUTO = 0x03
    HIGH_BEAM_ON = 0x10
    HIGH_BEAM_OFF = 0x11
    FOG_LIGHTS_ON = 0x20
    FOG_LIGHTS_OFF = 0x21
    INTERIOR_ON = 0x30
    INTERIOR_OFF = 0x31
    INTERIOR_DIM = 0x32


class TurnSignalCommands(IntEnum):
    """Turn signal commands"""
    LEFT_ON = 0x01
    LEFT_OFF = 0x02
    RIGHT_ON = 0x03
    RIGHT_OFF = 0x04
    HAZARD_ON = 0x10
    HAZARD_OFF = 0x11


class IgnitionState(IntEnum):
    """Ignition states"""
    OFF = 0x00
    ACC = 0x01
    ON = 0x02
    START = 0x03


class KeyfobCommands(IntEnum):
    """Key fob commands"""
    LOCK = 0x01
    UNLOCK = 0x02
    TRUNK = 0x03
    PANIC = 0x04


# =============================================================================
# CAN Message Classes
# =============================================================================

@dataclass
class CanMessage:
    """CAN message representation"""
    id: int
    data: bytes
    timestamp: float = 0.0
    
    def __str__(self) -> str:
        hex_data = ' '.join(f'{b:02X}' for b in self.data)
        return f"ID=0x{self.id:03X} [{len(self.data)}] {hex_data}"


# =============================================================================
# CAN Bus Interface
# =============================================================================

class CanInterface:
    """CAN bus interface wrapper"""
    
    def __init__(self, channel: str = 'vcan0', bustype: str = 'socketcan'):
        self.channel = channel
        self.bustype = bustype
        self.bus: Optional[can.Bus] = None
        self.running = False
        self.rx_callback: Optional[Callable] = None
        self.rx_thread: Optional[threading.Thread] = None
        
    def connect(self) -> bool:
        """Connect to CAN bus"""
        if not CAN_AVAILABLE:
            print(f"[SIM] Would connect to {self.channel}")
            return True
            
        try:
            self.bus = can.Bus(channel=self.channel, bustype=self.bustype)
            print(f"Connected to {self.channel}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from CAN bus"""
        self.running = False
        if self.rx_thread:
            self.rx_thread.join(timeout=1.0)
        if self.bus:
            self.bus.shutdown()
            self.bus = None
    
    def send(self, msg: CanMessage) -> bool:
        """Send a CAN message"""
        if not CAN_AVAILABLE:
            print(f"[TX] {msg}")
            return True
            
        if not self.bus:
            return False
            
        try:
            can_msg = can.Message(
                arbitration_id=msg.id,
                data=msg.data,
                is_extended_id=False
            )
            self.bus.send(can_msg)
            print(f"[TX] {msg}")
            return True
        except Exception as e:
            print(f"TX Error: {e}")
            return False
    
    def start_receive(self, callback: Callable):
        """Start receiving messages in background"""
        self.rx_callback = callback
        self.running = True
        self.rx_thread = threading.Thread(target=self._receive_loop, daemon=True)
        self.rx_thread.start()
    
    def _receive_loop(self):
        """Background receive loop"""
        while self.running and self.bus:
            try:
                msg = self.bus.recv(timeout=0.1)
                if msg and self.rx_callback:
                    can_msg = CanMessage(
                        id=msg.arbitration_id,
                        data=bytes(msg.data),
                        timestamp=msg.timestamp
                    )
                    self.rx_callback(can_msg)
            except Exception:
                pass


# =============================================================================
# BCM Simulator
# =============================================================================

class BcmSimulator:
    """BCM test simulator"""
    
    def __init__(self, interface: CanInterface):
        self.can = interface
        self.vehicle_speed = 0
        self.ignition = IgnitionState.OFF
        self.ambient_light = 500  # lux
        self.rain_detected = False
        self.running = False
        
    def start(self):
        """Start the simulator"""
        self.running = True
        self.can.connect()
        self.can.start_receive(self._on_message_received)
        
    def stop(self):
        """Stop the simulator"""
        self.running = False
        self.can.disconnect()
    
    def _on_message_received(self, msg: CanMessage):
        """Handle received CAN message"""
        print(f"[RX] {msg}")
        
        # Parse BCM status messages
        if msg.id == CanIds.BCM_STATUS:
            self._parse_bcm_status(msg.data)
        elif msg.id == CanIds.BCM_HEARTBEAT:
            self._parse_heartbeat(msg.data)
    
    def _parse_bcm_status(self, data: bytes):
        """Parse BCM status message"""
        if len(data) >= 8:
            state = data[0]
            ign = data[1]
            doors = data[2]
            lights = data[3]
            turn = data[4]
            speed = (data[5] << 8) | data[6]
            print(f"  BCM State: {state}, Ignition: {ign}, Speed: {speed} km/h")
    
    def _parse_heartbeat(self, data: bytes):
        """Parse BCM heartbeat message"""
        if len(data) >= 6:
            state = data[0]
            uptime = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4]
            faults = data[5]
            print(f"  Heartbeat: State={state}, Uptime={uptime}ms, Faults={faults}")
    
    # =========================================================================
    # Command Senders
    # =========================================================================
    
    def send_ignition(self, state: IgnitionState):
        """Send ignition status"""
        self.ignition = state
        msg = CanMessage(CanIds.IGNITION_STATUS, bytes([state]))
        self.can.send(msg)
    
    def send_vehicle_speed(self, speed_kmh: int):
        """Send vehicle speed"""
        self.vehicle_speed = speed_kmh
        data = struct.pack('>H', speed_kmh)
        msg = CanMessage(CanIds.VEHICLE_SPEED, data)
        self.can.send(msg)
    
    def send_engine_status(self, running: bool):
        """Send engine status"""
        msg = CanMessage(CanIds.ENGINE_STATUS, bytes([1 if running else 0]))
        self.can.send(msg)
    
    def send_keyfob(self, cmd: KeyfobCommands):
        """Send key fob command"""
        msg = CanMessage(CanIds.KEYFOB_CMD, bytes([cmd]))
        self.can.send(msg)
    
    def send_ambient_light(self, lux: int):
        """Send ambient light sensor value"""
        self.ambient_light = lux
        data = struct.pack('>H', lux)
        msg = CanMessage(CanIds.AMBIENT_LIGHT, data)
        self.can.send(msg)
    
    def send_rain_sensor(self, detected: bool):
        """Send rain sensor status"""
        self.rain_detected = detected
        msg = CanMessage(CanIds.RAIN_SENSOR, bytes([1 if detected else 0]))
        self.can.send(msg)
    
    def send_door_command(self, cmd: DoorCommands, door: int = 0, extra: int = 0):
        """Send door control command"""
        msg = CanMessage(CanIds.DOOR_CMD, bytes([cmd, door, extra]))
        self.can.send(msg)
    
    def send_lighting_command(self, cmd: LightingCommands, param: int = 0):
        """Send lighting control command"""
        msg = CanMessage(CanIds.LIGHTING_CMD, bytes([cmd, param]))
        self.can.send(msg)
    
    def send_turn_signal_command(self, cmd: TurnSignalCommands):
        """Send turn signal command"""
        msg = CanMessage(CanIds.TURN_SIGNAL_CMD, bytes([cmd]))
        self.can.send(msg)


# =============================================================================
# Interactive Console
# =============================================================================

def print_help():
    """Print help message"""
    print("""
BCM CAN Simulator Commands:
==========================

Vehicle State:
  ign <off|acc|on|start>  - Set ignition state
  speed <kmh>             - Set vehicle speed
  engine <on|off>         - Set engine status
  light <lux>             - Set ambient light level
  rain <on|off>           - Set rain sensor

Key Fob:
  key <lock|unlock|trunk|panic>

Door Control:
  door lock               - Lock all doors
  door unlock             - Unlock all doors
  door lock <0-3>         - Lock specific door
  door unlock <0-3>       - Unlock specific door
  window up <0-3>         - Window up
  window down <0-3>       - Window down
  window stop <0-3>       - Stop window
  child on <2|3>          - Enable child lock
  child off <2|3>         - Disable child lock

Lighting:
  head on                 - Headlights on
  head off                - Headlights off
  head auto               - Auto headlights
  high on                 - High beam on
  high off                - High beam off
  fog on                  - Fog lights on
  fog off                 - Fog lights off
  int on                  - Interior light on
  int off                 - Interior light off
  int dim <0-255>         - Set interior brightness

Turn Signals:
  turn left               - Left turn signal on
  turn right              - Right turn signal on
  turn off                - Turn signals off
  hazard on               - Hazard lights on
  hazard off              - Hazard lights off

Other:
  demo                    - Run demo sequence
  help                    - Show this help
  quit                    - Exit simulator
""")


def run_interactive(sim: BcmSimulator):
    """Run interactive console"""
    print_help()
    
    while True:
        try:
            cmd = input("\n> ").strip().lower()
            parts = cmd.split()
            
            if not parts:
                continue
            
            # Ignition
            if parts[0] == 'ign' and len(parts) > 1:
                states = {'off': IgnitionState.OFF, 'acc': IgnitionState.ACC,
                         'on': IgnitionState.ON, 'start': IgnitionState.START}
                if parts[1] in states:
                    sim.send_ignition(states[parts[1]])
            
            # Speed
            elif parts[0] == 'speed' and len(parts) > 1:
                sim.send_vehicle_speed(int(parts[1]))
            
            # Engine
            elif parts[0] == 'engine' and len(parts) > 1:
                sim.send_engine_status(parts[1] == 'on')
            
            # Ambient light
            elif parts[0] == 'light' and len(parts) > 1:
                sim.send_ambient_light(int(parts[1]))
            
            # Rain
            elif parts[0] == 'rain' and len(parts) > 1:
                sim.send_rain_sensor(parts[1] == 'on')
            
            # Key fob
            elif parts[0] == 'key' and len(parts) > 1:
                cmds = {'lock': KeyfobCommands.LOCK, 'unlock': KeyfobCommands.UNLOCK,
                       'trunk': KeyfobCommands.TRUNK, 'panic': KeyfobCommands.PANIC}
                if parts[1] in cmds:
                    sim.send_keyfob(cmds[parts[1]])
            
            # Door commands
            elif parts[0] == 'door' and len(parts) > 1:
                if parts[1] == 'lock':
                    if len(parts) > 2:
                        sim.send_door_command(DoorCommands.LOCK_SINGLE, int(parts[2]))
                    else:
                        sim.send_door_command(DoorCommands.LOCK_ALL)
                elif parts[1] == 'unlock':
                    if len(parts) > 2:
                        sim.send_door_command(DoorCommands.UNLOCK_SINGLE, int(parts[2]))
                    else:
                        sim.send_door_command(DoorCommands.UNLOCK_ALL)
            
            # Window commands
            elif parts[0] == 'window' and len(parts) > 2:
                door = int(parts[2])
                if parts[1] == 'up':
                    sim.send_door_command(DoorCommands.WINDOW_UP, door)
                elif parts[1] == 'down':
                    sim.send_door_command(DoorCommands.WINDOW_DOWN, door)
                elif parts[1] == 'stop':
                    sim.send_door_command(DoorCommands.WINDOW_STOP, door)
            
            # Child lock
            elif parts[0] == 'child' and len(parts) > 2:
                door = int(parts[2])
                if parts[1] == 'on':
                    sim.send_door_command(DoorCommands.CHILD_LOCK_ON, door)
                elif parts[1] == 'off':
                    sim.send_door_command(DoorCommands.CHILD_LOCK_OFF, door)
            
            # Headlights
            elif parts[0] == 'head' and len(parts) > 1:
                if parts[1] == 'on':
                    sim.send_lighting_command(LightingCommands.HEADLIGHTS_ON)
                elif parts[1] == 'off':
                    sim.send_lighting_command(LightingCommands.HEADLIGHTS_OFF)
                elif parts[1] == 'auto':
                    sim.send_lighting_command(LightingCommands.HEADLIGHTS_AUTO)
            
            # High beam
            elif parts[0] == 'high' and len(parts) > 1:
                if parts[1] == 'on':
                    sim.send_lighting_command(LightingCommands.HIGH_BEAM_ON)
                elif parts[1] == 'off':
                    sim.send_lighting_command(LightingCommands.HIGH_BEAM_OFF)
            
            # Fog lights
            elif parts[0] == 'fog' and len(parts) > 1:
                if parts[1] == 'on':
                    sim.send_lighting_command(LightingCommands.FOG_LIGHTS_ON)
                elif parts[1] == 'off':
                    sim.send_lighting_command(LightingCommands.FOG_LIGHTS_OFF)
            
            # Interior light
            elif parts[0] == 'int' and len(parts) > 1:
                if parts[1] == 'on':
                    sim.send_lighting_command(LightingCommands.INTERIOR_ON)
                elif parts[1] == 'off':
                    sim.send_lighting_command(LightingCommands.INTERIOR_OFF)
                elif parts[1] == 'dim' and len(parts) > 2:
                    sim.send_lighting_command(LightingCommands.INTERIOR_DIM, int(parts[2]))
            
            # Turn signals
            elif parts[0] == 'turn' and len(parts) > 1:
                if parts[1] == 'left':
                    sim.send_turn_signal_command(TurnSignalCommands.LEFT_ON)
                elif parts[1] == 'right':
                    sim.send_turn_signal_command(TurnSignalCommands.RIGHT_ON)
                elif parts[1] == 'off':
                    sim.send_turn_signal_command(TurnSignalCommands.LEFT_OFF)
            
            # Hazard
            elif parts[0] == 'hazard' and len(parts) > 1:
                if parts[1] == 'on':
                    sim.send_turn_signal_command(TurnSignalCommands.HAZARD_ON)
                elif parts[1] == 'off':
                    sim.send_turn_signal_command(TurnSignalCommands.HAZARD_OFF)
            
            # Demo
            elif parts[0] == 'demo':
                run_demo(sim)
            
            # Help
            elif parts[0] == 'help':
                print_help()
            
            # Quit
            elif parts[0] in ['quit', 'exit', 'q']:
                break
            
            else:
                print("Unknown command. Type 'help' for available commands.")
                
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")


def run_demo(sim: BcmSimulator):
    """Run a demonstration sequence"""
    print("\n=== Running Demo Sequence ===\n")
    
    steps = [
        ("Setting ignition to ACC", lambda: sim.send_ignition(IgnitionState.ACC)),
        ("Unlocking doors via key fob", lambda: sim.send_keyfob(KeyfobCommands.UNLOCK)),
        ("Setting ignition to ON", lambda: sim.send_ignition(IgnitionState.ON)),
        ("Starting engine", lambda: sim.send_engine_status(True)),
        ("Accelerating to 20 km/h", lambda: sim.send_vehicle_speed(20)),
        ("Turning on headlights", lambda: sim.send_lighting_command(LightingCommands.HEADLIGHTS_ON)),
        ("Left turn signal", lambda: sim.send_turn_signal_command(TurnSignalCommands.LEFT_ON)),
        ("Turn signal off", lambda: sim.send_turn_signal_command(TurnSignalCommands.LEFT_OFF)),
        ("Slowing to 0 km/h", lambda: sim.send_vehicle_speed(0)),
        ("Turning off engine", lambda: sim.send_engine_status(False)),
        ("Setting ignition to OFF", lambda: sim.send_ignition(IgnitionState.OFF)),
        ("Locking doors via key fob", lambda: sim.send_keyfob(KeyfobCommands.LOCK)),
    ]
    
    for desc, action in steps:
        print(f"  {desc}...")
        action()
        time.sleep(1.0)
    
    print("\n=== Demo Complete ===\n")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description='BCM CAN Bus Simulator')
    parser.add_argument('--interface', '-i', default='vcan0',
                       help='CAN interface (default: vcan0)')
    parser.add_argument('--bustype', '-b', default='socketcan',
                       help='CAN bus type (default: socketcan)')
    parser.add_argument('--mode', '-m', choices=['interactive', 'demo'],
                       default='interactive', help='Run mode')
    
    args = parser.parse_args()
    
    print("BCM CAN Bus Simulator")
    print("=====================\n")
    
    interface = CanInterface(args.interface, args.bustype)
    sim = BcmSimulator(interface)
    
    try:
        sim.start()
        
        if args.mode == 'demo':
            run_demo(sim)
        else:
            run_interactive(sim)
            
    except KeyboardInterrupt:
        print("\nShutting down...")
    finally:
        sim.stop()
    
    print("Goodbye!")


if __name__ == '__main__':
    main()
