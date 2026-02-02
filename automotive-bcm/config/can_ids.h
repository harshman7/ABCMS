/**
 * @file can_ids.h
 * @brief CAN Message ID Definitions for Body Control Module
 *
 * This file contains all CAN message identifiers used by the BCM.
 * IDs follow standard automotive CAN 2.0B conventions.
 *
 * CAN ID Structure (11-bit standard):
 *   Bits 10-8: Priority (0=highest, 7=lowest)
 *   Bits 7-4:  Source ECU ID
 *   Bits 3-0:  Message Type
 *
 * @note Modify these values according to your vehicle network specification
 */

#ifndef CAN_IDS_H
#define CAN_IDS_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CAN Network Configuration
 * ========================================================================== */

/** CAN bus baud rate in bits per second */
#define CAN_BAUD_RATE           500000U

/** Maximum CAN payload size (standard CAN) */
#define CAN_MAX_DATA_LEN        8U

/* =============================================================================
 * BCM Transmit Message IDs (Messages sent by BCM)
 * ========================================================================== */

/** BCM Status broadcast message */
#define CAN_ID_BCM_STATUS               0x200U

/** Door status message (locks, windows, positions) */
#define CAN_ID_DOOR_STATUS              0x210U

/** Lighting status message (all lights) */
#define CAN_ID_LIGHTING_STATUS          0x220U

/** Turn signal status message */
#define CAN_ID_TURN_SIGNAL_STATUS       0x230U

/** BCM fault/diagnostic message */
#define CAN_ID_BCM_FAULT                0x240U

/** BCM heartbeat (alive signal) */
#define CAN_ID_BCM_HEARTBEAT            0x250U

/* =============================================================================
 * BCM Receive Message IDs (Commands received by BCM)
 * ========================================================================== */

/** Door control commands */
#define CAN_ID_DOOR_CMD                 0x300U

/** Lighting control commands */
#define CAN_ID_LIGHTING_CMD             0x310U

/** Turn signal control commands */
#define CAN_ID_TURN_SIGNAL_CMD          0x320U

/** BCM configuration commands */
#define CAN_ID_BCM_CONFIG_CMD           0x330U

/** Diagnostic request message */
#define CAN_ID_DIAG_REQUEST             0x7DFU

/** BCM-specific diagnostic request */
#define CAN_ID_BCM_DIAG_REQUEST         0x720U

/* =============================================================================
 * External ECU Message IDs (Messages from other modules)
 * ========================================================================== */

/** Ignition status from Body Computer */
#define CAN_ID_IGNITION_STATUS          0x100U

/** Vehicle speed from ABS/ESP */
#define CAN_ID_VEHICLE_SPEED            0x110U

/** Engine status from ECM */
#define CAN_ID_ENGINE_STATUS            0x120U

/** Key fob commands (from PEPS/RKE module) */
#define CAN_ID_KEYFOB_CMD               0x130U

/** Ambient light sensor data */
#define CAN_ID_AMBIENT_LIGHT            0x140U

/** Rain sensor data */
#define CAN_ID_RAIN_SENSOR              0x150U

/* =============================================================================
 * Diagnostic Message IDs (UDS/OBD-II)
 * ========================================================================== */

/** BCM diagnostic response */
#define CAN_ID_BCM_DIAG_RESPONSE        0x728U

/** Functional diagnostic response (broadcast) */
#define CAN_ID_DIAG_RESPONSE            0x7E8U

/* =============================================================================
 * Message ID Masks and Filters
 * ========================================================================== */

/** Mask for BCM transmit messages */
#define CAN_MASK_BCM_TX                 0x7F0U

/** Mask for BCM receive messages */
#define CAN_MASK_BCM_RX                 0x7F0U

/** Mask for diagnostic messages */
#define CAN_MASK_DIAG                   0x700U

/* =============================================================================
 * Message Data Field Definitions
 * ========================================================================== */

/* Door Command Message (CAN_ID_DOOR_CMD) Byte 0 */
#define DOOR_CMD_LOCK_ALL               0x01U
#define DOOR_CMD_UNLOCK_ALL             0x02U
#define DOOR_CMD_LOCK_SINGLE            0x03U
#define DOOR_CMD_UNLOCK_SINGLE          0x04U
#define DOOR_CMD_WINDOW_UP              0x10U
#define DOOR_CMD_WINDOW_DOWN            0x11U
#define DOOR_CMD_WINDOW_STOP            0x12U
#define DOOR_CMD_CHILD_LOCK_ON          0x20U
#define DOOR_CMD_CHILD_LOCK_OFF         0x21U

/* Lighting Command Message (CAN_ID_LIGHTING_CMD) Byte 0 */
#define LIGHT_CMD_HEADLIGHTS_ON         0x01U
#define LIGHT_CMD_HEADLIGHTS_OFF        0x02U
#define LIGHT_CMD_HEADLIGHTS_AUTO       0x03U
#define LIGHT_CMD_HIGH_BEAM_ON          0x10U
#define LIGHT_CMD_HIGH_BEAM_OFF         0x11U
#define LIGHT_CMD_FOG_LIGHTS_ON         0x20U
#define LIGHT_CMD_FOG_LIGHTS_OFF        0x21U
#define LIGHT_CMD_INTERIOR_ON           0x30U
#define LIGHT_CMD_INTERIOR_OFF          0x31U
#define LIGHT_CMD_INTERIOR_DIM          0x32U

/* Turn Signal Command Message (CAN_ID_TURN_SIGNAL_CMD) Byte 0 */
#define TURN_CMD_LEFT_ON                0x01U
#define TURN_CMD_LEFT_OFF               0x02U
#define TURN_CMD_RIGHT_ON               0x03U
#define TURN_CMD_RIGHT_OFF              0x04U
#define TURN_CMD_HAZARD_ON              0x10U
#define TURN_CMD_HAZARD_OFF             0x11U

/* Ignition Status Message (CAN_ID_IGNITION_STATUS) Byte 0 */
#define IGN_STATUS_OFF                  0x00U
#define IGN_STATUS_ACC                  0x01U
#define IGN_STATUS_ON                   0x02U
#define IGN_STATUS_START                0x03U

#ifdef __cplusplus
}
#endif

#endif /* CAN_IDS_H */
