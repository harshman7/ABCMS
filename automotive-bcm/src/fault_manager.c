/**
 * @file fault_manager.c
 * @brief Fault Management Module Implementation
 *
 * Implements fault detection, logging, recovery, and DTC management.
 */

#include <string.h>
#include "fault_manager.h"
#include "bcm_config.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Maximum number of registered recovery actions */
#define MAX_RECOVERY_ACTIONS    16

/** Fault entry with internal timing data */
typedef struct {
    fault_entry_t entry;
    uint32_t debounce_start;
    uint32_t healing_start;
    bool in_debounce;
    bool in_healing;
} fault_internal_t;

/** Recovery action registration */
typedef struct {
    fault_code_t code;
    fault_recovery_action_t action;
} recovery_registration_t;

/** Fault manager state */
typedef struct {
    bool initialized;
    fault_internal_t faults[BCM_MAX_FAULT_COUNT];
    uint16_t fault_count;
    recovery_registration_t recovery_actions[MAX_RECOVERY_ACTIONS];
    uint8_t recovery_count;
} fault_manager_state_t;

static fault_manager_state_t g_fault_mgr;

/* =============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Find fault entry by code
 * @return Index if found, -1 if not found
 */
static int find_fault_index(fault_code_t code)
{
    for (uint16_t i = 0; i < g_fault_mgr.fault_count; i++) {
        if (g_fault_mgr.faults[i].entry.code == code) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Find or create fault entry
 * @return Index of entry, -1 if full
 */
static int get_or_create_fault(fault_code_t code, fault_severity_t severity)
{
    int idx = find_fault_index(code);
    
    if (idx >= 0) {
        return idx;
    }
    
    /* Create new entry */
    if (g_fault_mgr.fault_count >= BCM_MAX_FAULT_COUNT) {
        return -1;  /* Fault log full */
    }
    
    idx = (int)g_fault_mgr.fault_count;
    memset(&g_fault_mgr.faults[idx], 0, sizeof(fault_internal_t));
    g_fault_mgr.faults[idx].entry.code = code;
    g_fault_mgr.faults[idx].entry.severity = severity;
    g_fault_mgr.faults[idx].entry.status = FAULT_STATUS_INACTIVE;
    g_fault_mgr.fault_count++;
    
    return idx;
}

/**
 * @brief Find recovery action for fault
 */
static fault_recovery_action_t find_recovery_action(fault_code_t code)
{
    for (uint8_t i = 0; i < g_fault_mgr.recovery_count; i++) {
        if (g_fault_mgr.recovery_actions[i].code == code) {
            return g_fault_mgr.recovery_actions[i].action;
        }
    }
    return NULL;
}

/* =============================================================================
 * Initialization
 * ========================================================================== */

bcm_result_t fault_manager_init(void)
{
    memset(&g_fault_mgr, 0, sizeof(g_fault_mgr));
    g_fault_mgr.initialized = true;
    return BCM_OK;
}

void fault_manager_deinit(void)
{
    g_fault_mgr.initialized = false;
}

/* =============================================================================
 * Processing
 * ========================================================================== */

bcm_result_t fault_manager_process(uint32_t timestamp_ms)
{
    if (!g_fault_mgr.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    for (uint16_t i = 0; i < g_fault_mgr.fault_count; i++) {
        fault_internal_t *fault = &g_fault_mgr.faults[i];
        
        /* Process debouncing */
        if (fault->in_debounce) {
            if ((timestamp_ms - fault->debounce_start) >= FAULT_DEBOUNCE_TIME_MS) {
                fault->entry.status = FAULT_STATUS_ACTIVE;
                fault->in_debounce = false;
                
                /* Attempt automatic recovery if configured */
                if (FAULT_ENABLE_RECOVERY && 
                    fault->entry.recovery_attempts < FAULT_MAX_RECOVERY_ATTEMPTS) {
                    fault_attempt_recovery(fault->entry.code);
                }
            }
        }
        
        /* Process healing */
        if (fault->in_healing) {
            if ((timestamp_ms - fault->healing_start) >= FAULT_HEALING_TIME_MS) {
                fault->entry.status = FAULT_STATUS_STORED;
                fault->in_healing = false;
            }
        }
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Fault Reporting
 * ========================================================================== */

bcm_result_t fault_report(fault_code_t code, fault_severity_t severity)
{
    return fault_report_with_data(code, severity, NULL);
}

bcm_result_t fault_report_with_data(fault_code_t code, 
                                     fault_severity_t severity,
                                     const uint8_t *freeze_frame)
{
    if (!g_fault_mgr.initialized) {
        return BCM_ERROR_NOT_READY;
    }
    
    int idx = get_or_create_fault(code, severity);
    if (idx < 0) {
        return BCM_ERROR_BUFFER_FULL;
    }
    
    fault_internal_t *fault = &g_fault_mgr.faults[idx];
    uint32_t now = system_state_get()->uptime_ms;
    
    /* Update fault data */
    fault->entry.last_occurrence = now;
    fault->entry.occurrence_count++;
    
    if (fault->entry.first_occurrence == 0) {
        fault->entry.first_occurrence = now;
    }
    
    if (freeze_frame) {
        memcpy(fault->entry.freeze_frame, freeze_frame, 8);
    }
    
    /* Start debounce if not already active/pending */
    if (fault->entry.status == FAULT_STATUS_INACTIVE ||
        fault->entry.status == FAULT_STATUS_STORED ||
        fault->entry.status == FAULT_STATUS_HEALED) {
        fault->entry.status = FAULT_STATUS_PENDING;
        fault->debounce_start = now;
        fault->in_debounce = true;
        fault->in_healing = false;
    }
    
    return BCM_OK;
}

bcm_result_t fault_clear(fault_code_t code)
{
    int idx = find_fault_index(code);
    if (idx < 0) {
        return BCM_ERROR;
    }
    
    g_fault_mgr.faults[idx].entry.status = FAULT_STATUS_INACTIVE;
    g_fault_mgr.faults[idx].in_debounce = false;
    g_fault_mgr.faults[idx].in_healing = false;
    
    return BCM_OK;
}

bcm_result_t fault_clear_all(void)
{
    for (uint16_t i = 0; i < g_fault_mgr.fault_count; i++) {
        g_fault_mgr.faults[i].entry.status = FAULT_STATUS_INACTIVE;
        g_fault_mgr.faults[i].in_debounce = false;
        g_fault_mgr.faults[i].in_healing = false;
    }
    
    return BCM_OK;
}

bcm_result_t fault_heal(fault_code_t code)
{
    int idx = find_fault_index(code);
    if (idx < 0) {
        return BCM_ERROR;
    }
    
    fault_internal_t *fault = &g_fault_mgr.faults[idx];
    
    if (fault->entry.status == FAULT_STATUS_ACTIVE ||
        fault->entry.status == FAULT_STATUS_PENDING) {
        fault->entry.status = FAULT_STATUS_HEALED;
        fault->healing_start = system_state_get()->uptime_ms;
        fault->in_healing = true;
        fault->in_debounce = false;
    }
    
    return BCM_OK;
}

/* =============================================================================
 * Fault Status Queries
 * ========================================================================== */

bool fault_is_active(fault_code_t code)
{
    int idx = find_fault_index(code);
    if (idx < 0) {
        return false;
    }
    
    return g_fault_mgr.faults[idx].entry.status == FAULT_STATUS_ACTIVE;
}

bool fault_is_present(fault_code_t code)
{
    int idx = find_fault_index(code);
    if (idx < 0) {
        return false;
    }
    
    fault_status_t status = g_fault_mgr.faults[idx].entry.status;
    return (status == FAULT_STATUS_ACTIVE || status == FAULT_STATUS_PENDING);
}

fault_status_t fault_get_status(fault_code_t code)
{
    int idx = find_fault_index(code);
    if (idx < 0) {
        return FAULT_STATUS_INACTIVE;
    }
    
    return g_fault_mgr.faults[idx].entry.status;
}

bcm_result_t fault_get_entry(fault_code_t code, fault_entry_t *entry)
{
    if (!entry) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    int idx = find_fault_index(code);
    if (idx < 0) {
        return BCM_ERROR;
    }
    
    *entry = g_fault_mgr.faults[idx].entry;
    return BCM_OK;
}

uint16_t fault_get_active_count(void)
{
    uint16_t count = 0;
    
    for (uint16_t i = 0; i < g_fault_mgr.fault_count; i++) {
        if (g_fault_mgr.faults[i].entry.status == FAULT_STATUS_ACTIVE ||
            g_fault_mgr.faults[i].entry.status == FAULT_STATUS_PENDING) {
            count++;
        }
    }
    
    return count;
}

uint16_t fault_get_stored_count(void)
{
    return g_fault_mgr.fault_count;
}

bool fault_any_critical(void)
{
    for (uint16_t i = 0; i < g_fault_mgr.fault_count; i++) {
        if (g_fault_mgr.faults[i].entry.severity == FAULT_SEVERITY_CRITICAL &&
            (g_fault_mgr.faults[i].entry.status == FAULT_STATUS_ACTIVE ||
             g_fault_mgr.faults[i].entry.status == FAULT_STATUS_PENDING)) {
            return true;
        }
    }
    
    return false;
}

/* =============================================================================
 * Fault Log Access
 * ========================================================================== */

bcm_result_t fault_get_by_index(uint16_t index, fault_entry_t *entry)
{
    if (!entry || index >= g_fault_mgr.fault_count) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    *entry = g_fault_mgr.faults[index].entry;
    return BCM_OK;
}

uint16_t fault_get_active_codes(fault_code_t *codes, uint16_t max_codes)
{
    if (!codes || max_codes == 0) {
        return 0;
    }
    
    uint16_t count = 0;
    
    for (uint16_t i = 0; i < g_fault_mgr.fault_count && count < max_codes; i++) {
        if (g_fault_mgr.faults[i].entry.status == FAULT_STATUS_ACTIVE ||
            g_fault_mgr.faults[i].entry.status == FAULT_STATUS_PENDING) {
            codes[count++] = g_fault_mgr.faults[i].entry.code;
        }
    }
    
    return count;
}

uint16_t fault_get_snapshot(uint8_t *buffer, uint16_t buffer_size)
{
    if (!buffer || buffer_size < 4) {
        return 0;
    }
    
    uint16_t offset = 0;
    
    /* Header: fault count */
    buffer[offset++] = (uint8_t)(g_fault_mgr.fault_count >> 8);
    buffer[offset++] = (uint8_t)(g_fault_mgr.fault_count);
    
    /* Active count */
    uint16_t active = fault_get_active_count();
    buffer[offset++] = (uint8_t)(active >> 8);
    buffer[offset++] = (uint8_t)(active);
    
    /* Fault codes (2 bytes each) */
    for (uint16_t i = 0; i < g_fault_mgr.fault_count && (offset + 3) <= buffer_size; i++) {
        buffer[offset++] = (uint8_t)(g_fault_mgr.faults[i].entry.code >> 8);
        buffer[offset++] = (uint8_t)(g_fault_mgr.faults[i].entry.code);
        buffer[offset++] = (uint8_t)g_fault_mgr.faults[i].entry.status;
    }
    
    return offset;
}

/* =============================================================================
 * Fault Recovery
 * ========================================================================== */

bcm_result_t fault_register_recovery(fault_code_t code, 
                                      fault_recovery_action_t action)
{
    if (!action) {
        return BCM_ERROR_INVALID_PARAM;
    }
    
    if (g_fault_mgr.recovery_count >= MAX_RECOVERY_ACTIONS) {
        return BCM_ERROR_BUFFER_FULL;
    }
    
    g_fault_mgr.recovery_actions[g_fault_mgr.recovery_count].code = code;
    g_fault_mgr.recovery_actions[g_fault_mgr.recovery_count].action = action;
    g_fault_mgr.recovery_count++;
    
    return BCM_OK;
}

bcm_result_t fault_attempt_recovery(fault_code_t code)
{
    int idx = find_fault_index(code);
    if (idx < 0) {
        return BCM_ERROR;
    }
    
    fault_recovery_action_t action = find_recovery_action(code);
    if (!action) {
        return BCM_ERROR_NOT_SUPPORTED;
    }
    
    g_fault_mgr.faults[idx].entry.recovery_attempts++;
    
    bcm_result_t result = action(code);
    if (result == BCM_OK) {
        /* Recovery successful - heal the fault */
        fault_heal(code);
    }
    
    return result;
}

/* =============================================================================
 * Diagnostic Interface
 * ========================================================================== */

uint16_t fault_read_dtc_by_status(uint8_t status_mask, 
                                   uint8_t *buffer, 
                                   uint16_t buffer_size)
{
    if (!buffer || buffer_size < 1) {
        return 0;
    }
    
    uint16_t offset = 0;
    
    /* Status availability mask */
    buffer[offset++] = status_mask;
    
    for (uint16_t i = 0; i < g_fault_mgr.fault_count && (offset + 4) <= buffer_size; i++) {
        fault_entry_t *entry = &g_fault_mgr.faults[i].entry;
        
        /* Convert internal status to UDS status byte */
        uint8_t uds_status = 0;
        if (entry->status == FAULT_STATUS_ACTIVE) {
            uds_status |= 0x01;  /* testFailed */
            uds_status |= 0x04;  /* pendingDTC */
            uds_status |= 0x08;  /* confirmedDTC */
        } else if (entry->status == FAULT_STATUS_PENDING) {
            uds_status |= 0x04;  /* pendingDTC */
        } else if (entry->status == FAULT_STATUS_STORED) {
            uds_status |= 0x08;  /* confirmedDTC */
        }
        
        if ((uds_status & status_mask) != 0) {
            /* DTC high byte */
            buffer[offset++] = (uint8_t)(entry->code >> 8);
            /* DTC low byte */
            buffer[offset++] = (uint8_t)(entry->code);
            /* DTC status byte */
            buffer[offset++] = uds_status;
        }
    }
    
    return offset;
}

bcm_result_t fault_clear_dtc(uint32_t group_of_dtc)
{
    if (group_of_dtc == 0xFFFFFF) {
        /* Clear all DTCs */
        return fault_clear_all();
    }
    
    /* Clear specific group - for now just clear if code matches */
    return fault_clear((fault_code_t)group_of_dtc);
}

/* =============================================================================
 * CAN Message Handling
 * ========================================================================== */

void fault_manager_get_can_status(uint8_t *data, uint8_t *len)
{
    uint16_t active_count = fault_get_active_count();
    uint16_t stored_count = fault_get_stored_count();
    
    /* Byte 0-1: Active fault count */
    data[0] = (uint8_t)(active_count >> 8);
    data[1] = (uint8_t)(active_count);
    
    /* Byte 2-3: Stored fault count */
    data[2] = (uint8_t)(stored_count >> 8);
    data[3] = (uint8_t)(stored_count);
    
    /* Byte 4-5: Most recent fault code */
    if (stored_count > 0) {
        fault_code_t recent = g_fault_mgr.faults[stored_count - 1].entry.code;
        data[4] = (uint8_t)(recent >> 8);
        data[5] = (uint8_t)(recent);
    } else {
        data[4] = 0;
        data[5] = 0;
    }
    
    /* Byte 6: Critical fault flag */
    data[6] = fault_any_critical() ? 0x01 : 0x00;
    
    /* Byte 7: Reserved */
    data[7] = 0;
    
    *len = 8;
}
