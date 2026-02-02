/**
 * @file door_control.h
 * @brief Door Control Module Interface
 *
 * This module manages door locks, windows, and related features
 * including child safety locks and auto-lock functionality.
 */

#ifndef DOOR_CONTROL_H
#define DOOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"

/* =============================================================================
 * Door Control Initialization
 * ========================================================================== */

/**
 * @brief Initialize door control module
 * @return BCM_OK on success
 */
bcm_result_t door_control_init(void);

/**
 * @brief Deinitialize door control module
 */
void door_control_deinit(void);

/* =============================================================================
 * Door Control Processing
 * ========================================================================== */

/**
 * @brief Process door control state machine
 *
 * Called periodically to update door states, process commands,
 * and manage timing-related features.
 *
 * @param timestamp_ms Current timestamp in milliseconds
 * @return BCM_OK on success
 */
bcm_result_t door_control_process(uint32_t timestamp_ms);

/* =============================================================================
 * Lock Control
 * ========================================================================== */

/**
 * @brief Lock all doors
 * @return BCM_OK on success
 */
bcm_result_t door_lock_all(void);

/**
 * @brief Unlock all doors
 * @return BCM_OK on success
 */
bcm_result_t door_unlock_all(void);

/**
 * @brief Lock a specific door
 * @param door Door to lock
 * @return BCM_OK on success
 */
bcm_result_t door_lock(door_position_t door);

/**
 * @brief Unlock a specific door
 * @param door Door to unlock
 * @return BCM_OK on success
 */
bcm_result_t door_unlock(door_position_t door);

/**
 * @brief Get lock state of a door
 * @param door Door to query
 * @return Current lock state
 */
lock_state_t door_get_lock_state(door_position_t door);

/**
 * @brief Check if all doors are locked
 * @return true if all doors are locked
 */
bool door_all_locked(void);

/**
 * @brief Check if any door is unlocked
 * @return true if at least one door is unlocked
 */
bool door_any_unlocked(void);

/* =============================================================================
 * Window Control
 * ========================================================================== */

/**
 * @brief Open window (move down)
 * @param door Door whose window to control
 * @param one_touch If true, window moves fully down automatically
 * @return BCM_OK on success
 */
bcm_result_t door_window_open(door_position_t door, bool one_touch);

/**
 * @brief Close window (move up)
 * @param door Door whose window to control
 * @param one_touch If true, window moves fully up automatically
 * @return BCM_OK on success
 */
bcm_result_t door_window_close(door_position_t door, bool one_touch);

/**
 * @brief Stop window movement
 * @param door Door whose window to stop
 * @return BCM_OK on success
 */
bcm_result_t door_window_stop(door_position_t door);

/**
 * @brief Move window to specific position
 * @param door Door whose window to control
 * @param position Target position (0=fully open, 100=fully closed)
 * @return BCM_OK on success
 */
bcm_result_t door_window_set_position(door_position_t door, uint8_t position);

/**
 * @brief Get window position
 * @param door Door to query
 * @return Window position (0-100%) or 0xFF if unknown
 */
uint8_t door_window_get_position(door_position_t door);

/**
 * @brief Get window state
 * @param door Door to query
 * @return Current window state
 */
window_state_t door_window_get_state(door_position_t door);

/**
 * @brief Close all windows (comfort close)
 * @return BCM_OK on success
 */
bcm_result_t door_window_close_all(void);

/* =============================================================================
 * Door Status
 * ========================================================================== */

/**
 * @brief Check if a door is open
 * @param door Door to check
 * @return true if door is open
 */
bool door_is_open(door_position_t door);

/**
 * @brief Check if any door is open
 * @return true if at least one door is open
 */
bool door_any_open(void);

/**
 * @brief Get complete door status
 * @param door Door to query
 * @param[out] status Status structure to fill
 * @return BCM_OK on success
 */
bcm_result_t door_get_status(door_position_t door, door_status_t *status);

/* =============================================================================
 * Child Safety Lock
 * ========================================================================== */

/**
 * @brief Enable child safety lock
 * @param door Door to enable child lock (typically rear doors)
 * @return BCM_OK on success
 */
bcm_result_t door_child_lock_enable(door_position_t door);

/**
 * @brief Disable child safety lock
 * @param door Door to disable child lock
 * @return BCM_OK on success
 */
bcm_result_t door_child_lock_disable(door_position_t door);

/**
 * @brief Check if child lock is active
 * @param door Door to check
 * @return true if child lock is active
 */
bool door_child_lock_active(door_position_t door);

/* =============================================================================
 * Auto-Lock Feature
 * ========================================================================== */

/**
 * @brief Enable auto-lock feature (locks doors when driving)
 * @param enable true to enable, false to disable
 */
void door_auto_lock_enable(bool enable);

/**
 * @brief Check if auto-lock is enabled
 * @return true if enabled
 */
bool door_auto_lock_enabled(void);

/**
 * @brief Update vehicle speed for auto-lock feature
 * @param speed_kmh Current vehicle speed in km/h
 */
void door_update_vehicle_speed(uint16_t speed_kmh);

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

/**
 * @brief Handle incoming door command from CAN
 * @param data CAN message data
 * @param len Data length
 * @return BCM_OK on success
 */
bcm_result_t door_control_handle_can_cmd(const uint8_t *data, uint8_t len);

/**
 * @brief Get door status data for CAN transmission
 * @param[out] data Buffer for CAN data (minimum 8 bytes)
 * @param[out] len Actual data length
 */
void door_control_get_can_status(uint8_t *data, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* DOOR_CONTROL_H */
