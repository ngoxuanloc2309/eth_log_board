#ifndef __TRANSPARENT_H__
#define __TRANSPARENT_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "tcp_client.h"
#include "tcp_server.h"
#include "udp_client.h"
#include "udp_server.h"

typedef int (*vc_send_t)(const uint8_t* data, uint16_t len);
typedef int (*vc_recv_t)(uint8_t* data, uint16_t len);

typedef struct {
    enum {
        MODE_NONE = 0,
        MODE_TCP_CLIENT,
        MODE_TCP_SERVER,
        MODE_UDP_CLIENT,
        MODE_UDP_SERVER
    } mode;
    ip_addr_t ip;
    uint16_t  port;
    vc_send_t vc_send;
    vc_recv_t vc_recv;
    void *tcpip; // Pointer to TCPClient_t, TCPServer_t, UDPClient_t, or UDPServer_t based on mode
} TransparentDevice;

void transparent_init(TransparentDevice* dev, int mode, const char* ip, uint16_t port, vc_send_t send_func, vc_recv_t recv_func);
bool transparent_start(TransparentDevice* dev);

#ifdef __cplusplus
}
#endif
#endif /* __TRANSPARENT_H__ */