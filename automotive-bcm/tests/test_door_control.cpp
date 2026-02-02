/**
 * @file test_door_control.cpp
 * @brief Unit tests for Door Control Module
 *
 * Tests for door lock, window control, and related functionality.
 */

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

extern "C" {
#include "door_control.h"
#include "system_state.h"
#include "bcm_config.h"
}

/* =============================================================================
 * Test Group: Door Control Initialization
 * ========================================================================== */

TEST_GROUP(DoorControlInit)
{
    void setup() override
    {
        system_state_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(DoorControlInit, InitializesSuccessfully)
{
    bcm_result_t result = door_control_init();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DoorControlInit, CanBeDeinitialized)
{
    door_control_init();
    door_control_deinit();
    /* Should not crash */
}

TEST(DoorControlInit, DoubleInitIsOk)
{
    CHECK_EQUAL(BCM_OK, door_control_init());
    CHECK_EQUAL(BCM_OK, door_control_init());
}

/* =============================================================================
 * Test Group: Door Lock Control
 * ========================================================================== */

TEST_GROUP(DoorLockControl)
{
    void setup() override
    {
        system_state_init();
        door_control_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(DoorLockControl, LockAllDoorsSucceeds)
{
    bcm_result_t result = door_lock_all();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DoorLockControl, UnlockAllDoorsSucceeds)
{
    bcm_result_t result = door_unlock_all();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DoorLockControl, LockSingleDoorSucceeds)
{
    bcm_result_t result = door_lock(DOOR_FRONT_LEFT);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(LOCK_STATE_LOCKED, door_get_lock_state(DOOR_FRONT_LEFT));
}

TEST(DoorLockControl, UnlockSingleDoorSucceeds)
{
    door_lock(DOOR_FRONT_RIGHT);
    bcm_result_t result = door_unlock(DOOR_FRONT_RIGHT);
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(LOCK_STATE_UNLOCKED, door_get_lock_state(DOOR_FRONT_RIGHT));
}

TEST(DoorLockControl, LockAllSetsAllDoorsLocked)
{
    door_lock_all();
    
    CHECK_EQUAL(LOCK_STATE_LOCKED, door_get_lock_state(DOOR_FRONT_LEFT));
    CHECK_EQUAL(LOCK_STATE_LOCKED, door_get_lock_state(DOOR_FRONT_RIGHT));
    CHECK_EQUAL(LOCK_STATE_LOCKED, door_get_lock_state(DOOR_REAR_LEFT));
    CHECK_EQUAL(LOCK_STATE_LOCKED, door_get_lock_state(DOOR_REAR_RIGHT));
}

TEST(DoorLockControl, UnlockAllSetsAllDoorsUnlocked)
{
    door_lock_all();
    door_unlock_all();
    
    CHECK_EQUAL(LOCK_STATE_UNLOCKED, door_get_lock_state(DOOR_FRONT_LEFT));
    CHECK_EQUAL(LOCK_STATE_UNLOCKED, door_get_lock_state(DOOR_FRONT_RIGHT));
    CHECK_EQUAL(LOCK_STATE_UNLOCKED, door_get_lock_state(DOOR_REAR_LEFT));
    CHECK_EQUAL(LOCK_STATE_UNLOCKED, door_get_lock_state(DOOR_REAR_RIGHT));
}

TEST(DoorLockControl, AllLockedReturnsTrueWhenAllLocked)
{
    door_lock_all();
    CHECK_TRUE(door_all_locked());
}

TEST(DoorLockControl, AllLockedReturnsFalseWhenOneUnlocked)
{
    door_lock_all();
    door_unlock(DOOR_FRONT_LEFT);
    CHECK_FALSE(door_all_locked());
}

TEST(DoorLockControl, AnyUnlockedReturnsTrueWhenOneUnlocked)
{
    door_lock_all();
    door_unlock(DOOR_REAR_RIGHT);
    CHECK_TRUE(door_any_unlocked());
}

TEST(DoorLockControl, AnyUnlockedReturnsFalseWhenAllLocked)
{
    door_lock_all();
    CHECK_FALSE(door_any_unlocked());
}

TEST(DoorLockControl, InvalidDoorReturnsUnknownState)
{
    CHECK_EQUAL(LOCK_STATE_UNKNOWN, door_get_lock_state((door_position_t)99));
}

/* =============================================================================
 * Test Group: Window Control
 * ========================================================================== */

TEST_GROUP(WindowControl)
{
    void setup() override
    {
        system_state_init();
        door_control_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(WindowControl, OpenWindowSucceeds)
{
    bcm_result_t result = door_window_open(DOOR_FRONT_LEFT, false);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(WindowControl, CloseWindowSucceeds)
{
    bcm_result_t result = door_window_close(DOOR_FRONT_LEFT, false);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(WindowControl, StopWindowSucceeds)
{
    door_window_open(DOOR_FRONT_LEFT, false);
    bcm_result_t result = door_window_stop(DOOR_FRONT_LEFT);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(WindowControl, InvalidDoorReturnsError)
{
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, door_window_open((door_position_t)99, false));
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, door_window_close((door_position_t)99, false));
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, door_window_stop((door_position_t)99));
}

TEST(WindowControl, GetPositionReturnsValidValue)
{
    uint8_t pos = door_window_get_position(DOOR_FRONT_LEFT);
    CHECK_TRUE(pos <= 100 || pos == 0xFF);
}

TEST(WindowControl, InvalidDoorPositionReturns0xFF)
{
    CHECK_EQUAL(0xFF, door_window_get_position((door_position_t)99));
}

TEST(WindowControl, CloseAllWindowsSucceeds)
{
    bcm_result_t result = door_window_close_all();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(WindowControl, SetPositionSucceeds)
{
    bcm_result_t result = door_window_set_position(DOOR_FRONT_LEFT, 50);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(WindowControl, SetPositionInvalidPercentageReturnsError)
{
    bcm_result_t result = door_window_set_position(DOOR_FRONT_LEFT, 150);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

/* =============================================================================
 * Test Group: Child Safety Lock
 * ========================================================================== */

TEST_GROUP(ChildSafetyLock)
{
    void setup() override
    {
        system_state_init();
        door_control_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(ChildSafetyLock, EnableOnRearDoorSucceeds)
{
    CHECK_EQUAL(BCM_OK, door_child_lock_enable(DOOR_REAR_LEFT));
    CHECK_TRUE(door_child_lock_active(DOOR_REAR_LEFT));
}

TEST(ChildSafetyLock, DisableOnRearDoorSucceeds)
{
    door_child_lock_enable(DOOR_REAR_RIGHT);
    CHECK_EQUAL(BCM_OK, door_child_lock_disable(DOOR_REAR_RIGHT));
    CHECK_FALSE(door_child_lock_active(DOOR_REAR_RIGHT));
}

TEST(ChildSafetyLock, EnableOnFrontDoorFails)
{
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, door_child_lock_enable(DOOR_FRONT_LEFT));
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, door_child_lock_enable(DOOR_FRONT_RIGHT));
}

TEST(ChildSafetyLock, InvalidDoorReturnsFalse)
{
    CHECK_FALSE(door_child_lock_active((door_position_t)99));
}

/* =============================================================================
 * Test Group: Auto-Lock Feature
 * ========================================================================== */

TEST_GROUP(AutoLockFeature)
{
    void setup() override
    {
        system_state_init();
        door_control_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(AutoLockFeature, IsEnabledByDefault)
{
    CHECK_TRUE(door_auto_lock_enabled());
}

TEST(AutoLockFeature, CanBeDisabled)
{
    door_auto_lock_enable(false);
    CHECK_FALSE(door_auto_lock_enabled());
}

TEST(AutoLockFeature, CanBeReEnabled)
{
    door_auto_lock_enable(false);
    door_auto_lock_enable(true);
    CHECK_TRUE(door_auto_lock_enabled());
}

/* =============================================================================
 * Test Group: Door Status
 * ========================================================================== */

TEST_GROUP(DoorStatus)
{
    void setup() override
    {
        system_state_init();
        door_control_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(DoorStatus, GetStatusSucceeds)
{
    door_status_t status;
    bcm_result_t result = door_get_status(DOOR_FRONT_LEFT, &status);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DoorStatus, GetStatusWithNullPointerFails)
{
    bcm_result_t result = door_get_status(DOOR_FRONT_LEFT, nullptr);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(DoorStatus, InvalidDoorReturnsFalse)
{
    CHECK_FALSE(door_is_open((door_position_t)99));
}

/* =============================================================================
 * Test Group: CAN Message Handling
 * ========================================================================== */

TEST_GROUP(DoorCanMessages)
{
    void setup() override
    {
        system_state_init();
        door_control_init();
    }

    void teardown() override
    {
        door_control_deinit();
        mock().clear();
    }
};

TEST(DoorCanMessages, HandleLockAllCommand)
{
    uint8_t data[] = {0x01};  /* DOOR_CMD_LOCK_ALL */
    bcm_result_t result = door_control_handle_can_cmd(data, sizeof(data));
    CHECK_EQUAL(BCM_OK, result);
    CHECK_TRUE(door_all_locked());
}

TEST(DoorCanMessages, HandleUnlockAllCommand)
{
    door_lock_all();
    uint8_t data[] = {0x02};  /* DOOR_CMD_UNLOCK_ALL */
    bcm_result_t result = door_control_handle_can_cmd(data, sizeof(data));
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DoorCanMessages, HandleNullDataFails)
{
    bcm_result_t result = door_control_handle_can_cmd(nullptr, 0);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(DoorCanMessages, HandleInvalidCommandFails)
{
    uint8_t data[] = {0xFF};
    bcm_result_t result = door_control_handle_can_cmd(data, sizeof(data));
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(DoorCanMessages, GetCanStatusFillsBuffer)
{
    uint8_t data[8] = {0};
    uint8_t len = 0;
    door_control_get_can_status(data, &len);
    CHECK_EQUAL(8, len);
}

/* =============================================================================
 * CppUTest Main - Run Door Control Tests
 * ========================================================================== */

/* Note: Main is provided in a separate file or by CppUTest */
