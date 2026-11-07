


#include "socketserver.h"
#include "socketclient.h"
#include "logger.h"

static const char* TAG = "SocketServer";

static void socket_server_task(void* arg) {
    SocketServer* server = (SocketServer*)arg;
    if (server == NULL) {
        log_error(TAG,"SocketServer pointer is NULL");
        vTaskDelete(NULL);
        return;
    }
    log_info(TAG,"%s starting on port %d",server->server_name, server->port);
    // Create the socket
    if(server->mode == TCP_MODE ){
        server->server_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    } else {
        server->server_fd = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    }
    if (server->server_fd < 0) {
        log_error(TAG,"%s Failed to create socket",server->server_name);
        vTaskDelete(server->task_handle);
        return;
    }
    // Set up the server address structure
    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_port = htons(server->port);
    server->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket
    if (lwip_bind(server->server_fd, (struct sockaddr*)&server->server_addr, sizeof(server->server_addr)) < 0) {
        log_error(TAG,"%s Failed to bind socket",server->server_name);
        lwip_close(server->server_fd);
        vTaskDelete(server->task_handle);
        return;
    }

    // Listen for incoming connections
    if (lwip_listen(server->server_fd,4) < 0) {
        log_error(TAG,"%s Failed to listen on socket",server->server_name);
        shutdown(server->server_fd,2);
        lwip_close(server->server_fd);
        vTaskDelete(server->task_handle);
        return;
    }

    log_info(TAG,"%s started on port %d",server->server_name, server->port);
    while (1) {
        // Accept a new client connection
        SocketClient_t client;
        client.client_fd = lwip_accept(server->server_fd, (struct sockaddr *)&client.client_addr, (socklen_t *)&client.size);
        if (client.client_fd < 0) {
            log_error(TAG,"%s Failed to accept client connection", server->server_name);
            continue; // Continue to accept next client
        }
        log_info(TAG,"%s Accepted client connection from %s:%d",server->server_name, inet_ntoa(client.client_addr.sin_addr), ntohs(client.client_addr.sin_port));
        server->num_clients++;
        // Handle the client connection using the provided handler
        if (server->client_handler != NULL) {
            // Create a task to handle the client connection
            SocketClient_t* client_ptr = (SocketClient_t*)pvPortMalloc(sizeof(SocketClient_t));

            if (client_ptr == NULL) {
                log_error(TAG,"%s Failed to allocate memory for client handler", server->server_name);
                lwip_close(client.client_fd); // Close the client socket if memory allocation fails
                server->num_clients--;
                continue; // Continue to accept next client
            }
            memcpy(client_ptr, &client, sizeof(SocketClient_t)); // Copy client data to the new structure
            log_info(TAG,"%s Creating task for client handler", server->server_name);
            client_ptr->arg = server->arg;
            xTaskCreate(server->client_handler, "ClientHandler", 1024*4, client_ptr,configMAX_PRIORITIES-10, &client_ptr->task_handle);
            if (client_ptr->task_handle == NULL) {
                log_error(TAG,"%s Failed to create task for client handler", server->server_name);
                lwip_close(client_ptr->client_fd); // Close the client socket if task creation fails
                vPortFree(client_ptr); // Free the allocated memory for client handler
                server->num_clients--;
                continue; // Continue to accept next client
            }
        } else {
            log_warn(TAG,"%s No client handler set for server", server->server_name);
            shutdown(client.client_fd,2);
            lwip_close(client.client_fd); // Close the client socket if no handler is set
            server->num_clients--;
        }
    }
}

int socket_server_init(SocketServer* server,enum MODE mode, int port,const char * server_name, ClientHandler handler,void *arg) {
    if (server == NULL) {
        log_error(TAG,"SocketServer pointer is NULL");
        return -1; // Invalid server pointer
    }
    server->port = port;
    server->server_name = server_name;
    server->client_handler = handler;
    server->arg = arg;
    server->mode = mode;
    server->server_fd = -1;
    server->task_handle = NULL;
    server->num_clients = 0;
    return 0; // Success
}
int socket_server_start(SocketServer* server){
    if (server == NULL) {
        log_error(TAG,"SocketServer pointer is NULL");
        return -1; // Invalid server pointer
    }

    // Create a task for the socket server
    BaseType_t result = xTaskCreate(socket_server_task, server->server_name,1024*4, server,10, &server->task_handle);
    if (result != pdPASS) {
        log_error(TAG,"%s Failed to create socket server task",server->server_name);
        return -1; // Task creation failed
    }
    return 0; // Success
}