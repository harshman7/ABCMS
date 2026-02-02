/**
 * @file test_door_control.cpp
 * @brief Unit tests for Door Control Module
 *
 * Tests:
 * - Lock/unlock commands
 * - Invalid payload rejection
 * - Checksum validation
 * - Rolling counter validation
 * - State machine transitions
 */

#include "CppUTest/TestHarness.h"

extern "C" {
#include "door_control.h"
#include "fault_manager.h"
#include "can_interface.h"
#include "can_ids.h"
}

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

static can_frame_t build_door_cmd(uint8_t cmd, uint8_t door_id, uint8_t counter)
{
    can_frame_t frame;
    frame.id = CAN_ID_DOOR_CMD;
    frame.dlc = DOOR_CMD_DLC;
    
    frame.data[DOOR_CMD_BYTE_CMD] = cmd;
    frame.data[DOOR_CMD_BYTE_DOOR_ID] = door_id;
    frame.data[DOOR_CMD_BYTE_VER_CTR] = CAN_BUILD_VER_CTR(CAN_SCHEMA_VERSION, counter);
    frame.data[DOOR_CMD_BYTE_CHECKSUM] = can_calculate_checksum(frame.data, DOOR_CMD_DLC - 1);
    
    return frame;
}

/*******************************************************************************
 * Test Group: Door Control Initialization
 ******************************************************************************/

TEST_GROUP(DoorInit)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        door_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(DoorInit, AllDoorsUnlockedInitially)
{
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(i));
    }
}

TEST(DoorInit, NotAllLockedInitially)
{
    CHECK_FALSE(door_control_all_locked());
}

/*******************************************************************************
 * Test Group: Door Lock Commands
 ******************************************************************************/

TEST_GROUP(DoorLockCommands)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        door_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(DoorLockCommands, LockAllSucceeds)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 0);
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    
    /* State should be LOCKING */
    for (uint8_t i = 0; i < NUM_DOORS; i++) {
        CHECK_EQUAL(DOOR_STATE_LOCKING, door_control_get_lock_state(i));
    }
    
    /* After update, should be LOCKED */
    door_control_update(100);
    CHECK_TRUE(door_control_all_locked());
}

TEST(DoorLockCommands, UnlockAllSucceeds)
{
    /* First lock all */
    door_control_lock_all();
    door_control_update(100);
    CHECK_TRUE(door_control_all_locked());
    
    /* Then unlock */
    can_frame_t frame = build_door_cmd(DOOR_CMD_UNLOCK_ALL, DOOR_ID_ALL, 0);
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    
    door_control_update(200);
    CHECK_FALSE(door_control_all_locked());
}

TEST(DoorLockCommands, LockSingleDoor)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_SINGLE, DOOR_ID_FRONT_LEFT, 0);
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    
    door_control_update(100);
    
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(DOOR_ID_FRONT_LEFT));
    CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(DOOR_ID_FRONT_RIGHT));
}

TEST(DoorLockCommands, UnlockSingleDoor)
{
    /* Lock all first */
    door_control_lock_all();
    door_control_update(100);
    
    /* Unlock single door */
    can_frame_t frame = build_door_cmd(DOOR_CMD_UNLOCK_SINGLE, DOOR_ID_REAR_RIGHT, 1);
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_OK, result);
    
    door_control_update(200);
    
    CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(DOOR_ID_REAR_RIGHT));
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(DOOR_ID_FRONT_LEFT));
}

/*******************************************************************************
 * Test Group: Door Command Validation
 ******************************************************************************/

TEST_GROUP(DoorCommandValidation)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        door_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(DoorCommandValidation, RejectsWrongDLC)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 0);
    frame.dlc = 3; /* Wrong length */
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
    CHECK_TRUE(fault_manager_is_active(FAULT_CODE_INVALID_LENGTH));
}

TEST(DoorCommandValidation, RejectsInvalidChecksum)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 0);
    frame.data[DOOR_CMD_BYTE_CHECKSUM] = 0xFF; /* Bad checksum */
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_CHECKSUM_ERROR, result);
    CHECK_TRUE(fault_manager_is_active(FAULT_CODE_INVALID_CHECKSUM));
}

TEST(DoorCommandValidation, RejectsInvalidCounter)
{
    /* First valid command */
    can_frame_t frame1 = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 5);
    cmd_result_t result1 = door_control_handle_cmd(&frame1);
    CHECK_EQUAL(CMD_RESULT_OK, result1);
    
    /* Second command with wrong counter (should be 6, not 7) */
    can_frame_t frame2 = build_door_cmd(DOOR_CMD_UNLOCK_ALL, DOOR_ID_ALL, 7);
    cmd_result_t result2 = door_control_handle_cmd(&frame2);
    CHECK_EQUAL(CMD_RESULT_COUNTER_ERROR, result2);
    CHECK_TRUE(fault_manager_is_active(FAULT_CODE_INVALID_COUNTER));
}

TEST(DoorCommandValidation, AcceptsWrappingCounter)
{
    /* Command with counter 15 */
    can_frame_t frame1 = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 15);
    cmd_result_t result1 = door_control_handle_cmd(&frame1);
    CHECK_EQUAL(CMD_RESULT_OK, result1);
    
    /* Next command should have counter 0 (wrap) */
    can_frame_t frame2 = build_door_cmd(DOOR_CMD_UNLOCK_ALL, DOOR_ID_ALL, 0);
    cmd_result_t result2 = door_control_handle_cmd(&frame2);
    CHECK_EQUAL(CMD_RESULT_OK, result2);
}

TEST(DoorCommandValidation, RejectsInvalidCommand)
{
    can_frame_t frame = build_door_cmd(0xFF, DOOR_ID_ALL, 0);
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

TEST(DoorCommandValidation, RejectsInvalidDoorId)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_SINGLE, 0x10, 0);
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

TEST(DoorCommandValidation, RejectsNullFrame)
{
    cmd_result_t result = door_control_handle_cmd(NULL);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

TEST(DoorCommandValidation, RejectsWrongCanId)
{
    can_frame_t frame = build_door_cmd(DOOR_CMD_LOCK_ALL, DOOR_ID_ALL, 0);
    frame.id = 0x999; /* Wrong ID */
    
    cmd_result_t result = door_control_handle_cmd(&frame);
    CHECK_EQUAL(CMD_RESULT_INVALID_CMD, result);
}

/*******************************************************************************
 * Test Group: Door State Machine Transitions
 ******************************************************************************/

TEST_GROUP(DoorStateMachine)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        door_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(DoorStateMachine, UnlockedToLockingToLocked)
{
    CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(0));
    
    door_control_lock(0);
    CHECK_EQUAL(DOOR_STATE_LOCKING, door_control_get_lock_state(0));
    
    door_control_update(100);
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(0));
}

TEST(DoorStateMachine, LockedToUnlockingToUnlocked)
{
    door_control_lock(0);
    door_control_update(100);
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(0));
    
    door_control_unlock(0);
    CHECK_EQUAL(DOOR_STATE_UNLOCKING, door_control_get_lock_state(0));
    
    door_control_update(200);
    CHECK_EQUAL(DOOR_STATE_UNLOCKED, door_control_get_lock_state(0));
}

TEST(DoorStateMachine, LockingAlreadyLockedNoChange)
{
    door_control_lock(0);
    door_control_update(100);
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(0));
    
    /* Try to lock again */
    door_control_lock(0);
    CHECK_EQUAL(DOOR_STATE_LOCKED, door_control_get_lock_state(0));
}

/*******************************************************************************
 * Test Group: Door Status Frame
 ******************************************************************************/

TEST_GROUP(DoorStatusFrame)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
        door_control_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(DoorStatusFrame, HasCorrectId)
{
    can_frame_t frame;
    door_control_build_status_frame(&frame);
    CHECK_EQUAL(CAN_ID_DOOR_STATUS, frame.id);
}

TEST(DoorStatusFrame, HasCorrectDlc)
{
    can_frame_t frame;
    door_control_build_status_frame(&frame);
    CHECK_EQUAL(DOOR_STATUS_DLC, frame.dlc);
}

TEST(DoorStatusFrame, LockBitsCorrect)
{
    door_control_lock(DOOR_ID_FRONT_LEFT);
    door_control_lock(DOOR_ID_REAR_RIGHT);
    door_control_update(100);
    
    can_frame_t frame;
    door_control_build_status_frame(&frame);
    
    CHECK_EQUAL(DOOR_LOCK_BIT_FL | DOOR_LOCK_BIT_RR, frame.data[DOOR_STATUS_BYTE_LOCKS]);
}

TEST(DoorStatusFrame, ChecksumValid)
{
    can_frame_t frame;
    door_control_build_status_frame(&frame);
    
    uint8_t calc = can_calculate_checksum(frame.data, DOOR_STATUS_DLC - 1);
    CHECK_EQUAL(calc, frame.data[DOOR_STATUS_BYTE_CHECKSUM]);
}

TEST(DoorStatusFrame, CounterIncrements)
{
    can_frame_t frame1, frame2;
    
    door_control_build_status_frame(&frame1);
    uint8_t counter1 = CAN_GET_COUNTER(frame1.data[DOOR_STATUS_BYTE_VER_CTR]);
    
    door_control_build_status_frame(&frame2);
    uint8_t counter2 = CAN_GET_COUNTER(frame2.data[DOOR_STATUS_BYTE_VER_CTR]);
    
    CHECK_EQUAL((counter1 + 1) & CAN_COUNTER_MASK, counter2);
}
