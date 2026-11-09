#include "logger.h"

#define LOGGER_BUFFER_SIZE 4096

#if FREE_RTOS

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <message_buffer.h>

static SemaphoreHandle_t logger_mutex = NULL;
static MessageBufferHandle_t logger_message = NULL;
static TaskHandle_t logger_handle = NULL;
#define STORAGE_SIZE_BYTES LOGGER_BUFFER_SIZE * 8
static uint8_t ucMessageBufferStorage[ STORAGE_SIZE_BYTES ];
StaticMessageBuffer_t xMessageBufferStruct;
#define STACK_SIZE 256
static StaticTask_t xTaskBuffer;
static StackType_t xStack[ STACK_SIZE ];
static void logger_task(void *arg);

#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
// #include "fdk_config.h"
#define LOGGER_EN 1
// Global variables to keep track of log files.
static int LOG_LEVEL = LOGGER_DEBUG;
// static bool write_to_file = false;

static void serial_write_func(const char *s,size_t len);

static p_log_func serial_write = serial_write_func;

static void serial_write_func(const char *s,size_t len)
{
    printf(s);
}

static const char *log_level_strings[] = {
    "[OFF]",
    "[ERROR]",
    "[WARNING]",
    "[INFO]",
    "[DEBUG]",
    
};
const char *colors[] = {
    "\x1B[0m",
    "\x1B[31m",
    "\x1B[33m",
    "\x1B[32m",
    "\x1B[34m", 
    
};
static const char *banner =
    "                                                                         ******               ((((((\r\n"
    "                                                                           *****,.          ######\r\n"
    "                                                                             ,,,,,,,     ,######\r\n"
    " ###########                                                     ##     --     ,,,,,,,   #####\r\n"
    " ##          (##      ## (##########)  ##########  ###########. #####   ##       ,,,,,,,   #\r\n"
    "   -------##   ##   (##  (##       ##  ##.......#. ##       ,##  ##     ##         ,,,,,.\r\n"
    " ###########    (####    (#,       ##  ##       #. #########,##  ##     ##       ,***,..   #\r\n"
    "                 *##                               ##                          ,,,,,,*   (###(\r\n"
    "               ###                                 ##                        ,,,,,,.      ######\r\n"
    "                                                                          ......,,         ######\r\n";



char buff[LOGGER_BUFFER_SIZE];

void log_func(LOGGING_LEVELS level, const char *TAG, const char *frmt, ...)
{
#if LOGGER_EN
    if (LOG_LEVEL < level)
    {
        return;
    }
#if FREE_RTOS
    if(logger_handle == NULL || logger_message == NULL || logger_mutex == NULL) return;
    xSemaphoreTake(logger_mutex, portMAX_DELAY);
#endif
    char *format = buff;
    // time_t timestamp = 0;
    int n = snprintf(format,LOGGER_BUFFER_SIZE, "%s%s[%s] : ", colors[level], log_level_strings[level], TAG);
    va_list argp;
    va_start(argp, frmt);
    vsnprintf(format + n,LOGGER_BUFFER_SIZE - n, frmt, argp);
    va_end(argp);
    strcat(format, colors[LOGGER_OFF]);
    strcat(format, "\r\n");
#if FREE_RTOS
    xMessageBufferSend(logger_message,format,strlen(format),0);
    xSemaphoreGive(logger_mutex);
    return;
#else
    if(serial_write != NULL) 
        serial_write(format,strlen(format));
    return;
#endif

#endif
}
void logger_set_level(LOGGING_LEVELS level)
{
    LOG_LEVEL = level;
}
void log_print_hex(LOGGING_LEVELS level, const char *TAG, uint8_t *hex_buff, uint16_t length)
{
#if LOGGER_EN
    if (LOG_LEVEL < level)
    {
        return;
    }
#if FREE_RTOS
    if(logger_handle == NULL || logger_message == NULL || logger_mutex == NULL) return;
    xSemaphoreTake(logger_mutex, portMAX_DELAY);
#endif
    char *format = buff;
    memset(buff, 0, LOGGER_BUFFER_SIZE);
    // time_t timestamp = 0;
    snprintf(format, LOGGER_BUFFER_SIZE,"%s%s[%s] : ", colors[level], log_level_strings[level], TAG);
    char hex[6];
    hex[5] = 0;
    for (uint16_t i = 0; i < length; i++)
    {
        sprintf(hex, "%02X ", hex_buff[i]);
        strcat(format, hex);
    }
    strcat(format, "\r\n");
#if FREE_RTOS
    xMessageBufferSend(logger_message,format,strlen(format),0);
    xSemaphoreGive(logger_mutex);
    return;
#else
    if(serial_write != NULL) 
        serial_write(format,strlen(format));
    return;
#endif
#endif
}
void logger_init(LOGGING_LEVELS level, p_log_func p_func)
{
#if FREE_RTOS
    logger_mutex = xSemaphoreCreateMutex();
    logger_message = xMessageBufferCreateStatic(sizeof( ucMessageBufferStorage ),ucMessageBufferStorage,
                                                 &xMessageBufferStruct  );
    logger_handle = xTaskCreateStatic(logger_task,"logger",STACK_SIZE,      /* Number of indexes in the xStack array. */
                      ( void * ) 1,    /* Parameter passed into the task. */
                      tskIDLE_PRIORITY,/* Priority at which the task is created. */
                      xStack,          /* Array to use as the task's stack. */
                      &xTaskBuffer );
#endif
    LOG_LEVEL = level;
    if(p_func != NULL)
        serial_write = p_func;
    serial_write(colors[LOGGER_INFO],strlen(colors[LOGGER_INFO]));
    serial_write("\r\n\r\n",strlen("\r\n\r\n"));
    serial_write(banner,strlen(banner));
    serial_write("\r\n\r\n",strlen("\r\n\r\n"));
    serial_write(colors[LOGGER_INFO],strlen(colors[LOGGER_INFO]));
}

#if FREE_RTOS
char log_record[LOGGER_BUFFER_SIZE];
static void logger_task(void *arg){
    while(1){
        size_t received = xMessageBufferReceive(logger_message,log_record,LOGGER_BUFFER_SIZE,portMAX_DELAY);
        if(received > 0U && serial_write != NULL){
            serial_write(log_record,received);
        }
    }
}
#endif