/**
 * @file door_control.c
 * @brief Door Control Module Implementation
 *
 * State machine: UNLOCKED <-> LOCKING -> LOCKED <-> UNLOCKING -> UNLOCKED
 */

#include <string.h>
#include <stdio.h>
#include "door_control.h"
#include "fault_manager.h"
#include "can_ids.h"

/*******************************************************************************
 * Private Definitions
 ******************************************************************************/

#define DOOR_TRANSITION_TIME_MS     50U     /**< Time to complete lock/unlock */

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

/**
 * @brief Validate door command frame
 * @return cmd_result_t
 */
static cmd_result_t validate_door_cmd(const can_frame_t *frame)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Check DLC */
    if (frame->dlc != DOOR_CMD_DLC) {
        fault_manager_set(FAULT_CODE_INVALID_LENGTH);
        return CMD_RESULT_INVALID_CMD;
    }
    
    /* Extract fields */
    uint8_t cmd = frame->data[DOOR_CMD_BYTE_CMD];
    uint8_t door_id = frame->data[DOOR_CMD_BYTE_DOOR_ID];
    uint8_t ver_ctr = frame->data[DOOR_CMD_BYTE_VER_CTR];
    uint8_t checksum = frame->data[DOOR_CMD_BYTE_CHECKSUM];
    
    /* Validate checksum */
    uint8_t calc_checksum = can_calculate_checksum(frame->data, DOOR_CMD_DLC - 1);
    if (checksum != calc_checksum) {
        fault_manager_set(FAULT_CODE_INVALID_CHECKSUM);
        return CMD_RESULT_CHECKSUM_ERROR;
    }
    
    /* Validate counter */
    uint8_t counter = CAN_GET_COUNTER(ver_ctr);
    if (state->door.last_cmd_time_ms > 0) {
        if (!can_validate_counter(counter, state->door.last_counter)) {
            fault_manager_set(FAULT_CODE_INVALID_COUNTER);
            return CMD_RESULT_COUNTER_ERROR;
        }
    }
    state->door.last_counter = counter;
    
    /* Validate command value */
    if (cmd == 0 || cmd > DOOR_CMD_MAX) {
        fault_manager_set(FAULT_CODE_INVALID_CMD);
        return CMD_RESULT_INVALID_CMD;
    }
    
    /* Validate door ID for single door commands */
    if ((cmd == DOOR_CMD_LOCK_SINGLE || cmd == DOOR_CMD_UNLOCK_SINGLE) &&
        door_id > DOOR_ID_MAX) {
        fault_manager_set(FAULT_CODE_INVALID_CMD);
        return CMD_RESULT_INVALID_CMD;
    }
    
    return CMD_RESULT_OK;
}

/**
 * @brief Log door state change event
 */
static void log_door_event(uint8_t door_id, door_lock_state_t new_state)
{
    uint8_t data[4] = { door_id, (uint8_t)new_state, 0, 0 };
    event_log_add(EVENT_DOOR_LOCK_CHANGE, data);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

void door_control_init(void)
{
    system_state_t *state = sys_state_get_mut();
    
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        state->door.lock_state[i] = DOOR_STATE_UNLOCKED;
        state->door.is_open[i] = false;
    }
    
    state->door.last_cmd_time_ms = 0;
    state->door.last_counter = 0;
    state->door.last_result = CMD_RESULT_OK;
    
    printf("[DOOR] Initialized\n");
}

cmd_result_t door_control_handle_cmd(const can_frame_t *frame)
{
    if (frame == NULL || frame->id != CAN_ID_DOOR_CMD) {
        return CMD_RESULT_INVALID_CMD;
    }
    
    system_state_t *state = sys_state_get_mut();
    
    /* Validate frame */
    cmd_result_t result = validate_door_cmd(frame);
    if (result != CMD_RESULT_OK) {
        state->door.last_result = result;
        
        uint8_t data[4] = { (uint8_t)result, frame->data[0], 0, 0 };
        event_log_add(EVENT_CMD_ERROR, data);
        
        printf("[DOOR] Command error: %d\n", result);
        return result;
    }
    
    /* Process command */
    uint8_t cmd = frame->data[DOOR_CMD_BYTE_CMD];
    uint8_t door_id = frame->data[DOOR_CMD_BYTE_DOOR_ID];
    
    state->door.last_cmd_time_ms = state->uptime_ms;
    
    switch (cmd) {
        case DOOR_CMD_LOCK_ALL:
            door_control_lock_all();
            break;
            
        case DOOR_CMD_UNLOCK_ALL:
            door_control_unlock_all();
            break;
            
        case DOOR_CMD_LOCK_SINGLE:
            door_control_lock(door_id);
            break;
            
        case DOOR_CMD_UNLOCK_SINGLE:
            door_control_unlock(door_id);
            break;
            
        default:
            result = CMD_RESULT_INVALID_CMD;
            break;
    }
    
    state->door.last_result = result;
    
    uint8_t data[4] = { cmd, door_id, 0, 0 };
    event_log_add(EVENT_CMD_RECEIVED, data);
    
    return result;
}

void door_control_update(uint32_t current_ms)
{
    system_state_t *state = sys_state_get_mut();
    
    /* Process state transitions */
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        switch (state->door.lock_state[i]) {
            case DOOR_STATE_LOCKING:
                /* Transition to LOCKED after delay */
                state->door.lock_state[i] = DOOR_STATE_LOCKED;
                log_door_event(i, DOOR_STATE_LOCKED);
                printf("[DOOR] Door %d: LOCKED\n", i);
                break;
                
            case DOOR_STATE_UNLOCKING:
                /* Transition to UNLOCKED after delay */
                state->door.lock_state[i] = DOOR_STATE_UNLOCKED;
                log_door_event(i, DOOR_STATE_UNLOCKED);
                printf("[DOOR] Door %d: UNLOCKED\n", i);
                break;
                
            default:
                /* No transition needed */
                break;
        }
    }
    
    (void)current_ms;
}

void door_control_build_status_frame(can_frame_t *frame)
{
    const system_state_t *state = sys_state_get();
    system_state_t *mut_state = sys_state_get_mut();
    
    memset(frame, 0, sizeof(can_frame_t));
    frame->id = CAN_ID_DOOR_STATUS;
    frame->dlc = DOOR_STATUS_DLC;
    
    /* Byte 0: Lock states */
    uint8_t locks = 0;
    if (state->door.lock_state[0] == DOOR_STATE_LOCKED) locks |= DOOR_LOCK_BIT_FL;
    if (state->door.lock_state[1] == DOOR_STATE_LOCKED) locks |= DOOR_LOCK_BIT_FR;
    if (state->door.lock_state[2] == DOOR_STATE_LOCKED) locks |= DOOR_LOCK_BIT_RL;
    if (state->door.lock_state[3] == DOOR_STATE_LOCKED) locks |= DOOR_LOCK_BIT_RR;
    frame->data[DOOR_STATUS_BYTE_LOCKS] = locks;
    
    /* Byte 1: Open states */
    uint8_t opens = 0;
    if (state->door.is_open[0]) opens |= DOOR_OPEN_BIT_FL;
    if (state->door.is_open[1]) opens |= DOOR_OPEN_BIT_FR;
    if (state->door.is_open[2]) opens |= DOOR_OPEN_BIT_RL;
    if (state->door.is_open[3]) opens |= DOOR_OPEN_BIT_RR;
    frame->data[DOOR_STATUS_BYTE_OPENS] = opens;
    
    /* Byte 2: Last command result */
    frame->data[DOOR_STATUS_BYTE_RESULT] = (uint8_t)state->door.last_result;
    
    /* Byte 3: Active fault count */
    frame->data[DOOR_STATUS_BYTE_FAULTS] = fault_manager_get_count();
    
    /* Byte 4: Version and counter */
    frame->data[DOOR_STATUS_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(
        CAN_SCHEMA_VERSION, mut_state->tx_counter_door);
    mut_state->tx_counter_door = (mut_state->tx_counter_door + 1) & CAN_COUNTER_MASK;
    
    /* Byte 5: Checksum */
    frame->data[DOOR_STATUS_BYTE_CHECKSUM] = can_calculate_checksum(
        frame->data, DOOR_STATUS_DLC - 1);
}

door_lock_state_t door_control_get_lock_state(uint8_t door_id)
{
    if (door_id >= NUM_DOORS) {
        return DOOR_STATE_UNLOCKED;
    }
    return sys_state_get()->door.lock_state[door_id];
}

bool door_control_all_locked(void)
{
    const system_state_t *state = sys_state_get();
    
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        if (state->door.lock_state[i] != DOOR_STATE_LOCKED) {
            return false;
        }
    }
    return true;
}

bool door_control_any_open(void)
{
    const system_state_t *state = sys_state_get();
    
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        if (state->door.is_open[i]) {
            return true;
        }
    }
    return false;
}

void door_control_lock_all(void)
{
    system_state_t *state = sys_state_get_mut();
    
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        if (state->door.lock_state[i] == DOOR_STATE_UNLOCKED) {
            state->door.lock_state[i] = DOOR_STATE_LOCKING;
            printf("[DOOR] Door %d: LOCKING\n", i);
        }
    }
}

void door_control_unlock_all(void)
{
    system_state_t *state = sys_state_get_mut();
    
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        if (state->door.lock_state[i] == DOOR_STATE_LOCKED) {
            state->door.lock_state[i] = DOOR_STATE_UNLOCKING;
            printf("[DOOR] Door %d: UNLOCKING\n", i);
        }
    }
}

void door_control_lock(uint8_t door_id)
{
    if (door_id >= NUM_DOORS) {
        return;
    }
    
    system_state_t *state = sys_state_get_mut();
    
    if (state->door.lock_state[door_id] == DOOR_STATE_UNLOCKED) {
        state->door.lock_state[door_id] = DOOR_STATE_LOCKING;
        printf("[DOOR] Door %d: LOCKING\n", door_id);
    }
}

void door_control_unlock(uint8_t door_id)
{
    if (door_id >= NUM_DOORS) {
        return;
    }
    
    system_state_t *state = sys_state_get_mut();
    
    if (state->door.lock_state[door_id] == DOOR_STATE_LOCKED) {
        state->door.lock_state[door_id] = DOOR_STATE_UNLOCKING;
        printf("[DOOR] Door %d: UNLOCKING\n", door_id);
    }
}
