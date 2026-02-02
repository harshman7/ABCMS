/**
 * @file test_lighting_control.cpp
 * @brief Unit tests for Lighting Control Module
 *
 * Tests:
 * - Off/On/Auto mode transitions
 * - Invalid mode rejection
 * - High beam control
 * - Interior light control
 */

#include "CppUTest/TestHarness.h"

extern "C" {
#include "lighting_control.h"
#include "fault_manager.h"
#include "can_interface.h"
#include "can_ids.h"
}

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

static can_frame_t build_lighting_cmd(uint8_t headlight, uint8_t interior, uint8_t counter)
{
    can_frame_t frame;
    frame.id = CAN_ID_LIGHTING_CMD;
    frame.dlc = LIGHTING_CMD_DLC;
    
    frame.data[LIGHTING_CMD_BYTE_HEADLIGHT] = headlight;
    frame.data[LIGHTING_CMD_BYTE_INTERIOR] = interior;
    frame.data[LIGHTING_CMD_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(CAN_SCHEMA_VERSION, counter);
    frame.data[LIGHTING_CMD_BYTE_CHECKSUM] = can_calculate_checksum(frame.data, LIGHTING_CMD_DLC - 1);
    
    return frame;
}

/*******************************************************************************
 * Test Group: Lighting Initialization
 ******************************************************************************/

TEST_GROUP(LightingInit)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        lighting_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(LightingInit, HeadlightOffInitially)
{
    CHECK_EQUAL(LIGHTING_STATE_OFF, lighting_control_get_headlight_mode());
    CHECK_EQUAL(HEADLIGHT_STATE_OFF, lighting_control_get_headlight_output());
}

TEST(LightingInit, InteriorOffInitially)
{
    CHECK_EQUAL(0, lighting_control_get_interior_brightness());
}

TEST(LightingInit, HeadlightsNotOnInitially)
{
    CHECK_FALSE(lighting_control_headlights_on());
}

/*******************************************************************************
 * Test Group: Headlight Mode Commands
 ******************************************************************************/

TEST_GROUP(HeadlightModes)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        lighting_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(HeadlightModes, TurnOnSucceeds)
{
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_ON, INTERIOR_CMD_OFF, 0);
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(LIGHTING_STATE_ON, lighting_control_get_headlight_mode());
    CHECK_TRUE(lighting_control_headlights_on());
}

TEST(HeadlightModes, TurnOffSucceeds)
{
    /* Turn on first */
    lighting_control_set_headlight_mode(LIGHTING_STATE_ON);
    CHECK_TRUE(lighting_control_headlights_on());
    
    /* Turn off */
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_OFF, INTERIOR_CMD_OFF, 0);
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(LIGHTING_STATE_OFF, lighting_control_get_headlight_mode());
    CHECK_FALSE(lighting_control_headlights_on());
}

TEST(HeadlightModes, AutoModeSucceeds)
{
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_AUTO, INTERIOR_CMD_OFF, 0);
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(LIGHTING_STATE_AUTO, lighting_control_get_headlight_mode());
}

TEST(HeadlightModes, HighBeamOnSucceeds)
{
    /* Turn on low beam first */
    lighting_control_set_headlight_mode(LIGHTING_STATE_ON);
    
    /* Turn on high beam */
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_HIGH_ON, INTERIOR_CMD_OFF, 0);
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(HEADLIGHT_STATE_HIGH_BEAM, lighting_control_get_headlight_output());
}

TEST(HeadlightModes, HighBeamOffSucceeds)
{
    lighting_control_set_headlight_mode(LIGHTING_STATE_ON);
    lighting_control_set_high_beam(true);
    CHECK_EQUAL(HEADLIGHT_STATE_HIGH_BEAM, lighting_control_get_headlight_output());
    
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_HIGH_OFF, INTERIOR_CMD_OFF, 0);
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(HEADLIGHT_STATE_ON, lighting_control_get_headlight_output());
}

/*******************************************************************************
 * Test Group: Auto Mode Behavior
 ******************************************************************************/

TEST_GROUP(AutoModeBehavior)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        lighting_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(AutoModeBehavior, TurnsOnWhenDark)
{
    lighting_control_set_headlight_mode(LIGHTING_STATE_AUTO);
    lighting_control_set_ambient(50); /* Below threshold */
    lighting_control_update(100);
    
    CHECK_EQUAL(HEADLIGHT_STATE_AUTO, lighting_control_get_headlight_output());
}

TEST(AutoModeBehavior, StaysOffWhenBright)
{
    lighting_control_set_headlight_mode(LIGHTING_STATE_AUTO);
    lighting_control_set_ambient(200); /* Above threshold */
    lighting_control_update(100);
    
    CHECK_EQUAL(HEADLIGHT_STATE_OFF, lighting_control_get_headlight_output());
}

/*******************************************************************************
 * Test Group: Interior Light Control
 ******************************************************************************/

TEST_GROUP(InteriorControl)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        lighting_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(InteriorControl, TurnOnWithBrightness)
{
    uint8_t brightness = 10;
    uint8_t interior_byte = INTERIOR_CMD_ON | (brightness << 4);
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_OFF, interior_byte, 0);
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(brightness, lighting_control_get_interior_brightness());
}

TEST(InteriorControl, TurnOff)
{
    lighting_control_set_interior(LIGHTING_STATE_ON, 15);
    CHECK_EQUAL(15, lighting_control_get_interior_brightness());
    
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_OFF, INTERIOR_CMD_OFF, 0);
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    
    CHECK_EQUAL(CMD_RESULT_OK, result);
    CHECK_EQUAL(0, lighting_control_get_interior_brightness());
}

/*******************************************************************************
 * Test Group: Lighting Command Validation
 ******************************************************************************/

TEST_GROUP(LightingValidation)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        lighting_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(LightingValidation, RejectsInvalidHeadlightMode)
{
    can_frame_t frame = build_lighting_cmd(0xFF, INTERIOR_CMD_OFF, 0);
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

TEST(LightingValidation, RejectsInvalidInteriorMode)
{
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_OFF, 0x0F, 0);
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

TEST(LightingValidation, RejectsWrongDLC)
{
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_ON, INTERIOR_CMD_OFF, 0);
    frame.dlc = 2;
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

TEST(LightingValidation, RejectsInvalidChecksum)
{
    can_frame_t frame = build_lighting_cmd(HEADLIGHT_CMD_ON, INTERIOR_CMD_OFF, 0);
    frame.data[LIGHTING_CMD_BYTE_CHECKSUM] = 0x00;
    
    cmd_result_t result = lighting_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_CHECKSUM_ERROR, result);
}

TEST(LightingValidation, RejectsInvalidCounter)
{
    /* First command */
    can_frame_t frame1 = build_lighting_cmd(HEADLIGHT_CMD_ON, INTERIOR_CMD_OFF, 5);
    lighting_control_handle_cmd(&frame1);
    
    /* Second with bad counter */
    can_frame_t frame2 = build_lighting_cmd(HEADLIGHT_CMD_OFF, INTERIOR_CMD_OFF, 10);
    cmd_result_t result = lighting_control_handle_cmd(&frame2);
    
    CHECK_EQUAL(CMD_RESULT_COUNTER_ERROR, result);
}

/*******************************************************************************
 * Test Group: Lighting Status Frame
 ******************************************************************************/

TEST_GROUP(LightingStatusFrame)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        lighting_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(LightingStatusFrame, HasCorrectId)
{
    can_frame_t frame;
    lighting_control_build_status_frame(&frame);
    CHECK_EQUAL(CAN_ID_LIGHTING_STATUS, frame.id);
}

TEST(LightingStatusFrame, HasCorrectDlc)
{
    can_frame_t frame;
    lighting_control_build_status_frame(&frame);
    CHECK_EQUAL(LIGHTING_STATUS_DLC, frame.dlc);
}

TEST(LightingStatusFrame, ChecksumValid)
{
    can_frame_t frame;
    lighting_control_build_status_frame(&frame);
    
    uint8_t calc = can_calculate_checksum(frame.data, LIGHTING_STATUS_DLC - 1);
    CHECK_EQUAL(calc, frame.data[LIGHTING_STATUS_BYTE_CHECKSUM]);
}

TEST(LightingStatusFrame, HeadlightStateCorrect)
{
    lighting_control_set_headlight_mode(LIGHTING_STATE_ON);
    
    can_frame_t frame;
    lighting_control_build_status_frame(&frame);
    
    CHECK_EQUAL(HEADLIGHT_STATE_ON, frame.data[LIGHTING_STATUS_BYTE_HEADLIGHT]);
}
