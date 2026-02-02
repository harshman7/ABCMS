/**
 * @file fault_manager.h
 * @brief Fault Manager Module Interface
 *
 * Records, manages, and reports active faults.
 * Exposes fault status CAN frame.
 */

#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"
#include "can_interface.h"

/*******************************************************************************
 * Fault Manager Initialization
 ******************************************************************************/

/**
 * @brief Initialize fault manager
 */
void fault_manager_init(void);

/*******************************************************************************
 * Fault Management
 ******************************************************************************/

/**
 * @brief Set a fault active
 * @param code Fault code to set
 */
void fault_manager_set(fault_code_t code);

/**
 * @brief Clear a fault
 * @param code Fault code to clear
 */
void fault_manager_clear(fault_code_t code);

/**
 * @brief Clear all faults
 */
void fault_manager_clear_all(void);

/**
 * @brief Check if a fault is active
 * @param code Fault code to check
 * @return true if fault is active
 */
bool fault_manager_is_active(fault_code_t code);

/**
 * @brief Get number of active faults
 */
uint8_t fault_manager_get_count(void);

/**
 * @brief Get fault flags byte 1 (for CAN status)
 */
uint8_t fault_manager_get_flags1(void);

/**
 * @brief Get fault flags byte 2 (for CAN status)
 */
uint8_t fault_manager_get_flags2(void);

/**
 * @brief Get most recent fault code
 */
fault_code_t fault_manager_get_most_recent(void);

/*******************************************************************************
 * Fault Status Frame
 ******************************************************************************/

/**
 * @brief Build fault status CAN frame for transmission
 * @param frame Output frame buffer
 */
void fault_manager_build_status_frame(can_frame_t *frame);

/*******************************************************************************
 * Periodic Processing
 ******************************************************************************/

/**
 * @brief Periodic update (called from 100ms or 1000ms task)
 * @param current_ms Current time
 */
void fault_manager_update(uint32_t current_ms);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_MANAGER_H */
