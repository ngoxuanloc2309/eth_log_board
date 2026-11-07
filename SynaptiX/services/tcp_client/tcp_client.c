#include "tcp_client.h"

void tcp_client_init(TCPClient_t* tcp_client, const char * server_ip, int port, TCPClientHandler handler, void * arg){
    socket_client_init(&tcp_client->client, TCP_MODE, server_ip, port, handler, arg);
}