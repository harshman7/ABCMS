/**
 * @file fault_manager.h
 * @brief Fault Management Module Interface
 *
 * This module handles fault detection, logging, recovery attempts,
 * and diagnostic trouble code (DTC) management for the BCM.
 */

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"

/* =============================================================================
 * Fault Code Definitions
 * ========================================================================== */

/** Fault code type */
typedef uint16_t fault_code_t;

/** Fault severity levels */
typedef enum {
    FAULT_SEVERITY_INFO = 0,    /**< Informational only */
    FAULT_SEVERITY_WARNING,     /**< Warning - degraded operation */
    FAULT_SEVERITY_ERROR,       /**< Error - feature disabled */
    FAULT_SEVERITY_CRITICAL     /**< Critical - system shutdown */
} fault_severity_t;

/** Fault status */
typedef enum {
    FAULT_STATUS_INACTIVE = 0,  /**< Fault not present */
    FAULT_STATUS_PENDING,       /**< Fault detected, debouncing */
    FAULT_STATUS_ACTIVE,        /**< Fault confirmed active */
    FAULT_STATUS_HEALED,        /**< Fault healed, in healing period */
    FAULT_STATUS_STORED         /**< Historical fault (cleared) */
} fault_status_t;

/* Fault code ranges */
#define FAULT_CODE_DOOR_BASE        0x1000U
#define FAULT_CODE_LIGHTING_BASE    0x2000U
#define FAULT_CODE_TURN_SIGNAL_BASE 0x3000U
#define FAULT_CODE_CAN_BASE         0x4000U
#define FAULT_CODE_SYSTEM_BASE      0x5000U

/* Door faults */
#define FAULT_DOOR_LOCK_FL_STUCK    (FAULT_CODE_DOOR_BASE + 0x01U)
#define FAULT_DOOR_LOCK_FR_STUCK    (FAULT_CODE_DOOR_BASE + 0x02U)
#define FAULT_DOOR_LOCK_RL_STUCK    (FAULT_CODE_DOOR_BASE + 0x03U)
#define FAULT_DOOR_LOCK_RR_STUCK    (FAULT_CODE_DOOR_BASE + 0x04U)
#define FAULT_DOOR_WINDOW_FL_MOTOR  (FAULT_CODE_DOOR_BASE + 0x10U)
#define FAULT_DOOR_WINDOW_FR_MOTOR  (FAULT_CODE_DOOR_BASE + 0x11U)
#define FAULT_DOOR_WINDOW_RL_MOTOR  (FAULT_CODE_DOOR_BASE + 0x12U)
#define FAULT_DOOR_WINDOW_RR_MOTOR  (FAULT_CODE_DOOR_BASE + 0x13U)
#define FAULT_DOOR_WINDOW_ANTI_PINCH (FAULT_CODE_DOOR_BASE + 0x20U)

/* Lighting faults */
#define FAULT_LIGHT_HEADLIGHT_LEFT  (FAULT_CODE_LIGHTING_BASE + 0x01U)
#define FAULT_LIGHT_HEADLIGHT_RIGHT (FAULT_CODE_LIGHTING_BASE + 0x02U)
#define FAULT_LIGHT_HIGH_BEAM_LEFT  (FAULT_CODE_LIGHTING_BASE + 0x03U)
#define FAULT_LIGHT_HIGH_BEAM_RIGHT (FAULT_CODE_LIGHTING_BASE + 0x04U)
#define FAULT_LIGHT_TAIL_LEFT       (FAULT_CODE_LIGHTING_BASE + 0x10U)
#define FAULT_LIGHT_TAIL_RIGHT      (FAULT_CODE_LIGHTING_BASE + 0x11U)
#define FAULT_LIGHT_BRAKE_LEFT      (FAULT_CODE_LIGHTING_BASE + 0x12U)
#define FAULT_LIGHT_BRAKE_RIGHT     (FAULT_CODE_LIGHTING_BASE + 0x13U)
#define FAULT_LIGHT_INTERIOR        (FAULT_CODE_LIGHTING_BASE + 0x20U)

/* Turn signal faults */
#define FAULT_TURN_BULB_FL          (FAULT_CODE_TURN_SIGNAL_BASE + 0x01U)
#define FAULT_TURN_BULB_FR          (FAULT_CODE_TURN_SIGNAL_BASE + 0x02U)
#define FAULT_TURN_BULB_RL          (FAULT_CODE_TURN_SIGNAL_BASE + 0x03U)
#define FAULT_TURN_BULB_RR          (FAULT_CODE_TURN_SIGNAL_BASE + 0x04U)
#define FAULT_TURN_RELAY            (FAULT_CODE_TURN_SIGNAL_BASE + 0x10U)

/* CAN faults */
#define FAULT_CAN_BUS_OFF           (FAULT_CODE_CAN_BASE + 0x01U)
#define FAULT_CAN_TX_ERROR          (FAULT_CODE_CAN_BASE + 0x02U)
#define FAULT_CAN_RX_TIMEOUT        (FAULT_CODE_CAN_BASE + 0x03U)
#define FAULT_CAN_MSG_LOST          (FAULT_CODE_CAN_BASE + 0x04U)

/* System faults */
#define FAULT_SYSTEM_WATCHDOG       (FAULT_CODE_SYSTEM_BASE + 0x01U)
#define FAULT_SYSTEM_VOLTAGE_LOW    (FAULT_CODE_SYSTEM_BASE + 0x02U)
#define FAULT_SYSTEM_VOLTAGE_HIGH   (FAULT_CODE_SYSTEM_BASE + 0x03U)
#define FAULT_SYSTEM_TEMP_HIGH      (FAULT_CODE_SYSTEM_BASE + 0x04U)
#define FAULT_SYSTEM_MEMORY         (FAULT_CODE_SYSTEM_BASE + 0x10U)

/* =============================================================================
 * Fault Entry Structure
 * ========================================================================== */

/** Fault log entry */
typedef struct {
    fault_code_t code;          /**< Fault code */
    fault_status_t status;      /**< Current status */
    fault_severity_t severity;  /**< Severity level */
    uint32_t first_occurrence;  /**< Timestamp of first occurrence */
    uint32_t last_occurrence;   /**< Timestamp of last occurrence */
    uint16_t occurrence_count;  /**< Number of occurrences */
    uint8_t recovery_attempts;  /**< Number of recovery attempts */
    uint8_t freeze_frame[8];    /**< Freeze frame data */
} fault_entry_t;

/* =============================================================================
 * Fault Manager Initialization
 * ========================================================================== */

/**
 * @brief Initialize fault manager
 * @return BCM_OK on success
 */
bcm_result_t fault_manager_init(void);

/**
 * @brief Deinitialize fault manager
 */
void fault_manager_deinit(void);

/* =============================================================================
 * Fault Manager Processing
 * ========================================================================== */

/**
 * @brief Process fault manager
 *
 * Handles debouncing, healing, and recovery attempts.
 *
 * @param timestamp_ms Current timestamp in milliseconds
 * @return BCM_OK on success
 */
bcm_result_t fault_manager_process(uint32_t timestamp_ms);

/* =============================================================================
 * Fault Reporting
 * ========================================================================== */

/**
 * @brief Report a fault condition
 * @param code Fault code
 * @param severity Fault severity
 * @return BCM_OK on success
 */
bcm_result_t fault_report(fault_code_t code, fault_severity_t severity);

/**
 * @brief Report fault with freeze frame data
 * @param code Fault code
 * @param severity Fault severity
 * @param freeze_frame Freeze frame data (8 bytes)
 * @return BCM_OK on success
 */
bcm_result_t fault_report_with_data(fault_code_t code, 
                                     fault_severity_t severity,
                                     const uint8_t *freeze_frame);

/**
 * @brief Clear a specific fault
 * @param code Fault code to clear
 * @return BCM_OK on success
 */
bcm_result_t fault_clear(fault_code_t code);

/**
 * @brief Clear all faults
 * @return BCM_OK on success
 */
bcm_result_t fault_clear_all(void);

/**
 * @brief Mark fault as healed (condition no longer present)
 * @param code Fault code
 * @return BCM_OK on success
 */
bcm_result_t fault_heal(fault_code_t code);

/* =============================================================================
 * Fault Status Queries
 * ========================================================================== */

/**
 * @brief Check if a fault is active
 * @param code Fault code to check
 * @return true if fault is active
 */
bool fault_is_active(fault_code_t code);

/**
 * @brief Check if a fault is present (active or pending)
 * @param code Fault code to check
 * @return true if fault is present
 */
bool fault_is_present(fault_code_t code);

/**
 * @brief Get fault status
 * @param code Fault code to query
 * @return Current fault status
 */
fault_status_t fault_get_status(fault_code_t code);

/**
 * @brief Get fault entry details
 * @param code Fault code to query
 * @param[out] entry Fault entry structure to fill
 * @return BCM_OK if found, BCM_ERROR if not found
 */
bcm_result_t fault_get_entry(fault_code_t code, fault_entry_t *entry);

/**
 * @brief Get count of active faults
 * @return Number of active faults
 */
uint16_t fault_get_active_count(void);

/**
 * @brief Get count of stored faults
 * @return Number of stored faults
 */
uint16_t fault_get_stored_count(void);

/**
 * @brief Check if any critical fault is active
 * @return true if critical fault exists
 */
bool fault_any_critical(void);

/* =============================================================================
 * Fault Log Access
 * ========================================================================== */

/**
 * @brief Get fault at index in log
 * @param index Index in fault log (0 to fault_get_stored_count()-1)
 * @param[out] entry Fault entry to fill
 * @return BCM_OK if found
 */
bcm_result_t fault_get_by_index(uint16_t index, fault_entry_t *entry);

/**
 * @brief Get all active fault codes
 * @param[out] codes Array to fill with fault codes
 * @param max_codes Maximum number of codes to return
 * @return Number of active faults
 */
uint16_t fault_get_active_codes(fault_code_t *codes, uint16_t max_codes);

/**
 * @brief Get snapshot of fault manager state
 * @param[out] buffer Buffer to fill with snapshot data
 * @param buffer_size Size of buffer
 * @return Number of bytes written
 */
uint16_t fault_get_snapshot(uint8_t *buffer, uint16_t buffer_size);

/* =============================================================================
 * Fault Recovery
 * ========================================================================== */

/** Recovery action callback type */
typedef bcm_result_t (*fault_recovery_action_t)(fault_code_t code);

/**
 * @brief Register recovery action for a fault
 * @param code Fault code
 * @param action Recovery function to call
 * @return BCM_OK on success
 */
bcm_result_t fault_register_recovery(fault_code_t code, 
                                      fault_recovery_action_t action);

/**
 * @brief Attempt recovery for a fault
 * @param code Fault code to recover
 * @return BCM_OK if recovery succeeded
 */
bcm_result_t fault_attempt_recovery(fault_code_t code);

/* =============================================================================
 * Diagnostic Interface
 * ========================================================================== */

/**
 * @brief Read DTC by status mask (UDS service 0x19)
 * @param status_mask Status mask for filtering
 * @param[out] buffer Output buffer for DTCs
 * @param buffer_size Size of output buffer
 * @return Number of bytes written
 */
uint16_t fault_read_dtc_by_status(uint8_t status_mask, 
                                   uint8_t *buffer, 
                                   uint16_t buffer_size);

/**
 * @brief Clear diagnostic information (UDS service 0x14)
 * @param group_of_dtc DTC group to clear (0xFFFFFF for all)
 * @return BCM_OK on success
 */
bcm_result_t fault_clear_dtc(uint32_t group_of_dtc);

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

/**
 * @brief Get fault data for CAN transmission
 * @param[out] data Buffer for CAN data (minimum 8 bytes)
 * @param[out] len Actual data length
 */
void fault_manager_get_can_status(uint8_t *data, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_MANAGER_H */
