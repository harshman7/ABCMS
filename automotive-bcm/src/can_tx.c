/**
 * @file can_tx.c
 * @brief CAN Transmit Handler Implementation
 *
 * Implements CAN message transmission, queuing, and the CAN
 * interface abstraction layer.
 */

#include <string.h>
#include <stdio.h>
#include "can_interface.h"
#include "fault_manager.h"
#include "bcm_config.h"
#include "can_ids.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Transmit queue entry */
typedef struct {
    can_message_t msg;
    bool valid;
    uint8_t retry_count;
} tx_queue_entry_t;

/** CAN TX state */
typedef struct {
    bool initialized;
    uint32_t baud_rate;
    tx_queue_entry_t queue[CAN_TX_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t tx_count;
    uint32_t error_count;
    uint8_t tx_error_counter;
    uint8_t rx_error_counter;
    can_status_t status;
    can_error_t last_error;
    can_error_callback_t error_callback;
    uint32_t last_busoff_time;
} can_tx_state_t;

static can_tx_state_t g_can_tx;

/* Forward declaration from can_rx.c */
extern void can_rx_init(void);

/* =============================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Add message to transmit queue
 */
static bool tx_queue_push(const can_message_t *msg)
{
    if (g_can_tx.count >= CAN_TX_QUEUE_SIZE) {
        return false;
    }
    
    g_can_tx.queue[g_can_tx.tail].msg = *msg;
    g_can_tx.queue[g_can_tx.tail].valid = true;
    g_can_tx.queue[g_can_tx.tail].retry_count = 0;
    g_can_tx.tail = (g_can_tx.tail + 1) % CAN_TX_QUEUE_SIZE;
    g_can_tx.count++;
    
    return true;
}

/**
 * @brief Get message from transmit queue
 */
static bool tx_queue_pop(can_message_t *msg)
{
    if (g_can_tx.count == 0) {
        return false;
    }
    
    *msg = g_can_tx.queue[g_can_tx.head].msg;
    g_can_tx.queue[g_can_tx.head].valid = false;
    g_can_tx.head = (g_can_tx.head + 1) % CAN_TX_QUEUE_SIZE;
    g_can_tx.count--;
    
    return true;
}

/**
 * @brief Peek at front of queue without removing
 */
static tx_queue_entry_t* tx_queue_peek(void)
{
    if (g_can_tx.count == 0) {
        return NULL;
    }
    return &g_can_tx.queue[g_can_tx.head];
}

/* =============================================================================
 * CAN Interface Implementation
 * ========================================================================== */

bcm_result_t can_interface_init(uint32_t baud_rate)
{
    memset(&g_can_tx, 0, sizeof(g_can_tx));
    
    g_can_tx.baud_rate = baud_rate;
    g_can_tx.status = CAN_STATUS_OK;
    g_can_tx.last_error = CAN_ERROR_NONE;
    
    /* Initialize receive side */
    can_rx_init();
    
    /* Initialize hardware */
    bcm_result_t result = can_hal_init(baud_rate);
    if (result != BCM_OK) {
        g_can_tx.status = CAN_STATUS_NOT_INITIALIZED;
        return result;
    }
    
    g_can_tx.initialized = true;
    return BCM_OK;
}

void can_interface_deinit(void)
{
    g_can_tx.initialized = false;
    g_can_tx.status = CAN_STATUS_NOT_INITIALIZED;
}

bcm_result_t can_interface_reset(void)
{
    if (!g_can_tx.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Clear queues */
    g_can_tx.head = 0;
    g_can_tx.tail = 0;
    g_can_tx.count = 0;
    
    /* Reset error counters */
    g_can_tx.tx_error_counter = 0;
    g_can_tx.rx_error_counter = 0;
    g_can_tx.last_error = CAN_ERROR_NONE;
    g_can_tx.status = CAN_STATUS_OK;
    
    /* Reinitialize hardware */
    return can_hal_init(g_can_tx.baud_rate);
}

/* =============================================================================
 * Transmission Functions
 * ========================================================================== */

bcm_result_t can_transmit(const can_message_t *msg)
{
    if (!msg) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_tx.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    if (g_can_tx.status == CAN_STATUS_BUS_OFF) {
        return BCM_ERROR_COMM;
    }
    
    /* Attempt direct transmission */
    bcm_result_t result = can_hal_transmit(msg);
    
    if (result == BCM_OK) {
        g_can_tx.tx_count++;
    } else {
        g_can_tx.error_count++;
        g_can_tx.tx_error_counter++;
        
        /* Check for error passive */
        if (g_can_tx.tx_error_counter >= 128) {
            g_can_tx.status = CAN_STATUS_ERROR_PASSIVE;
        }
        
        /* Check for bus off */
        if (g_can_tx.tx_error_counter >= 255) {
            g_can_tx.status = CAN_STATUS_BUS_OFF;
            g_can_tx.last_busoff_time = system_state_get()->uptime_ms;
            fault_report(FAULT_CAN_BUS_OFF, FAULT_SEVERITY_ERROR);
        }
    }
    
    return result;
}

bcm_result_t can_transmit_queued(const can_message_t *msg)
{
    if (!msg) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    if (!g_can_tx.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    if (!tx_queue_push(msg)) {
        return BCM_ERROR_BUFFER_FULL;
    }
    
    return BCM_OK;
}

bcm_result_t can_transmit_std(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (!data && len > 0) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    if (len > CAN_MAX_DATA_LEN) {
        len = CAN_MAX_DATA_LEN;
    }
    
    can_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.id = id;
    msg.dlc = len;
    msg.is_extended = false;
    msg.is_remote = false;
    
    if (data && len > 0) {
        memcpy(msg.data, data, len);
    }
    
    return can_transmit_queued(&msg);
}

bcm_result_t can_flush_tx(uint32_t timeout_ms)
{
    uint32_t start = system_state_get()->uptime_ms;
    
    while (g_can_tx.count > 0) {
        can_process_tx(system_state_get()->uptime_ms);
        
        if ((system_state_get()->uptime_ms - start) >= timeout_ms) {
            return BCM_ERROR_TIMEOUT;
        }
    }
    
    return BCM_OK;
}

bcm_result_t can_abort_tx(void)
{
    /* Clear the queue */
    g_can_tx.head = 0;
    g_can_tx.tail = 0;
    g_can_tx.count = 0;
    
    return BCM_OK;
}

/* =============================================================================
 * Status Functions
 * ========================================================================== */

can_status_t can_get_status(void)
{
    return g_can_tx.status;
}

can_error_t can_get_last_error(void)
{
    return g_can_tx.last_error;
}

uint8_t can_get_tx_error_count(void)
{
    return g_can_tx.tx_error_counter;
}

uint8_t can_get_rx_error_count(void)
{
    return g_can_tx.rx_error_counter;
}

bool can_is_error_passive(void)
{
    return g_can_tx.status == CAN_STATUS_ERROR_PASSIVE;
}

bool can_is_bus_off(void)
{
    return g_can_tx.status == CAN_STATUS_BUS_OFF;
}

void can_get_statistics(uint32_t *tx_count, uint32_t *rx_count, uint32_t *error_count)
{
    if (tx_count) *tx_count = g_can_tx.tx_count;
    if (rx_count) *rx_count = 0;  /* Retrieved from RX module */
    if (error_count) *error_count = g_can_tx.error_count;
}

/* =============================================================================
 * Callback Registration
 * ========================================================================== */

void can_register_error_callback(can_error_callback_t callback)
{
    g_can_tx.error_callback = callback;
}

/* =============================================================================
 * Processing
 * ========================================================================== */

bcm_result_t can_process_tx(uint32_t timestamp_ms)
{
    if (!g_can_tx.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Handle bus-off recovery */
    if (g_can_tx.status == CAN_STATUS_BUS_OFF) {
        if ((timestamp_ms - g_can_tx.last_busoff_time) >= CAN_BUSOFF_RECOVERY_MS) {
            /* Attempt recovery */
            can_interface_reset();
        }
        return BCM_ERROR_COMM;
    }
    
    /* Process transmit queue */
    tx_queue_entry_t *entry;
    while ((entry = tx_queue_peek()) != NULL) {
        bcm_result_t result = can_hal_transmit(&entry->msg);
        
        if (result == BCM_OK) {
            can_message_t msg;
            tx_queue_pop(&msg);
            g_can_tx.tx_count++;
        } else if (result == BCM_ERROR_BUSY) {
            /* Hardware busy - try again later */
            break;
        } else {
            /* Transmission error */
            entry->retry_count++;
            g_can_tx.error_count++;
            
            if (entry->retry_count >= 3) {
                /* Give up on this message */
                can_message_t msg;
                tx_queue_pop(&msg);
                
                g_can_tx.last_error = CAN_ERROR_TX_FAILED;
                if (g_can_tx.error_callback) {
                    g_can_tx.error_callback(CAN_ERROR_TX_FAILED);
                }
            }
            break;
        }
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Filter Configuration
 * ========================================================================== */

bcm_result_t can_set_filter(uint8_t filter_id, uint32_t id, uint32_t mask)
{
    (void)filter_id;
    (void)id;
    (void)mask;
    /* Platform-specific implementation */
    return BCM_OK;
}

bcm_result_t can_clear_filter(uint8_t filter_id)
{
    (void)filter_id;
    return BCM_OK;
}

bcm_result_t can_clear_all_filters(void)
{
    return BCM_OK;
}

/* =============================================================================
 * HAL Stub Implementation (Simulation)
 *
 * In a real embedded system, these would be replaced with actual
 * hardware driver calls.
 * ========================================================================== */

bcm_result_t can_hal_init(uint32_t baud_rate)
{
    (void)baud_rate;
    /* Simulation: Always succeeds */
    printf("[CAN HAL] Initialized at %u bps\n", baud_rate);
    return BCM_OK;
}

bcm_result_t can_hal_transmit(const can_message_t *msg)
{
    if (!msg) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    /* Simulation: Print message and succeed */
    #ifdef DEBUG
    printf("[CAN TX] ID=0x%03X DLC=%d Data=", msg->id, msg->dlc);
    for (int i = 0; i < msg->dlc; i++) {
        printf("%02X ", msg->data[i]);
    }
    printf("\n");
    #else
    (void)msg;
    #endif
    
    return BCM_OK;
}

bcm_result_t can_hal_receive(can_message_t *msg)
{
    (void)msg;
    /* Simulation: No messages available */
    return BCM_ERROR;
}
