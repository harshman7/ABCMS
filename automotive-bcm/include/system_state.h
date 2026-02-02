/**
 * @file system_state.h
 * @brief System State Definitions and Types
 *
 * This file contains common type definitions, enumerations, and state
 * structures used throughout the BCM system.
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* =============================================================================
 * Common Type Definitions
 * ========================================================================== */

/** Standard result type for BCM operations */
typedef enum {
    BCM_OK = 0,             /**< Operation successful */
    BCM_ERROR,              /**< General error */
    BCM_ERROR_INVALID_PARAM,/**< Invalid parameter */
    BCM_ERROR_TIMEOUT,      /**< Operation timed out */
    BCM_ERROR_BUSY,         /**< Resource busy */
    BCM_ERROR_NOT_READY,    /**< System not ready */
    BCM_ERROR_HW_FAULT,     /**< Hardware fault detected */
    BCM_ERROR_COMM,         /**< Communication error */
    BCM_ERROR_BUFFER_FULL,  /**< Buffer full */
    BCM_ERROR_NOT_SUPPORTED /**< Feature not supported */
} bcm_result_t;

/* =============================================================================
 * System State Enumerations
 * ========================================================================== */

/** BCM operational state */
typedef enum {
    BCM_STATE_INIT = 0,     /**< Initialization state */
    BCM_STATE_NORMAL,       /**< Normal operation */
    BCM_STATE_SLEEP,        /**< Low power sleep mode */
    BCM_STATE_WAKEUP,       /**< Waking up from sleep */
    BCM_STATE_FAULT,        /**< Fault state */
    BCM_STATE_DIAGNOSTIC    /**< Diagnostic mode */
} bcm_state_t;

/** Ignition state */
typedef enum {
    IGNITION_OFF = 0,       /**< Ignition off */
    IGNITION_ACC,           /**< Accessory mode */
    IGNITION_ON,            /**< Ignition on (run) */
    IGNITION_START          /**< Engine cranking */
} ignition_state_t;

/** Door position enumeration */
typedef enum {
    DOOR_FRONT_LEFT = 0,    /**< Front left (driver) door */
    DOOR_FRONT_RIGHT,       /**< Front right (passenger) door */
    DOOR_REAR_LEFT,         /**< Rear left door */
    DOOR_REAR_RIGHT,        /**< Rear right door */
    DOOR_TRUNK,             /**< Trunk/tailgate */
    DOOR_HOOD,              /**< Hood */
    DOOR_COUNT_MAX          /**< Maximum door count */
} door_position_t;

/** Lock state */
typedef enum {
    LOCK_STATE_UNKNOWN = 0, /**< Lock state unknown */
    LOCK_STATE_UNLOCKED,    /**< Door unlocked */
    LOCK_STATE_LOCKED,      /**< Door locked */
    LOCK_STATE_MOVING       /**< Lock actuator moving */
} lock_state_t;

/** Window state */
typedef enum {
    WINDOW_STATE_UNKNOWN = 0,   /**< Window position unknown */
    WINDOW_STATE_CLOSED,        /**< Window fully closed */
    WINDOW_STATE_OPEN,          /**< Window fully open */
    WINDOW_STATE_PARTIAL,       /**< Window partially open */
    WINDOW_STATE_MOVING_UP,     /**< Window moving up */
    WINDOW_STATE_MOVING_DOWN,   /**< Window moving down */
    WINDOW_STATE_BLOCKED        /**< Window blocked (anti-pinch) */
} window_state_t;

/** Light state */
typedef enum {
    LIGHT_OFF = 0,          /**< Light is off */
    LIGHT_ON,               /**< Light is on (full brightness) */
    LIGHT_DIMMED,           /**< Light is dimmed */
    LIGHT_FLASHING,         /**< Light is flashing */
    LIGHT_FAULT             /**< Light fault detected */
} light_state_t;

/** Turn signal state */
typedef enum {
    TURN_SIGNAL_OFF = 0,    /**< Turn signal off */
    TURN_SIGNAL_LEFT,       /**< Left turn signal active */
    TURN_SIGNAL_RIGHT,      /**< Right turn signal active */
    TURN_SIGNAL_HAZARD      /**< Hazard lights active */
} turn_signal_state_t;

/* =============================================================================
 * State Structures
 * ========================================================================== */

/** Door status structure */
typedef struct {
    lock_state_t lock_state;        /**< Current lock state */
    window_state_t window_state;    /**< Current window state */
    uint8_t window_position;        /**< Window position 0-100% */
    bool is_open;                   /**< Door open sensor */
    bool child_lock_active;         /**< Child lock engaged */
} door_status_t;

/** Lighting status structure */
typedef struct {
    light_state_t headlights;       /**< Headlight state */
    light_state_t high_beam;        /**< High beam state */
    light_state_t fog_lights;       /**< Fog light state */
    light_state_t tail_lights;      /**< Tail light state */
    light_state_t interior;         /**< Interior light state */
    uint8_t interior_brightness;    /**< Interior brightness 0-255 */
    bool auto_mode_active;          /**< Auto headlight mode */
} lighting_status_t;

/** Turn signal status structure */
typedef struct {
    turn_signal_state_t state;      /**< Current turn signal state */
    bool left_bulb_ok;              /**< Left bulb status */
    bool right_bulb_ok;             /**< Right bulb status */
    uint8_t blink_count;            /**< Current blink count */
    bool output_state;              /**< Current output (on/off) */
} turn_signal_status_t;

/** Vehicle state structure (from CAN) */
typedef struct {
    ignition_state_t ignition;      /**< Current ignition state */
    uint16_t vehicle_speed_kmh;     /**< Vehicle speed in km/h */
    bool engine_running;            /**< Engine running status */
    uint16_t ambient_light_lux;     /**< Ambient light in lux */
    bool rain_detected;             /**< Rain sensor status */
} vehicle_state_t;

/** Complete BCM state structure */
typedef struct {
    bcm_state_t bcm_state;          /**< BCM operational state */
    vehicle_state_t vehicle;        /**< Vehicle state from CAN */
    door_status_t doors[DOOR_COUNT_MAX]; /**< Door status array */
    lighting_status_t lighting;     /**< Lighting status */
    turn_signal_status_t turn_signal; /**< Turn signal status */
    uint32_t uptime_ms;             /**< System uptime in ms */
    uint16_t active_faults;         /**< Number of active faults */
} system_state_t;

/* =============================================================================
 * Global State Access
 * ========================================================================== */

/**
 * @brief Get pointer to the global system state
 * @return Pointer to system state structure
 */
const system_state_t* system_state_get(void);

/**
 * @brief Get modifiable pointer to system state (internal use)
 * @return Pointer to system state structure
 */
system_state_t* system_state_get_mutable(void);

/**
 * @brief Initialize system state to defaults
 */
void system_state_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_STATE_H */
