/**
 * @file lighting_control.c
 * @brief Lighting Control Module Implementation
 *
 * Headlight state machine: OFF <-> ON <-> AUTO
 * AUTO mode turns on when ambient < threshold
 */

#include <string.h>
#include <stdio.h>
#include "lighting_control.h"
#include "fault_manager.h"
#include "can_ids.h"

/*******************************************************************************
 * Private Definitions
 ******************************************************************************/

#define AUTO_ON_THRESHOLD       80U     /**< Turn on below this (0-255 scale) */
#define AUTO_OFF_THRESHOLD      120U    /**< Turn off above this */
#define AUTO_UPDATE_TIMEOUT_MS  10000U  /**< Fault if no ambient update */

/*******************************************************************************
 * Private Data
 ******************************************************************************/

static uint32_t g_last_ambient_update_ms = 0;

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

/**
 * @brief Validate lighting command frame
 */
static cmd_result_t validate_lighting_cmd(const can_frame_t *frame)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Check DLC */
    if (frame->dlc != LIGHTING_CMD_DLC) {
        fault_manager_set(FAULT_CODE_INVALID_LENGTH);
        return CMD_RESULT_INVALID_CMD;
    }
    
    /* Extract fields */
    uint8_t ver_ctr = frame->data[LIGHTING_CMD_BYTE_VER_CTR];
    uint8_t checksum = frame->data[LIGHTING_CMD_BYTE_CHECKSUM];
    
    /* Validate checksum */
    uint8_t calc_checksum = can_calculate_checksum(frame->data, LIGHTING_CMD_DLC - 1);
    if (checksum != calc_checksum) {
        fault_manager_set(FAULT_CODE_INVALID_CHECKSUM);
        return CMD_RESULT_CHECKSUM_ERROR;
    }
    
    /* Validate counter */
    uint8_t counter = CAN_GET_COUNTER(ver_ctr);
    if (state->lighting.last_cmd_time_ms > 0) {
        if (!can_validate_counter(counter, state->lighting.last_counter)) {
            fault_manager_set(FAULT_CODE_INVALID_COUNTER);
            return CMD_RESULT_COUNTER_ERROR;
        }
    }
    state->lighting.last_counter = counter;
    
    /* Validate headlight command */
    uint8_t headlight_cmd = frame->data[LIGHTING_CMD_BYTE_HEADLIGHT];
    if (headlight_cmd > HEADLIGHT_CMD_MAX) {
        fault_manager_set(FAULT_CODE_INVALID_CMD);
        return CMD_RESULT_INVALID_CMD;
    }
    
    /* Validate interior command */
    uint8_t interior_cmd = frame->data[LIGHTING_CMD_BYTE_INTERIOR] & INTERIOR_MODE_MASK;
    if (interior_cmd > INTERIOR_CMD_MAX) {
        fault_manager_set(FAULT_CODE_INVALID_CMD);
        return CMD_RESULT_INVALID_CMD;
    }
    
    return CMD_RESULT_OK;
}

/**
 * @brief Log lighting state change event
 */
static void log_lighting_event(uint8_t type, uint8_t old_state, uint8_t new_state)
{
    uint8_t data[4] = { type, old_state, new_state, 0 };
    event_log_add(EVENT_HEADLIGHT_CHANGE, data);
}

/**
 * @brief Update headlight output based on mode and ambient
 */
static void update_headlight_output(void)
{
    system_state_t *state = sys_state_get_mut();
    headlight_state_t old_output = state->lighting.headlight_output;
    
    switch (state->lighting.headlight_mode) {
        case LIGHTING_STATE_OFF:
            state->lighting.headlight_output = HEADLIGHT_STATE_OFF;
            state->lighting.high_beam_active = false;
            break;
            
        case LIGHTING_STATE_ON:
            if (state->lighting.high_beam_active) {
                state->lighting.headlight_output = HEADLIGHT_STATE_HIGH_BEAM;
            } else {
                state->lighting.headlight_output = HEADLIGHT_STATE_ON;
            }
            break;
            
        case LIGHTING_STATE_AUTO:
            /* Hysteresis for auto mode */
            if (state->lighting.headlight_output == HEADLIGHT_STATE_OFF ||
                state->lighting.headlight_output == HEADLIGHT_STATE_AUTO) {
                if (state->lighting.ambient_light < AUTO_ON_THRESHOLD) {
                    state->lighting.headlight_output = HEADLIGHT_STATE_AUTO;
                } else if (state->lighting.ambient_light > AUTO_OFF_THRESHOLD) {
                    state->lighting.headlight_output = HEADLIGHT_STATE_OFF;
                }
            } else {
                state->lighting.headlight_output = HEADLIGHT_STATE_AUTO;
            }
            
            /* High beam in auto mode */
            if (state->lighting.high_beam_active && 
                state->lighting.headlight_output != HEADLIGHT_STATE_OFF) {
                state->lighting.headlight_output = HEADLIGHT_STATE_HIGH_BEAM;
            }
            break;
    }
    
    if (old_output != state->lighting.headlight_output) {
        printf("[LIGHT] Headlight output: %d -> %d\n", 
               old_output, state->lighting.headlight_output);
    }
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

void lighting_control_init(void)
{
    system_state_t *state = sys_state_get_mut();
    
    state->lighting.headlight_mode = LIGHTING_STATE_OFF;
    state->lighting.headlight_output = HEADLIGHT_STATE_OFF;
    state->lighting.high_beam_active = false;
    state->lighting.interior_mode = LIGHTING_STATE_OFF;
    state->lighting.interior_brightness = 0;
    state->lighting.interior_on = false;
    state->lighting.ambient_light = 128;
    state->lighting.last_cmd_time_ms = 0;
    state->lighting.last_counter = 0;
    state->lighting.last_result = CMD_RESULT_OK;
    
    g_last_ambient_update_ms = 0;
    
    printf("[LIGHT] Initialized\n");
}

cmd_result_t lighting_control_handle_cmd(const can_frame_t *frame)
{
    if (frame == NULL || frame->id != CAN_ID_LIGHTING_CMD) {
        return CMD_RESULT_INVALID_CMD;
    }
    
    system_state_t *state = sys_state_get_mut();
    
    /* Validate frame */
    cmd_result_t result = validate_lighting_cmd(frame);
    if (result != CMD_RESULT_OK) {
        state->lighting.last_result = result;
        
        uint8_t data[4] = { (uint8_t)result, frame->data[0], 0, 0 };
        event_log_add(EVENT_CMD_ERROR, data);
        
        printf("[LIGHT] Command error: %d\n", result);
        return result;
    }
    
    /* Process headlight command */
    uint8_t headlight_cmd = frame->data[LIGHTING_CMD_BYTE_HEADLIGHT];
    lighting_mode_state_t old_mode = state->lighting.headlight_mode;
    
    switch (headlight_cmd) {
        case HEADLIGHT_CMD_OFF:
            state->lighting.headlight_mode = LIGHTING_STATE_OFF;
            state->lighting.high_beam_active = false;
            break;
            
        case HEADLIGHT_CMD_ON:
            state->lighting.headlight_mode = LIGHTING_STATE_ON;
            break;
            
        case HEADLIGHT_CMD_AUTO:
            state->lighting.headlight_mode = LIGHTING_STATE_AUTO;
            break;
            
        case HEADLIGHT_CMD_HIGH_ON:
            state->lighting.high_beam_active = true;
            break;
            
        case HEADLIGHT_CMD_HIGH_OFF:
            state->lighting.high_beam_active = false;
            break;
    }
    
    if (old_mode != state->lighting.headlight_mode) {
        log_lighting_event(0, (uint8_t)old_mode, (uint8_t)state->lighting.headlight_mode);
        printf("[LIGHT] Headlight mode: %d -> %d\n", old_mode, state->lighting.headlight_mode);
    }
    
    /* Process interior command */
    uint8_t interior_byte = frame->data[LIGHTING_CMD_BYTE_INTERIOR];
    uint8_t interior_cmd = interior_byte & INTERIOR_MODE_MASK;
    uint8_t brightness = (interior_byte >> 4) & 0x0FU;
    
    lighting_mode_state_t old_interior = state->lighting.interior_mode;
    
    switch (interior_cmd) {
        case INTERIOR_CMD_OFF:
            state->lighting.interior_mode = LIGHTING_STATE_OFF;
            state->lighting.interior_on = false;
            state->lighting.interior_brightness = 0;
            break;
            
        case INTERIOR_CMD_ON:
            state->lighting.interior_mode = LIGHTING_STATE_ON;
            state->lighting.interior_on = true;
            state->lighting.interior_brightness = brightness;
            break;
            
        case INTERIOR_CMD_AUTO:
            state->lighting.interior_mode = LIGHTING_STATE_AUTO;
            break;
    }
    
    if (old_interior != state->lighting.interior_mode) {
        log_lighting_event(1, (uint8_t)old_interior, (uint8_t)state->lighting.interior_mode);
        printf("[LIGHT] Interior mode: %d (brightness %d)\n", 
               state->lighting.interior_mode, state->lighting.interior_brightness);
    }
    
    /* Update output */
    update_headlight_output();
    
    state->lighting.last_cmd_time_ms = state->uptime_ms;
    state->lighting.last_result = CMD_RESULT_OK;
    
    uint8_t data[4] = { headlight_cmd, interior_cmd, brightness, 0 };
    event_log_add(EVENT_CMD_RECEIVED, data);
    
    return CMD_RESULT_OK;
}

void lighting_control_update(uint32_t current_ms)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Update output based on current mode */
    update_headlight_output();
    
    /* Check for ambient sensor timeout in AUTO mode */
    if (state->lighting.headlight_mode == LIGHTING_STATE_AUTO) {
        if (g_last_ambient_update_ms > 0 &&
            (current_ms - g_last_ambient_update_ms) > AUTO_UPDATE_TIMEOUT_MS) {
            fault_manager_set(FAULT_CODE_TIMEOUT);
            printf("[LIGHT] AUTO mode ambient sensor timeout\n");
        }
    }
}

void lighting_control_build_status_frame(can_frame_t *frame)
{
    const system_state_t *state = sys_state_get();
    system_state_t *mut_state = sys_state_get_mut();
    
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = CAN_ID_LIGHTING_STATUS;
    frame->dlc = LIGHTING_STATUS_DLC;
    
    /* Byte 0: Headlight state */
    frame->data[LIGHTING_STATUS_BYTE_HEADLIGHT] = (uint8_t)state->lighting.headlight_output;
    
    /* Byte 1: Interior state */
    uint8_t interior = (uint8_t)state->lighting.interior_mode;
    interior |= (state->lighting.interior_brightness << INTERIOR_STATE_BRIGHTNESS_SHIFT) & 
                INTERIOR_STATE_BRIGHTNESS_MASK;
    frame->data[LIGHTING_STATUS_BYTE_INTERIOR] = interior;
    
    /* Byte 2: Ambient light */
    frame->data[LIGHTING_STATUS_BYTE_AMBIENT] = state->lighting.ambient_light;
    
    /* Byte 3: Last command result */
    frame->data[LIGHTING_STATUS_BYTE_RESULT] = (uint8_t)state->lighting.last_result;
    
    /* Byte 4: Version and counter */
    frame->data[LIGHTING_STATUS_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(
        CAN_SCHEMA_VERSION, mut_state->tx_counter_lighting);
    mut_state->tx_counter_lighting = (mut_state->tx_counter_lighting + 1) & CAN_COUNTER_MASK;
    
    /* Byte 5: Checksum */
    frame->data[LIGHTING_STATUS_BYTE_CHECKSUM] = can_calculate_checksum(
        frame->data, LIGHTING_STATUS_DLC - 1);
}

lighting_mode_state_t lighting_control_get_headlight_mode(void)
{
    return sys_state_get()->lighting.headlight_mode;
}

headlight_state_t lighting_control_get_headlight_output(void)
{
    return sys_state_get()->lighting.headlight_output;
}

bool lighting_control_headlights_on(void)
{
    headlight_state_t output = sys_state_get()->lighting.headlight_output;
    return (output != HEADLIGHT_STATE_OFF);
}

uint8_t lighting_control_get_interior_brightness(void)
{
    return sys_state_get()->lighting.interior_brightness;
}

void lighting_control_set_headlight_mode(lighting_mode_state_t mode)
{
    system_state_t *state = sys_state_get_mut();
    state->lighting.headlight_mode = mode;
    update_headlight_output();
    printf("[LIGHT] Headlight mode set to: %d\n", mode);
}

void lighting_control_set_high_beam(bool on)
{
    system_state_t *state = sys_state_get_mut();
    state->lighting.high_beam_active = on;
    update_headlight_output();
    printf("[LIGHT] High beam: %s\n", on ? "ON" : "OFF");
}

void lighting_control_set_interior(lighting_mode_state_t mode, uint8_t brightness)
{
    system_state_t *state = sys_state_get_mut();
    state->lighting.interior_mode = mode;
    state->lighting.interior_brightness = brightness & 0x0FU;
    state->lighting.interior_on = (mode == LIGHTING_STATE_ON);
    printf("[LIGHT] Interior: mode=%d, brightness=%d\n", mode, brightness);
}

void lighting_control_set_ambient(uint8_t level)
{
    system_state_t *state = sys_state_get_mut();
    state->lighting.ambient_light = level;
    g_last_ambient_update_ms = state->uptime_ms;
    update_headlight_output();
}
