#include "socketclient.h"
#include "logger.h"

static const char* TAG = "SocketClient";

void socket_client_task(void* arg){
    SocketClient_t * socket_client = (SocketClient_t *)arg;
    if(socket_client->client_handler) socket_client->client_handler(socket_client->arg);

    socket_client->client_fd = socket(AF_INET, socket_client->mode, 0);
    if(socket_client->client_fd < 0){
        log_error(TAG,"Failed to create socket");
        vTaskDelete(NULL);
        return;
    }
    
    socket_client->client_addr.sin_family = AF_INET;
    socket_client->client_addr.sin_port = htons(socket_client->port);
    socket_client->client_addr.sin_addr.s_addr = inet_addr(socket_client->server_addr_str);
    log_info(TAG,"Connecting to %s:%d",socket_client->server_addr_str,socket_client->port);
    if(connect(socket_client->client_fd,(struct sockaddr*)&socket_client->client_addr,sizeof(socket_client->client_addr)) < 0){
        log_error(TAG,"Failed to connect to %s:%d",socket_client->server_addr_str,socket_client->port);
        shutdown(socket_client->client_fd,2);
        close(socket_client->client_fd);
        socket_client->client_fd = -1;
        vTaskDelete(NULL);
        return;
    }
    log_info(TAG,"Connected to %s:%d",socket_client->server_addr_str,socket_client->port);
    if(socket_client->client_handler) socket_client->client_handler(socket_client->arg);
    shutdown(socket_client->client_fd,2);
    close(socket_client->client_fd);
    log_info(TAG,"Disconnected from %s:%d",socket_client->server_addr_str,socket_client->port);
    socket_client->client_fd = -1;
    vTaskDelete(NULL);
}

void socket_client_init(SocketClient_t* socket_client, enum MODE mode, const char * server_ip, int port, ClientHandler handler, void * arg){
    socket_client->mode = mode;
    socket_client->port = port;
    socket_client->client_handler = handler;
    socket_client->arg = arg;
    socket_client->server_addr_str = (char *)server_ip;
    socket_client->client_fd = -1;
    memset(&socket_client->client_addr,0,sizeof(socket_client->client_addr));
    xTaskCreate(socket_client_task,"socket_client",1024*4,socket_client,10,&socket_client->task_handle);
    if(socket_client->task_handle == NULL){
        log_error(TAG,"Create socket client task failed");
    }
}