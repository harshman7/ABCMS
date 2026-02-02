/**
 * @file system_state.c
 * @brief System State Implementation
 */

#include <string.h>
#include "system_state.h"

/*******************************************************************************
 * Private Data
 ******************************************************************************/

static system_state_t g_system_state;

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

const system_state_t* sys_state_get(void)
{
    return &g_system_state;
}

system_state_t* sys_state_get_mut(void)
{
    return &g_system_state;
}

void sys_state_init(void)
{
    memset(&g_system_state, 0, sizeof(g_system_state));
    
    g_system_state.bcm_state = BCM_STATE_INIT;
    
    /* Initialize all doors to unlocked */
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        g_system_state.door.lock_state[i] = DOOR_STATE_UNLOCKED;
        g_system_state.door.is_open[i] = false;
    }
    g_system_state.door.last_result = CMD_RESULT_OK;
    
    /* Initialize lighting to off */
    g_system_state.lighting.headlight_mode = LIGHTING_STATE_OFF;
    g_system_state.lighting.headlight_output = HEADLIGHT_STATE_OFF;
    g_system_state.lighting.high_beam_active = false;
    g_system_state.lighting.interior_mode = LIGHTING_STATE_OFF;
    g_system_state.lighting.interior_brightness = 0;
    g_system_state.lighting.interior_on = false;
    g_system_state.lighting.ambient_light = 128; /* Mid-range default */
    g_system_state.lighting.last_result = CMD_RESULT_OK;
    
    /* Initialize turn signals to off */
    g_system_state.turn_signal.mode = TURN_SIG_STATE_OFF;
    g_system_state.turn_signal.left_output = false;
    g_system_state.turn_signal.right_output = false;
    g_system_state.turn_signal.flash_count = 0;
    g_system_state.turn_signal.last_result = CMD_RESULT_OK;
    
    /* Initialize fault manager */
    g_system_state.fault.flags1 = 0;
    g_system_state.fault.flags2 = 0;
    g_system_state.fault.active_count = 0;
    g_system_state.fault.total_count = 0;
    g_system_state.fault.most_recent_code = FAULT_CODE_NONE;
    
    /* Initialize event log */
    g_system_state.event_log.head = 0;
    g_system_state.event_log.count = 0;
}

void sys_state_update_time(uint32_t current_ms)
{
    g_system_state.uptime_ms = current_ms;
    
    /* Update minutes counter (wrapping) */
    g_system_state.uptime_minutes = (uint8_t)((current_ms / 60000U) & 0xFFU);
}

/*******************************************************************************
 * Event Log Functions
 ******************************************************************************/

void event_log_add(event_type_t type, const uint8_t *data)
{
    event_log_t *log = &g_system_state.event_log;
    event_log_entry_t *entry = &log->entries[log->head];
    
    entry->timestamp_ms = g_system_state.uptime_ms;
    entry->type = type;
    
    if (data != NULL) {
        memcpy(entry->data, data, 4);
    } else {
        memset(entry->data, 0, 4);
    }
    
    /* Advance head (ring buffer) */
    log->head = (log->head + 1U) % EVENT_LOG_SIZE;
    
    if (log->count < EVENT_LOG_SIZE) {
        log->count++;
    }
}

bool event_log_get(uint8_t index, event_log_entry_t *entry)
{
    const event_log_t *log = &g_system_state.event_log;
    
    if (entry == NULL || index >= log->count) {
        return false;
    }
    
    /* Calculate actual index (oldest first) */
    uint8_t start;
    if (log->count < EVENT_LOG_SIZE) {
        start = 0;
    } else {
        start = log->head; /* head points to oldest when full */
    }
    
    uint8_t actual_index = (start + index) % EVENT_LOG_SIZE;
    *entry = log->entries[actual_index];
    
    return true;
}

uint8_t event_log_count(void)
{
    return g_system_state.event_log.count;
}

void event_log_clear(void)
{
    g_system_state.event_log.head = 0;
    g_system_state.event_log.count = 0;
}
