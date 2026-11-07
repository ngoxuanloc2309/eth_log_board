#ifndef __UDPSERVER_H__
#define __UDPSERVER_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "socketserver.h"

typedef struct UDPServer{
    SocketServer server;
} UDPServer_t;

static inline void udp_server_init(UDPServer_t* udp_server, int port, const char * server_name, ClientHandler handler,void *arg){
    socket_server_init(&udp_server->server,UDP_MODE,port,server_name,handler,arg);
}
static inline int udp_server_start(UDPServer_t* udp_server){
    return socket_server_start(&udp_server->server);
}

#ifdef __cplusplus
}
#endif
#endif /* __UDPSERVER_H__ */