/**
 * @file fault_manager.c
 * @brief Fault Manager Implementation
 *
 * Tracks active faults and provides status frame generation.
 */

#include <string.h>
#include <stdio.h>
#include "fault_manager.h"
#include "can_ids.h"

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

/**
 * @brief Map fault code to flags bit
 */
static uint8_t fault_code_to_flag(fault_code_t code)
{
    switch (code) {
        case FAULT_CODE_DOOR_MOTOR:         return FAULT_BIT_DOOR_MOTOR;
        case FAULT_CODE_HEADLIGHT_BULB:     return FAULT_BIT_HEADLIGHT_BULB;
        case FAULT_CODE_TURN_BULB:          return FAULT_BIT_TURN_BULB;
        case FAULT_CODE_CAN_COMM:           return FAULT_BIT_CAN_COMM;
        case FAULT_CODE_INVALID_CHECKSUM:   return FAULT_BIT_CMD_CHECKSUM;
        case FAULT_CODE_INVALID_COUNTER:    return FAULT_BIT_CMD_COUNTER;
        case FAULT_CODE_TIMEOUT:            return FAULT_BIT_TIMEOUT;
        default:                            return 0;
    }
}

/**
 * @brief Find fault in active faults array
 * @return Index if found, -1 if not found
 */
static int find_fault(fault_code_t code)
{
    const fault_state_t *fault = &sys_state_get()->fault;
    
    for (uint8_t i = 0; i < fault->active_count; i++) {
        if (fault->active_faults[i].code == code) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Log fault event
 */
static void log_fault_event(bool set, fault_code_t code)
{
    uint8_t data[4] = { set ? 1U : 0U, (uint8_t)code, 0, 0 };
    event_log_add(set ? EVENT_FAULT_SET : EVENT_FAULT_CLEAR, data);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

void fault_manager_init(void)
{
    fault_state_t *fault = &sys_state_get_mut()->fault;
    
    fault->flags1 = 0;
    fault->flags2 = 0;
    fault->active_count = 0;
    fault->total_count = 0;
    fault->most_recent_code = FAULT_CODE_NONE;
    fault->most_recent_time_ms = 0;
    
    memset(fault->active_faults, 0, sizeof(fault->active_faults));
    
    printf("[FAULT] Initialized\n");
}

void fault_manager_set(fault_code_t code)
{
    fault_state_t *fault = &sys_state_get_mut()->fault;
    const system_state_t *state = sys_state_get();
    
    /* Check if already active */
    if (find_fault(code) >= 0) {
        return; /* Already active */
    }
    
    /* Add to active faults if room */
    if (fault->active_count < MAX_ACTIVE_FAULTS) {
        fault->active_faults[fault->active_count].code = code;
        fault->active_faults[fault->active_count].timestamp_ms = state->uptime_ms;
        fault->active_count++;
    }
    
    /* Update flags */
    fault->flags1 |= fault_code_to_flag(code);
    
    /* Update most recent */
    fault->most_recent_code = code;
    fault->most_recent_time_ms = state->uptime_ms;
    fault->total_count++;
    
    log_fault_event(true, code);
    printf("[FAULT] SET: 0x%02X\n", code);
}

void fault_manager_clear(fault_code_t code)
{
    fault_state_t *fault = &sys_state_get_mut()->fault;
    
    int idx = find_fault(code);
    if (idx < 0) {
        return; /* Not found */
    }
    
    /* Remove from array by shifting */
    for (uint8_t i = (uint8_t)idx; i < fault->active_count - 1; i++) {
        fault->active_faults[i] = fault->active_faults[i + 1];
    }
    fault->active_count--;
    
    /* Update flags */
    fault->flags1 &= ~fault_code_to_flag(code);
    
    log_fault_event(false, code);
    printf("[FAULT] CLEAR: 0x%02X\n", code);
}

void fault_manager_clear_all(void)
{
    fault_state_t *fault = &sys_state_get_mut()->fault;
    
    for (uint8_t i = 0; i < fault->active_count; i++) {
        log_fault_event(false, fault->active_faults[i].code);
    }
    
    fault->flags1 = 0;
    fault->flags2 = 0;
    fault->active_count = 0;
    
    printf("[FAULT] CLEAR ALL\n");
}

bool fault_manager_is_active(fault_code_t code)
{
    return (find_fault(code) >= 0);
}

uint8_t fault_manager_get_count(void)
{
    return sys_state_get()->fault.active_count;
}

uint8_t fault_manager_get_flags1(void)
{
    return sys_state_get()->fault.flags1;
}

uint8_t fault_manager_get_flags2(void)
{
    return sys_state_get()->fault.flags2;
}

fault_code_t fault_manager_get_most_recent(void)
{
    return sys_state_get()->fault.most_recent_code;
}

void fault_manager_build_status_frame(can_frame_t *frame)
{
    const fault_state_t *fault = &sys_state_get()->fault;
    system_state_t *mut_state = sys_state_get_mut();
    
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = CAN_ID_FAULT_STATUS;
    frame->dlc = FAULT_STATUS_DLC;
    
    /* Byte 0: Fault flags 1 */
    frame->data[FAULT_STATUS_BYTE_FLAGS1] = fault->flags1;
    
    /* Byte 1: Fault flags 2 */
    frame->data[FAULT_STATUS_BYTE_FLAGS2] = fault->flags2;
    
    /* Byte 2: Total fault count */
    frame->data[FAULT_STATUS_BYTE_COUNT] = fault->total_count;
    
    /* Byte 3: Most recent fault code */
    frame->data[FAULT_STATUS_BYTE_RECENT_CODE] = (uint8_t)fault->most_recent_code;
    
    /* Bytes 4-5: Timestamp (seconds since boot) */
    uint16_t timestamp_sec = (uint16_t)(fault->most_recent_time_ms / 1000U);
    frame->data[FAULT_STATUS_BYTE_TS_HIGH] = (uint8_t)(timestamp_sec >> 8);
    frame->data[FAULT_STATUS_BYTE_TS_LOW] = (uint8_t)(timestamp_sec & 0xFF);
    
    /* Byte 6: Version and counter */
    frame->data[FAULT_STATUS_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(
        CAN_SCHEMA_VERSION, mut_state->tx_counter_fault);
    mut_state->tx_counter_fault = (mut_state->tx_counter_fault + 1) & CAN_COUNTER_MASK;
    
    /* Byte 7: Checksum */
    frame->data[FAULT_STATUS_BYTE_CHECKSUM] = can_calculate_checksum(
        frame->data, FAULT_STATUS_DLC - 1);
}

void fault_manager_update(uint32_t current_ms)
{
    /* Future: implement fault aging, auto-clear, etc. */
    (void)current_ms;
}
