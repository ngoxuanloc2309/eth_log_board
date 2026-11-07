#ifndef __TCPCLIENT_H__
#define __TCPCLIENT_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "socketclient.h"

typedef void (*TCPClientHandler)(void* arg);

typedef struct TCPClient{
    SocketClient_t client;
} TCPClient_t;

void tcp_client_init(TCPClient_t* tcp_client, const char * server_ip, int port, TCPClientHandler handler, void * arg);
static inline bool tcp_client_is_connected(TCPClient_t* tcp_client){
    return socket_client_is_connected(&tcp_client->client);
}
#ifdef __cplusplus
}
#endif
#endif /* __TCPCLIENT_H__ */