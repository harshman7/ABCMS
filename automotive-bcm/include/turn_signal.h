/**
 * @file turn_signal.h
 * @brief Turn Signal Control Module Interface
 *
 * This module manages turn signals and hazard lights including
 * proper flash timing, lane change assist, and bulb monitoring.
 */

#ifndef TURN_SIGNAL_H
#define TURN_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"

/* =============================================================================
 * Turn Signal Types
 * ========================================================================== */

/** Turn signal direction */
typedef enum {
    TURN_DIRECTION_NONE = 0,    /**< No turn signal active */
    TURN_DIRECTION_LEFT,        /**< Left turn signal */
    TURN_DIRECTION_RIGHT        /**< Right turn signal */
} turn_direction_t;

/** Turn signal mode */
typedef enum {
    TURN_MODE_OFF = 0,          /**< Turn signals off */
    TURN_MODE_NORMAL,           /**< Normal turn signal operation */
    TURN_MODE_LANE_CHANGE,      /**< Lane change assist (3 blinks) */
    TURN_MODE_HAZARD            /**< Hazard lights active */
} turn_mode_t;

/* =============================================================================
 * Turn Signal Initialization
 * ========================================================================== */

/**
 * @brief Initialize turn signal module
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_init(void);

/**
 * @brief Deinitialize turn signal module
 */
void turn_signal_deinit(void);

/* =============================================================================
 * Turn Signal Processing
 * ========================================================================== */

/**
 * @brief Process turn signal state machine
 *
 * Called periodically to manage flash timing and state transitions.
 * Must be called frequently enough to maintain proper flash rate.
 *
 * @param timestamp_ms Current timestamp in milliseconds
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_process(uint32_t timestamp_ms);

/* =============================================================================
 * Turn Signal Control
 * ========================================================================== */

/**
 * @brief Activate left turn signal
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_left_on(void);

/**
 * @brief Deactivate left turn signal
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_left_off(void);

/**
 * @brief Activate right turn signal
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_right_on(void);

/**
 * @brief Deactivate right turn signal
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_right_off(void);

/**
 * @brief Turn off all turn signals
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_off(void);

/**
 * @brief Set turn signal direction
 * @param direction Desired turn direction
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_set_direction(turn_direction_t direction);

/**
 * @brief Get current turn signal direction
 * @return Current turn direction
 */
turn_direction_t turn_signal_get_direction(void);

/* =============================================================================
 * Hazard Light Control
 * ========================================================================== */

/**
 * @brief Activate hazard lights
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_hazard_on(void);

/**
 * @brief Deactivate hazard lights
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_hazard_off(void);

/**
 * @brief Toggle hazard lights
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_hazard_toggle(void);

/**
 * @brief Check if hazard lights are active
 * @return true if hazard lights are on
 */
bool turn_signal_hazard_active(void);

/* =============================================================================
 * Lane Change Assist
 * ========================================================================== */

/**
 * @brief Trigger lane change assist (comfort blinker)
 *
 * Activates turn signal for a preset number of blinks (typically 3).
 *
 * @param direction Turn direction for lane change
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_lane_change(turn_direction_t direction);

/**
 * @brief Set lane change blink count
 * @param count Number of blinks (typically 3)
 */
void turn_signal_set_lane_change_count(uint8_t count);

/**
 * @brief Get lane change blink count
 * @return Current lane change blink count
 */
uint8_t turn_signal_get_lane_change_count(void);

/* =============================================================================
 * Turn Signal Status
 * ========================================================================== */

/**
 * @brief Get current turn signal mode
 * @return Current mode
 */
turn_mode_t turn_signal_get_mode(void);

/**
 * @brief Get complete turn signal status
 * @param[out] status Status structure to fill
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_get_status(turn_signal_status_t *status);

/**
 * @brief Check if turn signal is currently illuminated
 * @param direction Which direction to check
 * @return true if currently on (in flash cycle)
 */
bool turn_signal_is_illuminated(turn_direction_t direction);

/**
 * @brief Get number of blinks since activation
 * @return Blink count
 */
uint8_t turn_signal_get_blink_count(void);

/* =============================================================================
 * Bulb Monitoring
 * ========================================================================== */

/**
 * @brief Check left turn signal bulb status
 * @return true if bulb is OK
 */
bool turn_signal_left_bulb_ok(void);

/**
 * @brief Check right turn signal bulb status
 * @return true if bulb is OK
 */
bool turn_signal_right_bulb_ok(void);

/**
 * @brief Check if any bulb fault is present
 * @return true if any bulb has failed
 */
bool turn_signal_bulb_fault_present(void);

/**
 * @brief Update bulb current measurement for fault detection
 * @param left_ma Left bulb current in milliamps
 * @param right_ma Right bulb current in milliamps
 */
void turn_signal_update_bulb_current(uint16_t left_ma, uint16_t right_ma);

/* =============================================================================
 * Flash Rate Control
 * ========================================================================== */

/**
 * @brief Set normal flash rate
 * @param on_time_ms On time in milliseconds
 * @param off_time_ms Off time in milliseconds
 */
void turn_signal_set_flash_rate(uint16_t on_time_ms, uint16_t off_time_ms);

/**
 * @brief Enable fast flash for bulb failure indication
 * @param enable true to enable fast flash mode
 */
void turn_signal_set_fast_flash(bool enable);

/**
 * @brief Check if fast flash mode is active
 * @return true if fast flash mode is active
 */
bool turn_signal_fast_flash_active(void);

/* =============================================================================
 * Steering Wheel Integration
 * ========================================================================== */

/**
 * @brief Notify turn signal of steering angle
 *
 * Used for automatic turn signal cancellation after turns.
 *
 * @param angle_deg Steering angle in degrees (+ = right, - = left)
 */
void turn_signal_update_steering_angle(int16_t angle_deg);

/**
 * @brief Enable automatic turn signal cancellation
 * @param enable true to enable
 */
void turn_signal_auto_cancel_enable(bool enable);

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

/**
 * @brief Handle incoming turn signal command from CAN
 * @param data CAN message data
 * @param len Data length
 * @return BCM_OK on success
 */
bcm_result_t turn_signal_handle_can_cmd(const uint8_t *data, uint8_t len);

/**
 * @brief Get turn signal status data for CAN transmission
 * @param[out] data Buffer for CAN data (minimum 8 bytes)
 * @param[out] len Actual data length
 */
void turn_signal_get_can_status(uint8_t *data, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* TURN_SIGNAL_H */
