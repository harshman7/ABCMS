/**
 * @file bcm.h
 * @brief BCM Core Interface
 *
 * Main BCM initialization and processing functions.
 * Implements a message-driven architecture with periodic processing.
 */

#ifndef BCM_H
#define BCM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"
#include "can_interface.h"

/*******************************************************************************
 * BCM Initialization
 ******************************************************************************/

/**
 * @brief Initialize BCM and all modules
 * @param can_ifname CAN interface name (e.g., "vcan0")
 * @return 0 on success, -1 on error
 */
int bcm_init(const char *can_ifname);

/**
 * @brief Deinitialize BCM and all modules
 */
void bcm_deinit(void);

/*******************************************************************************
 * BCM Processing
 ******************************************************************************/

/**
 * @brief Main BCM processing loop iteration
 *
 * Polls CAN RX, routes messages, updates states, transmits status.
 * Should be called continuously in main loop.
 *
 * @param current_ms Current system time in milliseconds
 * @return 0 on success
 */
int bcm_process(uint32_t current_ms);

/**
 * @brief 10ms periodic task processing
 *
 * Called from bcm_process when 10ms has elapsed.
 * Handles fast state machine updates (turn signal flashing, etc.)
 *
 * @param current_ms Current system time
 */
void bcm_process_10ms(uint32_t current_ms);

/**
 * @brief 100ms periodic task processing
 *
 * Called from bcm_process when 100ms has elapsed.
 * Handles status frame transmission.
 *
 * @param current_ms Current system time
 */
void bcm_process_100ms(uint32_t current_ms);

/**
 * @brief 1000ms periodic task processing
 *
 * Called from bcm_process when 1000ms has elapsed.
 * Handles heartbeat, timeouts, uptime updates.
 *
 * @param current_ms Current system time
 */
void bcm_process_1000ms(uint32_t current_ms);

/*******************************************************************************
 * BCM State
 ******************************************************************************/

/**
 * @brief Get current BCM state
 */
bcm_state_t bcm_get_state(void);

/**
 * @brief Get BCM version string
 */
const char* bcm_get_version(void);

/**
 * @brief Get system uptime in milliseconds
 */
uint32_t bcm_get_uptime_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* BCM_H */
