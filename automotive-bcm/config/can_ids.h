/**
 * @file can_ids.h
 * @brief CAN Message Schema for Body Control Module
 * @version 1.0.0
 *
 * This file defines the complete CAN message schema including:
 * - 11-bit standard CAN IDs
 * - Precise byte layouts for all messages
 * - Enum values and valid ranges
 * - Rolling counter and checksum definitions
 */

#ifndef CAN_IDS_H
#define CAN_IDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*******************************************************************************
 * CAN Configuration
 ******************************************************************************/

#define CAN_BAUD_RATE               500000U
#define CAN_MAX_DLC                 8U
#define CAN_SCHEMA_VERSION          0x01U

/* Rolling counter configuration */
#define CAN_COUNTER_MAX             15U
#define CAN_COUNTER_MASK            0x0FU

/* Checksum: XOR of all payload bytes except checksum byte itself */
#define CAN_CHECKSUM_SEED           0xAAU

/*******************************************************************************
 * CAN Message IDs - RX (Commands to BCM)
 ******************************************************************************/

#define CAN_ID_DOOR_CMD             0x100U  /**< Door lock/unlock commands */
#define CAN_ID_LIGHTING_CMD         0x110U  /**< Lighting control commands */
#define CAN_ID_TURN_SIGNAL_CMD      0x120U  /**< Turn signal/hazard commands */
#define CAN_ID_BCM_CONFIG           0x130U  /**< BCM configuration */

/*******************************************************************************
 * CAN Message IDs - TX (Status from BCM)
 ******************************************************************************/

#define CAN_ID_DOOR_STATUS          0x200U  /**< Door state status */
#define CAN_ID_LIGHTING_STATUS      0x210U  /**< Lighting state status */
#define CAN_ID_TURN_SIGNAL_STATUS   0x220U  /**< Turn signal state status */
#define CAN_ID_FAULT_STATUS         0x230U  /**< Fault status */
#define CAN_ID_BCM_HEARTBEAT        0x240U  /**< BCM heartbeat/alive */

/*******************************************************************************
 * DOOR_CMD (0x100) - Door Lock/Unlock Command
 * DLC: 4 bytes
 * 
 * Byte 0: Command
 *   0x01 = Lock all doors
 *   0x02 = Unlock all doors
 *   0x03 = Lock single door (door ID in byte 1)
 *   0x04 = Unlock single door (door ID in byte 1)
 * 
 * Byte 1: Door ID (for single door commands)
 *   0x00 = Front Left (Driver)
 *   0x01 = Front Right (Passenger)
 *   0x02 = Rear Left
 *   0x03 = Rear Right
 *   0xFF = All doors (for lock/unlock all)
 * 
 * Byte 2: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 3: Checksum (XOR of bytes 0-2 with seed 0xAA)
 ******************************************************************************/

#define DOOR_CMD_DLC                4U

/* Door command values */
typedef enum {
    DOOR_CMD_LOCK_ALL       = 0x01,
    DOOR_CMD_UNLOCK_ALL     = 0x02,
    DOOR_CMD_LOCK_SINGLE    = 0x03,
    DOOR_CMD_UNLOCK_SINGLE  = 0x04,
    DOOR_CMD_MAX            = 0x04
} door_cmd_t;

/* Door ID values */
typedef enum {
    DOOR_ID_FRONT_LEFT      = 0x00,
    DOOR_ID_FRONT_RIGHT     = 0x01,
    DOOR_ID_REAR_LEFT       = 0x02,
    DOOR_ID_REAR_RIGHT      = 0x03,
    DOOR_ID_ALL             = 0xFF,
    DOOR_ID_MAX             = 0x03
} door_id_t;

#define DOOR_CMD_BYTE_CMD           0
#define DOOR_CMD_BYTE_DOOR_ID       1
#define DOOR_CMD_BYTE_VER_CTR       2
#define DOOR_CMD_BYTE_CHECKSUM      3

/*******************************************************************************
 * LIGHTING_CMD (0x110) - Lighting Control Command
 * DLC: 4 bytes
 * 
 * Byte 0: Headlight Mode Command
 *   0x00 = Off
 *   0x01 = On (low beam)
 *   0x02 = Auto (ambient sensor controlled)
 *   0x03 = High beam on
 *   0x04 = High beam off
 * 
 * Byte 1: Interior Light Command
 *   0x00 = Off
 *   0x01 = On
 *   0x02 = Auto (door triggered)
 *   [7:4] = Reserved
 *   [3:0] = Brightness (0-15) when On
 * 
 * Byte 2: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 3: Checksum (XOR of bytes 0-2 with seed 0xAA)
 ******************************************************************************/

#define LIGHTING_CMD_DLC            4U

/* Headlight mode values */
typedef enum {
    HEADLIGHT_CMD_OFF       = 0x00,
    HEADLIGHT_CMD_ON        = 0x01,
    HEADLIGHT_CMD_AUTO      = 0x02,
    HEADLIGHT_CMD_HIGH_ON   = 0x03,
    HEADLIGHT_CMD_HIGH_OFF  = 0x04,
    HEADLIGHT_CMD_MAX       = 0x04
} headlight_cmd_t;

/* Interior light mode values */
typedef enum {
    INTERIOR_CMD_OFF        = 0x00,
    INTERIOR_CMD_ON         = 0x01,
    INTERIOR_CMD_AUTO       = 0x02,
    INTERIOR_CMD_MAX        = 0x02
} interior_cmd_t;

#define LIGHTING_CMD_BYTE_HEADLIGHT     0
#define LIGHTING_CMD_BYTE_INTERIOR      1
#define LIGHTING_CMD_BYTE_VER_CTR       2
#define LIGHTING_CMD_BYTE_CHECKSUM      3

#define INTERIOR_BRIGHTNESS_MASK        0x0FU
#define INTERIOR_MODE_MASK              0x03U

/*******************************************************************************
 * TURN_SIGNAL_CMD (0x120) - Turn Signal/Hazard Command
 * DLC: 4 bytes
 * 
 * Byte 0: Turn Signal Command
 *   0x00 = All off
 *   0x01 = Left signal on
 *   0x02 = Right signal on
 *   0x03 = Hazard on
 *   0x04 = Hazard off
 * 
 * Byte 1: Reserved (0x00)
 * 
 * Byte 2: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 3: Checksum (XOR of bytes 0-2 with seed 0xAA)
 ******************************************************************************/

#define TURN_SIGNAL_CMD_DLC         4U

/* Turn signal command values */
typedef enum {
    TURN_CMD_OFF            = 0x00,
    TURN_CMD_LEFT_ON        = 0x01,
    TURN_CMD_RIGHT_ON       = 0x02,
    TURN_CMD_HAZARD_ON      = 0x03,
    TURN_CMD_HAZARD_OFF     = 0x04,
    TURN_CMD_MAX            = 0x04
} turn_cmd_t;

#define TURN_CMD_BYTE_CMD           0
#define TURN_CMD_BYTE_RESERVED      1
#define TURN_CMD_BYTE_VER_CTR       2
#define TURN_CMD_BYTE_CHECKSUM      3

/*******************************************************************************
 * DOOR_STATUS (0x200) - Door State Status
 * DLC: 6 bytes
 * TX Period: 100ms
 * 
 * Byte 0: Door Lock States (bitfield)
 *   Bit 0 = Front Left locked (1=locked, 0=unlocked)
 *   Bit 1 = Front Right locked
 *   Bit 2 = Rear Left locked
 *   Bit 3 = Rear Right locked
 *   Bits 4-7 = Reserved
 * 
 * Byte 1: Door Open States (bitfield)
 *   Bit 0 = Front Left open (1=open, 0=closed)
 *   Bit 1 = Front Right open
 *   Bit 2 = Rear Left open
 *   Bit 3 = Rear Right open
 *   Bits 4-7 = Reserved
 * 
 * Byte 2: Last Command Result
 *   0x00 = OK
 *   0x01 = Invalid command
 *   0x02 = Checksum error
 *   0x03 = Counter error
 *   0x04 = Timeout
 * 
 * Byte 3: Active Fault Count (0-255)
 * 
 * Byte 4: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 5: Checksum (XOR of bytes 0-4 with seed 0xAA)
 ******************************************************************************/

#define DOOR_STATUS_DLC             6U
#define DOOR_STATUS_PERIOD_MS       100U

/* Door lock state bit positions */
#define DOOR_LOCK_BIT_FL            0x01U
#define DOOR_LOCK_BIT_FR            0x02U
#define DOOR_LOCK_BIT_RL            0x04U
#define DOOR_LOCK_BIT_RR            0x08U

/* Door open state bit positions */
#define DOOR_OPEN_BIT_FL            0x01U
#define DOOR_OPEN_BIT_FR            0x02U
#define DOOR_OPEN_BIT_RL            0x04U
#define DOOR_OPEN_BIT_RR            0x08U

/* Command result values */
typedef enum {
    CMD_RESULT_OK               = 0x00,
    CMD_RESULT_INVALID_CMD      = 0x01,
    CMD_RESULT_CHECKSUM_ERROR   = 0x02,
    CMD_RESULT_COUNTER_ERROR    = 0x03,
    CMD_RESULT_TIMEOUT          = 0x04
} cmd_result_t;

#define DOOR_STATUS_BYTE_LOCKS      0
#define DOOR_STATUS_BYTE_OPENS      1
#define DOOR_STATUS_BYTE_RESULT     2
#define DOOR_STATUS_BYTE_FAULTS     3
#define DOOR_STATUS_BYTE_VER_CTR    4
#define DOOR_STATUS_BYTE_CHECKSUM   5

/*******************************************************************************
 * LIGHTING_STATUS (0x210) - Lighting State Status
 * DLC: 6 bytes
 * TX Period: 100ms
 * 
 * Byte 0: Headlight State
 *   0x00 = Off
 *   0x01 = On (low beam)
 *   0x02 = Auto mode active
 *   0x03 = High beam active
 * 
 * Byte 1: Interior Light State
 *   [1:0] Mode (0=off, 1=on, 2=auto)
 *   [5:2] Brightness (0-15)
 *   [7:6] Reserved
 * 
 * Byte 2: Ambient Light Level (0-255, scaled lux/10)
 * 
 * Byte 3: Last Command Result (same enum as door)
 * 
 * Byte 4: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 5: Checksum (XOR of bytes 0-4 with seed 0xAA)
 ******************************************************************************/

#define LIGHTING_STATUS_DLC         6U
#define LIGHTING_STATUS_PERIOD_MS   100U

/* Headlight state values */
typedef enum {
    HEADLIGHT_STATE_OFF         = 0x00,
    HEADLIGHT_STATE_ON          = 0x01,
    HEADLIGHT_STATE_AUTO        = 0x02,
    HEADLIGHT_STATE_HIGH_BEAM   = 0x03
} headlight_state_t;

/* Interior light state masks */
#define INTERIOR_STATE_MODE_MASK        0x03U
#define INTERIOR_STATE_BRIGHTNESS_MASK  0x3CU
#define INTERIOR_STATE_BRIGHTNESS_SHIFT 2U

#define LIGHTING_STATUS_BYTE_HEADLIGHT  0
#define LIGHTING_STATUS_BYTE_INTERIOR   1
#define LIGHTING_STATUS_BYTE_AMBIENT    2
#define LIGHTING_STATUS_BYTE_RESULT     3
#define LIGHTING_STATUS_BYTE_VER_CTR    4
#define LIGHTING_STATUS_BYTE_CHECKSUM   5

/*******************************************************************************
 * TURN_SIGNAL_STATUS (0x220) - Turn Signal State Status
 * DLC: 6 bytes
 * TX Period: 100ms
 * 
 * Byte 0: Turn Signal State
 *   0x00 = All off
 *   0x01 = Left active
 *   0x02 = Right active
 *   0x03 = Hazard active
 * 
 * Byte 1: Output State
 *   Bit 0 = Left lamp on (current flash state)
 *   Bit 1 = Right lamp on (current flash state)
 *   Bits 2-7 = Reserved
 * 
 * Byte 2: Flash Count (0-255, wraps)
 * 
 * Byte 3: Last Command Result
 * 
 * Byte 4: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 5: Checksum (XOR of bytes 0-4 with seed 0xAA)
 ******************************************************************************/

#define TURN_SIGNAL_STATUS_DLC          6U
#define TURN_SIGNAL_STATUS_PERIOD_MS    100U

/* Turn signal state values */
typedef enum {
    TURN_STATE_OFF              = 0x00,
    TURN_STATE_LEFT             = 0x01,
    TURN_STATE_RIGHT            = 0x02,
    TURN_STATE_HAZARD           = 0x03
} turn_state_t;

/* Output state bit positions */
#define TURN_OUTPUT_LEFT_BIT            0x01U
#define TURN_OUTPUT_RIGHT_BIT           0x02U

#define TURN_STATUS_BYTE_STATE          0
#define TURN_STATUS_BYTE_OUTPUT         1
#define TURN_STATUS_BYTE_FLASH_CNT      2
#define TURN_STATUS_BYTE_RESULT         3
#define TURN_STATUS_BYTE_VER_CTR        4
#define TURN_STATUS_BYTE_CHECKSUM       5

/*******************************************************************************
 * FAULT_STATUS (0x230) - Fault Status
 * DLC: 8 bytes
 * TX Period: 500ms (or on change)
 * 
 * Byte 0: Active Fault Flags 1
 *   Bit 0 = Door lock motor fault
 *   Bit 1 = Headlight bulb fault
 *   Bit 2 = Turn signal bulb fault
 *   Bit 3 = CAN communication fault
 *   Bit 4 = Command checksum fault
 *   Bit 5 = Command counter fault
 *   Bit 6 = Timeout fault
 *   Bit 7 = Reserved
 * 
 * Byte 1: Active Fault Flags 2 (reserved for expansion)
 * 
 * Byte 2: Total Fault Count (0-255)
 * 
 * Byte 3: Most Recent Fault Code (0-255)
 * 
 * Byte 4: Fault Timestamp High (seconds since boot, high byte)
 * 
 * Byte 5: Fault Timestamp Low (seconds since boot, low byte)
 * 
 * Byte 6: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 7: Checksum (XOR of bytes 0-6 with seed 0xAA)
 ******************************************************************************/

#define FAULT_STATUS_DLC                8U
#define FAULT_STATUS_PERIOD_MS          500U

/* Fault flag bit positions */
#define FAULT_BIT_DOOR_MOTOR            0x01U
#define FAULT_BIT_HEADLIGHT_BULB        0x02U
#define FAULT_BIT_TURN_BULB             0x04U
#define FAULT_BIT_CAN_COMM              0x08U
#define FAULT_BIT_CMD_CHECKSUM          0x10U
#define FAULT_BIT_CMD_COUNTER           0x20U
#define FAULT_BIT_TIMEOUT               0x40U

/* Fault codes */
typedef enum {
    FAULT_CODE_NONE                 = 0x00,
    FAULT_CODE_DOOR_MOTOR           = 0x01,
    FAULT_CODE_HEADLIGHT_BULB       = 0x02,
    FAULT_CODE_TURN_BULB            = 0x03,
    FAULT_CODE_CAN_COMM             = 0x10,
    FAULT_CODE_INVALID_CHECKSUM     = 0x20,
    FAULT_CODE_INVALID_COUNTER      = 0x21,
    FAULT_CODE_INVALID_CMD          = 0x22,
    FAULT_CODE_INVALID_LENGTH       = 0x23,
    FAULT_CODE_TIMEOUT              = 0x30
} fault_code_t;

#define FAULT_STATUS_BYTE_FLAGS1        0
#define FAULT_STATUS_BYTE_FLAGS2        1
#define FAULT_STATUS_BYTE_COUNT         2
#define FAULT_STATUS_BYTE_RECENT_CODE   3
#define FAULT_STATUS_BYTE_TS_HIGH       4
#define FAULT_STATUS_BYTE_TS_LOW        5
#define FAULT_STATUS_BYTE_VER_CTR       6
#define FAULT_STATUS_BYTE_CHECKSUM      7

/*******************************************************************************
 * BCM_HEARTBEAT (0x240) - BCM Alive/Heartbeat
 * DLC: 4 bytes
 * TX Period: 1000ms
 * 
 * Byte 0: BCM State
 *   0x00 = Init
 *   0x01 = Normal
 *   0x02 = Fault
 *   0x03 = Diagnostic
 * 
 * Byte 1: Uptime (minutes, 0-255, wraps)
 * 
 * Byte 2: [7:4] Version, [3:0] Rolling Counter (0-15)
 * 
 * Byte 3: Checksum (XOR of bytes 0-2 with seed 0xAA)
 ******************************************************************************/

#define BCM_HEARTBEAT_DLC               4U
#define BCM_HEARTBEAT_PERIOD_MS         1000U

/* BCM state values */
typedef enum {
    BCM_STATE_INIT          = 0x00,
    BCM_STATE_NORMAL        = 0x01,
    BCM_STATE_FAULT         = 0x02,
    BCM_STATE_DIAGNOSTIC    = 0x03
} bcm_state_t;

#define HEARTBEAT_BYTE_STATE            0
#define HEARTBEAT_BYTE_UPTIME           1
#define HEARTBEAT_BYTE_VER_CTR          2
#define HEARTBEAT_BYTE_CHECKSUM         3

/*******************************************************************************
 * Utility Macros
 ******************************************************************************/

/** Build version/counter byte: [7:4]=version, [3:0]=counter */
#define CAN_BUILD_VER_CTR(ver, ctr)     (((uint8_t)(ver) << 4) | ((uint8_t)(ctr) & CAN_COUNTER_MASK))

/** Extract version from ver/ctr byte */
#define CAN_GET_VERSION(byte)           (((byte) >> 4) & 0x0FU)

/** Extract counter from ver/ctr byte */
#define CAN_GET_COUNTER(byte)           ((byte) & CAN_COUNTER_MASK)

/** Calculate XOR checksum with seed */
static inline uint8_t can_calculate_checksum(const uint8_t *data, uint8_t len)
{
    uint8_t checksum = CAN_CHECKSUM_SEED;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/** Validate checksum: returns true if valid */
static inline int can_validate_checksum(const uint8_t *data, uint8_t len, uint8_t received_checksum)
{
    return (can_calculate_checksum(data, len) == received_checksum);
}

/** Validate rolling counter: returns true if valid (expected = last + 1, with wrap) */
static inline int can_validate_counter(uint8_t received, uint8_t last)
{
    uint8_t expected = (last + 1) & CAN_COUNTER_MASK;
    return (received == expected);
}

#ifdef __cplusplus
}
#endif

#endif /* CAN_IDS_H */
