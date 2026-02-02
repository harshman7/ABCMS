/**
 * @file test_fault_manager.cpp
 * @brief Unit tests for Fault Management Module
 *
 * Tests for fault detection, logging, recovery, and diagnostics.
 */

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

extern "C" {
#include "fault_manager.h"
#include "system_state.h"
#include "bcm_config.h"
}

/* Test fault codes */
static const fault_code_t TEST_FAULT_1 = 0x1001;
static const fault_code_t TEST_FAULT_2 = 0x1002;
static const fault_code_t TEST_FAULT_CRITICAL = 0x5001;

/* =============================================================================
 * Test Group: Fault Manager Initialization
 * ========================================================================== */

TEST_GROUP(FaultManagerInit)
{
    void setup() override
    {
        system_state_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultManagerInit, InitializesSuccessfully)
{
    bcm_result_t result = fault_manager_init();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultManagerInit, CanBeDeinitialized)
{
    fault_manager_init();
    fault_manager_deinit();
    /* Should not crash */
}

TEST(FaultManagerInit, NoFaultsAfterInit)
{
    fault_manager_init();
    CHECK_EQUAL(0, fault_get_active_count());
    CHECK_EQUAL(0, fault_get_stored_count());
}

/* =============================================================================
 * Test Group: Fault Reporting
 * ========================================================================== */

TEST_GROUP(FaultReporting)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultReporting, ReportFaultSucceeds)
{
    bcm_result_t result = fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultReporting, ReportedFaultIsPending)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_EQUAL(FAULT_STATUS_PENDING, fault_get_status(TEST_FAULT_1));
}

TEST(FaultReporting, ReportedFaultIsPresent)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_TRUE(fault_is_present(TEST_FAULT_1));
}

TEST(FaultReporting, MultipleFaultsCanBeReported)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_ERROR);
    
    CHECK_TRUE(fault_is_present(TEST_FAULT_1));
    CHECK_TRUE(fault_is_present(TEST_FAULT_2));
}

TEST(FaultReporting, ReportWithFreezeFrameSucceeds)
{
    uint8_t freeze_frame[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    bcm_result_t result = fault_report_with_data(TEST_FAULT_1, 
                                                  FAULT_SEVERITY_WARNING,
                                                  freeze_frame);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultReporting, StoredCountIncrements)
{
    CHECK_EQUAL(0, fault_get_stored_count());
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_EQUAL(1, fault_get_stored_count());
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_WARNING);
    CHECK_EQUAL(2, fault_get_stored_count());
}

TEST(FaultReporting, SameFaultReportedTwiceOnlyStoredOnce)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_EQUAL(1, fault_get_stored_count());
}

/* =============================================================================
 * Test Group: Fault Clearing
 * ========================================================================== */

TEST_GROUP(FaultClearing)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultClearing, ClearFaultSucceeds)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    bcm_result_t result = fault_clear(TEST_FAULT_1);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultClearing, ClearedFaultIsInactive)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_clear(TEST_FAULT_1);
    CHECK_EQUAL(FAULT_STATUS_INACTIVE, fault_get_status(TEST_FAULT_1));
}

TEST(FaultClearing, ClearedFaultIsNotPresent)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_clear(TEST_FAULT_1);
    CHECK_FALSE(fault_is_present(TEST_FAULT_1));
}

TEST(FaultClearing, ClearAllFaultsSucceeds)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_ERROR);
    bcm_result_t result = fault_clear_all();
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultClearing, ClearAllMakesAllInactive)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_ERROR);
    fault_clear_all();
    
    CHECK_FALSE(fault_is_present(TEST_FAULT_1));
    CHECK_FALSE(fault_is_present(TEST_FAULT_2));
}

TEST(FaultClearing, ClearNonExistentFaultReturnsError)
{
    bcm_result_t result = fault_clear(0xFFFF);
    CHECK_EQUAL(BCM_ERROR, result);
}

/* =============================================================================
 * Test Group: Fault Healing
 * ========================================================================== */

TEST_GROUP(FaultHealing)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultHealing, HealFaultSucceeds)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    bcm_result_t result = fault_heal(TEST_FAULT_1);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultHealing, HealedFaultHasHealedStatus)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_heal(TEST_FAULT_1);
    CHECK_EQUAL(FAULT_STATUS_HEALED, fault_get_status(TEST_FAULT_1));
}

TEST(FaultHealing, HealNonExistentFaultReturnsError)
{
    bcm_result_t result = fault_heal(0xFFFF);
    CHECK_EQUAL(BCM_ERROR, result);
}

/* =============================================================================
 * Test Group: Fault Status Queries
 * ========================================================================== */

TEST_GROUP(FaultStatusQueries)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultStatusQueries, IsActiveReturnsFalseForInactive)
{
    CHECK_FALSE(fault_is_active(TEST_FAULT_1));
}

TEST(FaultStatusQueries, IsPresentReturnsFalseForUnreported)
{
    CHECK_FALSE(fault_is_present(TEST_FAULT_1));
}

TEST(FaultStatusQueries, GetStatusReturnsInactiveForUnreported)
{
    CHECK_EQUAL(FAULT_STATUS_INACTIVE, fault_get_status(0xFFFF));
}

TEST(FaultStatusQueries, GetEntrySucceeds)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    
    fault_entry_t entry;
    bcm_result_t result = fault_get_entry(TEST_FAULT_1, &entry);
    
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(TEST_FAULT_1, entry.code);
    CHECK_EQUAL(FAULT_SEVERITY_WARNING, entry.severity);
}

TEST(FaultStatusQueries, GetEntryWithNullPointerFails)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    bcm_result_t result = fault_get_entry(TEST_FAULT_1, nullptr);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(FaultStatusQueries, GetEntryForNonExistentFails)
{
    fault_entry_t entry;
    bcm_result_t result = fault_get_entry(0xFFFF, &entry);
    CHECK_EQUAL(BCM_ERROR, result);
}

TEST(FaultStatusQueries, GetActiveCountReturnsCorrectValue)
{
    CHECK_EQUAL(0, fault_get_active_count());
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_TRUE(fault_get_active_count() >= 1);  /* Pending counts */
}

/* =============================================================================
 * Test Group: Critical Faults
 * ========================================================================== */

TEST_GROUP(CriticalFaults)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(CriticalFaults, AnyCriticalReturnsFalseWhenNone)
{
    CHECK_FALSE(fault_any_critical());
}

TEST(CriticalFaults, AnyCriticalReturnsTrueWhenCriticalPresent)
{
    fault_report(TEST_FAULT_CRITICAL, FAULT_SEVERITY_CRITICAL);
    CHECK_TRUE(fault_any_critical());
}

TEST(CriticalFaults, AnyCriticalReturnsFalseForWarnings)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    CHECK_FALSE(fault_any_critical());
}

TEST(CriticalFaults, AnyCriticalReturnsFalseForErrors)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_ERROR);
    CHECK_FALSE(fault_any_critical());
}

/* =============================================================================
 * Test Group: Fault Log Access
 * ========================================================================== */

TEST_GROUP(FaultLogAccess)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultLogAccess, GetByIndexSucceeds)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    
    fault_entry_t entry;
    bcm_result_t result = fault_get_by_index(0, &entry);
    
    CHECK_EQUAL(BCM_OK, result);
    CHECK_EQUAL(TEST_FAULT_1, entry.code);
}

TEST(FaultLogAccess, GetByIndexInvalidIndexFails)
{
    fault_entry_t entry;
    bcm_result_t result = fault_get_by_index(999, &entry);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(FaultLogAccess, GetByIndexWithNullPointerFails)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    bcm_result_t result = fault_get_by_index(0, nullptr);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(FaultLogAccess, GetActiveCodesReturnsCorrectCodes)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_ERROR);
    
    fault_code_t codes[10];
    uint16_t count = fault_get_active_codes(codes, 10);
    
    CHECK_TRUE(count >= 2);
}

TEST(FaultLogAccess, GetActiveCodesWithNullReturnsZero)
{
    uint16_t count = fault_get_active_codes(nullptr, 10);
    CHECK_EQUAL(0, count);
}

TEST(FaultLogAccess, GetSnapshotReturnsData)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    
    uint8_t buffer[64];
    uint16_t len = fault_get_snapshot(buffer, sizeof(buffer));
    
    CHECK_TRUE(len > 0);
}

TEST(FaultLogAccess, GetSnapshotWithNullReturnsZero)
{
    uint16_t len = fault_get_snapshot(nullptr, 64);
    CHECK_EQUAL(0, len);
}

/* =============================================================================
 * Test Group: Fault Recovery
 * ========================================================================== */

static bcm_result_t test_recovery_action(fault_code_t code)
{
    (void)code;
    return BCM_OK;
}

static bcm_result_t test_recovery_fail(fault_code_t code)
{
    (void)code;
    return BCM_ERROR;
}

TEST_GROUP(FaultRecovery)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultRecovery, RegisterRecoverySucceeds)
{
    bcm_result_t result = fault_register_recovery(TEST_FAULT_1, test_recovery_action);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultRecovery, RegisterNullActionFails)
{
    bcm_result_t result = fault_register_recovery(TEST_FAULT_1, nullptr);
    CHECK_EQUAL(BCM_ERROR_INVALID_PARAM, result);
}

TEST(FaultRecovery, AttemptRecoveryWithoutRegistrationFails)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    bcm_result_t result = fault_attempt_recovery(TEST_FAULT_1);
    CHECK_EQUAL(BCM_ERROR_NOT_SUPPORTED, result);
}

TEST(FaultRecovery, AttemptRecoveryCallsAction)
{
    fault_register_recovery(TEST_FAULT_1, test_recovery_action);
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    
    bcm_result_t result = fault_attempt_recovery(TEST_FAULT_1);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(FaultRecovery, FailedRecoveryReturnsError)
{
    fault_register_recovery(TEST_FAULT_2, test_recovery_fail);
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_WARNING);
    
    bcm_result_t result = fault_attempt_recovery(TEST_FAULT_2);
    CHECK_EQUAL(BCM_ERROR, result);
}

/* =============================================================================
 * Test Group: Diagnostic Interface
 * ========================================================================== */

TEST_GROUP(DiagnosticInterface)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(DiagnosticInterface, ReadDtcByStatusReturnsData)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    
    uint8_t buffer[64];
    uint16_t len = fault_read_dtc_by_status(0xFF, buffer, sizeof(buffer));
    
    CHECK_TRUE(len > 0);
}

TEST(DiagnosticInterface, ClearDtcSucceeds)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    bcm_result_t result = fault_clear_dtc(0xFFFFFF);
    CHECK_EQUAL(BCM_OK, result);
}

TEST(DiagnosticInterface, ClearDtcClearsAllFaults)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    fault_report(TEST_FAULT_2, FAULT_SEVERITY_ERROR);
    
    fault_clear_dtc(0xFFFFFF);
    
    CHECK_FALSE(fault_is_present(TEST_FAULT_1));
    CHECK_FALSE(fault_is_present(TEST_FAULT_2));
}

/* =============================================================================
 * Test Group: CAN Message Handling
 * ========================================================================== */

TEST_GROUP(FaultCanMessages)
{
    void setup() override
    {
        system_state_init();
        fault_manager_init();
    }

    void teardown() override
    {
        fault_manager_deinit();
        mock().clear();
    }
};

TEST(FaultCanMessages, GetCanStatusFillsBuffer)
{
    uint8_t data[8] = {0};
    uint8_t len = 0;
    fault_manager_get_can_status(data, &len);
    CHECK_EQUAL(8, len);
}

TEST(FaultCanMessages, CanStatusContainsActiveCount)
{
    fault_report(TEST_FAULT_1, FAULT_SEVERITY_WARNING);
    
    uint8_t data[8] = {0};
    uint8_t len = 0;
    fault_manager_get_can_status(data, &len);
    
    /* Active count in bytes 0-1 */
    uint16_t active = (uint16_t)((uint16_t)data[0] << 8) | data[1];
    CHECK_TRUE(active >= 1);
}

/* =============================================================================
 * CppUTest Main
 * ========================================================================== */

int main(int argc, char** argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
