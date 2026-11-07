#ifndef __UDPCLIENT_H__
#define __UDPCLIENT_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "socketclient.h"

typedef struct UDPClient{
    SocketClient_t client;
    /* data */
}UDPClient_t;

static inline void udp_client_init(UDPClient_t* udp_client, const char * server_ip, int port, ClientHandler handler, void * arg){
    socket_client_init(&udp_client->client, UDP_MODE, server_ip, port, handler, arg);
}
static inline bool udp_client_is_connected(UDPClient_t* udp_client){
    return socket_client_is_connected(&udp_client->client);
}

#ifdef __cplusplus
}
#endif
#endif /* __UDPCLIENT_H__ */