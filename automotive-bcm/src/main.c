/**
 * @file main.c
 * @brief BCM Application Entry Point
 *
 * Simple main loop for SIL (Software-in-the-Loop) simulation.
 * Prints state changes to stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#ifdef __linux__
#include <time.h>
#include <unistd.h>
#else
/* macOS/POSIX fallback */
#include <time.h>
#include <unistd.h>
#endif

#include "bcm.h"
#include "system_state.h"

/*******************************************************************************
 * Configuration
 ******************************************************************************/

#define DEFAULT_CAN_INTERFACE   "vcan0"
#define MAIN_LOOP_PERIOD_US     1000    /* 1ms main loop */

/*******************************************************************************
 * Private Data
 ******************************************************************************/

static volatile bool g_running = true;

/*******************************************************************************
 * Signal Handler
 ******************************************************************************/

static void signal_handler(int signum)
{
    (void)signum;
    printf("\n[MAIN] Shutdown requested...\n");
    g_running = false;
}

/*******************************************************************************
 * Timing Functions
 ******************************************************************************/

/**
 * @brief Get current time in milliseconds
 */
static uint32_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

/**
 * @brief Sleep for microseconds
 */
static void sleep_us(uint32_t us)
{
    usleep(us);
}

/*******************************************************************************
 * Status Display
 ******************************************************************************/

/**
 * @brief Print current system status (called periodically)
 */
static void print_status(void)
{
    const system_state_t *state = sys_state_get();
    
    /* Build status line */
    char door_status[5] = "----";
    for (int i = 0; i < 4; i++) {
        if (state->door.lock_state[i] == DOOR_STATE_LOCKED) {
            door_status[i] = 'L';
        } else if (state->door.lock_state[i] == DOOR_STATE_LOCKING) {
            door_status[i] = 'l';
        } else if (state->door.lock_state[i] == DOOR_STATE_UNLOCKING) {
            door_status[i] = 'u';
        } else {
            door_status[i] = 'U';
        }
    }
    
    const char *headlight_str = "OFF";
    switch (state->lighting.headlight_output) {
        case HEADLIGHT_STATE_ON:        headlight_str = "ON "; break;
        case HEADLIGHT_STATE_AUTO:      headlight_str = "AUT"; break;
        case HEADLIGHT_STATE_HIGH_BEAM: headlight_str = "HI "; break;
        default: break;
    }
    
    const char *turn_str = "OFF";
    switch (state->turn_signal.mode) {
        case TURN_SIG_STATE_LEFT:   turn_str = "LEFT"; break;
        case TURN_SIG_STATE_RIGHT:  turn_str = "RIGHT"; break;
        case TURN_SIG_STATE_HAZARD: turn_str = "HAZ"; break;
        default: break;
    }
    
    char turn_output[3] = "--";
    turn_output[0] = state->turn_signal.left_output ? 'L' : '-';
    turn_output[1] = state->turn_signal.right_output ? 'R' : '-';
    
    printf("\r[%6u.%03us] Doors:%s | Head:%s | Turn:%s[%s] | Faults:%d    ",
           state->uptime_ms / 1000,
           state->uptime_ms % 1000,
           door_status,
           headlight_str,
           turn_str,
           turn_output,
           fault_manager_get_count());
    fflush(stdout);
}

/*******************************************************************************
 * Usage
 ******************************************************************************/

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -i <interface>  CAN interface name (default: %s)\n", DEFAULT_CAN_INTERFACE);
    printf("  -h              Show this help\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s -i vcan0\n", prog_name);
    printf("\n");
    printf("To create a virtual CAN interface:\n");
    printf("  sudo modprobe vcan\n");
    printf("  sudo ip link add dev vcan0 type vcan\n");
    printf("  sudo ip link set up vcan0\n");
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(int argc, char *argv[])
{
    const char *can_interface = DEFAULT_CAN_INTERFACE;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 && (i + 1) < argc) {
            can_interface = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize BCM */
    if (bcm_init(can_interface) != 0) {
        fprintf(stderr, "[MAIN] BCM initialization failed\n");
        return 1;
    }
    
    printf("[MAIN] BCM running. Press Ctrl+C to exit.\n");
    printf("[MAIN] Status updates every second:\n\n");
    
    /* Main loop */
    uint32_t last_status_print = 0;
    
    while (g_running) {
        uint32_t current_ms = get_time_ms();
        
        /* Process BCM */
        bcm_process(current_ms);
        
        /* Print status every second */
        if ((current_ms - last_status_print) >= 1000) {
            print_status();
            last_status_print = current_ms;
        }
        
        /* Sleep to maintain loop rate */
        sleep_us(MAIN_LOOP_PERIOD_US);
    }
    
    /* Cleanup */
    printf("\n\n");
    bcm_deinit();
    
    /* Print final event log */
    printf("\n[MAIN] Event Log (%d entries):\n", event_log_count());
    event_log_entry_t entry;
    for (uint8_t i = 0; i < event_log_count(); i++) {
        if (event_log_get(i, &entry)) {
            printf("  [%8u ms] Type=%d Data=[%02X %02X %02X %02X]\n",
                   entry.timestamp_ms,
                   entry.type,
                   entry.data[0], entry.data[1], entry.data[2], entry.data[3]);
        }
    }
    
    printf("\n[MAIN] Goodbye!\n");
    return 0;
}
