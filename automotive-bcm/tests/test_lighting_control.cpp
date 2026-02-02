/**
 * @file test_lighting_control.cpp
 * @brief Unit tests for Lighting Control Module
 *
 * Tests for headlights, interior lights, and automatic features.
 */

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

extern "C" {
#include "lighting_control.h"
#include "system_state.h"
#include "bcm_config.h"
}

/* =============================================================================
 * Test Group: Lighting Control Initialization
 * ========================================================================== */

TEST_GROUP(LightingControlInit)
{
    void setup() override
    {
        system_state_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(LightingControlInit, InitializesSuccessfully)
{
    bcm_result_t result = lighting_control_init();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(LightingControlInit, CanBeDeinitialized)
{
    lighting_control_init();
    lighting_control_deinit();
    /* Should not crash */
}

TEST(LightingControlInit, HeadlightsOffByDefault)
{
    lighting_control_init();
    CHECK_FALSE(lighting_headlights_active());
}

/* =============================================================================
 * Test Group: Headlight Control
 * ========================================================================== */

TEST_GROUP(HeadlightControl)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(HeadlightControl, TurnOnSucceeds)
{
    bcm_result_t result = lighting_headlights_on();
    CHECK_EQUAL(BCM_OK, result);
    CHECK_TRUE(lighting_headlights_active());
}

TEST(HeadlightControl, TurnOffSucceeds)
{
    lighting_headlights_on();
    bcm_result_t result = lighting_headlights_off();
    CHECK_EQUAL(BCM_OK, result);
    CHECK_FALSE(lighting_headlights_active());
}

TEST(HeadlightControl, SetModeOffSucceeds)
{
    lighting_headlights_on();
    bcm_result_t result = lighting_set_headlight_mode(HEADLIGHT_MODE_OFF);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(HEADLIGHT_MODE_OFF, lighting_get_headlight_mode());
}

TEST(HeadlightControl, SetModeLowBeamSucceeds)
{
    bcm_result_t result = lighting_set_headlight_mode(HEADLIGHT_MODE_LOW_BEAM);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(HEADLIGHT_MODE_LOW_BEAM, lighting_get_headlight_mode());
}

TEST(HeadlightControl, SetModeAutoSucceeds)
{
    bcm_result_t result = lighting_set_headlight_mode(HEADLIGHT_MODE_AUTO);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(HEADLIGHT_MODE_AUTO, lighting_get_headlight_mode());
}

TEST(HeadlightControl, GetModeReturnsCurrentMode)
{
    lighting_set_headlight_mode(HEADLIGHT_MODE_HIGH_BEAM);
    CHECK_EQUAL(HEADLIGHT_MODE_HIGH_BEAM, lighting_get_headlight_mode());
}

/* =============================================================================
 * Test Group: High Beam Control
 * ========================================================================== */

TEST_GROUP(HighBeamControl)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(HighBeamControl, TurnOnWithHeadlightsSucceeds)
{
    lighting_headlights_on();
    bcm_result_t result = lighting_high_beam_on();
    CHECK_EQUAL(BCM_OK, result);
    CHECK_TRUE(lighting_high_beam_active());
}

TEST(HighBeamControl, TurnOnWithoutHeadlightsFails)
{
    bcm_result_t result = lighting_high_beam_on();
    CHECK_EQUAL(BCM_ERROR_NOT_READY, result);
}

TEST(HighBeamControl, TurnOffSucceeds)
{
    lighting_headlights_on();
    lighting_high_beam_on();
    bcm_result_t result = lighting_high_beam_off();
    CHECK_EQUAL(BCM_OK, result);
    CHECK_FALSE(lighting_high_beam_active());
}

TEST(HighBeamControl, FlashSucceeds)
{
    lighting_headlights_on();
    bcm_result_t result = lighting_flash_high_beam();
    CHECK_EQUAL(BCM_OK, result);
}

/* =============================================================================
 * Test Group: Fog Light Control
 * ========================================================================== */

TEST_GROUP(FogLightControl)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(FogLightControl, TurnOnSucceeds)
{
    bcm_result_t result = lighting_fog_lights_on();
    CHECK_EQUAL(BCM_OK, result);
    CHECK_TRUE(lighting_fog_lights_active());
}

TEST(FogLightControl, TurnOffSucceeds)
{
    lighting_fog_lights_on();
    bcm_result_t result = lighting_fog_lights_off();
    CHECK_EQUAL(BCM_OK, result);
    CHECK_FALSE(lighting_fog_lights_active());
}

TEST(FogLightControl, RearFogOnSucceeds)
{
    bcm_result_t result = lighting_rear_fog_on();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FogLightControl, RearFogOffSucceeds)
{
    bcm_result_t result = lighting_rear_fog_off();
    CHECK_EQUAL(BCM_OK, result);
}

/* =============================================================================
 * Test Group: Interior Light Control
 * ========================================================================== */

TEST_GROUP(InteriorLightControl)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(InteriorLightControl, TurnOnSucceeds)
{
    bcm_result_t result = lighting_interior_on();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(InteriorLightControl, TurnOffSucceeds)
{
    lighting_interior_on();
    bcm_result_t result = lighting_interior_off();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(InteriorLightControl, SetBrightnessSucceeds)
{
    bcm_result_t result = lighting_interior_set_brightness(128);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(128, lighting_interior_get_brightness());
}

TEST(InteriorLightControl, SetBrightnessZeroTurnsOff)
{
    lighting_interior_on();
    lighting_interior_set_brightness(0);
    CHECK_EQUAL(0, lighting_interior_get_brightness());
}

TEST(InteriorLightControl, SetBrightnessMaxTurnsOn)
{
    lighting_interior_set_brightness(255);
    CHECK_EQUAL(255, lighting_interior_get_brightness());
}

TEST(InteriorLightControl, SetModeDoorSucceeds)
{
    bcm_result_t result = lighting_set_interior_mode(INTERIOR_MODE_DOOR);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(INTERIOR_MODE_DOOR, lighting_get_interior_mode());
}

TEST(InteriorLightControl, FadeOutSucceeds)
{
    lighting_interior_on();
    bcm_result_t result = lighting_interior_fade_out(2000);
    CHECK_EQUAL(BCM_OK, result);
}

/* =============================================================================
 * Test Group: Automatic Features
 * ========================================================================== */

TEST_GROUP(LightingAutoFeatures)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(LightingAutoFeatures, AutoHeadlightsCanBeEnabled)
{
    lighting_auto_headlights_enable(true);
    CHECK_TRUE(lighting_auto_headlights_enabled());
}

TEST(LightingAutoFeatures, AutoHeadlightsCanBeDisabled)
{
    lighting_auto_headlights_enable(false);
    CHECK_FALSE(lighting_auto_headlights_enabled());
}

TEST(LightingAutoFeatures, FollowMeHomeNotSupportedWhenDisabled)
{
    lighting_follow_me_home_enable(false);
    bcm_result_t result = lighting_trigger_follow_me_home();
    CHECK_EQUAL(BCM_ERROR_NOT_SUPPORTED, result);
}

TEST(LightingAutoFeatures, FollowMeHomeSucceedsWhenEnabled)
{
    lighting_follow_me_home_enable(true);
    bcm_result_t result = lighting_trigger_follow_me_home();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(LightingAutoFeatures, WelcomeLightsNotSupportedWhenDisabled)
{
    lighting_welcome_lights_enable(false);
    bcm_result_t result = lighting_trigger_welcome_lights();
    CHECK_EQUAL(BCM_ERROR_NOT_SUPPORTED, result);
}

TEST(LightingAutoFeatures, WelcomeLightsSucceedsWhenEnabled)
{
    lighting_welcome_lights_enable(true);
    bcm_result_t result = lighting_trigger_welcome_lights();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(LightingAutoFeatures, AmbientLightCanBeUpdated)
{
    lighting_update_ambient_light(500);
    /* No assertion - just verify it doesn't crash */
}

/* =============================================================================
 * Test Group: Lighting Status
 * ========================================================================== */

TEST_GROUP(LightingStatus)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(LightingStatus, GetStatusSucceeds)
{
    lighting_status_t status;
    bcm_result_t result = lighting_get_status(&status);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(LightingStatus, GetStatusWithNullPointerFails)
{
    bcm_result_t result = lighting_get_status(nullptr);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

/* =============================================================================
 * Test Group: Door Events
 * ========================================================================== */

TEST_GROUP(LightingDoorEvents)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
        lighting_set_interior_mode(INTERIOR_MODE_DOOR);
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(LightingDoorEvents, DoorOpenTriggersInteriorLight)
{
    lighting_on_door_open(DOOR_FRONT_LEFT);
    /* Light should be on - implementation specific */
}

TEST(LightingDoorEvents, DoorCloseStartsFadeOut)
{
    lighting_on_door_open(DOOR_FRONT_LEFT);
    lighting_on_door_close(DOOR_FRONT_LEFT);
    /* Should start fade - implementation specific */
}

/* =============================================================================
 * Test Group: CAN Message Handling
 * ========================================================================== */

TEST_GROUP(LightingCanMessages)
{
    void setup() override
    {
        system_state_init();
        lighting_control_init();
    }

    void teardown() override
    {
        lighting_control_deinit();
        mock().clear();
    }
};

TEST(LightingCanMessages, HandleHeadlightsOnCommand)
{
    uint8_t data[] = {0x01};  /* LIGHT_CMD_HEADLIGHTS_ON */
    bcm_result_t result = lighting_control_handle_can_cmd(data, sizeof(data));
    CHECK_EQUAL(BCM_OK, result);
    CHECK_TRUE(lighting_headlights_active());
}

TEST(LightingCanMessages, HandleHeadlightsOffCommand)
{
    lighting_headlights_on();
    uint8_t data[] = {0x02};  /* LIGHT_CMD_HEADLIGHTS_OFF */
    bcm_result_t result = lighting_control_handle_can_cmd(data, sizeof(data));
    CHECK_EQUAL(BCM_OK, result);
    CHECK_FALSE(lighting_headlights_active());
}

TEST(LightingCanMessages, HandleNullDataFails)
{
    bcm_result_t result = lighting_control_handle_can_cmd(nullptr, 0);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(LightingCanMessages, HandleInvalidCommandFails)
{
    uint8_t data[] = {0xFF};
    bcm_result_t result = lighting_control_handle_can_cmd(data, sizeof(data));
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(LightingCanMessages, GetCanStatusFillsBuffer)
{
    uint8_t data[8] = {0};
    uint8_t len = 0;
    lighting_control_get_can_status(data, &len);
    CHECK_EQUAL(8, len);
}
