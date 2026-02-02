/**
 * @file can_interface.c
 * @brief CAN Interface Implementation
 *
 * BCM_SIL=1: Linux SocketCAN implementation
 * BCM_SIL=0: Stub in-memory queue implementation
 */

#include <string.h>
#include <stdio.h>
#include "can_interface.h"

/*******************************************************************************
 * Configuration
 ******************************************************************************/

#define CAN_RX_QUEUE_SIZE   32U
#define CAN_TX_QUEUE_SIZE   16U

/*******************************************************************************
 * Private Types
 ******************************************************************************/

typedef struct {
    can_frame_t frames[CAN_RX_QUEUE_SIZE];
    uint8_t     head;
    uint8_t     tail;
    uint8_t     count;
} can_queue_t;

/*******************************************************************************
 * Private Data
 ******************************************************************************/

static bool         g_initialized = false;
static can_stats_t  g_stats;

#ifdef BCM_SIL
/* SocketCAN specific */
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static int g_socket_fd = -1;

#else
/* Stub mode specific */
static can_queue_t  g_rx_queue;
static can_queue_t  g_tx_queue;
static can_frame_t  g_last_tx;
static bool         g_last_tx_valid = false;

#endif

/*******************************************************************************
 * Queue Operations (Stub Mode)
 ******************************************************************************/

#ifndef BCM_SIL

static void queue_init(can_queue_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

static bool queue_push(can_queue_t *q, const can_frame_t *frame, uint8_t max_size)
{
    if (q->count >= max_size) {
        return false;
    }
    
    q->frames[q->tail] = *frame;
    q->tail = (q->tail + 1U) % max_size;
    q->count++;
    return true;
}

static bool queue_pop(can_queue_t *q, can_frame_t *frame, uint8_t max_size)
{
    if (q->count == 0) {
        return false;
    }
    
    *frame = q->frames[q->head];
    q->head = (q->head + 1U) % max_size;
    q->count--;
    return true;
}

#endif /* !BCM_SIL */

/*******************************************************************************
 * SocketCAN Implementation
 ******************************************************************************/

#ifdef BCM_SIL

can_status_t can_init(const char *ifname)
{
    if (g_initialized) {
        return CAN_STATUS_OK;
    }
    
    if (ifname == NULL) {
        ifname = "vcan0";
    }
    
    /* Create socket */
    g_socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (g_socket_fd < 0) {
        perror("[CAN] socket");
        return CAN_STATUS_ERROR;
    }
    
    /* Get interface index */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    
    if (ioctl(g_socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("[CAN] ioctl SIOCGIFINDEX");
        close(g_socket_fd);
        g_socket_fd = -1;
        return CAN_STATUS_ERROR;
    }
    
    /* Bind socket */
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if (bind(g_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[CAN] bind");
        close(g_socket_fd);
        g_socket_fd = -1;
        return CAN_STATUS_ERROR;
    }
    
    /* Set non-blocking */
    int flags = fcntl(g_socket_fd, F_GETFL, 0);
    fcntl(g_socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    memset(&g_stats, 0, sizeof(g_stats));
    g_initialized = true;
    
    printf("[CAN] Initialized on %s\n", ifname);
    return CAN_STATUS_OK;
}

void can_deinit(void)
{
    if (g_socket_fd >= 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }
    g_initialized = false;
}

bool can_is_initialized(void)
{
    return g_initialized;
}

can_status_t can_send(const can_frame_t *frame)
{
    if (!g_initialized || frame == NULL) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    struct can_frame cf;
    memset(&cf, 0, sizeof(cf));
    cf.can_id = frame->id;
    cf.can_dlc = (frame->dlc > CAN_FRAME_MAX_DLC) ? CAN_FRAME_MAX_DLC : frame->dlc;
    memcpy(cf.data, frame->data, cf.can_dlc);
    
    ssize_t nbytes = write(g_socket_fd, &cf, sizeof(cf));
    if (nbytes != sizeof(cf)) {
        g_stats.tx_errors++;
        return CAN_STATUS_ERROR;
    }
    
    g_stats.tx_count++;
    return CAN_STATUS_OK;
}

can_status_t can_recv(can_frame_t *frame)
{
    if (!g_initialized || frame == NULL) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    struct can_frame cf;
    ssize_t nbytes = read(g_socket_fd, &cf, sizeof(cf));
    
    if (nbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return CAN_STATUS_NO_DATA;
        }
        g_stats.rx_errors++;
        return CAN_STATUS_ERROR;
    }
    
    if (nbytes < (ssize_t)sizeof(cf)) {
        g_stats.rx_errors++;
        return CAN_STATUS_ERROR;
    }
    
    frame->id = cf.can_id & CAN_SFF_MASK; /* 11-bit only */
    frame->dlc = cf.can_dlc;
    memcpy(frame->data, cf.data, cf.can_dlc);
    
    g_stats.rx_count++;
    return CAN_STATUS_OK;
}

can_status_t can_rx_poll(void)
{
    return can_is_initialized() ? CAN_STATUS_OK : CAN_STATUS_NOT_INITIALIZED;
}

#else /* Stub Mode */

/*******************************************************************************
 * Stub Implementation
 ******************************************************************************/

can_status_t can_init(const char *ifname)
{
    (void)ifname;
    
    if (g_initialized) {
        return CAN_STATUS_OK;
    }
    
    queue_init(&g_rx_queue);
    queue_init(&g_tx_queue);
    g_last_tx_valid = false;
    memset(&g_stats, 0, sizeof(g_stats));
    g_initialized = true;
    
    printf("[CAN] Initialized (stub mode)\n");
    return CAN_STATUS_OK;
}

void can_deinit(void)
{
    g_initialized = false;
}

bool can_is_initialized(void)
{
    return g_initialized;
}

can_status_t can_send(const can_frame_t *frame)
{
    if (!g_initialized || frame == NULL) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    /* Store in TX queue and save as last TX */
    if (!queue_push(&g_tx_queue, frame, CAN_TX_QUEUE_SIZE)) {
        g_stats.tx_errors++;
        return CAN_STATUS_BUFFER_FULL;
    }
    
    g_last_tx = *frame;
    g_last_tx_valid = true;
    g_stats.tx_count++;
    
    return CAN_STATUS_OK;
}

can_status_t can_recv(can_frame_t *frame)
{
    if (!g_initialized || frame == NULL) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    if (!queue_pop(&g_rx_queue, frame, CAN_RX_QUEUE_SIZE)) {
        return CAN_STATUS_NO_DATA;
    }
    
    g_stats.rx_count++;
    return CAN_STATUS_OK;
}

can_status_t can_rx_poll(void)
{
    if (!g_initialized) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    return (g_rx_queue.count > 0) ? CAN_STATUS_OK : CAN_STATUS_NO_DATA;
}

can_status_t can_stub_inject_rx(const can_frame_t *frame)
{
    if (!g_initialized || frame == NULL) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    if (!queue_push(&g_rx_queue, frame, CAN_RX_QUEUE_SIZE)) {
        return CAN_STATUS_BUFFER_FULL;
    }
    
    return CAN_STATUS_OK;
}

can_status_t can_stub_get_last_tx(can_frame_t *frame)
{
    if (!g_initialized || frame == NULL) {
        return CAN_STATUS_NOT_INITIALIZED;
    }
    
    if (!g_last_tx_valid) {
        return CAN_STATUS_NO_DATA;
    }
    
    *frame = g_last_tx;
    return CAN_STATUS_OK;
}

void can_stub_clear(void)
{
    queue_init(&g_rx_queue);
    queue_init(&g_tx_queue);
    g_last_tx_valid = false;
}

#endif /* BCM_SIL */

/*******************************************************************************
 * Common Functions
 ******************************************************************************/

void can_get_stats(can_stats_t *stats)
{
    if (stats != NULL) {
        *stats = g_stats;
    }
}

void can_reset_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}
