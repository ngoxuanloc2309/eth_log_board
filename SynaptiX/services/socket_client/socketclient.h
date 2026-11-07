#ifndef __SOCKETCLIENT_H__
#define __SOCKETCLIENT_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdbool.h>
#include "socketserver.h"
typedef struct SocketClient SocketClient_t;
typedef void (*ClientHandler)(void* arg);
struct SocketClient
{
    /* data */
    SOCKET_MODE_t mode;
    int client_fd;
    int size;
    struct sockaddr_in client_addr;
    TaskHandle_t task_handle;
    ClientHandler client_handler;
    void *arg;
    char *server_addr_str;
    int port;
};

void socket_client_init(SocketClient_t* socket_client, enum MODE mode, const char * server_ip, int port, ClientHandler handler, void * arg);
static inline bool socket_client_is_connected(SocketClient_t* socket_client){
    return socket_client->client_fd >= 0;
}


#ifdef __cplusplus
}
#endif
#endif /* __SOCKETCLIENT_H__ */