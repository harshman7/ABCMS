/**
 * @file system_state.h
 * @brief System State and Common Types
 *
 * Central state repository for all BCM modules.
 * No dynamic allocation - all state is statically allocated.
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "can_ids.h"

/*******************************************************************************
 * Configuration
 ******************************************************************************/

#define NUM_DOORS                   4U
#define EVENT_LOG_SIZE              32U     /**< Ring buffer size for events */
#define CMD_TIMEOUT_MS              5000U   /**< Command timeout in ms */
#define TURN_SIGNAL_TIMEOUT_MS      30000U  /**< Turn signal auto-off timeout */

/*******************************************************************************
 * Event Log Entry
 ******************************************************************************/

typedef enum {
    EVENT_NONE = 0,
    EVENT_DOOR_LOCK_CHANGE,
    EVENT_DOOR_OPEN_CHANGE,
    EVENT_HEADLIGHT_CHANGE,
    EVENT_INTERIOR_CHANGE,
    EVENT_TURN_SIGNAL_CHANGE,
    EVENT_FAULT_SET,
    EVENT_FAULT_CLEAR,
    EVENT_CMD_RECEIVED,
    EVENT_CMD_ERROR,
    EVENT_STATE_CHANGE
} event_type_t;

typedef struct {
    uint32_t        timestamp_ms;   /**< Event timestamp */
    event_type_t    type;           /**< Event type */
    uint8_t         data[4];        /**< Event-specific data */
} event_log_entry_t;

/*******************************************************************************
 * Door Module State
 ******************************************************************************/

typedef enum {
    DOOR_STATE_UNLOCKED = 0,
    DOOR_STATE_LOCKED,
    DOOR_STATE_LOCKING,     /**< Transition state */
    DOOR_STATE_UNLOCKING    /**< Transition state */
} door_lock_state_t;

typedef struct {
    door_lock_state_t   lock_state[NUM_DOORS];
    bool                is_open[NUM_DOORS];
    uint32_t            last_cmd_time_ms;
    uint8_t             last_counter;
    cmd_result_t        last_result;
} door_state_t;

/*******************************************************************************
 * Lighting Module State
 ******************************************************************************/

typedef enum {
    LIGHTING_STATE_OFF = 0,
    LIGHTING_STATE_ON,
    LIGHTING_STATE_AUTO
} lighting_mode_state_t;

typedef struct {
    lighting_mode_state_t   headlight_mode;
    headlight_state_t       headlight_output;   /**< Actual output state */
    bool                    high_beam_active;
    lighting_mode_state_t   interior_mode;
    uint8_t                 interior_brightness; /**< 0-15 */
    bool                    interior_on;
    uint8_t                 ambient_light;      /**< Scaled 0-255 */
    uint32_t                last_cmd_time_ms;
    uint8_t                 last_counter;
    cmd_result_t            last_result;
} lighting_state_t;

/*******************************************************************************
 * Turn Signal Module State
 ******************************************************************************/

typedef enum {
    TURN_SIG_STATE_OFF = 0,
    TURN_SIG_STATE_LEFT,
    TURN_SIG_STATE_RIGHT,
    TURN_SIG_STATE_HAZARD
} turn_signal_mode_t;

typedef struct {
    turn_signal_mode_t  mode;
    bool                left_output;    /**< Current flash state */
    bool                right_output;   /**< Current flash state */
    uint8_t             flash_count;    /**< Wrapping counter */
    uint32_t            last_toggle_ms; /**< Last flash toggle time */
    uint32_t            last_cmd_time_ms;
    uint8_t             last_counter;
    cmd_result_t        last_result;
} turn_signal_state_t;

/*******************************************************************************
 * Fault Manager State
 ******************************************************************************/

#define MAX_ACTIVE_FAULTS           8U

typedef struct {
    fault_code_t    code;
    uint32_t        timestamp_ms;
} fault_entry_t;

typedef struct {
    uint8_t         flags1;             /**< Active fault flags byte 1 */
    uint8_t         flags2;             /**< Active fault flags byte 2 */
    fault_entry_t   active_faults[MAX_ACTIVE_FAULTS];
    uint8_t         active_count;
    uint8_t         total_count;        /**< Historical count */
    fault_code_t    most_recent_code;
    uint32_t        most_recent_time_ms;
} fault_state_t;

/*******************************************************************************
 * Event Log Ring Buffer
 ******************************************************************************/

typedef struct {
    event_log_entry_t   entries[EVENT_LOG_SIZE];
    uint8_t             head;           /**< Next write position */
    uint8_t             count;          /**< Number of valid entries */
} event_log_t;

/*******************************************************************************
 * Complete System State
 ******************************************************************************/

typedef struct {
    /* BCM Core State */
    bcm_state_t         bcm_state;
    uint32_t            uptime_ms;
    uint8_t             uptime_minutes;     /**< Wrapping minutes counter */
    
    /* Rolling counters for TX messages */
    uint8_t             tx_counter_door;
    uint8_t             tx_counter_lighting;
    uint8_t             tx_counter_turn;
    uint8_t             tx_counter_fault;
    uint8_t             tx_counter_heartbeat;
    
    /* Module States */
    door_state_t        door;
    lighting_state_t    lighting;
    turn_signal_state_t turn_signal;
    fault_state_t       fault;
    
    /* Event Log */
    event_log_t         event_log;
    
    /* Timing for periodic tasks */
    uint32_t            last_10ms_tick;
    uint32_t            last_100ms_tick;
    uint32_t            last_1000ms_tick;
    
} system_state_t;

/*******************************************************************************
 * Global State Access
 ******************************************************************************/

/**
 * @brief Get pointer to global system state (read-only)
 */
const system_state_t* sys_state_get(void);

/**
 * @brief Get mutable pointer to global system state (internal use)
 */
system_state_t* sys_state_get_mut(void);

/**
 * @brief Initialize system state to defaults
 */
void sys_state_init(void);

/**
 * @brief Update uptime based on current tick
 * @param current_ms Current millisecond tick
 */
void sys_state_update_time(uint32_t current_ms);

/*******************************************************************************
 * Event Log Functions
 ******************************************************************************/

/**
 * @brief Log an event
 * @param type Event type
 * @param data Event data (4 bytes, can be NULL)
 */
void event_log_add(event_type_t type, const uint8_t *data);

/**
 * @brief Get event at index (0 = oldest)
 * @param index Index into log
 * @param entry Output entry
 * @return true if valid entry returned
 */
bool event_log_get(uint8_t index, event_log_entry_t *entry);

/**
 * @brief Get number of events in log
 */
uint8_t event_log_count(void);

/**
 * @brief Clear event log
 */
void event_log_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_STATE_H */
