/**
 * @file can_interface.h
 * @brief CAN Interface Abstraction Layer
 *
 * Provides a platform-independent CAN interface.
 * - BCM_SIL=1: Uses Linux SocketCAN (vcan/can)
 * - BCM_SIL=0 or undefined: Uses stub in-memory queue
 */

#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*******************************************************************************
 * CAN Frame Structure
 ******************************************************************************/

#define CAN_FRAME_MAX_DLC   8U

typedef struct {
    uint32_t    id;                     /**< 11-bit standard CAN ID */
    uint8_t     dlc;                    /**< Data length code (0-8) */
    uint8_t     data[CAN_FRAME_MAX_DLC];/**< Frame payload */
} can_frame_t;

/*******************************************************************************
 * CAN Interface Status
 ******************************************************************************/

typedef enum {
    CAN_STATUS_OK = 0,
    CAN_STATUS_ERROR,
    CAN_STATUS_NO_DATA,
    CAN_STATUS_BUFFER_FULL,
    CAN_STATUS_NOT_INITIALIZED
} can_status_t;

/*******************************************************************************
 * CAN Interface Functions
 ******************************************************************************/

/**
 * @brief Initialize CAN interface
 * @param ifname Interface name (e.g., "vcan0" for SocketCAN, ignored for stub)
 * @return CAN_STATUS_OK on success
 */
can_status_t can_init(const char *ifname);

/**
 * @brief Deinitialize CAN interface
 */
void can_deinit(void);

/**
 * @brief Check if CAN interface is initialized
 * @return true if initialized
 */
bool can_is_initialized(void);

/**
 * @brief Send a CAN frame
 * @param frame Frame to send
 * @return CAN_STATUS_OK on success
 */
can_status_t can_send(const can_frame_t *frame);

/**
 * @brief Receive a CAN frame (non-blocking)
 * @param frame Output frame buffer
 * @return CAN_STATUS_OK if frame received, CAN_STATUS_NO_DATA if no frame available
 */
can_status_t can_recv(can_frame_t *frame);

/**
 * @brief Poll for received frames (called in main loop)
 * @return CAN_STATUS_OK if frame available, CAN_STATUS_NO_DATA otherwise
 */
can_status_t can_rx_poll(void);

/*******************************************************************************
 * Stub Interface Functions (for testing without SocketCAN)
 ******************************************************************************/

#ifndef BCM_SIL

/**
 * @brief Inject a frame into the RX queue (for testing)
 * @param frame Frame to inject
 * @return CAN_STATUS_OK on success
 */
can_status_t can_stub_inject_rx(const can_frame_t *frame);

/**
 * @brief Get last transmitted frame (for testing)
 * @param frame Output frame buffer
 * @return CAN_STATUS_OK if frame available
 */
can_status_t can_stub_get_last_tx(can_frame_t *frame);

/**
 * @brief Clear all queues (for testing)
 */
void can_stub_clear(void);

#endif /* !BCM_SIL */

/*******************************************************************************
 * Debug/Statistics
 ******************************************************************************/

typedef struct {
    uint32_t    tx_count;
    uint32_t    rx_count;
    uint32_t    tx_errors;
    uint32_t    rx_errors;
} can_stats_t;

/**
 * @brief Get CAN statistics
 * @param stats Output statistics structure
 */
void can_get_stats(can_stats_t *stats);

/**
 * @brief Reset CAN statistics
 */
void can_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_INTERFACE_H */
