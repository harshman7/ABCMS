/**
 * @file turn_signal.h
 * @brief Turn Signal Control Module Interface
 *
 * State machine for turn signal and hazard light control.
 * States: OFF, LEFT, RIGHT, HAZARD
 */

#ifndef TURN_SIGNAL_H
#define TURN_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"
#include "can_interface.h"

/*******************************************************************************
 * Configuration
 ******************************************************************************/

#define TURN_SIGNAL_FLASH_ON_MS     500U    /**< Flash on duration */
#define TURN_SIGNAL_FLASH_OFF_MS    500U    /**< Flash off duration */
#define HAZARD_FLASH_ON_MS          400U    /**< Hazard on duration */
#define HAZARD_FLASH_OFF_MS         400U    /**< Hazard off duration */

/*******************************************************************************
 * Turn Signal Initialization
 ******************************************************************************/

/**
 * @brief Initialize turn signal module
 */
void turn_signal_init(void);

/*******************************************************************************
 * Turn Signal Processing
 ******************************************************************************/

/**
 * @brief Handle incoming turn signal command CAN frame
 *
 * Validates: length, checksum, counter, command range.
 * On valid command, transitions state machine.
 *
 * @param frame Received CAN frame
 * @return cmd_result_t indicating success or error type
 */
cmd_result_t turn_signal_handle_cmd(const can_frame_t *frame);

/**
 * @brief Periodic update (called from 10ms task)
 *
 * Handles flash timing, timeout auto-off.
 *
 * @param current_ms Current time
 */
void turn_signal_update(uint32_t current_ms);

/**
 * @brief Check for timeout and auto-off
 *
 * If no command received for TURN_SIGNAL_TIMEOUT_MS, turns off.
 * Called from 1000ms task.
 *
 * @param current_ms Current time
 */
void turn_signal_check_timeout(uint32_t current_ms);

/*******************************************************************************
 * Turn Signal Status
 ******************************************************************************/

/**
 * @brief Build turn signal status CAN frame for transmission
 * @param frame Output frame buffer
 */
void turn_signal_build_status_frame(can_frame_t *frame);

/**
 * @brief Get current turn signal mode
 */
turn_signal_mode_t turn_signal_get_mode(void);

/**
 * @brief Get current flash state
 * @param left_on Output: true if left lamp on
 * @param right_on Output: true if right lamp on
 */
void turn_signal_get_output_state(bool *left_on, bool *right_on);

/**
 * @brief Get flash count
 */
uint8_t turn_signal_get_flash_count(void);

/*******************************************************************************
 * Turn Signal Commands (for testing)
 ******************************************************************************/

/**
 * @brief Turn off all signals
 */
void turn_signal_off(void);

/**
 * @brief Turn on left signal
 */
void turn_signal_left_on(void);

/**
 * @brief Turn on right signal
 */
void turn_signal_right_on(void);

/**
 * @brief Turn on hazard lights
 */
void turn_signal_hazard_on(void);

#ifdef __cplusplus
}
#endif

#endif /* TURN_SIGNAL_H */
