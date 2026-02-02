/**
 * @file can_interface.h
 * @brief CAN Bus Interface Abstraction Layer
 *
 * This file provides a hardware-independent interface for CAN
 * communication. Platform-specific implementations should implement
 * these functions.
 */

#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"

/* =============================================================================
 * CAN Message Structure
 * ========================================================================== */

/** CAN message structure */
typedef struct {
    uint32_t id;                /**< CAN identifier (11 or 29 bit) */
    uint8_t data[8];            /**< Message data payload */
    uint8_t dlc;                /**< Data length code (0-8) */
    bool is_extended;           /**< Extended ID flag (29-bit) */
    bool is_remote;             /**< Remote transmission request */
    uint32_t timestamp;         /**< Receive timestamp (ms) */
} can_message_t;

/** CAN bus status */
typedef enum {
    CAN_STATUS_OK = 0,          /**< Bus operating normally */
    CAN_STATUS_ERROR_PASSIVE,   /**< Error passive state */
    CAN_STATUS_BUS_OFF,         /**< Bus off state */
    CAN_STATUS_NOT_INITIALIZED  /**< Not initialized */
} can_status_t;

/** CAN error types */
typedef enum {
    CAN_ERROR_NONE = 0,         /**< No error */
    CAN_ERROR_TX_TIMEOUT,       /**< Transmit timeout */
    CAN_ERROR_TX_FAILED,        /**< Transmit failed */
    CAN_ERROR_RX_OVERFLOW,      /**< Receive buffer overflow */
    CAN_ERROR_BUS_OFF,          /**< Bus off condition */
    CAN_ERROR_STUFF,            /**< Stuff error */
    CAN_ERROR_FORM,             /**< Form error */
    CAN_ERROR_ACK,              /**< Acknowledgment error */
    CAN_ERROR_CRC               /**< CRC error */
} can_error_t;

/* =============================================================================
 * CAN Interface Initialization
 * ========================================================================== */

/**
 * @brief Initialize CAN interface
 * @param baud_rate Desired baud rate in bits/second
 * @return BCM_OK on success
 */
bcm_result_t can_interface_init(uint32_t baud_rate);

/**
 * @brief Deinitialize CAN interface
 */
void can_interface_deinit(void);

/**
 * @brief Reset CAN interface
 *
 * Resets the CAN controller and clears all errors.
 * @return BCM_OK on success
 */
bcm_result_t can_interface_reset(void);

/* =============================================================================
 * CAN Message Transmission
 * ========================================================================== */

/**
 * @brief Transmit a CAN message
 * @param msg Message to transmit
 * @return BCM_OK on success
 */
bcm_result_t can_transmit(const can_message_t *msg);

/**
 * @brief Queue a CAN message for transmission
 * @param msg Message to queue
 * @return BCM_OK if queued successfully
 */
bcm_result_t can_transmit_queued(const can_message_t *msg);

/**
 * @brief Transmit a message with standard ID
 * @param id CAN identifier (11-bit)
 * @param data Data buffer
 * @param len Data length (0-8)
 * @return BCM_OK on success
 */
bcm_result_t can_transmit_std(uint32_t id, const uint8_t *data, uint8_t len);

/**
 * @brief Flush transmit queue
 *
 * Blocks until all queued messages are transmitted or timeout.
 * @param timeout_ms Maximum time to wait
 * @return BCM_OK if queue flushed
 */
bcm_result_t can_flush_tx(uint32_t timeout_ms);

/**
 * @brief Abort pending transmissions
 * @return BCM_OK on success
 */
bcm_result_t can_abort_tx(void);

/* =============================================================================
 * CAN Message Reception
 * ========================================================================== */

/**
 * @brief Check if a message is available
 * @return true if message available
 */
bool can_message_available(void);

/**
 * @brief Receive a CAN message (non-blocking)
 * @param[out] msg Message structure to fill
 * @return BCM_OK if message received, BCM_ERROR if no message
 */
bcm_result_t can_receive(can_message_t *msg);

/**
 * @brief Receive a CAN message (blocking)
 * @param[out] msg Message structure to fill
 * @param timeout_ms Maximum time to wait
 * @return BCM_OK if message received
 */
bcm_result_t can_receive_blocking(can_message_t *msg, uint32_t timeout_ms);

/**
 * @brief Get number of messages in receive queue
 * @return Number of pending messages
 */
uint8_t can_get_rx_queue_count(void);

/* =============================================================================
 * CAN Filtering
 * ========================================================================== */

/**
 * @brief Set acceptance filter
 * @param filter_id Filter identifier
 * @param id CAN ID to accept
 * @param mask Acceptance mask (1 = must match, 0 = don't care)
 * @return BCM_OK on success
 */
bcm_result_t can_set_filter(uint8_t filter_id, uint32_t id, uint32_t mask);

/**
 * @brief Clear acceptance filter
 * @param filter_id Filter to clear
 * @return BCM_OK on success
 */
bcm_result_t can_clear_filter(uint8_t filter_id);

/**
 * @brief Clear all acceptance filters
 * @return BCM_OK on success
 */
bcm_result_t can_clear_all_filters(void);

/* =============================================================================
 * CAN Status and Diagnostics
 * ========================================================================== */

/**
 * @brief Get CAN bus status
 * @return Current bus status
 */
can_status_t can_get_status(void);

/**
 * @brief Get last CAN error
 * @return Last error type
 */
can_error_t can_get_last_error(void);

/**
 * @brief Get transmit error counter
 * @return TX error count (0-255)
 */
uint8_t can_get_tx_error_count(void);

/**
 * @brief Get receive error counter
 * @return RX error count (0-255)
 */
uint8_t can_get_rx_error_count(void);

/**
 * @brief Check if bus is in error passive state
 * @return true if error passive
 */
bool can_is_error_passive(void);

/**
 * @brief Check if bus is off
 * @return true if bus off
 */
bool can_is_bus_off(void);

/**
 * @brief Get CAN statistics
 * @param[out] tx_count Number of transmitted messages
 * @param[out] rx_count Number of received messages
 * @param[out] error_count Number of errors
 */
void can_get_statistics(uint32_t *tx_count, uint32_t *rx_count, 
                        uint32_t *error_count);

/* =============================================================================
 * CAN Callback Registration
 * ========================================================================== */

/** CAN receive callback type */
typedef void (*can_rx_callback_t)(const can_message_t *msg);

/** CAN error callback type */
typedef void (*can_error_callback_t)(can_error_t error);

/**
 * @brief Register receive callback
 * @param callback Function to call on message receipt (NULL to unregister)
 */
void can_register_rx_callback(can_rx_callback_t callback);

/**
 * @brief Register error callback
 * @param callback Function to call on error (NULL to unregister)
 */
void can_register_error_callback(can_error_callback_t callback);

/* =============================================================================
 * CAN Processing
 * ========================================================================== */

/**
 * @brief Process CAN reception (poll mode)
 *
 * Should be called periodically to process received messages.
 * @param timestamp_ms Current timestamp
 * @return BCM_OK on success
 */
bcm_result_t can_process_rx(uint32_t timestamp_ms);

/**
 * @brief Process CAN transmission (poll mode)
 *
 * Should be called periodically to process transmit queue.
 * @param timestamp_ms Current timestamp
 * @return BCM_OK on success
 */
bcm_result_t can_process_tx(uint32_t timestamp_ms);

/* =============================================================================
 * Platform-Specific Functions (to be implemented per target)
 * ========================================================================== */

/**
 * @brief Platform-specific CAN hardware initialization
 * @param baud_rate Desired baud rate
 * @return BCM_OK on success
 */
bcm_result_t can_hal_init(uint32_t baud_rate);

/**
 * @brief Platform-specific CAN hardware transmit
 * @param msg Message to send
 * @return BCM_OK on success
 */
bcm_result_t can_hal_transmit(const can_message_t *msg);

/**
 * @brief Platform-specific CAN hardware receive
 * @param[out] msg Received message
 * @return BCM_OK if message available
 */
bcm_result_t can_hal_receive(can_message_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* CAN_INTERFACE_H */
