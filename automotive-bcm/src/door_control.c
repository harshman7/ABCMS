/**
 * @file door_control.c
 * @brief Door Control Module Implementation
 *
 * Implements door lock, window control, and related features.
 */

#include <string.h>
#include "door_control.h"
#include "fault_manager.h"
#include "lighting_control.h"
#include "bcm_config.h"
#include "can_ids.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Door control state */
typedef struct {
    bool initialized;
    bool auto_lock_enabled;
    uint16_t vehicle_speed;
    bool speed_lock_triggered;
    uint32_t last_unlock_time;
    uint32_t window_move_start[DOOR_COUNT_MAX];
    bool window_moving[DOOR_COUNT_MAX];
    bool window_one_touch[DOOR_COUNT_MAX];
} door_control_state_t;

static door_control_state_t g_door_state;

/* =============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Validate door position parameter
 */
static bool is_valid_door(door_position_t door)
{
    return (door >= DOOR_FRONT_LEFT && door < DOOR_COUNT_MAX);
}

/**
 * @brief Actuate door lock hardware
 */
static bcm_result_t actuate_lock(door_position_t door, bool lock)
{
    system_state_t *state = system_state_get_mutable();
    
    if (!is_valid_door(door)) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    /* Set lock state */
    state->doors[door].lock_state = lock ? LOCK_STATE_LOCKED : LOCK_STATE_UNLOCKED;
    
    /* In real implementation, drive GPIO here */
    
    return BCM_OK;
}

/**
 * @brief Actuate window motor
 */
static bcm_result_t actuate_window(door_position_t door, int8_t direction)
{
    system_state_t *state = system_state_get_mutable();
    
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    if (direction > 0) {
        state->doors[door].window_state = WINDOW_STATE_MOVING_UP;
    } else if (direction < 0) {
        state->doors[door].window_state = WINDOW_STATE_MOVING_DOWN;
    } else {
        /* Determine final state based on position */
        if (state->doors[door].window_position >= 100) {
            state->doors[door].window_state = WINDOW_STATE_CLOSED;
        } else if (state->doors[door].window_position == 0) {
            state->doors[door].window_state = WINDOW_STATE_OPEN;
        } else {
            state->doors[door].window_state = WINDOW_STATE_PARTIAL;
        }
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Initialization
 * ========================================================================== */

bcm_result_t door_control_init(void)
{
    memset(&g_door_state, 0, sizeof(g_door_state));
    g_door_state.auto_lock_enabled = true;
    g_door_state.initialized = true;
    
    return BCM_OK;
}

void door_control_deinit(void)
{
    /* Stop any window motors */
    for (int i = 0; i <= DOOR_REAR_RIGHT; i++) {
        if (g_door_state.window_moving[i]) {
            actuate_window((door_position_t)i, 0);
        }
    }
    
    g_door_state.initialized = false;
}

/* =============================================================================
 * Processing
 * ========================================================================== */

bcm_result_t door_control_process(uint32_t timestamp_ms)
{
    system_state_t *state = system_state_get_mutable();
    
    if (!g_door_state.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Auto-lock feature */
    if (g_door_state.auto_lock_enabled && !g_door_state.speed_lock_triggered) {
        if (g_door_state.vehicle_speed >= DOOR_AUTO_LOCK_SPEED_KMH) {
            door_lock_all();
            g_door_state.speed_lock_triggered = true;
        }
    }
    
    /* Reset speed lock when stopped */
    if (g_door_state.vehicle_speed == 0) {
        g_door_state.speed_lock_triggered = false;
    }
    
    /* Process window movements */
    for (int i = 0; i <= DOOR_REAR_RIGHT; i++) {
        if (g_door_state.window_moving[i]) {
            uint32_t elapsed = timestamp_ms - g_door_state.window_move_start[i];
            
            /* Check for timeout */
            if (elapsed >= DOOR_WINDOW_MOTOR_TIMEOUT_MS) {
                actuate_window((door_position_t)i, 0);
                g_door_state.window_moving[i] = false;
                fault_report((fault_code_t)(FAULT_DOOR_WINDOW_FL_MOTOR + (uint16_t)i), 
                            FAULT_SEVERITY_WARNING);
            }
            
            /* Simulate window movement */
            if (state->doors[i].window_state == WINDOW_STATE_MOVING_UP) {
                if (state->doors[i].window_position < 100) {
                    state->doors[i].window_position++;
                } else {
                    actuate_window((door_position_t)i, 0);
                    g_door_state.window_moving[i] = false;
                }
            } else if (state->doors[i].window_state == WINDOW_STATE_MOVING_DOWN) {
                if (state->doors[i].window_position > 0) {
                    state->doors[i].window_position--;
                } else {
                    actuate_window((door_position_t)i, 0);
                    g_door_state.window_moving[i] = false;
                }
            }
        }
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Lock Control
 * ========================================================================== */

bcm_result_t door_lock_all(void)
{
    bcm_result_t result = BCM_OK;
    
    for (int i = DOOR_FRONT_LEFT; i <= DOOR_REAR_RIGHT; i++) {
        bcm_result_t r = actuate_lock((door_position_t)i, true);
        if (r != BCM_OK) {
            result = r;
        }
    }
    
    return result;
}

bcm_result_t door_unlock_all(void)
{
    bcm_result_t result = BCM_OK;
    
    for (int i = DOOR_FRONT_LEFT; i <= DOOR_REAR_RIGHT; i++) {
        bcm_result_t r = actuate_lock((door_position_t)i, false);
        if (r != BCM_OK) {
            result = r;
        }
    }
    
    /* Trigger welcome lights */
    lighting_trigger_welcome_lights();
    
    return result;
}

bcm_result_t door_lock(door_position_t door)
{
    return actuate_lock(door, true);
}

bcm_result_t door_unlock(door_position_t door)
{
    return actuate_lock(door, false);
}

lock_state_t door_get_lock_state(door_position_t door)
{
    if (!is_valid_door(door)) {
        return LOCK_STATE_UNKNOWN;
    }
    return system_state_get()->doors[door].lock_state;
}

bool door_all_locked(void)
{
    const system_state_t *state = system_state_get();
    
    for (int i = DOOR_FRONT_LEFT; i <= DOOR_REAR_RIGHT; i++) {
        if (state->doors[i].lock_state != LOCK_STATE_LOCKED) {
            return false;
        }
    }
    return true;
}

bool door_any_unlocked(void)
{
    const system_state_t *state = system_state_get();
    
    for (int i = DOOR_FRONT_LEFT; i <= DOOR_REAR_RIGHT; i++) {
        if (state->doors[i].lock_state == LOCK_STATE_UNLOCKED) {
            return true;
        }
    }
    return false;
}

/* =============================================================================
 * Window Control
 * ========================================================================== */

bcm_result_t door_window_open(door_position_t door, bool one_touch)
{
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    g_door_state.window_moving[(int)door] = true;
    g_door_state.window_one_touch[(int)door] = one_touch;
    g_door_state.window_move_start[(int)door] = system_state_get()->uptime_ms;
    
    return actuate_window(door, -1);
}

bcm_result_t door_window_close(door_position_t door, bool one_touch)
{
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    g_door_state.window_moving[(int)door] = true;
    g_door_state.window_one_touch[(int)door] = one_touch;
    g_door_state.window_move_start[(int)door] = system_state_get()->uptime_ms;
    
    return actuate_window(door, 1);
}

bcm_result_t door_window_stop(door_position_t door)
{
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    g_door_state.window_moving[(int)door] = false;
    return actuate_window(door, 0);
}

bcm_result_t door_window_set_position(door_position_t door, uint8_t position)
{
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT || position > 100) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    uint8_t current = system_state_get()->doors[door].window_position;
    
    if (position > current) {
        return door_window_close(door, true);
    } else if (position < current) {
        return door_window_open(door, true);
    }
    
    return BCM_OK;
}

uint8_t door_window_get_position(door_position_t door)
{
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT) {
        return 0xFF;
    }
    return system_state_get()->doors[door].window_position;
}

window_state_t door_window_get_state(door_position_t door)
{
    if (!is_valid_door(door) || door > DOOR_REAR_RIGHT) {
        return WINDOW_STATE_UNKNOWN;
    }
    return system_state_get()->doors[door].window_state;
}

bcm_result_t door_window_close_all(void)
{
    bcm_result_t result = BCM_OK;
    
    for (int i = DOOR_FRONT_LEFT; i <= DOOR_REAR_RIGHT; i++) {
        bcm_result_t r = door_window_close((door_position_t)i, true);
        if (r != BCM_OK) {
            result = r;
        }
    }
    
    return result;
}

/* =============================================================================
 * Door Status
 * ========================================================================== */

bool door_is_open(door_position_t door)
{
    if (!is_valid_door(door)) {
        return false;
    }
    return system_state_get()->doors[door].is_open;
}

bool door_any_open(void)
{
    const system_state_t *state = system_state_get();
    
    for (int i = 0; i < DOOR_COUNT_MAX; i++) {
        if (state->doors[i].is_open) {
            return true;
        }
    }
    return false;
}

bcm_result_t door_get_status(door_position_t door, door_status_t *status)
{
    if (!is_valid_door(door) || !status) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    *status = system_state_get()->doors[door];
    return BCM_OK;
}

/* =============================================================================
 * Child Safety Lock
 * ========================================================================== */

bcm_result_t door_child_lock_enable(door_position_t door)
{
    if (door != DOOR_REAR_LEFT && door != DOOR_REAR_RIGHT) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    system_state_get_mutable()->doors[door].child_lock_active = true;
    return BCM_OK;
}

bcm_result_t door_child_lock_disable(door_position_t door)
{
    if (door != DOOR_REAR_LEFT && door != DOOR_REAR_RIGHT) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    system_state_get_mutable()->doors[door].child_lock_active = false;
    return BCM_OK;
}

bool door_child_lock_active(door_position_t door)
{
    if (!is_valid_door(door)) {
        return false;
    }
    return system_state_get()->doors[door].child_lock_active;
}

/* =============================================================================
 * Auto-Lock Feature
 * ========================================================================== */

void door_auto_lock_enable(bool enable)
{
    g_door_state.auto_lock_enabled = enable;
}

bool door_auto_lock_enabled(void)
{
    return g_door_state.auto_lock_enabled;
}

void door_update_vehicle_speed(uint16_t speed_kmh)
{
    g_door_state.vehicle_speed = speed_kmh;
}

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

bcm_result_t door_control_handle_can_cmd(const uint8_t *data, uint8_t len)
{
    if (!data || len < 1) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    uint8_t cmd = data[0];
    door_position_t door = (len > 1) ? (door_position_t)data[1] : DOOR_FRONT_LEFT;
    
    switch (cmd) {
        case DOOR_CMD_LOCK_ALL:
            return door_lock_all();
            
        case DOOR_CMD_UNLOCK_ALL:
            return door_unlock_all();
            
        case DOOR_CMD_LOCK_SINGLE:
            return door_lock(door);
            
        case DOOR_CMD_UNLOCK_SINGLE:
            return door_unlock(door);
            
        case DOOR_CMD_WINDOW_UP:
            return door_window_close(door, (len > 2) && data[2]);
            
        case DOOR_CMD_WINDOW_DOWN:
            return door_window_open(door, (len > 2) && data[2]);
            
        case DOOR_CMD_WINDOW_STOP:
            return door_window_stop(door);
            
        case DOOR_CMD_CHILD_LOCK_ON:
            return door_child_lock_enable(door);
            
        case DOOR_CMD_CHILD_LOCK_OFF:
            return door_child_lock_disable(door);
            
        default:
            return BCM_ERROR_INVALID_PARAM;
    }
}

void door_control_get_can_status(uint8_t *data, uint8_t *len)
{
    const system_state_t *state = system_state_get();
    
    /* Byte 0: Lock states (bit field) */
    data[0] = 0;
    for (int i = 0; i < 4; i++) {
        if (state->doors[i].lock_state == LOCK_STATE_LOCKED) {
            data[0] |= (uint8_t)(1U << (uint8_t)i);
        }
    }
    
    /* Byte 1: Door open states (bit field) */
    data[1] = 0;
    for (int i = 0; i < DOOR_COUNT_MAX; i++) {
        if (state->doors[i].is_open) {
            data[1] |= (uint8_t)(1U << (uint8_t)i);
        }
    }
    
    /* Bytes 2-5: Window positions */
    for (int i = 0; i < 4; i++) {
        data[2 + i] = state->doors[i].window_position;
    }
    
    /* Byte 6: Child locks */
    data[6] = 0;
    if (state->doors[DOOR_REAR_LEFT].child_lock_active) data[6] |= 0x01;
    if (state->doors[DOOR_REAR_RIGHT].child_lock_active) data[6] |= 0x02;
    
    /* Byte 7: Reserved */
    data[7] = 0;
    
    *len = 8;
}
