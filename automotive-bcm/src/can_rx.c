/**
 * @file can_rx.c
 * @brief CAN Receive Handler Implementation
 *
 * Implements CAN message reception, queuing, and dispatching to
 * appropriate handlers.
 */

#include <string.h>
#include "can_interface.h"
#include "door_control.h"
#include "lighting_control.h"
#include "turn_signal.h"
#include "fault_manager.h"
#include "bcm_config.h"
#include "can_ids.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Receive queue entry */
typedef struct {
    can_message_t msg;
    bool valid;
} rx_queue_entry_t;

/** CAN RX state */
typedef struct {
    rx_queue_entry_t queue[CAN_RX_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t rx_count;
    uint32_t error_count;
    uint32_t overflow_count;
    can_rx_callback_t user_callback;
    uint32_t last_ignition_msg;
    uint32_t last_speed_msg;
} can_rx_state_t;

static can_rx_state_t g_can_rx;

/* =============================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Handle ignition status message
 */
static void handle_ignition_status(const can_message_t *msg)
{
    if (msg->dlc < 1) {
        return;
    }
    
    system_state_t *state = system_state_get_mutable();
    
    uint8_t ign_status = msg->data[0];
    
    switch (ign_status) {
        case IGN_STATUS_OFF:
            state->vehicle.ignition = IGNITION_OFF;
            break;
        case IGN_STATUS_ACC:
            state->vehicle.ignition = IGNITION_ACC;
            break;
        case IGN_STATUS_ON:
            state->vehicle.ignition = IGNITION_ON;
            break;
        case IGN_STATUS_START:
            state->vehicle.ignition = IGNITION_START;
            break;
        default:
            break;
    }
    
    g_can_rx.last_ignition_msg = msg->timestamp;
}

/**
 * @brief Handle vehicle speed message
 */
static void handle_vehicle_speed(const can_message_t *msg)
{
    if (msg->dlc < 2) {
        return;
    }
    
    system_state_t *state = system_state_get_mutable();
    
    /* Speed in km/h (big endian) */
    uint16_t speed = (uint16_t)((uint16_t)msg->data[0] << 8) | msg->data[1];
    state->vehicle.vehicle_speed_kmh = speed;
    
    /* Update door control for auto-lock */
    door_update_vehicle_speed(speed);
    
    g_can_rx.last_speed_msg = msg->timestamp;
}

/**
 * @brief Handle engine status message
 */
static void handle_engine_status(const can_message_t *msg)
{
    if (msg->dlc < 1) {
        return;
    }
    
    system_state_t *state = system_state_get_mutable();
    state->vehicle.engine_running = (msg->data[0] & 0x01) != 0;
}

/**
 * @brief Handle key fob command message
 */
static void handle_keyfob_cmd(const can_message_t *msg)
{
    if (msg->dlc < 1) {
        return;
    }
    
    uint8_t cmd = msg->data[0];
    
    /* Key fob commands */
    switch (cmd) {
        case 0x01:  /* Lock */
            door_lock_all();
            break;
        case 0x02:  /* Unlock */
            door_unlock_all();
            break;
        case 0x03:  /* Trunk release */
            /* Handle trunk release if supported */
            break;
        case 0x04:  /* Panic */
            turn_signal_hazard_on();
            break;
        default:
            break;
    }
}

/**
 * @brief Handle ambient light sensor message
 */
static void handle_ambient_light(const can_message_t *msg)
{
    if (msg->dlc < 2) {
        return;
    }
    
    system_state_t *state = system_state_get_mutable();
    
    /* Light level in lux (big endian) */
    uint16_t lux = (uint16_t)((uint16_t)msg->data[0] << 8) | msg->data[1];
    state->vehicle.ambient_light_lux = lux;
    
    /* Update lighting control */
    lighting_update_ambient_light(lux);
}

/**
 * @brief Handle rain sensor message
 */
static void handle_rain_sensor(const can_message_t *msg)
{
    if (msg->dlc < 1) {
        return;
    }
    
    system_state_t *state = system_state_get_mutable();
    state->vehicle.rain_detected = (msg->data[0] & 0x01) != 0;
}

/**
 * @brief Dispatch received message to appropriate handler
 */
static void dispatch_message(const can_message_t *msg)
{
    switch (msg->id) {
        /* Vehicle status messages */
        case CAN_ID_IGNITION_STATUS:
            handle_ignition_status(msg);
            break;
            
        case CAN_ID_VEHICLE_SPEED:
            handle_vehicle_speed(msg);
            break;
            
        case CAN_ID_ENGINE_STATUS:
            handle_engine_status(msg);
            break;
            
        case CAN_ID_KEYFOB_CMD:
            handle_keyfob_cmd(msg);
            break;
            
        case CAN_ID_AMBIENT_LIGHT:
            handle_ambient_light(msg);
            break;
            
        case CAN_ID_RAIN_SENSOR:
            handle_rain_sensor(msg);
            break;
            
        /* BCM command messages */
        case CAN_ID_DOOR_CMD:
            door_control_handle_can_cmd(msg->data, msg->dlc);
            break;
            
        case CAN_ID_LIGHTING_CMD:
            lighting_control_handle_can_cmd(msg->data, msg->dlc);
            break;
            
        case CAN_ID_TURN_SIGNAL_CMD:
            turn_signal_handle_can_cmd(msg->data, msg->dlc);
            break;
            
        /* Diagnostic messages */
        case CAN_ID_DIAG_REQUEST:
        case CAN_ID_BCM_DIAG_REQUEST:
            /* Handle diagnostic requests */
            break;
            
        default:
            /* Unknown message - ignore */
            break;
    }
}

/* =============================================================================
 * Queue Management (Internal)
 * ========================================================================== */

/**
 * @brief Add message to receive queue
 * @return true if added successfully
 */
static bool queue_push(const can_message_t *msg)
{
    if (g_can_rx.count >= CAN_RX_QUEUE_SIZE) {
        g_can_rx.overflow_count++;
        return false;
    }
    
    g_can_rx.queue[g_can_rx.tail].msg = *msg;
    g_can_rx.queue[g_can_rx.tail].valid = true;
    g_can_rx.tail = (g_can_rx.tail + 1) % CAN_RX_QUEUE_SIZE;
    g_can_rx.count++;
    
    return true;
}

/**
 * @brief Get message from receive queue
 * @return true if message available
 */
static bool queue_pop(can_message_t *msg)
{
    if (g_can_rx.count == 0) {
        return false;
    }
    
    *msg = g_can_rx.queue[g_can_rx.head].msg;
    g_can_rx.queue[g_can_rx.head].valid = false;
    g_can_rx.head = (g_can_rx.head + 1) % CAN_RX_QUEUE_SIZE;
    g_can_rx.count--;
    
    return true;
}

/* =============================================================================
 * Public Interface Implementation
 * ========================================================================== */

bool can_message_available(void)
{
    return g_can_rx.count > 0;
}

bcm_result_t can_receive(can_message_t *msg)
{
    if (!msg) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    if (queue_pop(msg)) {
        return BCM_OK;
    }
    
    return BCM_ERROR;
}

bcm_result_t can_receive_blocking(can_message_t *msg, uint32_t timeout_ms)
{
    (void)timeout_ms;  /* Simplified - no actual blocking */
    return can_receive(msg);
}

uint8_t can_get_rx_queue_count(void)
{
    return g_can_rx.count;
}

void can_register_rx_callback(can_rx_callback_t callback)
{
    g_can_rx.user_callback = callback;
}

bcm_result_t can_process_rx(uint32_t timestamp_ms)
{
    can_message_t msg;
    
    /* Check for message timeout faults */
    if (g_can_rx.last_ignition_msg > 0 &&
        (timestamp_ms - g_can_rx.last_ignition_msg) > CAN_MSG_TIMEOUT_MS * 10) {
        fault_report(FAULT_CAN_RX_TIMEOUT, FAULT_SEVERITY_WARNING);
    }
    
    /* Process any messages from hardware */
    while (can_hal_receive(&msg) == BCM_OK) {
        msg.timestamp = timestamp_ms;
        g_can_rx.rx_count++;
        
        /* Call user callback if registered */
        if (g_can_rx.user_callback) {
            g_can_rx.user_callback(&msg);
        }
        
        /* Queue or process directly */
        if (!queue_push(&msg)) {
            /* Queue full - process immediately */
            dispatch_message(&msg);
        }
    }
    
    /* Process queued messages */
    while (queue_pop(&msg)) {
        dispatch_message(&msg);
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Statistics
 * ========================================================================== */

void can_rx_get_statistics(uint32_t *rx_count, uint32_t *error_count, uint32_t *overflow_count)
{
    if (rx_count) *rx_count = g_can_rx.rx_count;
    if (error_count) *error_count = g_can_rx.error_count;
    if (overflow_count) *overflow_count = g_can_rx.overflow_count;
}

void can_rx_reset_statistics(void)
{
    g_can_rx.rx_count = 0;
    g_can_rx.error_count = 0;
    g_can_rx.overflow_count = 0;
}

/* =============================================================================
 * Initialization (called from can_interface)
 * ========================================================================== */

void can_rx_init(void)
{
    memset(&g_can_rx, 0, sizeof(g_can_rx));
}
