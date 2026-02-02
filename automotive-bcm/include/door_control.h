/**
 * @file door_control.h
 * @brief Door Control Module Interface
 *
 * State machine for door lock control.
 * States: UNLOCKED, LOCKED, LOCKING, UNLOCKING
 */

#ifndef DOOR_CONTROL_H
#define DOOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"
#include "can_interface.h"

/*******************************************************************************
 * Door Control Initialization
 ******************************************************************************/

/**
 * @brief Initialize door control module
 */
void door_control_init(void);

/*******************************************************************************
 * Door Control Processing
 ******************************************************************************/

/**
 * @brief Handle incoming door command CAN frame
 *
 * Validates: length, checksum, counter, command range, door ID.
 * On valid command, transitions state machine.
 *
 * @param frame Received CAN frame
 * @return cmd_result_t indicating success or error type
 */
cmd_result_t door_control_handle_cmd(const can_frame_t *frame);

/**
 * @brief Periodic update (called from 10ms task)
 *
 * Handles state transitions (LOCKING->LOCKED, etc.)
 *
 * @param current_ms Current time
 */
void door_control_update(uint32_t current_ms);

/*******************************************************************************
 * Door Control Status
 ******************************************************************************/

/**
 * @brief Build door status CAN frame for transmission
 * @param frame Output frame buffer
 */
void door_control_build_status_frame(can_frame_t *frame);

/**
 * @brief Get lock state for a specific door
 * @param door_id Door ID (0-3)
 * @return Current lock state
 */
door_lock_state_t door_control_get_lock_state(uint8_t door_id);

/**
 * @brief Check if all doors are locked
 * @return true if all locked
 */
bool door_control_all_locked(void);

/**
 * @brief Check if any door is open
 * @return true if any door open
 */
bool door_control_any_open(void);

/*******************************************************************************
 * Door Control Commands (for testing)
 ******************************************************************************/

/**
 * @brief Lock all doors (direct command)
 */
void door_control_lock_all(void);

/**
 * @brief Unlock all doors (direct command)
 */
void door_control_unlock_all(void);

/**
 * @brief Lock single door (direct command)
 * @param door_id Door ID (0-3)
 */
void door_control_lock(uint8_t door_id);

/**
 * @brief Unlock single door (direct command)
 * @param door_id Door ID (0-3)
 */
void door_control_unlock(uint8_t door_id);

#ifdef __cplusplus
}
#endif

#endif /* DOOR_CONTROL_H */
