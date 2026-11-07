#include "shell_tcp.h"
#include "logger.h"
#include "socketclient.h"
#include "app_config.h"

#define MAX_BYTE_CLIENT_CAN_READ 256

static const char *TAG = "Shell";

static void shell_tcp_task(void *arg);

static int console_putc(void *arg,char c);
static int console_puts(void *arg,char *str);

void shell_tcp_init(SocketServer *shell){
    log_info(TAG,"Initialize");
    socket_server_init(shell,TCP_MODE,SHELL_TCP_PORT,"Shell",shell_tcp_task,shell);
    socket_server_start(shell);
}

static void shell_tcp_task(void *arg){
    SocketClient_t *client = (SocketClient_t*) arg;

    if(client == NULL){
        log_error(TAG,"Shell Client is NULL, return");
        vTaskDelete(NULL);
        return;
    }
    log_info(TAG,"%s:%d Connected",inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));
    ShellContext_t *shell = (ShellContext_t*) malloc(sizeof(ShellContext_t));
    if(shell == NULL){
        log_error(TAG,"Can't create Shell Context,close %s:%d and return",inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));
        close(client->client_fd);
        vTaskDelete(client->task_handle);
        return;
    }

    shell->impl.send_char = console_putc;
    shell->impl.send_str = console_puts;
    shell->impl.arg = client;

    cli_shell_boot(shell);
    char buff[MAX_BYTE_CLIENT_CAN_READ];
    while(1){
        int ret = read(client->client_fd,buff,MAX_BYTE_CLIENT_CAN_READ);
        if(ret < 0) break;
        else if(ret == 0) continue;

        for(int i = 0;i<ret;i++){
            cli_shell_receive_char(shell,buff[i]);
        }
    }
    log_warn(TAG,"%s:%d disconnected",inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));
    shutdown(client->client_fd,2);
    close(client->client_fd);
    vTaskDelete(client->task_handle);
}


static int console_putc(void *arg,char c){
    SocketClient_t *client = (SocketClient_t*)arg;
    if(client == NULL){
        log_error(TAG,"console_putc : Shell Client is NULL, return");
        return -1;
    }
    send(client->client_fd,&c,1,0);
    return 1;
}
static int console_puts(void *arg,char *str){
    SocketClient_t *client = (SocketClient_t*)arg;
    if(client == NULL){
        log_error(TAG,"console_putc : Shell Client is NULL, return");
        return -1;
    }
    send(client->client_fd,str,strlen(str),0);
    return 1;
}