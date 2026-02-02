/**
 * @file turn_signal.c
 * @brief Turn Signal Control Module Implementation
 *
 * State machine: OFF <-> LEFT / RIGHT / HAZARD
 * Flash timing managed in 10ms update task.
 */

#include <string.h>
#include <stdio.h>
#include "turn_signal.h"
#include "fault_manager.h"
#include "can_ids.h"

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

/**
 * @brief Validate turn signal command frame
 */
static cmd_result_t validate_turn_cmd(const can_frame_t *frame)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Check DLC */
    if (frame->dlc != TURN_SIGNAL_CMD_DLC) {
        fault_manager_set(FAULT_CODE_INVALID_LENGTH);
        return CMD_RESULT_INVALID_CMD;
    }
    
    /* Extract fields */
    uint8_t ver_ctr = frame->data[TURN_CMD_BYTE_VER_CTR];
    uint8_t checksum = frame->data[TURN_CMD_BYTE_CHECKSUM];
    
    /* Validate checksum */
    uint8_t calc_checksum = can_calculate_checksum(frame->data, TURN_SIGNAL_CMD_DLC - 1);
    if (checksum != calc_checksum) {
        fault_manager_set(FAULT_CODE_INVALID_CHECKSUM);
        return CMD_RESULT_CHECKSUM_ERROR;
    }
    
    /* Validate counter */
    uint8_t counter = CAN_GET_COUNTER(ver_ctr);
    if (state->turn_signal.last_cmd_time_ms > 0) {
        if (!can_validate_counter(counter, state->turn_signal.last_counter)) {
            fault_manager_set(FAULT_CODE_INVALID_COUNTER);
            return CMD_RESULT_COUNTER_ERROR;
        }
    }
    state->turn_signal.last_counter = counter;
    
    /* Validate command value */
    uint8_t cmd = frame->data[TURN_CMD_BYTE_CMD];
    if (cmd > TURN_CMD_MAX) {
        fault_manager_set(FAULT_CODE_INVALID_CMD);
        return CMD_RESULT_INVALID_CMD;
    }
    
    return CMD_RESULT_OK;
}

/**
 * @brief Log turn signal state change event
 */
static void log_turn_event(turn_signal_mode_t old_mode, turn_signal_mode_t new_mode)
{
    uint8_t data[4] = { (uint8_t)old_mode, (uint8_t)new_mode, 0, 0 };
    event_log_add(EVENT_TURN_SIGNAL_CHANGE, data);
}

/**
 * @brief Get flash timing for current mode
 */
static void get_flash_timing(turn_signal_mode_t mode, uint16_t *on_ms, uint16_t *off_ms)
{
    if (mode == TURN_SIG_STATE_HAZARD) {
        *on_ms = HAZARD_FLASH_ON_MS;
        *off_ms = HAZARD_FLASH_OFF_MS;
    } else {
        *on_ms = TURN_SIGNAL_FLASH_ON_MS;
        *off_ms = TURN_SIGNAL_FLASH_OFF_MS;
    }
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

void turn_signal_init(void)
{
    system_state_t *state = sys_state_get_mut();
    
    state->turn_signal.mode = TURN_SIG_STATE_OFF;
    state->turn_signal.left_output = false;
    state->turn_signal.right_output = false;
    state->turn_signal.flash_count = 0;
    state->turn_signal.last_toggle_ms = 0;
    state->turn_signal.last_cmd_time_ms = 0;
    state->turn_signal.last_counter = 0;
    state->turn_signal.last_result = CMD_RESULT_OK;
    
    printf("[TURN] Initialized\n");
}

cmd_result_t turn_signal_handle_cmd(const can_frame_t *frame)
{
    if (frame == NULL || frame->id != CAN_ID_TURN_SIGNAL_CMD) {
        return CMD_RESULT_INVALID_CMD;
    }
    
    system_state_t *state = sys_state_get_mut();
    
    /* Validate frame */
    cmd_result_t result = validate_turn_cmd(frame);
    if (result != CMD_RESULT_OK) {
        state->turn_signal.last_result = result;
        
        uint8_t data[4] = { (uint8_t)result, frame->data[0], 0, 0 };
        event_log_add(EVENT_CMD_ERROR, data);
        
        printf("[TURN] Command error: %d\n", result);
        return result;
    }
    
    /* Process command */
    uint8_t cmd = frame->data[TURN_CMD_BYTE_CMD];
    turn_signal_mode_t old_mode = state->turn_signal.mode;
    
    switch (cmd) {
        case TURN_CMD_OFF:
            turn_signal_off();
            break;
            
        case TURN_CMD_LEFT_ON:
            turn_signal_left_on();
            break;
            
        case TURN_CMD_RIGHT_ON:
            turn_signal_right_on();
            break;
            
        case TURN_CMD_HAZARD_ON:
            turn_signal_hazard_on();
            break;
            
        case TURN_CMD_HAZARD_OFF:
            if (state->turn_signal.mode == TURN_SIG_STATE_HAZARD) {
                turn_signal_off();
            }
            break;
    }
    
    if (old_mode != state->turn_signal.mode) {
        log_turn_event(old_mode, state->turn_signal.mode);
    }
    
    state->turn_signal.last_cmd_time_ms = state->uptime_ms;
    state->turn_signal.last_result = CMD_RESULT_OK;
    
    uint8_t data[4] = { cmd, 0, 0, 0 };
    event_log_add(EVENT_CMD_RECEIVED, data);
    
    return CMD_RESULT_OK;
}

void turn_signal_update(uint32_t current_ms)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Nothing to do if off */
    if (state->turn_signal.mode == TURN_SIG_STATE_OFF) {
        state->turn_signal.left_output = false;
        state->turn_signal.right_output = false;
        return;
    }
    
    /* Get flash timing */
    uint16_t on_ms, off_ms;
    get_flash_timing(state->turn_signal.mode, &on_ms, &off_ms);
    
    /* Determine if currently in ON or OFF phase */
    bool currently_on = state->turn_signal.left_output || state->turn_signal.right_output;
    uint16_t phase_duration = currently_on ? on_ms : off_ms;
    
    /* Check if time to toggle */
    uint32_t elapsed = current_ms - state->turn_signal.last_toggle_ms;
    if (elapsed >= phase_duration) {
        /* Toggle */
        currently_on = !currently_on;
        state->turn_signal.last_toggle_ms = current_ms;
        
        /* Update outputs based on mode */
        switch (state->turn_signal.mode) {
            case TURN_SIG_STATE_LEFT:
                state->turn_signal.left_output = currently_on;
                state->turn_signal.right_output = false;
                break;
                
            case TURN_SIG_STATE_RIGHT:
                state->turn_signal.left_output = false;
                state->turn_signal.right_output = currently_on;
                break;
                
            case TURN_SIG_STATE_HAZARD:
                state->turn_signal.left_output = currently_on;
                state->turn_signal.right_output = currently_on;
                break;
                
            default:
                state->turn_signal.left_output = false;
                state->turn_signal.right_output = false;
                break;
        }
        
        /* Increment flash count on rising edge */
        if (currently_on) {
            state->turn_signal.flash_count++;
        }
    }
}

void turn_signal_check_timeout(uint32_t current_ms)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Only timeout for non-hazard signals */
    if (state->turn_signal.mode == TURN_SIG_STATE_LEFT ||
        state->turn_signal.mode == TURN_SIG_STATE_RIGHT) {
        
        if (state->turn_signal.last_cmd_time_ms > 0) {
            uint32_t elapsed = current_ms - state->turn_signal.last_cmd_time_ms;
            
            if (elapsed > TURN_SIGNAL_TIMEOUT_MS) {
                printf("[TURN] Timeout - auto off\n");
                turn_signal_off();
                fault_manager_set(FAULT_CODE_TIMEOUT);
            }
        }
    }
}

void turn_signal_build_status_frame(can_frame_t *frame)
{
    const system_state_t *state = sys_state_get();
    system_state_t *mut_state = sys_state_get_mut();
    
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = CAN_ID_TURN_SIGNAL_STATUS;
    frame->dlc = TURN_SIGNAL_STATUS_DLC;
    
    /* Byte 0: Turn signal state */
    frame->data[TURN_STATUS_BYTE_STATE] = (uint8_t)state->turn_signal.mode;
    
    /* Byte 1: Output state */
    uint8_t output = 0;
    if (state->turn_signal.left_output) output |= TURN_OUTPUT_LEFT_BIT;
    if (state->turn_signal.right_output) output |= TURN_OUTPUT_RIGHT_BIT;
    frame->data[TURN_STATUS_BYTE_OUTPUT] = output;
    
    /* Byte 2: Flash count */
    frame->data[TURN_STATUS_BYTE_FLASH_CNT] = state->turn_signal.flash_count;
    
    /* Byte 3: Last command result */
    frame->data[TURN_STATUS_BYTE_RESULT] = (uint8_t)state->turn_signal.last_result;
    
    /* Byte 4: Version and counter */
    frame->data[TURN_STATUS_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(
        CAN_SCHEMA_VERSION, mut_state->tx_counter_turn);
    mut_state->tx_counter_turn = (mut_state->tx_counter_turn + 1) & CAN_COUNTER_MASK;
    
    /* Byte 5: Checksum */
    frame->data[TURN_STATUS_BYTE_CHECKSUM] = can_calculate_checksum(
        frame->data, TURN_SIGNAL_STATUS_DLC - 1);
}

turn_signal_mode_t turn_signal_get_mode(void)
{
    return sys_state_get()->turn_signal.mode;
}

void turn_signal_get_output_state(bool *left_on, bool *right_on)
{
    const system_state_t *state = sys_state_get();
    
    if (left_on != NULL) {
        *left_on = state->turn_signal.left_output;
    }
    if (right_on != NULL) {
        *right_on = state->turn_signal.right_output;
    }
}

uint8_t turn_signal_get_flash_count(void)
{
    return sys_state_get()->turn_signal.flash_count;
}

void turn_signal_off(void)
{
    system_state_t *state = sys_state_get_mut();
    
    if (state->turn_signal.mode != TURN_SIG_STATE_OFF) {
        printf("[TURN] OFF\n");
    }
    
    state->turn_signal.mode = TURN_SIG_STATE_OFF;
    state->turn_signal.left_output = false;
    state->turn_signal.right_output = false;
    state->turn_signal.flash_count = 0;
}

void turn_signal_left_on(void)
{
    system_state_t *state = sys_state_get_mut();
    
    state->turn_signal.mode = TURN_SIG_STATE_LEFT;
    state->turn_signal.left_output = true;
    state->turn_signal.right_output = false;
    state->turn_signal.flash_count = 0;
    state->turn_signal.last_toggle_ms = state->uptime_ms;
    
    printf("[TURN] LEFT ON\n");
}

void turn_signal_right_on(void)
{
    system_state_t *state = sys_state_get_mut();
    
    state->turn_signal.mode = TURN_SIG_STATE_RIGHT;
    state->turn_signal.left_output = false;
    state->turn_signal.right_output = true;
    state->turn_signal.flash_count = 0;
    state->turn_signal.last_toggle_ms = state->uptime_ms;
    
    printf("[TURN] RIGHT ON\n");
}

void turn_signal_hazard_on(void)
{
    system_state_t *state = sys_state_get_mut();
    
    state->turn_signal.mode = TURN_SIG_STATE_HAZARD;
    state->turn_signal.left_output = true;
    state->turn_signal.right_output = true;
    state->turn_signal.flash_count = 0;
    state->turn_signal.last_toggle_ms = state->uptime_ms;
    
    printf("[TURN] HAZARD ON\n");
}
