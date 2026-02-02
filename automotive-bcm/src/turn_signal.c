/**
 * @file turn_signal.c
 * @brief Turn Signal Control Module Implementation
 *
 * Implements turn signal and hazard light control with proper
 * timing, lane change assist, and bulb monitoring.
 */

#include <string.h>
#include "turn_signal.h"
#include "fault_manager.h"
#include "bcm_config.h"
#include "can_ids.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Turn signal internal state */
typedef struct {
    bool initialized;
    turn_mode_t mode;
    turn_direction_t direction;
    bool output_state;              /* Current output level (on/off) */
    uint32_t last_toggle_time;      /* Time of last toggle */
    uint8_t blink_count;            /* Number of blinks since activation */
    uint8_t lane_change_blinks;     /* Configured lane change blink count */
    uint16_t on_time_ms;            /* Flash on duration */
    uint16_t off_time_ms;           /* Flash off duration */
    bool fast_flash_enabled;        /* Fast flash for bulb failure */
    bool left_bulb_ok;              /* Left bulb status */
    bool right_bulb_ok;             /* Right bulb status */
    uint16_t left_bulb_current;     /* Left bulb current (mA) */
    uint16_t right_bulb_current;    /* Right bulb current (mA) */
    int16_t steering_angle;         /* Current steering angle */
    bool auto_cancel_enabled;       /* Auto-cancel after turn */
    int16_t turn_start_angle;       /* Steering angle when turn started */
} turn_signal_state_t;

static turn_signal_state_t g_turn_state;

/* =============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Set physical turn signal outputs
 */
static void set_turn_outputs(bool left, bool right)
{
    system_state_t *state = system_state_get_mutable();
    state->turn_signal.output_state = left || right;
    
    /* In real implementation, drive GPIO here */
    (void)left;
    (void)right;
}

/**
 * @brief Update system state from internal state
 */
static void update_system_state(void)
{
    system_state_t *state = system_state_get_mutable();
    
    if (g_turn_state.mode == TURN_MODE_HAZARD) {
        state->turn_signal.state = TURN_SIGNAL_HAZARD;
    } else if (g_turn_state.direction == TURN_DIRECTION_LEFT) {
        state->turn_signal.state = TURN_SIGNAL_LEFT;
    } else if (g_turn_state.direction == TURN_DIRECTION_RIGHT) {
        state->turn_signal.state = TURN_SIGNAL_RIGHT;
    } else {
        state->turn_signal.state = TURN_SIGNAL_OFF;
    }
    
    state->turn_signal.left_bulb_ok = g_turn_state.left_bulb_ok;
    state->turn_signal.right_bulb_ok = g_turn_state.right_bulb_ok;
    state->turn_signal.blink_count = g_turn_state.blink_count;
    state->turn_signal.output_state = g_turn_state.output_state;
}

/**
 * @brief Get current flash timing based on mode
 */
static void get_flash_timing(uint16_t *on_ms, uint16_t *off_ms)
{
    if (g_turn_state.fast_flash_enabled) {
        *on_ms = TURN_FAST_FLASH_ON_MS;
        *off_ms = TURN_FAST_FLASH_OFF_MS;
    } else if (g_turn_state.mode == TURN_MODE_HAZARD) {
        *on_ms = HAZARD_ON_TIME_MS;
        *off_ms = HAZARD_OFF_TIME_MS;
    } else {
        *on_ms = g_turn_state.on_time_ms;
        *off_ms = g_turn_state.off_time_ms;
    }
}

/* =============================================================================
 * Initialization
 * ========================================================================== */

bcm_result_t turn_signal_init(void)
{
    memset(&g_turn_state, 0, sizeof(g_turn_state));
    
    g_turn_state.mode = TURN_MODE_OFF;
    g_turn_state.direction = TURN_DIRECTION_NONE;
    g_turn_state.on_time_ms = TURN_SIGNAL_ON_TIME_MS;
    g_turn_state.off_time_ms = TURN_SIGNAL_OFF_TIME_MS;
    g_turn_state.lane_change_blinks = TURN_LANE_CHANGE_BLINKS;
    g_turn_state.left_bulb_ok = true;
    g_turn_state.right_bulb_ok = true;
    g_turn_state.auto_cancel_enabled = true;
    g_turn_state.initialized = true;
    
    update_system_state();
    
    return BCM_OK;
}

void turn_signal_deinit(void)
{
    set_turn_outputs(false, false);
    g_turn_state.initialized = false;
}

/* =============================================================================
 * Processing
 * ========================================================================== */

bcm_result_t turn_signal_process(uint32_t timestamp_ms)
{
    if (!g_turn_state.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Nothing to do if off */
    if (g_turn_state.mode == TURN_MODE_OFF) {
        return BCM_OK;
    }
    
    /* Get timing parameters */
    uint16_t on_time, off_time;
    get_flash_timing(&on_time, &off_time);
    
    /* Calculate time since last toggle */
    uint32_t elapsed = timestamp_ms - g_turn_state.last_toggle_time;
    uint16_t threshold = g_turn_state.output_state ? on_time : off_time;
    
    /* Check if it's time to toggle */
    if (elapsed >= threshold) {
        g_turn_state.output_state = !g_turn_state.output_state;
        g_turn_state.last_toggle_time = timestamp_ms;
        
        /* Count blinks on rising edge */
        if (g_turn_state.output_state) {
            g_turn_state.blink_count++;
        }
        
        /* Update physical outputs */
        bool left_on = false;
        bool right_on = false;
        
        if (g_turn_state.output_state) {
            if (g_turn_state.mode == TURN_MODE_HAZARD) {
                left_on = true;
                right_on = true;
            } else if (g_turn_state.direction == TURN_DIRECTION_LEFT) {
                left_on = true;
            } else if (g_turn_state.direction == TURN_DIRECTION_RIGHT) {
                right_on = true;
            }
        }
        
        set_turn_outputs(left_on, right_on);
    }
    
    /* Check lane change completion */
    if (g_turn_state.mode == TURN_MODE_LANE_CHANGE) {
        if (g_turn_state.blink_count >= g_turn_state.lane_change_blinks) {
            turn_signal_off();
        }
    }
    
    /* Check auto-cancel (simplified) */
    if (g_turn_state.auto_cancel_enabled && 
        g_turn_state.mode == TURN_MODE_NORMAL) {
        /* In real implementation, check steering wheel return */
    }
    
    /* Bulb monitoring */
    if (g_turn_state.output_state) {
        if (g_turn_state.direction == TURN_DIRECTION_LEFT || 
            g_turn_state.mode == TURN_MODE_HAZARD) {
            if (g_turn_state.left_bulb_current < TURN_BULB_CHECK_THRESHOLD_MA) {
                if (g_turn_state.left_bulb_ok) {
                    g_turn_state.left_bulb_ok = false;
                    fault_report(FAULT_TURN_BULB_FL, FAULT_SEVERITY_WARNING);
                    g_turn_state.fast_flash_enabled = true;
                }
            }
        }
        
        if (g_turn_state.direction == TURN_DIRECTION_RIGHT || 
            g_turn_state.mode == TURN_MODE_HAZARD) {
            if (g_turn_state.right_bulb_current < TURN_BULB_CHECK_THRESHOLD_MA) {
                if (g_turn_state.right_bulb_ok) {
                    g_turn_state.right_bulb_ok = false;
                    fault_report(FAULT_TURN_BULB_FR, FAULT_SEVERITY_WARNING);
                    g_turn_state.fast_flash_enabled = true;
                }
            }
        }
    }
    
    update_system_state();
    
    return BCM_OK;
}

/* =============================================================================
 * Turn Signal Control
 * ========================================================================== */

bcm_result_t turn_signal_left_on(void)
{
    g_turn_state.mode = TURN_MODE_NORMAL;
    g_turn_state.direction = TURN_DIRECTION_LEFT;
    g_turn_state.blink_count = 0;
    g_turn_state.output_state = true;
    g_turn_state.last_toggle_time = system_state_get()->uptime_ms;
    g_turn_state.turn_start_angle = g_turn_state.steering_angle;
    
    set_turn_outputs(true, false);
    update_system_state();
    
    return BCM_OK;
}

bcm_result_t turn_signal_left_off(void)
{
    if (g_turn_state.direction == TURN_DIRECTION_LEFT &&
        g_turn_state.mode != TURN_MODE_HAZARD) {
        return turn_signal_off();
    }
    return BCM_OK;
}

bcm_result_t turn_signal_right_on(void)
{
    g_turn_state.mode = TURN_MODE_NORMAL;
    g_turn_state.direction = TURN_DIRECTION_RIGHT;
    g_turn_state.blink_count = 0;
    g_turn_state.output_state = true;
    g_turn_state.last_toggle_time = system_state_get()->uptime_ms;
    g_turn_state.turn_start_angle = g_turn_state.steering_angle;
    
    set_turn_outputs(false, true);
    update_system_state();
    
    return BCM_OK;
}

bcm_result_t turn_signal_right_off(void)
{
    if (g_turn_state.direction == TURN_DIRECTION_RIGHT &&
        g_turn_state.mode != TURN_MODE_HAZARD) {
        return turn_signal_off();
    }
    return BCM_OK;
}

bcm_result_t turn_signal_off(void)
{
    g_turn_state.mode = TURN_MODE_OFF;
    g_turn_state.direction = TURN_DIRECTION_NONE;
    g_turn_state.output_state = false;
    g_turn_state.blink_count = 0;
    g_turn_state.fast_flash_enabled = false;
    
    set_turn_outputs(false, false);
    update_system_state();
    
    return BCM_OK;
}

bcm_result_t turn_signal_set_direction(turn_direction_t direction)
{
    switch (direction) {
        case TURN_DIRECTION_NONE:
            return turn_signal_off();
        case TURN_DIRECTION_LEFT:
            return turn_signal_left_on();
        case TURN_DIRECTION_RIGHT:
            return turn_signal_right_on();
        default:
            return BCM_ERROR_INVALID_PARAM;
    }
}

turn_direction_t turn_signal_get_direction(void)
{
    return g_turn_state.direction;
}

/* =============================================================================
 * Hazard Light Control
 * ========================================================================== */

bcm_result_t turn_signal_hazard_on(void)
{
    g_turn_state.mode = TURN_MODE_HAZARD;
    g_turn_state.direction = TURN_DIRECTION_NONE;
    g_turn_state.blink_count = 0;
    g_turn_state.output_state = true;
    g_turn_state.last_toggle_time = system_state_get()->uptime_ms;
    
    set_turn_outputs(true, true);
    update_system_state();
    
    return BCM_OK;
}

bcm_result_t turn_signal_hazard_off(void)
{
    if (g_turn_state.mode == TURN_MODE_HAZARD) {
        return turn_signal_off();
    }
    return BCM_OK;
}

bcm_result_t turn_signal_hazard_toggle(void)
{
    if (g_turn_state.mode == TURN_MODE_HAZARD) {
        return turn_signal_hazard_off();
    } else {
        return turn_signal_hazard_on();
    }
}

bool turn_signal_hazard_active(void)
{
    return g_turn_state.mode == TURN_MODE_HAZARD;
}

/* =============================================================================
 * Lane Change Assist
 * ========================================================================== */

bcm_result_t turn_signal_lane_change(turn_direction_t direction)
{
    if (direction == TURN_DIRECTION_NONE) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    g_turn_state.mode = TURN_MODE_LANE_CHANGE;
    g_turn_state.direction = direction;
    g_turn_state.blink_count = 0;
    g_turn_state.output_state = true;
    g_turn_state.last_toggle_time = system_state_get()->uptime_ms;
    
    bool left = (direction == TURN_DIRECTION_LEFT);
    set_turn_outputs(left, !left);
    update_system_state();
    
    return BCM_OK;
}

void turn_signal_set_lane_change_count(uint8_t count)
{
    g_turn_state.lane_change_blinks = count;
}

uint8_t turn_signal_get_lane_change_count(void)
{
    return g_turn_state.lane_change_blinks;
}

/* =============================================================================
 * Status Functions
 * ========================================================================== */

turn_mode_t turn_signal_get_mode(void)
{
    return g_turn_state.mode;
}

bcm_result_t turn_signal_get_status(turn_signal_status_t *status)
{
    if (!status) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    *status = system_state_get()->turn_signal;
    return BCM_OK;
}

bool turn_signal_is_illuminated(turn_direction_t direction)
{
    if (!g_turn_state.output_state) {
        return false;
    }
    
    if (g_turn_state.mode == TURN_MODE_HAZARD) {
        return true;
    }
    
    return g_turn_state.direction == direction;
}

uint8_t turn_signal_get_blink_count(void)
{
    return g_turn_state.blink_count;
}

/* =============================================================================
 * Bulb Monitoring
 * ========================================================================== */

bool turn_signal_left_bulb_ok(void)
{
    return g_turn_state.left_bulb_ok;
}

bool turn_signal_right_bulb_ok(void)
{
    return g_turn_state.right_bulb_ok;
}

bool turn_signal_bulb_fault_present(void)
{
    return !g_turn_state.left_bulb_ok || !g_turn_state.right_bulb_ok;
}

void turn_signal_update_bulb_current(uint16_t left_ma, uint16_t right_ma)
{
    g_turn_state.left_bulb_current = left_ma;
    g_turn_state.right_bulb_current = right_ma;
}

/* =============================================================================
 * Flash Rate Control
 * ========================================================================== */

void turn_signal_set_flash_rate(uint16_t on_time_ms, uint16_t off_time_ms)
{
    g_turn_state.on_time_ms = on_time_ms;
    g_turn_state.off_time_ms = off_time_ms;
}

void turn_signal_set_fast_flash(bool enable)
{
    g_turn_state.fast_flash_enabled = enable;
}

bool turn_signal_fast_flash_active(void)
{
    return g_turn_state.fast_flash_enabled;
}

/* =============================================================================
 * Steering Wheel Integration
 * ========================================================================== */

void turn_signal_update_steering_angle(int16_t angle_deg)
{
    g_turn_state.steering_angle = angle_deg;
}

void turn_signal_auto_cancel_enable(bool enable)
{
    g_turn_state.auto_cancel_enabled = enable;
}

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

bcm_result_t turn_signal_handle_can_cmd(const uint8_t *data, uint8_t len)
{
    if (!data || len < 1) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    uint8_t cmd = data[0];
    
    switch (cmd) {
        case TURN_CMD_LEFT_ON:
            return turn_signal_left_on();
            
        case TURN_CMD_LEFT_OFF:
            return turn_signal_left_off();
            
        case TURN_CMD_RIGHT_ON:
            return turn_signal_right_on();
            
        case TURN_CMD_RIGHT_OFF:
            return turn_signal_right_off();
            
        case TURN_CMD_HAZARD_ON:
            return turn_signal_hazard_on();
            
        case TURN_CMD_HAZARD_OFF:
            return turn_signal_hazard_off();
            
        default:
            return BCM_ERROR_INVALID_PARAM;
    }
}

void turn_signal_get_can_status(uint8_t *data, uint8_t *len)
{
    /* Byte 0: Mode and direction */
    data[0] = (uint8_t)g_turn_state.mode;
    data[0] |= (uint8_t)((uint8_t)g_turn_state.direction << 4);
    
    /* Byte 1: Output state and blink count */
    data[1] = g_turn_state.output_state ? 0x01 : 0x00;
    data[1] |= (uint8_t)(g_turn_state.blink_count << 1);
    
    /* Byte 2: Bulb status */
    data[2] = 0;
    if (g_turn_state.left_bulb_ok) data[2] |= 0x01;
    if (g_turn_state.right_bulb_ok) data[2] |= 0x02;
    if (g_turn_state.fast_flash_enabled) data[2] |= 0x80;
    
    /* Bytes 3-7: Reserved */
    data[3] = 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    
    *len = 8;
}
