#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "socketserver.h"

typedef struct TCPServer{
    SocketServer server;
} TCPServer_t;

void tcp_server_init(TCPServer_t* tcp_server, int port, const char * server_name, ClientHandler handler,void *arg);
int tcp_server_start(TCPServer_t* tcp_server);

#ifdef __cplusplus
}
#endif
#endif