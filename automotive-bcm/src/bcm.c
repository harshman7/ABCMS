/**
 * @file bcm.c
 * @brief BCM Core Implementation
 *
 * Message-driven architecture with periodic task scheduling.
 */

#include <string.h>
#include <stdio.h>
#include "bcm.h"
#include "door_control.h"
#include "lighting_control.h"
#include "turn_signal.h"
#include "fault_manager.h"
#include "can_ids.h"

/*******************************************************************************
 * Version
 ******************************************************************************/

#define BCM_VERSION_STRING  "1.0.0"

/*******************************************************************************
 * Private Data
 ******************************************************************************/

static bool g_initialized = false;

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

/**
 * @brief Route received CAN frame to appropriate handler
 */
static void route_can_frame(const can_frame_t *frame)
{
    cmd_result_t result;
    
    switch (frame->id) {
        case CAN_ID_DOOR_CMD:
            result = door_control_handle_cmd(frame);
            (void)result;
            break;
            
        case CAN_ID_LIGHTING_CMD:
            result = lighting_control_handle_cmd(frame);
            (void)result;
            break;
            
        case CAN_ID_TURN_SIGNAL_CMD:
            result = turn_signal_handle_cmd(frame);
            (void)result;
            break;
            
        default:
            /* Unknown message ID - ignore or log */
            break;
    }
}

/**
 * @brief Transmit all status frames
 */
static void transmit_status_frames(void)
{
    can_frame_t frame;
    
    /* Door status */
    door_control_build_status_frame(&frame);
    can_send(&frame);
    
    /* Lighting status */
    lighting_control_build_status_frame(&frame);
    can_send(&frame);
    
    /* Turn signal status */
    turn_signal_build_status_frame(&frame);
    can_send(&frame);
}

/**
 * @brief Transmit heartbeat frame
 */
static void transmit_heartbeat(void)
{
    can_frame_t frame;
    const system_state_t *state = sys_state_get();
    system_state_t *mut_state = sys_state_get_mut();
    
    memset(&frame, 0, sizeof(frame));
    frame.id = CAN_ID_BCM_HEARTBEAT;
    frame.dlc = BCM_HEARTBEAT_DLC;
    
    /* Byte 0: BCM state */
    frame.data[HEARTBEAT_BYTE_STATE] = (uint8_t)state->bcm_state;
    
    /* Byte 1: Uptime (minutes) */
    frame.data[HEARTBEAT_BYTE_UPTIME] = state->uptime_minutes;
    
    /* Byte 2: Version and counter */
    frame.data[HEARTBEAT_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(
        CAN_SCHEMA_VERSION, mut_state->tx_counter_heartbeat);
    mut_state->tx_counter_heartbeat = (mut_state->tx_counter_heartbeat + 1) & CAN_COUNTER_MASK;
    
    /* Byte 3: Checksum */
    frame.data[HEARTBEAT_BYTE_CHECKSUM] = can_calculate_checksum(
        frame.data, BCM_HEARTBEAT_DLC - 1);
    
    can_send(&frame);
}

/**
 * @brief Transmit fault status (less frequent)
 */
static void transmit_fault_status(void)
{
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    can_send(&frame);
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

int bcm_init(const char *can_ifname)
{
    printf("========================================\n");
    printf("  BCM - Body Control Module\n");
    printf("  Version: %s\n", BCM_VERSION_STRING);
    printf("========================================\n\n");
    
    /* Initialize system state */
    sys_state_init();
    
    /* Initialize CAN interface */
    if (can_init(can_ifname) != CAN_STATUS_OK) {
        printf("[BCM] CAN init failed\n");
        return -1;
    }
    
    /* Initialize modules */
    door_control_init();
    lighting_control_init();
    turn_signal_init();
    fault_manager_init();
    
    /* Set BCM to normal state */
    sys_state_get_mut()->bcm_state = BCM_STATE_NORMAL;
    
    /* Log state change */
    uint8_t data[4] = { BCM_STATE_INIT, BCM_STATE_NORMAL, 0, 0 };
    event_log_add(EVENT_STATE_CHANGE, data);
    
    g_initialized = true;
    printf("[BCM] Initialized successfully\n\n");
    
    return 0;
}

void bcm_deinit(void)
{
    if (!g_initialized) {
        return;
    }
    
    can_deinit();
    g_initialized = false;
    
    printf("[BCM] Deinitialized\n");
}

int bcm_process(uint32_t current_ms)
{
    if (!g_initialized) {
        return -1;
    }
    
    system_state_t *state = sys_state_get_mut();
    
    /* Update system time */
    sys_state_update_time(current_ms);
    
    /* Poll for received CAN frames */
    can_frame_t frame;
    while (can_recv(&frame) == CAN_STATUS_OK) {
        route_can_frame(&frame);
    }
    
    /* 10ms tasks */
    if ((current_ms - state->last_10ms_tick) >= 10) {
        state->last_10ms_tick = current_ms;
        bcm_process_10ms(current_ms);
    }
    
    /* 100ms tasks */
    if ((current_ms - state->last_100ms_tick) >= 100) {
        state->last_100ms_tick = current_ms;
        bcm_process_100ms(current_ms);
    }
    
    /* 1000ms tasks */
    if ((current_ms - state->last_1000ms_tick) >= 1000) {
        state->last_1000ms_tick = current_ms;
        bcm_process_1000ms(current_ms);
    }
    
    return 0;
}

void bcm_process_10ms(uint32_t current_ms)
{
    /* Fast state machine updates */
    door_control_update(current_ms);
    lighting_control_update(current_ms);
    turn_signal_update(current_ms);
}

void bcm_process_100ms(uint32_t current_ms)
{
    /* Transmit status frames */
    transmit_status_frames();
    
    /* Fault manager update */
    fault_manager_update(current_ms);
}

void bcm_process_1000ms(uint32_t current_ms)
{
    /* Transmit heartbeat */
    transmit_heartbeat();
    
    /* Transmit fault status (every 500ms, but we do it here for simplicity) */
    transmit_fault_status();
    
    /* Check timeouts */
    turn_signal_check_timeout(current_ms);
    
    /* Check BCM state based on faults */
    system_state_t *state = sys_state_get_mut();
    if (fault_manager_get_count() > 0 && state->bcm_state == BCM_STATE_NORMAL) {
        /* Could transition to FAULT state if critical faults present */
    }
}

bcm_state_t bcm_get_state(void)
{
    return sys_state_get()->bcm_state;
}

const char* bcm_get_version(void)
{
    return BCM_VERSION_STRING;
}

uint32_t bcm_get_uptime_ms(void)
{
    return sys_state_get()->uptime_ms;
}
