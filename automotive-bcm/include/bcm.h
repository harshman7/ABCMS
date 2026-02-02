/**
 * @file bcm.h
 * @brief Main Body Control Module Interface
 *
 * This file contains the main BCM initialization, processing, and
 * coordination functions.
 */

#ifndef BCM_H
#define BCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"

/* =============================================================================
 * BCM Version Information
 * ========================================================================== */

/**
 * @brief Get BCM software version string
 * @return Version string in format "X.Y.Z"
 */
const char* bcm_get_version(void);

/**
 * @brief Get BCM software version as numbers
 * @param[out] major Major version number
 * @param[out] minor Minor version number
 * @param[out] patch Patch version number
 */
void bcm_get_version_numbers(uint8_t *major, uint8_t *minor, uint8_t *patch);

/* =============================================================================
 * BCM Initialization
 * ========================================================================== */

/**
 * @brief Initialize the BCM and all subsystems
 *
 * This function initializes:
 * - System state
 * - CAN interface
 * - Door control module
 * - Lighting control module
 * - Turn signal module
 * - Fault manager
 *
 * @return BCM_OK on success, error code otherwise
 */
bcm_result_t bcm_init(void);

/**
 * @brief Deinitialize the BCM
 *
 * Safely shuts down all subsystems and releases resources.
 */
void bcm_deinit(void);

/* =============================================================================
 * BCM Processing
 * ========================================================================== */

/**
 * @brief Main BCM processing function
 *
 * This function should be called periodically from the main loop.
 * It processes:
 * - CAN message reception
 * - State machine updates
 * - Output control
 * - CAN message transmission
 * - Fault checking
 *
 * @param timestamp_ms Current system timestamp in milliseconds
 * @return BCM_OK on success, error code otherwise
 */
bcm_result_t bcm_process(uint32_t timestamp_ms);

/**
 * @brief Process a single cycle tick
 *
 * Lightweight version of bcm_process for time-critical paths.
 */
void bcm_tick(void);

/* =============================================================================
 * BCM State Control
 * ========================================================================== */

/**
 * @brief Get current BCM operational state
 * @return Current BCM state
 */
bcm_state_t bcm_get_state(void);

/**
 * @brief Request BCM state transition
 * @param new_state Requested new state
 * @return BCM_OK if transition accepted, error code otherwise
 */
bcm_result_t bcm_request_state(bcm_state_t new_state);

/**
 * @brief Enter sleep mode
 *
 * Prepares BCM for low-power sleep mode.
 * @return BCM_OK on success
 */
bcm_result_t bcm_enter_sleep(void);

/**
 * @brief Wake up from sleep mode
 *
 * Restores BCM to normal operation from sleep.
 * @return BCM_OK on success
 */
bcm_result_t bcm_wakeup(void);

/* =============================================================================
 * BCM Status and Diagnostics
 * ========================================================================== */

/**
 * @brief Check if BCM is ready for normal operation
 * @return true if BCM is ready
 */
bool bcm_is_ready(void);

/**
 * @brief Get BCM uptime
 * @return Uptime in milliseconds
 */
uint32_t bcm_get_uptime(void);

/**
 * @brief Perform BCM self-test
 * @return BCM_OK if all tests pass
 */
bcm_result_t bcm_self_test(void);

/**
 * @brief Get pointer to complete system state
 * @return Const pointer to system state
 */
const system_state_t* bcm_get_system_state(void);

/* =============================================================================
 * BCM Event Callbacks (Optional)
 * ========================================================================== */

/** Callback function type for state change notifications */
typedef void (*bcm_state_callback_t)(bcm_state_t old_state, bcm_state_t new_state);

/**
 * @brief Register callback for state changes
 * @param callback Function to call on state change (NULL to unregister)
 */
void bcm_register_state_callback(bcm_state_callback_t callback);

/* =============================================================================
 * BCM Diagnostic Mode
 * ========================================================================== */

/**
 * @brief Enter diagnostic mode
 * @return BCM_OK on success
 */
bcm_result_t bcm_enter_diagnostic_mode(void);

/**
 * @brief Exit diagnostic mode
 * @return BCM_OK on success
 */
bcm_result_t bcm_exit_diagnostic_mode(void);

/**
 * @brief Check if in diagnostic mode
 * @return true if in diagnostic mode
 */
bool bcm_is_diagnostic_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* BCM_H */
