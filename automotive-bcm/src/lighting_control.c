/**
 * @file lighting_control.c
 * @brief Lighting Control Module Implementation
 *
 * Implements all lighting control functions including headlights,
 * interior lights, and automatic lighting features.
 */

#include <string.h>
#include "lighting_control.h"
#include "fault_manager.h"
#include "bcm_config.h"
#include "can_ids.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Lighting control internal state */
typedef struct {
    bool initialized;
    headlight_mode_t headlight_mode;
    interior_mode_t interior_mode;
    bool auto_headlights_enabled;
    bool follow_me_home_enabled;
    bool welcome_lights_enabled;
    uint16_t ambient_light_lux;
    uint32_t follow_me_home_start;
    bool follow_me_home_active;
    uint32_t welcome_lights_start;
    bool welcome_lights_active;
    uint32_t interior_fade_start;
    bool interior_fading;
    uint16_t interior_fade_duration;
    uint8_t interior_fade_from;
    uint32_t door_open_time;
    bool door_triggered_interior;
} lighting_control_state_t;

static lighting_control_state_t g_lighting_state;

/* =============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Set physical headlight output
 */
static void set_headlight_output(bool on)
{
    system_state_t *state = system_state_get_mutable();
    state->lighting.headlights = on ? LIGHT_ON : LIGHT_OFF;
    state->lighting.tail_lights = on ? LIGHT_ON : LIGHT_OFF;
    /* In real implementation, drive GPIO/PWM here */
}

/**
 * @brief Set physical high beam output
 */
static void set_high_beam_output(bool on)
{
    system_state_t *state = system_state_get_mutable();
    state->lighting.high_beam = on ? LIGHT_ON : LIGHT_OFF;
}

/**
 * @brief Set physical fog light output
 */
static void set_fog_light_output(bool on)
{
    system_state_t *state = system_state_get_mutable();
    state->lighting.fog_lights = on ? LIGHT_ON : LIGHT_OFF;
}

/**
 * @brief Set interior light brightness
 */
static void set_interior_output(uint8_t brightness)
{
    system_state_t *state = system_state_get_mutable();
    state->lighting.interior_brightness = brightness;
    
    if (brightness == 0) {
        state->lighting.interior = LIGHT_OFF;
    } else if (brightness < LIGHT_PWM_MAX_DUTY) {
        state->lighting.interior = LIGHT_DIMMED;
    } else {
        state->lighting.interior = LIGHT_ON;
    }
}

/* =============================================================================
 * Initialization
 * ========================================================================== */

bcm_result_t lighting_control_init(void)
{
    memset(&g_lighting_state, 0, sizeof(g_lighting_state));
    
    g_lighting_state.headlight_mode = HEADLIGHT_MODE_OFF;
    g_lighting_state.interior_mode = INTERIOR_MODE_DOOR;
    g_lighting_state.auto_headlights_enabled = BCM_FEATURE_AUTO_HEADLIGHTS;
    g_lighting_state.follow_me_home_enabled = BCM_FEATURE_FOLLOW_ME_HOME;
    g_lighting_state.welcome_lights_enabled = BCM_FEATURE_WELCOME_LIGHTS;
    g_lighting_state.initialized = true;
    
    return BCM_OK;
}

void lighting_control_deinit(void)
{
    /* Turn off all lights */
    set_headlight_output(false);
    set_high_beam_output(false);
    set_fog_light_output(false);
    set_interior_output(0);
    
    g_lighting_state.initialized = false;
}

/* =============================================================================
 * Processing
 * ========================================================================== */

bcm_result_t lighting_control_process(uint32_t timestamp_ms)
{
    if (!g_lighting_state.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    /* Process automatic headlights */
    if (g_lighting_state.headlight_mode == HEADLIGHT_MODE_AUTO) {
        if (g_lighting_state.ambient_light_lux < LIGHT_AUTO_ON_THRESHOLD_LUX) {
            set_headlight_output(true);
        } else if (g_lighting_state.ambient_light_lux > LIGHT_AUTO_OFF_THRESHOLD_LUX) {
            set_headlight_output(false);
        }
    }
    
    /* Process follow-me-home lights */
    if (g_lighting_state.follow_me_home_active) {
        uint32_t elapsed = timestamp_ms - g_lighting_state.follow_me_home_start;
        if (elapsed >= (LIGHT_FOLLOW_ME_HOME_SEC * 1000)) {
            set_headlight_output(false);
            g_lighting_state.follow_me_home_active = false;
        }
    }
    
    /* Process welcome lights */
    if (g_lighting_state.welcome_lights_active) {
        uint32_t elapsed = timestamp_ms - g_lighting_state.welcome_lights_start;
        if (elapsed >= (LIGHT_WELCOME_DURATION_SEC * 1000)) {
            set_headlight_output(false);
            set_interior_output(0);
            g_lighting_state.welcome_lights_active = false;
        }
    }
    
    /* Process interior light fade */
    if (g_lighting_state.interior_fading) {
        uint32_t elapsed = timestamp_ms - g_lighting_state.interior_fade_start;
        
        if (elapsed >= g_lighting_state.interior_fade_duration) {
            set_interior_output(0);
            g_lighting_state.interior_fading = false;
        } else {
            /* Linear fade */
            uint32_t remaining = g_lighting_state.interior_fade_duration - elapsed;
            uint8_t brightness = (uint8_t)((g_lighting_state.interior_fade_from * remaining) / 
                                           g_lighting_state.interior_fade_duration);
            set_interior_output(brightness);
        }
    }
    
    /* Process door-triggered interior light timeout */
    if (g_lighting_state.door_triggered_interior) {
        uint32_t elapsed = timestamp_ms - g_lighting_state.door_open_time;
        if (elapsed >= (LIGHT_INTERIOR_DOOR_TIMEOUT_SEC * 1000)) {
            lighting_interior_fade_out(LIGHT_INTERIOR_FADE_MS);
            g_lighting_state.door_triggered_interior = false;
        }
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Headlight Control
 * ========================================================================== */

bcm_result_t lighting_set_headlight_mode(headlight_mode_t mode)
{
    g_lighting_state.headlight_mode = mode;
    system_state_get_mutable()->lighting.auto_mode_active = (mode == HEADLIGHT_MODE_AUTO);
    
    switch (mode) {
        case HEADLIGHT_MODE_OFF:
            set_headlight_output(false);
            set_high_beam_output(false);
            break;
            
        case HEADLIGHT_MODE_PARKING:
            /* Parking lights only - reduced brightness */
            set_headlight_output(false);
            break;
            
        case HEADLIGHT_MODE_LOW_BEAM:
            set_headlight_output(true);
            set_high_beam_output(false);
            break;
            
        case HEADLIGHT_MODE_HIGH_BEAM:
            set_headlight_output(true);
            set_high_beam_output(true);
            break;
            
        case HEADLIGHT_MODE_AUTO:
            /* Will be handled in process() */
            break;
    }
    
    return BCM_OK;
}

headlight_mode_t lighting_get_headlight_mode(void)
{
    return g_lighting_state.headlight_mode;
}

bcm_result_t lighting_headlights_on(void)
{
    return lighting_set_headlight_mode(HEADLIGHT_MODE_LOW_BEAM);
}

bcm_result_t lighting_headlights_off(void)
{
    return lighting_set_headlight_mode(HEADLIGHT_MODE_OFF);
}

bcm_result_t lighting_high_beam_on(void)
{
    if (g_lighting_state.headlight_mode == HEADLIGHT_MODE_OFF) {
        return BCM_ERROR_NOT_READY;
    }
    set_high_beam_output(true);
    return BCM_OK;
}

bcm_result_t lighting_high_beam_off(void)
{
    set_high_beam_output(false);
    return BCM_OK;
}

bcm_result_t lighting_flash_high_beam(void)
{
    /* Momentary flash - actual timing handled elsewhere */
    set_high_beam_output(true);
    return BCM_OK;
}

bool lighting_headlights_active(void)
{
    return system_state_get()->lighting.headlights != LIGHT_OFF;
}

bool lighting_high_beam_active(void)
{
    return system_state_get()->lighting.high_beam != LIGHT_OFF;
}

/* =============================================================================
 * Fog Light Control
 * ========================================================================== */

bcm_result_t lighting_fog_lights_on(void)
{
    set_fog_light_output(true);
    return BCM_OK;
}

bcm_result_t lighting_fog_lights_off(void)
{
    set_fog_light_output(false);
    return BCM_OK;
}

bcm_result_t lighting_rear_fog_on(void)
{
    /* Rear fog light control */
    return BCM_OK;
}

bcm_result_t lighting_rear_fog_off(void)
{
    return BCM_OK;
}

bool lighting_fog_lights_active(void)
{
    return system_state_get()->lighting.fog_lights != LIGHT_OFF;
}

/* =============================================================================
 * Interior Light Control
 * ========================================================================== */

bcm_result_t lighting_set_interior_mode(interior_mode_t mode)
{
    g_lighting_state.interior_mode = mode;
    
    switch (mode) {
        case INTERIOR_MODE_OFF:
            set_interior_output(0);
            break;
            
        case INTERIOR_MODE_ON:
            set_interior_output(LIGHT_PWM_MAX_DUTY);
            break;
            
        case INTERIOR_MODE_DOOR:
            /* Will be triggered by door events */
            break;
            
        case INTERIOR_MODE_DIMMED:
            set_interior_output(LIGHT_PWM_MAX_DUTY / 4);
            break;
    }
    
    return BCM_OK;
}

interior_mode_t lighting_get_interior_mode(void)
{
    return g_lighting_state.interior_mode;
}

bcm_result_t lighting_interior_on(void)
{
    g_lighting_state.interior_fading = false;
    set_interior_output(LIGHT_PWM_MAX_DUTY);
    return BCM_OK;
}

bcm_result_t lighting_interior_off(void)
{
    g_lighting_state.interior_fading = false;
    set_interior_output(0);
    return BCM_OK;
}

bcm_result_t lighting_interior_set_brightness(uint8_t brightness)
{
    g_lighting_state.interior_fading = false;
    set_interior_output(brightness);
    return BCM_OK;
}

uint8_t lighting_interior_get_brightness(void)
{
    return system_state_get()->lighting.interior_brightness;
}

bcm_result_t lighting_interior_fade_out(uint16_t duration_ms)
{
    g_lighting_state.interior_fade_start = system_state_get()->uptime_ms;
    g_lighting_state.interior_fade_duration = duration_ms;
    g_lighting_state.interior_fade_from = system_state_get()->lighting.interior_brightness;
    g_lighting_state.interior_fading = true;
    return BCM_OK;
}

/* =============================================================================
 * Automatic Features
 * ========================================================================== */

void lighting_auto_headlights_enable(bool enable)
{
    g_lighting_state.auto_headlights_enabled = enable;
}

bool lighting_auto_headlights_enabled(void)
{
    return g_lighting_state.auto_headlights_enabled;
}

void lighting_update_ambient_light(uint16_t lux)
{
    g_lighting_state.ambient_light_lux = lux;
}

void lighting_follow_me_home_enable(bool enable)
{
    g_lighting_state.follow_me_home_enabled = enable;
}

bcm_result_t lighting_trigger_follow_me_home(void)
{
    if (!g_lighting_state.follow_me_home_enabled) {
        return BCM_ERROR_NOT_SUPPORTED;
    }
    
    set_headlight_output(true);
    g_lighting_state.follow_me_home_start = system_state_get()->uptime_ms;
    g_lighting_state.follow_me_home_active = true;
    
    return BCM_OK;
}

void lighting_welcome_lights_enable(bool enable)
{
    g_lighting_state.welcome_lights_enabled = enable;
}

bcm_result_t lighting_trigger_welcome_lights(void)
{
    if (!g_lighting_state.welcome_lights_enabled) {
        return BCM_ERROR_NOT_SUPPORTED;
    }
    
    set_headlight_output(true);
    set_interior_output(LIGHT_PWM_MAX_DUTY);
    g_lighting_state.welcome_lights_start = system_state_get()->uptime_ms;
    g_lighting_state.welcome_lights_active = true;
    
    return BCM_OK;
}

/* =============================================================================
 * Status Functions
 * ========================================================================== */

bcm_result_t lighting_get_status(lighting_status_t *status)
{
    if (!status) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    *status = system_state_get()->lighting;
    return BCM_OK;
}

light_state_t lighting_get_light_state(uint8_t light)
{
    (void)light;
    /* Implementation specific */
    return LIGHT_OFF;
}

/* =============================================================================
 * Door Event Handling
 * ========================================================================== */

void lighting_on_door_open(door_position_t door)
{
    (void)door;
    
    if (g_lighting_state.interior_mode == INTERIOR_MODE_DOOR) {
        g_lighting_state.interior_fading = false;
        set_interior_output(LIGHT_PWM_MAX_DUTY);
        g_lighting_state.door_open_time = system_state_get()->uptime_ms;
        g_lighting_state.door_triggered_interior = true;
    }
}

void lighting_on_door_close(door_position_t door)
{
    (void)door;
    
    /* Start fade out after all doors closed */
    if (g_lighting_state.interior_mode == INTERIOR_MODE_DOOR &&
        g_lighting_state.door_triggered_interior) {
        /* Check if all doors are closed - simplified */
        lighting_interior_fade_out(LIGHT_INTERIOR_FADE_MS);
        g_lighting_state.door_triggered_interior = false;
    }
}

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

bcm_result_t lighting_control_handle_can_cmd(const uint8_t *data, uint8_t len)
{
    if (!data || len < 1) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    uint8_t cmd = data[0];
    
    switch (cmd) {
        case LIGHT_CMD_HEADLIGHTS_ON:
            return lighting_headlights_on();
            
        case LIGHT_CMD_HEADLIGHTS_OFF:
            return lighting_headlights_off();
            
        case LIGHT_CMD_HEADLIGHTS_AUTO:
            return lighting_set_headlight_mode(HEADLIGHT_MODE_AUTO);
            
        case LIGHT_CMD_HIGH_BEAM_ON:
            return lighting_high_beam_on();
            
        case LIGHT_CMD_HIGH_BEAM_OFF:
            return lighting_high_beam_off();
            
        case LIGHT_CMD_FOG_LIGHTS_ON:
            return lighting_fog_lights_on();
            
        case LIGHT_CMD_FOG_LIGHTS_OFF:
            return lighting_fog_lights_off();
            
        case LIGHT_CMD_INTERIOR_ON:
            return lighting_interior_on();
            
        case LIGHT_CMD_INTERIOR_OFF:
            return lighting_interior_off();
            
        case LIGHT_CMD_INTERIOR_DIM:
            return lighting_interior_set_brightness((len > 1) ? data[1] : (LIGHT_PWM_MAX_DUTY / 2));
            
        default:
            return BCM_ERROR_INVALID_PARAM;
    }
}

void lighting_control_get_can_status(uint8_t *data, uint8_t *len)
{
    const lighting_status_t *status = &system_state_get()->lighting;
    
    /* Byte 0: Headlight state and mode */
    data[0] = (uint8_t)status->headlights;
    data[0] |= (uint8_t)((uint8_t)g_lighting_state.headlight_mode << 4);
    
    /* Byte 1: High beam and fog lights */
    data[1] = (uint8_t)status->high_beam;
    data[1] |= (uint8_t)((uint8_t)status->fog_lights << 4);
    
    /* Byte 2: Tail lights */
    data[2] = (uint8_t)status->tail_lights;
    
    /* Byte 3: Interior light state */
    data[3] = (uint8_t)status->interior;
    
    /* Byte 4: Interior brightness */
    data[4] = status->interior_brightness;
    
    /* Byte 5: Feature status */
    data[5] = 0;
    if (status->auto_mode_active) data[5] |= 0x01;
    if (g_lighting_state.follow_me_home_active) data[5] |= 0x02;
    if (g_lighting_state.welcome_lights_active) data[5] |= 0x04;
    
    /* Bytes 6-7: Reserved */
    data[6] = 0;
    data[7] = 0;
    
    *len = 8;
}
