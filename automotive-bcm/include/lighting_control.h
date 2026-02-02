/**
 * @file lighting_control.h
 * @brief Lighting Control Module Interface
 *
 * This module manages all vehicle lighting including headlights,
 * tail lights, fog lights, interior lights, and ambient lighting.
 */

#ifndef LIGHTING_CONTROL_H
#define LIGHTING_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"

/* =============================================================================
 * Lighting Types
 * ========================================================================== */

/** Headlight mode enumeration */
typedef enum {
    HEADLIGHT_MODE_OFF = 0,     /**< Headlights off */
    HEADLIGHT_MODE_PARKING,     /**< Parking lights only */
    HEADLIGHT_MODE_LOW_BEAM,    /**< Low beam on */
    HEADLIGHT_MODE_HIGH_BEAM,   /**< High beam on */
    HEADLIGHT_MODE_AUTO         /**< Automatic mode */
} headlight_mode_t;

/** Interior light mode enumeration */
typedef enum {
    INTERIOR_MODE_OFF = 0,      /**< Interior lights off */
    INTERIOR_MODE_ON,           /**< Interior lights on */
    INTERIOR_MODE_DOOR,         /**< Door-activated mode */
    INTERIOR_MODE_DIMMED        /**< Dimmed for driving */
} interior_mode_t;

/* =============================================================================
 * Lighting Control Initialization
 * ========================================================================== */

/**
 * @brief Initialize lighting control module
 * @return BCM_OK on success
 */
bcm_result_t lighting_control_init(void);

/**
 * @brief Deinitialize lighting control module
 */
void lighting_control_deinit(void);

/* =============================================================================
 * Lighting Control Processing
 * ========================================================================== */

/**
 * @brief Process lighting control state machine
 *
 * Called periodically to update light states, handle timing,
 * and manage automatic features.
 *
 * @param timestamp_ms Current timestamp in milliseconds
 * @return BCM_OK on success
 */
bcm_result_t lighting_control_process(uint32_t timestamp_ms);

/* =============================================================================
 * Headlight Control
 * ========================================================================== */

/**
 * @brief Set headlight mode
 * @param mode Desired headlight mode
 * @return BCM_OK on success
 */
bcm_result_t lighting_set_headlight_mode(headlight_mode_t mode);

/**
 * @brief Get current headlight mode
 * @return Current headlight mode
 */
headlight_mode_t lighting_get_headlight_mode(void);

/**
 * @brief Turn on low beam headlights
 * @return BCM_OK on success
 */
bcm_result_t lighting_headlights_on(void);

/**
 * @brief Turn off headlights
 * @return BCM_OK on success
 */
bcm_result_t lighting_headlights_off(void);

/**
 * @brief Turn on high beam
 * @return BCM_OK on success
 */
bcm_result_t lighting_high_beam_on(void);

/**
 * @brief Turn off high beam
 * @return BCM_OK on success
 */
bcm_result_t lighting_high_beam_off(void);

/**
 * @brief Flash high beam (momentary)
 * @return BCM_OK on success
 */
bcm_result_t lighting_flash_high_beam(void);

/**
 * @brief Check if headlights are on
 * @return true if headlights are on
 */
bool lighting_headlights_active(void);

/**
 * @brief Check if high beam is on
 * @return true if high beam is on
 */
bool lighting_high_beam_active(void);

/* =============================================================================
 * Fog Light Control
 * ========================================================================== */

/**
 * @brief Turn on front fog lights
 * @return BCM_OK on success
 */
bcm_result_t lighting_fog_lights_on(void);

/**
 * @brief Turn off front fog lights
 * @return BCM_OK on success
 */
bcm_result_t lighting_fog_lights_off(void);

/**
 * @brief Turn on rear fog light
 * @return BCM_OK on success
 */
bcm_result_t lighting_rear_fog_on(void);

/**
 * @brief Turn off rear fog light
 * @return BCM_OK on success
 */
bcm_result_t lighting_rear_fog_off(void);

/**
 * @brief Check if fog lights are on
 * @return true if fog lights are on
 */
bool lighting_fog_lights_active(void);

/* =============================================================================
 * Interior Light Control
 * ========================================================================== */

/**
 * @brief Set interior light mode
 * @param mode Desired interior light mode
 * @return BCM_OK on success
 */
bcm_result_t lighting_set_interior_mode(interior_mode_t mode);

/**
 * @brief Get current interior light mode
 * @return Current interior light mode
 */
interior_mode_t lighting_get_interior_mode(void);

/**
 * @brief Turn on interior lights
 * @return BCM_OK on success
 */
bcm_result_t lighting_interior_on(void);

/**
 * @brief Turn off interior lights
 * @return BCM_OK on success
 */
bcm_result_t lighting_interior_off(void);

/**
 * @brief Set interior light brightness
 * @param brightness Brightness level (0-255)
 * @return BCM_OK on success
 */
bcm_result_t lighting_interior_set_brightness(uint8_t brightness);

/**
 * @brief Get interior light brightness
 * @return Current brightness (0-255)
 */
uint8_t lighting_interior_get_brightness(void);

/**
 * @brief Start interior light fade out
 * @param duration_ms Fade duration in milliseconds
 * @return BCM_OK on success
 */
bcm_result_t lighting_interior_fade_out(uint16_t duration_ms);

/* =============================================================================
 * Automatic Lighting Features
 * ========================================================================== */

/**
 * @brief Enable automatic headlight mode
 * @param enable true to enable, false to disable
 */
void lighting_auto_headlights_enable(bool enable);

/**
 * @brief Check if auto headlights are enabled
 * @return true if enabled
 */
bool lighting_auto_headlights_enabled(void);

/**
 * @brief Update ambient light sensor value
 * @param lux Ambient light level in lux
 */
void lighting_update_ambient_light(uint16_t lux);

/**
 * @brief Enable follow-me-home feature
 * @param enable true to enable, false to disable
 */
void lighting_follow_me_home_enable(bool enable);

/**
 * @brief Trigger follow-me-home lights
 *
 * Keeps headlights on for configured duration after ignition off.
 * @return BCM_OK on success
 */
bcm_result_t lighting_trigger_follow_me_home(void);

/**
 * @brief Enable welcome lights feature
 * @param enable true to enable, false to disable
 */
void lighting_welcome_lights_enable(bool enable);

/**
 * @brief Trigger welcome lights
 *
 * Turns on lights when vehicle is unlocked/approached.
 * @return BCM_OK on success
 */
bcm_result_t lighting_trigger_welcome_lights(void);

/* =============================================================================
 * Lighting Status
 * ========================================================================== */

/**
 * @brief Get complete lighting status
 * @param[out] status Status structure to fill
 * @return BCM_OK on success
 */
bcm_result_t lighting_get_status(lighting_status_t *status);

/**
 * @brief Get state of a specific light
 * @param light Light identifier (implementation specific)
 * @return Current light state
 */
light_state_t lighting_get_light_state(uint8_t light);

/* =============================================================================
 * Door Event Handling
 * ========================================================================== */

/**
 * @brief Notify lighting module of door open event
 * @param door Which door was opened
 */
void lighting_on_door_open(door_position_t door);

/**
 * @brief Notify lighting module of door close event
 * @param door Which door was closed
 */
void lighting_on_door_close(door_position_t door);

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

/**
 * @brief Handle incoming lighting command from CAN
 * @param data CAN message data
 * @param len Data length
 * @return BCM_OK on success
 */
bcm_result_t lighting_control_handle_can_cmd(const uint8_t *data, uint8_t len);

/**
 * @brief Get lighting status data for CAN transmission
 * @param[out] data Buffer for CAN data (minimum 8 bytes)
 * @param[out] len Actual data length
 */
void lighting_control_get_can_status(uint8_t *data, uint8_t *len);

#ifdef __cplusplus
}
#endif

#endif /* LIGHTING_CONTROL_H */
