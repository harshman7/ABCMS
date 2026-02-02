/**
 * @file test_fault_manager.cpp
 * @brief Unit tests for Fault Manager Module
 *
 * Tests:
 * - Fault set/clear behavior
 * - Fault flags correctness
 * - Status frame payload
 * - Multiple faults handling
 */

#include "CppUTest/TestHarness.h"

extern "C" {
#include "fault_manager.h"
#include "can_interface.h"
#include "can_ids.h"
}

/*******************************************************************************
 * Test Group: Fault Manager Initialization
 ******************************************************************************/

TEST_GROUP(FaultInit)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(FaultInit, NoFaultsInitially)
{
    CHECK_EQUAL(0, fault_manager_get_count());
}

TEST(FaultInit, FlagsZeroInitially)
{
    CHECK_EQUAL(0, fault_manager_get_flags1());
    CHECK_EQUAL(0, fault_manager_get_flags2());
}

TEST(FaultInit, MostRecentNoneInitially)
{
    CHECK_EQUAL(FAULT_CODE_NONE, fault_manager_get_most_recent());
}

/*******************************************************************************
 * Test Group: Fault Set/Clear
 ******************************************************************************/

TEST_GROUP(FaultSetClear)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(FaultSetClear, SetFaultIncrementsCount)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    CHECK_EQUAL(1, fault_manager_get_count());
}

TEST(FaultSetClear, SetFaultIsActive)
{
    fault_manager_set(FAULT_CODE_HEADLIGHT_BULB);
    CHECK_TRUE(fault_manager_is_active(FAULT_CODE_HEADLIGHT_BULB));
}

TEST(FaultSetClear, ClearFaultDecrementsCount)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    fault_manager_set(FAULT_CODE_TURN_BULB);
    CHECK_EQUAL(2, fault_manager_get_count());
    
    fault_manager_clear(FAULT_CODE_DOOR_MOTOR);
    CHECK_EQUAL(1, fault_manager_get_count());
}

TEST(FaultSetClear, ClearFaultIsNotActive)
{
    fault_manager_set(FAULT_CODE_CAN_COMM);
    fault_manager_clear(FAULT_CODE_CAN_COMM);
    CHECK_FALSE(fault_manager_is_active(FAULT_CODE_CAN_COMM));
}

TEST(FaultSetClear, ClearAllFaults)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    fault_manager_set(FAULT_CODE_HEADLIGHT_BULB);
    fault_manager_set(FAULT_CODE_TURN_BULB);
    CHECK_EQUAL(3, fault_manager_get_count());
    
    fault_manager_clear_all();
    CHECK_EQUAL(0, fault_manager_get_count());
}

TEST(FaultSetClear, SetSameFaultTwiceNoDouble)
{
    fault_manager_set(FAULT_CODE_TIMEOUT);
    fault_manager_set(FAULT_CODE_TIMEOUT);
    CHECK_EQUAL(1, fault_manager_get_count());
}

TEST(FaultSetClear, ClearNonexistentFaultNoError)
{
    fault_manager_clear(FAULT_CODE_TIMEOUT);
    CHECK_EQUAL(0, fault_manager_get_count());
}

TEST(FaultSetClear, MostRecentUpdated)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    CHECK_EQUAL(FAULT_CODE_DOOR_MOTOR, fault_manager_get_most_recent());
    
    fault_manager_set(FAULT_CODE_TURN_BULB);
    CHECK_EQUAL(FAULT_CODE_TURN_BULB, fault_manager_get_most_recent());
}

/*******************************************************************************
 * Test Group: Fault Flags
 ******************************************************************************/

TEST_GROUP(FaultFlags)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(FaultFlags, DoorMotorFaultSetsFlag)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    CHECK_EQUAL(FAULT_BIT_DOOR_MOTOR, fault_manager_get_flags1() & FAULT_BIT_DOOR_MOTOR);
}

TEST(FaultFlags, HeadlightBulbFaultSetsFlag)
{
    fault_manager_set(FAULT_CODE_HEADLIGHT_BULB);
    CHECK_EQUAL(FAULT_BIT_HEADLIGHT_BULB, fault_manager_get_flags1() & FAULT_BIT_HEADLIGHT_BULB);
}

TEST(FaultFlags, TurnBulbFaultSetsFlag)
{
    fault_manager_set(FAULT_CODE_TURN_BULB);
    CHECK_EQUAL(FAULT_BIT_TURN_BULB, fault_manager_get_flags1() & FAULT_BIT_TURN_BULB);
}

TEST(FaultFlags, ChecksumFaultSetsFlag)
{
    fault_manager_set(FAULT_CODE_INVALID_CHECKSUM);
    CHECK_EQUAL(FAULT_BIT_CMD_CHECKSUM, fault_manager_get_flags1() & FAULT_BIT_CMD_CHECKSUM);
}

TEST(FaultFlags, CounterFaultSetsFlag)
{
    fault_manager_set(FAULT_CODE_INVALID_COUNTER);
    CHECK_EQUAL(FAULT_BIT_CMD_COUNTER, fault_manager_get_flags1() & FAULT_BIT_CMD_COUNTER);
}

TEST(FaultFlags, TimeoutFaultSetsFlag)
{
    fault_manager_set(FAULT_CODE_TIMEOUT);
    CHECK_EQUAL(FAULT_BIT_TIMEOUT, fault_manager_get_flags1() & FAULT_BIT_TIMEOUT);
}

TEST(FaultFlags, ClearFaultClearsFlag)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    CHECK_EQUAL(FAULT_BIT_DOOR_MOTOR, fault_manager_get_flags1() & FAULT_BIT_DOOR_MOTOR);
    
    fault_manager_clear(FAULT_CODE_DOOR_MOTOR);
    CHECK_EQUAL(0, fault_manager_get_flags1() & FAULT_BIT_DOOR_MOTOR);
}

TEST(FaultFlags, MultipleFlagsSet)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    fault_manager_set(FAULT_CODE_HEADLIGHT_BULB);
    fault_manager_set(FAULT_CODE_TIMEOUT);
    
    uint8_t expected = FAULT_BIT_DOOR_MOTOR | FAULT_BIT_HEADLIGHT_BULB | FAULT_BIT_TIMEOUT;
    CHECK_EQUAL(expected, fault_manager_get_flags1());
}

/*******************************************************************************
 * Test Group: Fault Status Frame
 ******************************************************************************/

TEST_GROUP(FaultStatusFrame)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(FaultStatusFrame, HasCorrectId)
{
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    CHECK_EQUAL(CAN_ID_FAULT_STATUS, frame.id);
}

TEST(FaultStatusFrame, HasCorrectDlc)
{
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    CHECK_EQUAL(FAULT_STATUS_DLC, frame.dlc);
}

TEST(FaultStatusFrame, Flags1Correct)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    fault_manager_set(FAULT_CODE_TURN_BULB);
    
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    
    uint8_t expected = FAULT_BIT_DOOR_MOTOR | FAULT_BIT_TURN_BULB;
    CHECK_EQUAL(expected, frame.data[FAULT_STATUS_BYTE_FLAGS1]);
}

TEST(FaultStatusFrame, CountCorrect)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    fault_manager_set(FAULT_CODE_TURN_BULB);
    fault_manager_set(FAULT_CODE_CAN_COMM);
    
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    
    /* Total count includes historical */
    CHECK_TRUE(frame.data[FAULT_STATUS_BYTE_COUNT] >= 3);
}

TEST(FaultStatusFrame, MostRecentCodeCorrect)
{
    fault_manager_set(FAULT_CODE_TIMEOUT);
    
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    
    CHECK_EQUAL(FAULT_CODE_TIMEOUT, frame.data[FAULT_STATUS_BYTE_RECENT_CODE]);
}

TEST(FaultStatusFrame, ChecksumValid)
{
    fault_manager_set(FAULT_CODE_DOOR_MOTOR);
    
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    
    uint8_t calc = can_calculate_checksum(frame.data, FAULT_STATUS_DLC - 1);
    CHECK_EQUAL(calc, frame.data[FAULT_STATUS_BYTE_CHECKSUM]);
}

TEST(FaultStatusFrame, VersionCorrect)
{
    can_frame_t frame;
    fault_manager_build_status_frame(&frame);
    
    uint8_t version = CAN_GET_VERSION(frame.data[FAULT_STATUS_BYTE_VER_CTR]);
    CHECK_EQUAL(CAN_SCHEMA_VERSION, version);
}

TEST(FaultStatusFrame, CounterIncrements)
{
    can_frame_t frame1, frame2;
    
    fault_manager_build_status_frame(&frame1);
    uint8_t counter1 = CAN_GET_COUNTER(frame1.data[FAULT_STATUS_BYTE_VER_CTR]);
    
    fault_manager_build_status_frame(&frame2);
    uint8_t counter2 = CAN_GET_COUNTER(frame2.data[FAULT_STATUS_BYTE_VER_CTR]);
    
    CHECK_EQUAL((counter1 + 1) & CAN_COUNTER_MASK, counter2);
}

/*******************************************************************************
 * Test Group: Fault Manager Edge Cases
 ******************************************************************************/

TEST_GROUP(FaultEdgeCases)
{
    void setup() override
    {
        sys_state_init();
        can_init(NULL);
        fault_manager_init();
    }

    void teardown() override
    {
        can_deinit();
    }
};

TEST(FaultEdgeCases, MaxFaultsHandled)
{
    /* Set maximum faults */
    for (int i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        fault_manager_set((fault_code_t)(0x10 + i));
    }
    
    CHECK_EQUAL(MAX_ACTIVE_FAULTS, fault_manager_get_count());
    
    /* Try to add one more - should be silently ignored */
    fault_manager_set(FAULT_CODE_TIMEOUT);
    CHECK_EQUAL(MAX_ACTIVE_FAULTS, fault_manager_get_count());
}

TEST(FaultEdgeCases, ClearAllAfterMax)
{
    for (int i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        fault_manager_set((fault_code_t)(0x10 + i));
    }
    
    fault_manager_clear_all();
    CHECK_EQUAL(0, fault_manager_get_count());
    CHECK_EQUAL(0, fault_manager_get_flags1());
}

TEST(FaultEdgeCases, UnknownFaultCodeNoFlag)
{
    fault_manager_set((fault_code_t)0x99);
    CHECK_TRUE(fault_manager_is_active((fault_code_t)0x99));
    CHECK_EQUAL(0, fault_manager_get_flags1()); /* No flag mapped */
}
