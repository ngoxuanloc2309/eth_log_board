#include "shell_serial.h"
#include "board.h"
#include "cli_shell.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logger.h"
#include "cqueue.h"

// #define SHELL_TX_BUFF_SIZE 2048
// #define SHELL_TX_MAX_DEBOUND 10;
// CQueue_t shell_tx_queue;
// uint8_t shell_tx_buff[SHELL_TX_BUFF_SIZE];
// uint32_t shell_tx_deboud = 0;

static const char *TAG = "ShellSerial";

TaskHandle_t shell_serial_task_handle = NULL;
// TaskHandle_t shell_tx_task_handle = NULL;

ShellContext_t serial_shell;

int shell_send_char(void *arg,char c){
    // uart_write_bytes(LOG_PORT,(uint8_t*)&c,1);
    bsp_usb_cdc_transmit(USB_SHELL,(uint8_t*)&c,1);
    bsp_delay(5);
    return c;
}
int shell_send_str(void *arg,char *c){
    // uart_write_bytes(LOG_PORT,(uint8_t*)&c,1);
    bsp_usb_cdc_transmit(USB_SHELL,(uint8_t*)c,strlen(c));
    bsp_delay(10);
    return strlen(c);
}
int shell_recv_char(void *arg){
    int c = 0;
    // uart_read_bytes(LOG_PORT,(uint8_t*)&c,1,portMAX_DELAY);
    bsp_usb_cdc_read(USB_SHELL,(uint8_t*)&c,1);
    return c;
}
void shell_serial_task(void *arg){
    log_info("Shell","Serial shell task started");
    serial_shell.impl.send_char = shell_send_char;
    serial_shell.impl.send_str = shell_send_str;
    serial_shell.impl.receive_char = shell_recv_char;
    serial_shell.impl.arg = NULL;
    cli_shell_boot(&serial_shell);
    char c;
    while(1){
        c = shell_recv_char(NULL);
        if(c > 0){
            cli_shell_receive_char(&serial_shell,c);
        }  
        bsp_delay(1);
    }
}
// static void shell_tx_handle(void *arg){
//     cqueue_init_static(&shell_tx_queue,shell_tx_buff,SHELL_TX_BUFF_SIZE,sizeof(uint8_t));
//     while(1){
//         if(shell_tx_deboud > 0 ) shell_tx_deboud--;
//         if(shell_tx_deboud == 0 && shell_tx_queue.count > 0){
//             bsp_usb_cdc_transmit(USB_SHELL,shell_tx_buff,shell_tx_queue.count);
//             cqueue_init_static(&shell_tx_queue,shell_tx_buff,SHELL_TX_BUFF_SIZE,sizeof(uint8_t));
//         }
//         bsp_delay(1);
//     }
// }
void shell_serial_init(void)
{
    xTaskCreate(shell_serial_task, "cli serial", 1024*4, NULL, 10, &shell_serial_task_handle);
    if(shell_serial_task_handle == NULL){
        log_error(TAG,"Create shell serial task failed");
    }
    // xTaskCreate(shell_tx_handle, "cli tx", 256*4, NULL, 10, &shell_tx_task_handle);
    // if(shell_tx_task_handle == NULL){
    //     log_error(TAG,"Create shell serial task tx failed");
    // }
}