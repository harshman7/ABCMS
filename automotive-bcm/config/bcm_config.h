/**
 * @file bcm_config.h
 * @brief Body Control Module Configuration Parameters
 *
 * This file contains all configurable parameters for the BCM.
 * Modify these values to customize BCM behavior for specific vehicle platforms.
 */

#ifndef BCM_CONFIG_H
#define BCM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =============================================================================
 * System Configuration
 * ========================================================================== */

/** BCM software version */
#define BCM_SW_VERSION_MAJOR        1U
#define BCM_SW_VERSION_MINOR        0U
#define BCM_SW_VERSION_PATCH        0U

/** Main loop cycle time in milliseconds */
#define BCM_MAIN_CYCLE_TIME_MS      10U

/** Watchdog timeout in milliseconds */
#define BCM_WATCHDOG_TIMEOUT_MS     100U

/** Maximum number of faults to store */
#define BCM_MAX_FAULT_COUNT         32U

/* =============================================================================
 * Feature Enables
 * ========================================================================== */

/** Enable automatic headlight control based on ambient light */
#define BCM_FEATURE_AUTO_HEADLIGHTS     1

/** Enable rain-sensing wipers */
#define BCM_FEATURE_RAIN_SENSING        1

/** Enable comfort close (windows close when locking) */
#define BCM_FEATURE_COMFORT_CLOSE       1

/** Enable welcome lights (lights on when approaching) */
#define BCM_FEATURE_WELCOME_LIGHTS      1

/** Enable follow-me-home lights */
#define BCM_FEATURE_FOLLOW_ME_HOME      1

/** Enable one-touch window control */
#define BCM_FEATURE_ONE_TOUCH_WINDOW    1

/** Enable anti-pinch protection for windows */
#define BCM_FEATURE_ANTI_PINCH          1

/* =============================================================================
 * Door Control Configuration
 * ========================================================================== */

/** Number of doors (2 for coupe, 4 for sedan, 5 for hatchback) */
#define DOOR_COUNT                      4U

/** Auto-lock speed threshold in km/h */
#define DOOR_AUTO_LOCK_SPEED_KMH        15U

/** Door lock confirmation horn chirp duration in ms */
#define DOOR_LOCK_CHIRP_DURATION_MS     50U

/** Window anti-pinch force threshold (ADC counts) */
#define DOOR_WINDOW_ANTI_PINCH_THRESH   200U

/** Window motor timeout in milliseconds */
#define DOOR_WINDOW_MOTOR_TIMEOUT_MS    10000U

/** Double-unlock timeout for single-door unlock (ms) */
#define DOOR_DOUBLE_UNLOCK_TIMEOUT_MS   3000U

/* =============================================================================
 * Lighting Control Configuration
 * ========================================================================== */

/** Headlight auto-on ambient light threshold (lux) */
#define LIGHT_AUTO_ON_THRESHOLD_LUX     200U

/** Headlight auto-off ambient light threshold (lux) */
#define LIGHT_AUTO_OFF_THRESHOLD_LUX    400U

/** Follow-me-home lights duration in seconds */
#define LIGHT_FOLLOW_ME_HOME_SEC        30U

/** Interior light fade-out duration in milliseconds */
#define LIGHT_INTERIOR_FADE_MS          2000U

/** Interior light door-open timeout in seconds */
#define LIGHT_INTERIOR_DOOR_TIMEOUT_SEC 30U

/** Welcome light duration in seconds */
#define LIGHT_WELCOME_DURATION_SEC      10U

/** PWM frequency for dimmable lights in Hz */
#define LIGHT_PWM_FREQUENCY_HZ          200U

/** Maximum PWM duty cycle (0-255) */
#define LIGHT_PWM_MAX_DUTY              255U

/* =============================================================================
 * Turn Signal Configuration
 * ========================================================================== */

/** Turn signal flash rate in milliseconds (on time) */
#define TURN_SIGNAL_ON_TIME_MS          500U

/** Turn signal flash rate in milliseconds (off time) */
#define TURN_SIGNAL_OFF_TIME_MS         500U

/** Hazard light flash rate in milliseconds (on time) */
#define HAZARD_ON_TIME_MS               400U

/** Hazard light flash rate in milliseconds (off time) */
#define HAZARD_OFF_TIME_MS              400U

/** Lane change assist blinks count */
#define TURN_LANE_CHANGE_BLINKS         3U

/** Turn signal bulb check current threshold (mA) */
#define TURN_BULB_CHECK_THRESHOLD_MA    100U

/** Fast flash rate for bulb failure (on time ms) */
#define TURN_FAST_FLASH_ON_MS           250U

/** Fast flash rate for bulb failure (off time ms) */
#define TURN_FAST_FLASH_OFF_MS          250U

/* =============================================================================
 * Fault Manager Configuration
 * ========================================================================== */

/** Fault debounce time in milliseconds */
#define FAULT_DEBOUNCE_TIME_MS          100U

/** Fault healing time in milliseconds */
#define FAULT_HEALING_TIME_MS           1000U

/** Maximum fault occurrences before permanent fault */
#define FAULT_MAX_OCCURRENCES           10U

/** Fault log retention count */
#define FAULT_LOG_SIZE                  64U

/** Enable fault recovery attempts */
#define FAULT_ENABLE_RECOVERY           1

/** Maximum recovery attempts per fault */
#define FAULT_MAX_RECOVERY_ATTEMPTS     3U

/* =============================================================================
 * CAN Communication Configuration
 * ========================================================================== */

/** CAN transmit queue size */
#define CAN_TX_QUEUE_SIZE               16U

/** CAN receive queue size */
#define CAN_RX_QUEUE_SIZE               32U

/** CAN message timeout in milliseconds */
#define CAN_MSG_TIMEOUT_MS              100U

/** BCM status broadcast period in milliseconds */
#define CAN_BCM_STATUS_PERIOD_MS        100U

/** BCM heartbeat period in milliseconds */
#define CAN_HEARTBEAT_PERIOD_MS         1000U

/** CAN bus-off recovery time in milliseconds */
#define CAN_BUSOFF_RECOVERY_MS          500U

/* =============================================================================
 * Hardware Pin Assignments (Platform Specific)
 * ========================================================================== */

/* Note: These are example assignments. Modify for your specific MCU. */

/** Door lock output pins */
#define PIN_DOOR_LOCK_FL                0U
#define PIN_DOOR_LOCK_FR                1U
#define PIN_DOOR_LOCK_RL                2U
#define PIN_DOOR_LOCK_RR                3U

/** Door switch input pins */
#define PIN_DOOR_SW_FL                  10U
#define PIN_DOOR_SW_FR                  11U
#define PIN_DOOR_SW_RL                  12U
#define PIN_DOOR_SW_RR                  13U

/** Headlight output pins */
#define PIN_HEADLIGHT_LEFT              20U
#define PIN_HEADLIGHT_RIGHT             21U
#define PIN_HIGH_BEAM_LEFT              22U
#define PIN_HIGH_BEAM_RIGHT             23U

/** Turn signal output pins */
#define PIN_TURN_SIGNAL_FL              30U
#define PIN_TURN_SIGNAL_FR              31U
#define PIN_TURN_SIGNAL_RL              32U
#define PIN_TURN_SIGNAL_RR              33U

/* =============================================================================
 * Timing Configuration
 * ========================================================================== */

/** Timer tick period in microseconds */
#define TIMER_TICK_US                   1000U

/** Debounce time for switch inputs in milliseconds */
#define INPUT_DEBOUNCE_MS               20U

/** Minimum time between same key events (ms) */
#define KEY_REPEAT_DELAY_MS             200U

#ifdef __cplusplus
}
#endif

#endif /* BCM_CONFIG_H */
