/**
 * @file bcm.c
 * @brief Body Control Module Core Implementation
 *
 * This file implements the main BCM coordination logic, state machine,
 * and initialization/processing routines.
 */

#include <string.h>
#include <stdio.h>

#include "bcm.h"
#include "door_control.h"
#include "lighting_control.h"
#include "turn_signal.h"
#include "fault_manager.h"
#include "can_interface.h"
#include "bcm_config.h"
#include "can_ids.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Version string buffer */
static char g_version_string[16];

/** Global system state */
static system_state_t g_system_state;

/** BCM initialized flag */
static bool g_initialized = false;

/** State change callback */
static bcm_state_callback_t g_state_callback = NULL;

/** Last heartbeat timestamp */
static uint32_t g_last_heartbeat_ms = 0;

/** Last status broadcast timestamp */
static uint32_t g_last_status_ms = 0;

/* =============================================================================
 * System State Functions
 * ========================================================================== */

const system_state_t* system_state_get(void)
{
    return &g_system_state;
}

system_state_t* system_state_get_mutable(void)
{
    return &g_system_state;
}

void system_state_init(void)
{
    memset(&g_system_state, 0, sizeof(system_state_t));
    g_system_state.bcm_state = BCM_STATE_INIT;
    g_system_state.vehicle.ignition = IGNITION_OFF;
    
    /* Initialize door states */
    for (int i = 0; i < DOOR_COUNT_MAX; i++) {
        g_system_state.doors[i].lock_state = LOCK_STATE_UNKNOWN;
        g_system_state.doors[i].window_state = WINDOW_STATE_UNKNOWN;
        g_system_state.doors[i].window_position = 100; /* Assume closed */
        g_system_state.doors[i].is_open = false;
        g_system_state.doors[i].child_lock_active = false;
    }
    
    /* Initialize lighting state */
    g_system_state.lighting.headlights = LIGHT_OFF;
    g_system_state.lighting.high_beam = LIGHT_OFF;
    g_system_state.lighting.fog_lights = LIGHT_OFF;
    g_system_state.lighting.tail_lights = LIGHT_OFF;
    g_system_state.lighting.interior = LIGHT_OFF;
    g_system_state.lighting.interior_brightness = 0;
    g_system_state.lighting.auto_mode_active = false;
    
    /* Initialize turn signal state */
    g_system_state.turn_signal.state = TURN_SIGNAL_OFF;
    g_system_state.turn_signal.left_bulb_ok = true;
    g_system_state.turn_signal.right_bulb_ok = true;
    g_system_state.turn_signal.blink_count = 0;
    g_system_state.turn_signal.output_state = false;
}

/* =============================================================================
 * Version Functions
 * ========================================================================== */

const char* bcm_get_version(void)
{
    snprintf(g_version_string, sizeof(g_version_string), "%u.%u.%u",
             BCM_SW_VERSION_MAJOR,
             BCM_SW_VERSION_MINOR,
             BCM_SW_VERSION_PATCH);
    return g_version_string;
}

void bcm_get_version_numbers(uint8_t *major, uint8_t *minor, uint8_t *patch)
{
    if (major) *major = BCM_SW_VERSION_MAJOR;
    if (minor) *minor = BCM_SW_VERSION_MINOR;
    if (patch) *patch = BCM_SW_VERSION_PATCH;
}

/* =============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Change BCM state with callback notification
 */
static void set_bcm_state(bcm_state_t new_state)
{
    bcm_state_t old_state = g_system_state.bcm_state;
    
    if (old_state != new_state) {
        g_system_state.bcm_state = new_state;
        
        if (g_state_callback) {
            g_state_callback(old_state, new_state);
        }
    }
}

/**
 * @brief Transmit BCM heartbeat message
 */
static void transmit_heartbeat(void)
{
    uint8_t data[8] = {0};
    
    /* Byte 0: BCM state */
    data[0] = (uint8_t)g_system_state.bcm_state;
    
    /* Byte 1-4: Uptime */
    data[1] = (uint8_t)(g_system_state.uptime_ms >> 24);
    data[2] = (uint8_t)(g_system_state.uptime_ms >> 16);
    data[3] = (uint8_t)(g_system_state.uptime_ms >> 8);
    data[4] = (uint8_t)(g_system_state.uptime_ms);
    
    /* Byte 5: Active fault count */
    data[5] = (uint8_t)(g_system_state.active_faults & 0xFF);
    
    /* Byte 6-7: Reserved */
    data[6] = 0;
    data[7] = 0;
    
    can_transmit_std(CAN_ID_BCM_HEARTBEAT, data, 8);
}

/**
 * @brief Transmit BCM status message
 */
static void transmit_status(void)
{
    uint8_t data[8] = {0};
    
    /* Byte 0: BCM state */
    data[0] = (uint8_t)g_system_state.bcm_state;
    
    /* Byte 1: Vehicle state summary */
    data[1] = (uint8_t)g_system_state.vehicle.ignition;
    
    /* Byte 2: Door summary (bit field) */
    data[2] = 0;
    for (int i = 0; i < 4 && i < DOOR_COUNT_MAX; i++) {
        if (g_system_state.doors[i].is_open) {
            data[2] |= (uint8_t)(1U << (uint8_t)i);
        }
        if (g_system_state.doors[i].lock_state == LOCK_STATE_LOCKED) {
            data[2] |= (uint8_t)(1U << ((uint8_t)i + 4U));
        }
    }
    
    /* Byte 3: Lighting summary */
    data[3] = 0;
    if (g_system_state.lighting.headlights != LIGHT_OFF) data[3] |= 0x01;
    if (g_system_state.lighting.high_beam != LIGHT_OFF) data[3] |= 0x02;
    if (g_system_state.lighting.fog_lights != LIGHT_OFF) data[3] |= 0x04;
    if (g_system_state.lighting.interior != LIGHT_OFF) data[3] |= 0x08;
    
    /* Byte 4: Turn signal state */
    data[4] = (uint8_t)g_system_state.turn_signal.state;
    
    /* Byte 5-6: Vehicle speed */
    data[5] = (uint8_t)(g_system_state.vehicle.vehicle_speed_kmh >> 8);
    data[6] = (uint8_t)(g_system_state.vehicle.vehicle_speed_kmh);
    
    /* Byte 7: Reserved */
    data[7] = 0;
    
    can_transmit_std(CAN_ID_BCM_STATUS, data, 8);
}

/* =============================================================================
 * Initialization Functions
 * ========================================================================== */

bcm_result_t bcm_init(void)
{
    bcm_result_t result;
    
    if (g_initialized) {
        return BCM_ERROR;
    }
    
    /* Initialize system state */
    system_state_init();
    
    /* Initialize CAN interface */
    result = can_interface_init(CAN_BAUD_RATE);
    if (result != BCM_OK) {
        return result;
    }
    
    /* Initialize subsystems */
    result = fault_manager_init();
    if (result != BCM_OK) {
        return result;
    }
    
    result = door_control_init();
    if (result != BCM_OK) {
        return result;
    }
    
    result = lighting_control_init();
    if (result != BCM_OK) {
        return result;
    }
    
    result = turn_signal_init();
    if (result != BCM_OK) {
        return result;
    }
    
    /* Transition to normal state */
    set_bcm_state(BCM_STATE_NORMAL);
    
    g_initialized = true;
    return BCM_OK;
}

void bcm_deinit(void)
{
    if (!g_initialized) {
        return;
    }
    
    /* Deinitialize in reverse order */
    turn_signal_deinit();
    lighting_control_deinit();
    door_control_deinit();
    fault_manager_deinit();
    can_interface_deinit();
    
    g_initialized = false;
    set_bcm_state(BCM_STATE_INIT);
}

/* =============================================================================
 * Processing Functions
 * ========================================================================== */

bcm_result_t bcm_process(uint32_t timestamp_ms)
{
    bcm_result_t result;
    
    if (!g_initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Update uptime */
    g_system_state.uptime_ms = timestamp_ms;
    
    /* Process CAN reception */
    result = can_process_rx(timestamp_ms);
    if (result != BCM_OK && result != BCM_ERROR) {
        /* Log CAN errors but continue */
    }
    
    /* Process subsystems based on state */
    if (g_system_state.bcm_state == BCM_STATE_NORMAL ||
        g_system_state.bcm_state == BCM_STATE_DIAGNOSTIC) {
        
        /* Process door control */
        result = door_control_process(timestamp_ms);
        if (result != BCM_OK) {
            /* Handle error */
        }
        
        /* Process lighting control */
        result = lighting_control_process(timestamp_ms);
        if (result != BCM_OK) {
            /* Handle error */
        }
        
        /* Process turn signals */
        result = turn_signal_process(timestamp_ms);
        if (result != BCM_OK) {
            /* Handle error */
        }
    }
    
    /* Process fault manager (always) */
    result = fault_manager_process(timestamp_ms);
    if (result != BCM_OK) {
        /* Handle error */
    }
    
    /* Update fault count */
    g_system_state.active_faults = fault_get_active_count();
    
    /* Check for critical faults */
    if (fault_any_critical() && g_system_state.bcm_state == BCM_STATE_NORMAL) {
        set_bcm_state(BCM_STATE_FAULT);
    }
    
    /* Transmit periodic messages */
    if ((timestamp_ms - g_last_heartbeat_ms) >= CAN_HEARTBEAT_PERIOD_MS) {
        transmit_heartbeat();
        g_last_heartbeat_ms = timestamp_ms;
    }
    
    if ((timestamp_ms - g_last_status_ms) >= CAN_BCM_STATUS_PERIOD_MS) {
        transmit_status();
        g_last_status_ms = timestamp_ms;
    }
    
    /* Process CAN transmission */
    result = can_process_tx(timestamp_ms);
    
    return BCM_OK;
}

void bcm_tick(void)
{
    /* Lightweight tick for time-critical operations */
    /* Can be called from interrupt context */
}

/* =============================================================================
 * State Control Functions
 * ========================================================================== */

bcm_state_t bcm_get_state(void)
{
    return g_system_state.bcm_state;
}

bcm_result_t bcm_request_state(bcm_state_t new_state)
{
    /* Validate state transition */
    switch (g_system_state.bcm_state) {
        case BCM_STATE_INIT:
            if (new_state != BCM_STATE_NORMAL) {
                return BCM_ERROR_INVALID_PARAM;
            }
            break;
            
        case BCM_STATE_NORMAL:
            if (new_state != BCM_STATE_SLEEP &&
                new_state != BCM_STATE_DIAGNOSTIC &&
                new_state != BCM_STATE_FAULT) {
                return BCM_ERROR_INVALID_PARAM;
            }
            break;
            
        case BCM_STATE_SLEEP:
            if (new_state != BCM_STATE_WAKEUP) {
                return BCM_ERROR_INVALID_PARAM;
            }
            break;
            
        case BCM_STATE_WAKEUP:
            if (new_state != BCM_STATE_NORMAL) {
                return BCM_ERROR_INVALID_PARAM;
            }
            break;
            
        case BCM_STATE_FAULT:
            if (new_state != BCM_STATE_NORMAL &&
                new_state != BCM_STATE_DIAGNOSTIC) {
                return BCM_ERROR_INVALID_PARAM;
            }
            break;
            
        case BCM_STATE_DIAGNOSTIC:
            if (new_state != BCM_STATE_NORMAL) {
                return BCM_ERROR_INVALID_PARAM;
            }
            break;
    }
    
    set_bcm_state(new_state);
    return BCM_OK;
}

bcm_result_t bcm_enter_sleep(void)
{
    if (g_system_state.bcm_state != BCM_STATE_NORMAL) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Turn off all outputs */
    turn_signal_off();
    lighting_headlights_off();
    lighting_interior_off();
    
    set_bcm_state(BCM_STATE_SLEEP);
    return BCM_OK;
}

bcm_result_t bcm_wakeup(void)
{
    if (g_system_state.bcm_state != BCM_STATE_SLEEP) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    set_bcm_state(BCM_STATE_WAKEUP);
    
    /* Re-initialize hardware as needed */
    
    set_bcm_state(BCM_STATE_NORMAL);
    return BCM_OK;
}

/* =============================================================================
 * Status Functions
 * ========================================================================== */

bool bcm_is_ready(void)
{
    return g_initialized && 
           (g_system_state.bcm_state == BCM_STATE_NORMAL ||
            g_system_state.bcm_state == BCM_STATE_DIAGNOSTIC);
}

uint32_t bcm_get_uptime(void)
{
    return g_system_state.uptime_ms;
}

bcm_result_t bcm_self_test(void)
{
    /* Perform basic self-tests */
    
    /* Check CAN status */
    if (can_is_bus_off()) {
        fault_report(FAULT_CAN_BUS_OFF, FAULT_SEVERITY_ERROR);
        return BCM_ERROR_COMM;
    }
    
    /* Additional self-tests can be added here */
    
    return BCM_OK;
}

const system_state_t* bcm_get_system_state(void)
{
    return &g_system_state;
}

/* =============================================================================
 * Callback Registration
 * ========================================================================== */

void bcm_register_state_callback(bcm_state_callback_t callback)
{
    g_state_callback = callback;
}

/* =============================================================================
 * Diagnostic Mode
 * ========================================================================== */

bcm_result_t bcm_enter_diagnostic_mode(void)
{
    if (g_system_state.bcm_state == BCM_STATE_SLEEP) {
        return BCM_ERROR_NOT_READY;
    }
    
    set_bcm_state(BCM_STATE_DIAGNOSTIC);
    return BCM_OK;
}

bcm_result_t bcm_exit_diagnostic_mode(void)
{
    if (g_system_state.bcm_state != BCM_STATE_DIAGNOSTIC) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    set_bcm_state(BCM_STATE_NORMAL);
    return BCM_OK;
}

bool bcm_is_diagnostic_mode(void)
{
    return g_system_state.bcm_state == BCM_STATE_DIAGNOSTIC;
}
