#include "tcp_server.h"


void tcp_server_init(TCPServer_t* tcp_server, int port, const char * server_name, ClientHandler handler,void *arg){
    socket_server_init(&tcp_server->server,TCP_MODE,port,server_name,handler,arg);
}
int tcp_server_start(TCPServer_t* tcp_server){
    return socket_server_start(&tcp_server->server);
}