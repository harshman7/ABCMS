/**
 * @file main.c
 * @brief BCM Application Entry Point
 *
 * This file contains the main function and application initialization
 * for the Body Control Module.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

#include "bcm.h"
#include "bcm_config.h"

/* =============================================================================
 * Private Definitions
 * ========================================================================== */

/** Flag for graceful shutdown */
static volatile bool g_running = true;

/* =============================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int signum)
{
    (void)signum;
    printf("\nShutdown requested...\n");
    g_running = false;
}

/**
 * @brief Get current timestamp in milliseconds
 * @return Current time in milliseconds
 */
static uint32_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

/**
 * @brief Sleep for specified milliseconds
 * @param ms Milliseconds to sleep
 */
static void sleep_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/**
 * @brief Print BCM status to console
 */
static void print_status(void)
{
    const system_state_t *state = bcm_get_system_state();
    
    printf("\r[BCM] State: %d | Uptime: %u ms | Faults: %u    ",
           state->bcm_state,
           state->uptime_ms,
           state->active_faults);
    fflush(stdout);
}

/* =============================================================================
 * Main Function
 * ========================================================================== */

/**
 * @brief Application entry point
 * @return Exit code
 */
int main(void)
{
    bcm_result_t result;
    uint32_t timestamp;
    uint32_t last_status_print = 0;
    
    /* Print banner */
    printf("========================================\n");
    printf("  Automotive Body Control Module (BCM)\n");
    printf("  Version: %s\n", bcm_get_version());
    printf("========================================\n\n");
    
    /* Register signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize BCM */
    printf("[INIT] Initializing BCM...\n");
    result = bcm_init();
    if (result != BCM_OK) {
        fprintf(stderr, "[ERROR] BCM initialization failed: %d\n", result);
        return EXIT_FAILURE;
    }
    printf("[INIT] BCM initialization complete\n");
    
    /* Run self-test */
    printf("[INIT] Running self-test...\n");
    result = bcm_self_test();
    if (result != BCM_OK) {
        fprintf(stderr, "[WARN] Self-test reported issues: %d\n", result);
        /* Continue anyway for simulation */
    } else {
        printf("[INIT] Self-test passed\n");
    }
    
    printf("[RUN] BCM running. Press Ctrl+C to exit.\n\n");
    
    /* Main loop */
    while (g_running) {
        timestamp = get_timestamp_ms();
        
        /* Process BCM */
        result = bcm_process(timestamp);
        if (result != BCM_OK) {
            fprintf(stderr, "\n[ERROR] BCM process error: %d\n", result);
        }
        
        /* Print status periodically */
        if ((timestamp - last_status_print) >= 1000) {
            print_status();
            last_status_print = timestamp;
        }
        
        /* Sleep for cycle time */
        sleep_ms(BCM_MAIN_CYCLE_TIME_MS);
    }
    
    /* Cleanup */
    printf("\n[SHUTDOWN] Deinitializing BCM...\n");
    bcm_deinit();
    printf("[SHUTDOWN] BCM shutdown complete\n");
    
    return EXIT_SUCCESS;
}
