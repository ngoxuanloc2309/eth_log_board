


#ifndef __SOCKETSERVER_H__
#define __SOCKETSERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifndef SERVER_MAX_CLIENT_ACCEPT
#define SERVER_MAX_CLIENT_ACCEPT 3
#endif

typedef enum MODE{
        TCP_MODE = SOCK_STREAM,
        UDP_MODE = SOCK_DGRAM
}SOCKET_MODE_t;
// typedef struct SClient_t SClient;
typedef void (*ClientHandler)(void* arg);
/**
 * @brief Structure representing a socket server.
 *
 * This structure encapsulates all the necessary information and resources
 * required to manage a socket server instance, including its configuration,
 * state, and any associated resources.
 */
typedef struct SocketServer{
    int server_fd;
    int port;
    struct sockaddr_in server_addr;
    ClientHandler client_handler;
    void *arg;
    const char * server_name;
    TaskHandle_t task_handle;
    SOCKET_MODE_t mode;
    uint16_t num_clients;
} SocketServer;

typedef struct SClient {
    int client_fd;
    int size;
    struct sockaddr_in client_addr;
    TaskHandle_t task_handle;
    void *arg;
}SClient_t;

/**
 * @brief Initializes the socket server.
 * 
 * @param server Pointer to the SocketServer structure.
 * @param port Port number to listen on.
 * @return int 0 on success, -1 on failure.
 */
int socket_server_init(SocketServer* server,enum MODE mode, int port,const char * server_name, ClientHandler handler,void *arg);

/**
 * @brief Start the socket server.
 *
 * @param server Pointer to the SocketServer structure.
 * @return int The file descriptor for the accepted client, or -1 on failure.
 */
int socket_server_start(SocketServer* server);

#ifdef __cplusplus
}
#endif

#endif