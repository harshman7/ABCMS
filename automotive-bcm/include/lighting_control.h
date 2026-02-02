/**
 * @file lighting_control.h
 * @brief Lighting Control Module Interface
 *
 * State machine for headlight and interior light control.
 * Headlight states: OFF, ON, AUTO
 * Interior states: OFF, ON, AUTO
 */

#ifndef LIGHTING_CONTROL_H
#define LIGHTING_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"
#include "can_interface.h"

/*******************************************************************************
 * Lighting Control Initialization
 ******************************************************************************/

/**
 * @brief Initialize lighting control module
 */
void lighting_control_init(void);

/*******************************************************************************
 * Lighting Control Processing
 ******************************************************************************/

/**
 * @brief Handle incoming lighting command CAN frame
 *
 * Validates: length, checksum, counter, command range.
 * On valid command, transitions state machine.
 *
 * @param frame Received CAN frame
 * @return cmd_result_t indicating success or error type
 */
cmd_result_t lighting_control_handle_cmd(const can_frame_t *frame);

/**
 * @brief Periodic update (called from 10ms task)
 *
 * Handles AUTO mode logic, timeouts.
 *
 * @param current_ms Current time
 */
void lighting_control_update(uint32_t current_ms);

/*******************************************************************************
 * Lighting Control Status
 ******************************************************************************/

/**
 * @brief Build lighting status CAN frame for transmission
 * @param frame Output frame buffer
 */
void lighting_control_build_status_frame(can_frame_t *frame);

/**
 * @brief Get current headlight mode
 */
lighting_mode_state_t lighting_control_get_headlight_mode(void);

/**
 * @brief Get current headlight output state
 */
headlight_state_t lighting_control_get_headlight_output(void);

/**
 * @brief Check if headlights are on (any mode)
 */
bool lighting_control_headlights_on(void);

/**
 * @brief Get interior light brightness (0-15)
 */
uint8_t lighting_control_get_interior_brightness(void);

/*******************************************************************************
 * Lighting Control Commands (for testing)
 ******************************************************************************/

/**
 * @brief Set headlight mode directly
 * @param mode OFF, ON, or AUTO
 */
void lighting_control_set_headlight_mode(lighting_mode_state_t mode);

/**
 * @brief Set high beam state
 * @param on true to turn on, false to turn off
 */
void lighting_control_set_high_beam(bool on);

/**
 * @brief Set interior light mode
 * @param mode OFF, ON, or AUTO
 * @param brightness Brightness level (0-15) when ON
 */
void lighting_control_set_interior(lighting_mode_state_t mode, uint8_t brightness);

/**
 * @brief Update ambient light sensor value (for AUTO mode)
 * @param level Ambient level (0-255)
 */
void lighting_control_set_ambient(uint8_t level);

#ifdef __cplusplus
}
#endif

#endif /* LIGHTING_CONTROL_H */
