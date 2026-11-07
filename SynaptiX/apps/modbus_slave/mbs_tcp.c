#include "mbs_tcp.h"
#include "port.h"
#include "mb.h"
#include "logger.h"
#include "socketclient.h"
static const char *TAG = "ModbusTCPServer";

static void client_task(void *pvParameters)
{
    // Your client code here
    SocketClient_t *client = (SocketClient_t *)pvParameters;
    if (client == NULL) {
        log_error(TAG,"Client pointer is NULL");
        vTaskDelete(NULL);
        return;
    }

	log_info(TAG,"Connected %s:%d",inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));

    uint8_t aucTCPBuf[MB_TCP_BUF_SIZE];

    ModbusTCPFrame_t frame;
    int ret;
    uint16_t usLength;
    while (1)
    {
    	vTaskDelay(1);
        if (((ret = recv(client->client_fd,aucTCPBuf, MB_TCP_BUF_SIZE,0)) == SOCKET_ERROR) || (!ret))
        {
            break;
        }
        // log_print_hex(LOGGER_DEBUG,TAG,aucTCPBuf,ret);
        frame.TDI[0] = aucTCPBuf[0];
        frame.TDI[1] = aucTCPBuf[1];
        frame.PID[0] = aucTCPBuf[2];
        frame.PID[1] = aucTCPBuf[3];
        frame.LEN[0] = aucTCPBuf[4];
        frame.LEN[1] = aucTCPBuf[5];
        usLength = frame.LEN[0] << 8U | frame.LEN[1];

        if(ret == MB_TCP_UID + usLength)
        {
            frame.UID = aucTCPBuf[6];
            frame.FUNC = aucTCPBuf[7];
            frame.DATA = &aucTCPBuf[8];

            eMBException eException;

            eException = MB_EX_ILLEGAL_FUNCTION;

            for(int i = 0; i < MB_FUNC_HANDLERS_MAX; i++ )
            {
                /* No more function handlers registered. Abort. */
                if( MBFunc[i].ucFunctionCode == 0 )
                {
                    break;
                }
                else if( MBFunc[i].ucFunctionCode == frame.FUNC )
                {
                    usLength = usLength -1;
                    eException = MBFunc[i].pxHandler( &aucTCPBuf[7], &usLength );
                    break;
                }
            }
            // log_debug(TAG,"Exception : %d" , eException);
            if(eException != MB_EX_NONE)
            {
                usLength = 0;
                frame.DATA[usLength++] = ( UCHAR )( frame.FUNC | MB_FUNC_ERROR );
                frame.DATA[usLength++] = eException;
            }
            // log_print_hex(LOGGER_DEBUG,TAG,aucTCPBuf,usLength + 7);
            ret = send(client->client_fd, aucTCPBuf, usLength + 7, 0);
            if(ret < 0)
            {
                break;
            }
        }
        else
        {
            continue;
        }

    }
    log_warn(TAG,"Client %s:%d disconnect",inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));
    shutdown(client->client_fd,2);
    close(client->client_fd);
    TaskHandle_t task_handle = client->task_handle;
    vPortFree(pvParameters);
    if (task_handle != NULL) {
        vTaskDelete(task_handle); // Delete the task associated with the client
        return;
    }
    vTaskDelete(NULL); // Delete the current task if no task handle is available
}

int modbus_tcp_server_init(ModbusTCPServer* server, int port){
    if (server == NULL) {
        return -1; // Invalid server pointer
    }

    server->port = port;
    server->server.server_name = "ModbusTCPServer";
    server->server.port = port;
    server->server.client_handler = NULL; // Set to NULL or assign a handler function
    server->server.task_handle = NULL;

   socket_server_init(&server->server,TCP_MODE, port, server->server.server_name, client_task,server);

   return socket_server_start(&server->server);
}