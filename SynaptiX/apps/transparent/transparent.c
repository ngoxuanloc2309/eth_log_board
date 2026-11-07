#include "transparent.h"
#include "logger.h"

static const char *TAG = "Transparent";

static void socket_recv_client_task(void *arg){
    TransparentDevice* dev = (TransparentDevice*) arg;
    if(dev == NULL) {
        log_error(TAG,"Transparent Device is NULL, return");
        vTaskDelete(NULL);
        return;
    }
    int socket_fd = dev->mode == MODE_TCP_SERVER ? ((TCPServer_t*)dev->tcpip)->server.server_fd : ((UDPServer_t*)dev->tcpip)->server.server_fd;
    while(1){
        if(dev->vc_recv == NULL) break;
        uint8_t buff[256];
        int ret = recv(socket_fd, buff, 256, 0);
        if(ret > 0){
            // Send data to virtual com
            if(dev->vc_send) dev->vc_send(buff, ret);
        } else if(ret < 0){
            log_warn(TAG,"Connection closed by remote");
            break;
        }
    }
    vTaskDelete(NULL);
}
static void socket_recv_server_task(void *arg){
    SClient_t* client = (SClient_t*) arg;
    TransparentDevice* dev = (TransparentDevice*) client->arg;
    if(dev == NULL) {
        log_error(TAG,"Transparent Device is NULL, return");
        vTaskDelete(NULL);
        return;
    }
    int socket_fd = dev->mode == MODE_TCP_SERVER ? ((TCPServer_t*)dev->tcpip)->server.server_fd : ((UDPServer_t*)dev->tcpip)->server.server_fd;
    while(1){
        if(dev->vc_recv == NULL) break;
        uint8_t buff[256];
        int ret = recv(socket_fd, buff, 256, 0);
        if(ret > 0){
            // Send data to virtual com
            if(dev->vc_send) dev->vc_send(buff, ret);
        } else if(ret < 0){
            log_warn(TAG,"Connection closed by remote");
            break;
        }
    }
    vTaskDelete(NULL);
}
static void transparent_client_handler(void* arg){
    TransparentDevice* dev = (TransparentDevice*) arg;
    if(dev == NULL) {
        log_error(TAG,"Transparent Device is NULL, return");
        return;
    }
    uint8_t buffer[256];
    int socket_fd = dev->mode == MODE_TCP_CLIENT ? ((TCPClient_t*)dev->tcpip)->client.client_fd : ((UDPClient_t*)dev->tcpip)->client.client_fd;
    xTaskCreate(socket_recv_client_task, "SocketListener", 2048, dev, 10, NULL);
    while(1){
        if(dev->vc_recv == NULL) break;
        int len = dev->vc_recv(buffer,256);
        if(len > 0){
            // Send data to TCP/UDP
           int ret = send(socket_fd, buffer, len, 0);
           if(ret < 0) {
                log_error(TAG,"Send data to socket failed, close connection and return");
                break;
           }
        }
    }
}
static void transparent_server_handler(void* arg){
    SClient_t* client = (SClient_t*) arg;
    TransparentDevice* dev = (TransparentDevice*) client->arg;
    if(dev == NULL) {
        log_error(TAG,"Transparent Device is NULL, return");
        return;
    }
    uint8_t buffer[256];
    int socket_fd = dev->mode == MODE_TCP_SERVER ? ((TCPServer_t*)dev->tcpip)->server.server_fd : ((UDPServer_t*)dev->tcpip)->server.server_fd;
    xTaskCreate(socket_recv_server_task, "SocketListener", 2048, client, 10, NULL);
    while(1){
        if(dev->vc_recv == NULL) break;
        int len = dev->vc_recv(buffer,256);
        if(len > 0){
            // Send data to TCP/UDP
           int ret = send(socket_fd, buffer, len, 0);
           if(ret < 0) {
                log_error(TAG,"Send data to socket failed, close connection and return");
                break;
           }
        }
    }
}
void transparent_init(TransparentDevice* dev, int mode, const char* ip, uint16_t port, vc_send_t send_func, vc_recv_t recv_func){
    dev->mode = mode;
    dev->port = port;
    dev->vc_send = send_func;
    dev->vc_recv = recv_func;
    ipaddr_aton(ip, &dev->ip);
    dev->tcpip = NULL;

    switch (mode)
    {
    case MODE_TCP_CLIENT:
        /* code */
        dev->tcpip = malloc(sizeof(TCPClient_t));
        tcp_client_init((TCPClient_t*)dev->tcpip, ip, port, transparent_client_handler, dev);
        break;
    case MODE_TCP_SERVER:
        /* code */
        dev->tcpip = malloc(sizeof(TCPServer_t));
        tcp_server_init((TCPServer_t*)dev->tcpip, port, "Transparent", transparent_server_handler,dev);
        break;
    case MODE_UDP_CLIENT:
        /* code */
        dev->tcpip = malloc(sizeof(UDPClient_t));
        udp_client_init((UDPClient_t*)dev->tcpip, ip, port, transparent_client_handler, dev);
        break;
    case MODE_UDP_SERVER:
        /* code */
        dev->tcpip = malloc(sizeof(UDPServer_t));
        udp_server_init((UDPServer_t*)dev->tcpip, port, "Transparent", transparent_server_handler,dev);
    break; 
    default:
        break;
    }
}
bool transparent_start(TransparentDevice* dev){
    if(dev == NULL || dev->tcpip == NULL) {
        log_error(TAG,"Transparent Device or tcpip is NULL, return");
        return false;
    }
    switch (dev->mode)
    {
    case MODE_TCP_CLIENT:
        /* code */
        // return tcp_client_start((TCPClient_t*)dev->tcpip);
        return true;
    case MODE_TCP_SERVER:
        /* code */
        return tcp_server_start((TCPServer_t*)dev->tcpip);
    case MODE_UDP_CLIENT:
        /* code */
        // return udp_client_start((UDPClient_t*)dev->tcpip);
        return true;
    case MODE_UDP_SERVER:
        /* code */
        return udp_server_start((UDPServer_t*)dev->tcpip);
    default:
        break;
    }
    return false;
}