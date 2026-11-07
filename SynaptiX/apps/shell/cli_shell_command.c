

#include "cli_shell.h"

#include <stddef.h>
// #include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
// #include "dirent.h"
// #include "stdlib.h"
#include "rf_app.h"
#include "board.h"
#include "logger.h"
#include "function.h"
#include "zigbee.h"
#include "app_config.h"


static const char *TAG = "CliShell";

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

int cli_cmd_restart(ShellContext_t *shell, int argc, char *argv[]);
int cli_create_file(ShellContext_t *shell, int argc, char *argv[]);
int cli_delete_file(ShellContext_t *shell, int argc, char *argv[]);
int cli_list_files(ShellContext_t *shell, int argc, char *argv[]);
int cli_read_file(ShellContext_t *shell, int argc, char *argv[]);
int cli_write_file(ShellContext_t *shell, int argc, char *argv[]);
int cli_cd_direct(ShellContext_t *shell,int argc,char *argv[]);
int cli_menu_config(ShellContext_t *shell, int argc, char *argv[]);
int cli_get_ip_config(ShellContext_t *shell,int argc,char *argv[]);
int cli_get_service(ShellContext_t *shell,int argc,char *argv[]);
#if ZIGBEE_ENABLE
static int cli_zigbee(ShellContext_t *shell, int argc, char *argv[]);
#endif
static const Cli_Shell_Cmd s_shell_commands[] = {
  {"restart", cli_cmd_restart, "Restart device"},
  {"help", cli_shell_help_handler, "Lists all commands"},
  {"touch", cli_create_file, "Create a file: touch <filename>"},
  {"rm", cli_delete_file, "Delete a file: rm <filename>"},
  {"ls", cli_list_files, "List files: ls"},
  {"cat", cli_read_file, "Read a file: cat <filename>"},
  {"write", cli_write_file, "Write to a file: write <filename> <data>"},
  {"cd",cli_cd_direct,"Change directory: cd <directory>"},
  {"menuconfig", cli_menu_config, "Configure menu"},
  {"ipconfig",cli_get_ip_config,"IP Configuration"},
  {"service",cli_get_service,"service -show : Show Services"},
  #if ZIGBEE_ENABLE
    {"zigbee", cli_zigbee, "\r\nzigbee -c : Connect to Zigbee Module\r\n"\
                           "zigbee -r : Read parameter from Zigbee Module\r\n"\
                           "zigbee -rs : Restart Zigbee Module\r\n"\
                           "zigbee -lp : Enter low power mode\r\n" \
                           "zigbee -w : Write parameter to Zigbee Module:\r\n"\
                                                                         "\t\t-p[panid] 0x0001 - 0xFF00\r\n"\
                                                                         "\t\t-c[channel]: 11-26\r\n"\
                                                                         "\t\t-add[address] 0x0001\r\n"\
                                                                         "\t\t-t[type] : 1-Coordinator,2-Router,3-End Device\r\n"\
                                                                         "\t\t-tm[transfer mode] : 1-Parency Transfer,2-Parency Transfer User Address,3-Parency Transfer Short Add,4-Parency Transfer Mac Address,5-Transfer N-N)\r\n"\
                                                                         "\t\t-b[baudrate] : 1-1200,2-2400,3-4800,4-9600,5-19200,6-38400,7-57600,8-115200\r\n"\
                                                                         "\t\t-d[databits] : 1-8bits\r\n"\
                                                                         "\t\t-s[stopbits] : 1-stop one\r\n"\
                                                                         "\t\t-p[parity]: 1-none,2-even,3-odd\r\n"\
                                                                         "\t\t-at[antenna] : 0-on board,1-external\r\n"\
                                                                         "\t\t-s[security] : 0-off,1-on\r\n"\
                                                                         "\t\t-sc[security code] : hex 4 bytes (0x01 0x02 0x03 0x04)\r\n" \
                           "zigbee -s : Send data to Zigbee Module [ -d[data] 0x01 0x02 ... ]\r\n"\
                           "zigbee -qr : Query End Device position\r\n",
                           },
#endif
};

char *path = "/";

int cli_cmd_restart(ShellContext_t *shell,int argc, char *argv[]){
  (void) argc;
  (void) argv;
  for(int i = 3;i>0;i--){
      cli_shell_printf(shell,"Device restart after %d seconds\r\n",i);
      vTaskDelay(pdMS_TO_TICKS(1000));
  }
  bsp_restart();
	return 0;
}

int cli_menu_config(ShellContext_t *shell, int argc, char *argv[]){
  return -1;
  if(argc > 1){
      cli_shell_printf(shell,"Usage: menuconfig\n");
      return -1;
  }

  cli_shell_printf(shell,"1. RS485\n");
  cli_shell_printf(shell,"2. Ethernet\n");
  cli_shell_printf(shell,"3. WiFi\n");
  cli_shell_printf(shell,"Select option: ");
  char option = 0;
  if(shell->impl.receive_char == NULL){
      cli_shell_printf(shell,"No input method available\n");
      return -1;
  }
  while(option == 0){ 
    option = shell->impl.receive_char(shell->impl.arg);
    vTaskDelay(1);
  }

  if(option < '1' || option > '3'){
      cli_shell_printf(shell,"Invalid option\n");
      return -1;
  }

  switch (option)
  {
  case '1':
      // RS485
      cli_shell_printf(shell,"RS485 selected\n");
      break;
  case '2':
      // Ethernet
      cli_shell_printf(shell,"Ethernet selected\n");
      break;
  case '3':
      // WiFi
      cli_shell_printf(shell,"WiFi selected\n");
      break;
  default:
      break;
  }

  return 0;
}

#if ZIGBEE_ENABLE

static void zigbee_connect_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess,void *arg)
{
  ShellContext_t *shell = (ShellContext_t *)arg;
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    log_info(TAG, "Connect to Zigbee module success");
    // cli_shell_printf(shell, "Connect to Zigbee module success\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Connect to Zigbee module\",\"result\" : \"success\"}\r\n");
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Connect to Zigbee module fail");
    // cli_shell_printf(shell, "Connect to Zigbee module fail\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Connect to Zigbee module\",\"result\" : \"fail\"}\r\n");
  }
  else
  {
    log_error(TAG, "Connect to Zigbee module timeout");
    // cli_shell_printf(shell, "Connect to Zigbee module timeout\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Connect to Zigbee module\",\"result\" : \"timeout\"}\r\n");
  }
}

static void zigbee_read_cb(ZigbeeMesh_t *zigbee,  uint8_t isSuccess, void *arg)
{
  ShellContext_t *shell = (ShellContext_t *)arg;
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    ZigbeeParameter_t *param = &zigbee->param;
    log_info(TAG, "Read Zigbee module param success");
    log_info(TAG, "PAN ID : 0x%04X", param->PAN_ID);
    log_info(TAG, "Channel : %d", param->Channel);
    log_info(TAG, "User Address : 0x%04X", param->userAddress);
    log_info(TAG, "Point Type : %d", param->pointType);
    log_info(TAG, "Transfer Model : %d", param->transferModel);
    log_info(TAG, "Uart Baudrate : %d", param->uartBraudRate);
    log_info(TAG, "Uart Data bits : %d", param->uartDataBits);
    log_info(TAG, "Uart Stop bits : %d", param->uartStopBits);
    log_info(TAG, "Uart Parity : %d", param->uartParity);
    log_info(TAG, "Antenna Select : %d", param->antennaSelect);
    log_info(TAG, "Is Security : %d", param->isSecurity);
    log_info(TAG, "Security Code : 0x%02X 0x%02X 0x%02X 0x%02X", param->securityCode[0], param->securityCode[1], param->securityCode[2], param->securityCode[3]);
    log_info(TAG, "Short Address : 0x%04X", param->shortAddress);
    log_info(TAG, "Mac Address : 0x%02X%02X%02X%02X%02X%02X%02X%02X", param->macAddress[0], param->macAddress[1], param->macAddress[2], param->macAddress[3], param->macAddress[4], param->macAddress[5], param->macAddress[6], param->macAddress[7]);
    char buffer[512];
    sprintf(buffer,"{\"cmd\" : \"Read zigbee parameter\",\"result\" : \"success\", \"PAN_ID\" : \"0x%04X\", \"Channel\" : \"%d\", \"User Address\" : \"0x%04X\", \"Point Type\" : \"%d\", \"Transfer Model\" : \"%d\", \"Uart Baudrate\" : \"%d\", \"Uart Data bits\" : \"%d\", \"Uart Stop bits\" : \"%d\", \"Uart Parity\" : \"%d\", \"Antenna Select\" : \"%d\", \"Is Security\" : \"%d\", \"Security Code\" : \"0x%02X%02X%02X%02X\", \"Short Address\" : \"0x%04X\", \"Mac Address\" : \"0x%02X%02X%02X%02X%02X%02X%02X%02X\"}\r\n",
            param->PAN_ID,
            param->Channel,
            param->userAddress,
            param->pointType,
            param->transferModel,
            param->uartBraudRate,
            param->uartDataBits,
            param->uartStopBits,
            param->uartParity,
            param->antennaSelect,
            param->isSecurity,
            param->securityCode[0], param->securityCode[1], param->securityCode[2], param->securityCode[3],
            param->shortAddress,
            param->macAddress[0], param->macAddress[1], param->macAddress[2], param->macAddress[3], param->macAddress[4], param->macAddress[5], param->macAddress[6], param->macAddress[7]);
    cli_shell_put_line(shell, buffer);
  }
  else if(isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Read Zigbee module param fail");
    // cli_shell_printf(shell, "Read Zigbee module param fail\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Read zigbee parameter\",\"result\" : \"fail\"}\r\n");
  }
  else
  {
    log_error(TAG, "Read Zigbee module param timeout");
    // cli_shell_printf(shell, "Read Zigbee module param timeout\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Read zigbee parameter\",\"result\" : \"timeout\"}\r\n");
  }
}
void zigbee_query_ed(ZigbeeMesh_t *zigbee,uint8_t isSuccess,void *arg){
  ShellContext_t *shell = (ShellContext_t *)arg;
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    log_info(TAG, "Query End Device position success");
    // cli_shell_printf(shell, "Reset Zigbee module success\r\n");
    if(zigbee->buff_len > 0){
    char buff[ZIGBEE_BUFFER_SIZE];
    memset(buff,0,ZIGBEE_BUFFER_SIZE);
    char hex[6];
    sprintf(buff,"[");
    for(uint32_t i = 0;i<zigbee->buff_len-1;i++){
      memset(hex,0,6);
      sprintf(hex,"%d,",zigbee->buff[i]);
      strcat(buff,hex);
    }
    memset(hex,0,6);
    sprintf(hex,"%d",zigbee->buff[zigbee->buff_len-1]);
    strcat(buff,hex);
    strcat(buff,"]");
    cli_shell_printf(shell, "{\"cmd\" : \"Query ED pos\",\"result\" : \"success\",\"data\" : %s}\r\n",buff);
    }
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Query End Device position fail");
    // cli_shell_printf(shell, "Reset Zigbee module fail\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Query ED pos\",\"result\" : \"fail\"}\r\n");
  }
  else
  {
    log_error(TAG, "Query End Device position timeout");
    // cli_shell_printf(shell, "Reset Zigbee module timeout\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Query ED pos\",\"result\" : \"timeout\"}\r\n");
  }
}
void zigbee_reset_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess,void *arg)
{
  ShellContext_t *shell = (ShellContext_t *)arg;
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    log_info(TAG, "Reset Zigbee module success");
    // cli_shell_printf(shell, "Reset Zigbee module success\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Reset zigbee module\",\"result\" : \"success\"}\r\n");
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Reset Zigbee module fail");
    // cli_shell_printf(shell, "Reset Zigbee module fail\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Reset zigbee module\",\"result\" : \"fail\"}\r\n");
  }
  else
  {
    log_error(TAG, "Reset Zigbee module timeout");
    // cli_shell_printf(shell, "Reset Zigbee module timeout\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Reset zigbee module\",\"result\" : \"timeout\"}\r\n");
  }
}

void zigbee_lowpower_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess,void *arg)
{
  ShellContext_t *shell = (ShellContext_t *)arg;
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    log_info(TAG, "Enter low power mode success");
    // cli_shell_printf(shell, "Enter low power mode success\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Enter zigbee low power mode\",\"result\" : \"success\"}\r\n");
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Enter low power mode fail");
    // cli_shell_printf(shell, "Enter low power mode fail\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Enter zigbee low power mode\",\"result\" : \"fail\"}\r\n");
  }
  else
  {
    log_error(TAG, "Enter low power mode timeout");
    // cli_shell_printf(shell, "Enter low power mode timeout\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Enter zigbee low power mode\",\"result\" : \"timeout\"}\r\n");
  }
}
void zigbee_write_cb(ZigbeeMesh_t *zigbee, uint8_t isSuccess,void *arg)
{
  ShellContext_t *shell = (ShellContext_t *)arg;
  if (isSuccess == ZIGBEE_RES_SUCCESS)
  {
    log_info(TAG, "Write param to Zigbee module success");
    // cli_shell_printf(shell, "Write param to Zigbee module success\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Write zigbee parameter\",\"result\" : \"success\"}\r\n");
  }
  else if (isSuccess == ZIGBEE_RES_FAIL)
  {
    log_error(TAG, "Write param to Zigbee module fail");
    // cli_shell_printf(shell, "Write param to Zigbee module fail\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Write zigbee parameter\",\"result\" : \"fail\"}\r\n");
  }
  else
  {
    log_error(TAG, "Write param to Zigbee module timeout");
    // cli_shell_printf(shell, "Write param to Zigbee module timeout\r\n");
    cli_shell_put_line(shell, "{\"cmd\" : \"Write zigbee parameter\",\"result\" : \"timeout\"}\r\n");
  }
}

static int cli_zigbee(ShellContext_t *shell, int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  if( argc < 2 || argc % 2 != 0)
  {
    cli_shell_help_handler(shell, 0, NULL);
    return -1;
  }
  if(strcmp(argv[1],"-c") == 0)
  {
    zigbee_connect(&zigbee, zigbee_connect_cb,shell);
  }
  else if(strcmp(argv[1],"-s") == 0)
  {
      uint8_t data[128] = {0};
      int data_len = 0;
      for(int i = 2 ; i < argc; i+=2)
      {
          if(strcmp(argv[i], "-d") == 0)
          {
              for(int j = i+1; j < argc; j++)
              {
                  int byte = 0;
                  sscanf(argv[j], "0x%02X", &byte);
                  data[data_len++] = byte & 0xFF;
              }
          }
      }
      if(data_len > 0)
      {
          zigbee.driver->p_write(data, data_len);
          log_info(TAG, "Send data to Zigbee module success");
      }
      else
      {
          log_error(TAG, "No data to send");
          return -1;
      }
  }
  else if(strcmp(argv[1],"-r") == 0)
  {
    zigbee_read_module(&zigbee, zigbee_read_cb,shell);
  }
  else if(strcmp(argv[1],"-rs") == 0)
  {
    zigbee_reset_module(&zigbee, zigbee_reset_cb,shell);
  }
  else if(strcmp(argv[1],"-qr") == 0){
    zigbee_query_ed_pos(&zigbee,zigbee_query_ed,shell);
  }
  else if(strcmp(argv[1],"-w") == 0)
  {
      for(int i = 2 ; i < argc; i+=2)
      {
          if(strcmp(argv[i], "-p") == 0)
          {
              int panid = 0;
              sscanf(argv[i+1], "0x%04X", &panid);
              if(panid < 0x0001 || panid > 0xFF00) panid = 0x1234;
              zigbee.param.PAN_ID = panid;
          }
          else if(strcmp(argv[i], "-c") == 0)
          {
              int channel = 0;
              sscanf(argv[i+1], "%d", &channel);
              if(channel < 0x0B || channel > 0x1A) channel = 0x0B;
              zigbee.param.Channel = channel;
          }
          else if(strcmp(argv[i], "-add") == 0)
          {
              int address = 0;
              sscanf(argv[i+1], "0x%04X", &address);
              zigbee.param.userAddress = address;
              zigbee.param.shortAddress = address;
          }
          else if(strcmp(argv[i], "-t") == 0)
          {
              int type = 0;
              sscanf(argv[i+1], "%d", &type);
              if(type < 1 || type > 3) type = 2;
              zigbee.param.pointType = type;
          }
          else if(strcmp(argv[i], "-tm") == 0)
          {
              int model = 0;
              sscanf(argv[i+1], "%d", &model);
              zigbee.param.transferModel = model;
          }
          else if(strcmp(argv[i], "-b") == 0)
          {
              unsigned int baudrate = 0;
              sscanf(argv[i+1], "%u", &baudrate);
              if(baudrate < 1 || baudrate > 8) baudrate = 8;
              zigbee.param.uartBraudRate = baudrate;
          }
          else if(strcmp(argv[i], "-d") == 0)
          {
              int databits = 8;
              sscanf(argv[i+1], "%d", &databits);
              if(databits != 1) databits = 1;
              zigbee.param.uartDataBits = databits;
          }
          else if(strcmp(argv[i], "-s") == 0)
          {
              int stopbits = 1;
              // sscanf(argv[i+1], "%d", &stopbits);
              stopbits = 1;
              zigbee.param.uartStopBits = stopbits;
          }
          else if(strcmp(argv[i], "-p") == 0)
          {
              if(strcmp(argv[i+1], "1") == 0)
                  zigbee.param.uartParity = 1;
              else if(strcmp(argv[i+1], "2") == 0)
                  zigbee.param.uartParity = 3;
              else if(strcmp(argv[i+1], "3") == 0)
                  zigbee.param.uartParity = 2;
              else zigbee.param.uartParity = 1;
          }
          else if(strcmp(argv[i], "-at") == 0)
          {
              int antenna = 0;
              sscanf(argv[i+1], "%d", &antenna);
              if(antenna == 0) antenna = 0;
              else antenna = 1;
              zigbee.param.antennaSelect = antenna;
          }
          else if(strcmp(argv[i], "-s") == 0)
          {
              int security = 0;
              sscanf(argv[i+1], "%d", &security);
              zigbee.param.isSecurity = security;
          }
          else if(strcmp(argv[i], "-sc") == 0)
          {
              int scode[4] = {0};
              sscanf(argv[i+1], "0x%02X", &scode[0]);
              sscanf(argv[i+2], "0x%02X", &scode[1]);
              sscanf(argv[i+3], "0x%02X", &scode[2]);
              sscanf(argv[i+4], "0x%02X", &scode[3]);
              memcpy(zigbee.param.securityCode, scode, 4);
              i+=3;
          }
        }
      zigbee_write_module(&zigbee, zigbee_write_cb,shell);
  }
  else
  {
    cli_shell_help_handler(shell, 0, NULL);
    return -1;
  }
  return 0;
}
#endif
#include "lwip.h"
#include "file_io.h"

int cli_get_ip_config(ShellContext_t *shell,int argc,char *argv[]){
  if(argc != 1) return -1;
  cli_shell_printf(shell,"\r\nGW-ETH-ZB IP Configuration\r\n\r\n");
  cli_shell_printf(shell,"Ethernet adapter Ethernet\r\n\r\n");
  cli_shell_printf(shell,"\tConnection-specific DNS Suffix  . : lan\r\n");
  cli_shell_printf(shell,"\tIPv4 Address. . . . . . . . . . . : %s\r\n",ipaddr_ntoa(&gnetif.ip_addr));
  cli_shell_printf(shell,"\tSubnet Mask . . . . . . . . . . . : %s\r\n",ipaddr_ntoa(&gnetif.netmask));
  cli_shell_printf(shell,"\tDefault Gateway . . . . . . . . . : %s\r\n",ipaddr_ntoa(&gnetif.gw));
  return 1;
}

int cli_get_service(ShellContext_t *shell,int argc,char *argv[]){
  if(strcmp(argv[1],"-show") == 0){
    cli_shell_printf(shell,"Sevices : \r\n\r\n");
    cli_shell_printf(shell,"MQTT : \r\n");
    cli_shell_printf(shell,"\tHost : %s\r\n",MQTT_HOST);
    cli_shell_printf(shell,"\tPort : %d\r\n",USER_MQTT_PORT);
    cli_shell_printf(shell,"\tCLI ID : %s\r\n",MQTT_CLIENT_ID);
    cli_shell_printf(shell,"\tUser : %s\r\n",MQTT_CLIENT_USER);
    cli_shell_printf(shell,"\tPass : %s\r\n",MQTT_CLIENT_PASS);
    cli_shell_printf(shell,"\r\nModbus TCP : \r\n");
    cli_shell_printf(shell,"\tPort : %d\r\n",MODBUS_TCP_PORT);
    cli_shell_printf(shell,"\r\nHTTP Server : \r\n\tPort : %d\r\n",HTTP_SERVER_PORT);
    cli_shell_printf(shell,"\r\nShell Server : \r\n\tPort : %d\r\n",SHELL_TCP_PORT);
    cli_shell_printf(shell,"\r\nShell USB : \r\n\tChannel : %d\r\n",USB_SHELL);
    cli_shell_printf(shell,"\r\nModbus RTU over USB : \r\n\tChannel : %d\r\n\tID : %d\r\n",USB_MB,MBS_USB_ID);
  }
  return -1;
}
int cli_create_file(ShellContext_t *shell, int argc, char *argv[]){
  if(argc < 2){
      cli_shell_printf(shell,"Usage: touch <filename>\n");
      return -1;
  }
  const char * filename = argv[1];
  FILE * f = fopen(filename, "w");
  if(f == NULL){
      cli_shell_printf(shell,"Create file %s failed\n",filename);
      return -1;
  }
  fclose(f);
  cli_shell_printf(shell,"File %s created\n",filename);
  return 0;
}
int cli_delete_file(ShellContext_t *shell, int argc, char *argv[]){
  if(argc < 2){
      cli_shell_printf(shell,"Usage: rm <filename>\n");
      return -1;
  }
  const char * filename = argv[1];
  if(remove(filename) != 0){
      cli_shell_printf(shell,"Delete file %s failed\n",filename);
      return -1;
  }
  cli_shell_printf(shell,"File %s deleted\n",filename);
  return 0;
}
int cli_list_files(ShellContext_t *shell, int argc, char *argv[]){
  // if(argv != 1) return -1;
  lfs_dir_t dir;
  struct lfs_info info;
  int err;

  err = lfs_dir_open(CONFIG_LITTLEFS, &dir, path);
  if (err < 0)
  {
    cli_shell_printf(shell, "Failed to open dir %s, err=%d", path, err);
    return -1;
  }
  while (true)
  {
    err = lfs_dir_read(CONFIG_LITTLEFS, &dir, &info);
    if (err < 0)
    {
      cli_shell_printf(shell, "Read dir error %d", err);
      break;
    }
    if (err == 0)
    {
      // end of directory
      break;
    }

    if (info.type == LFS_TYPE_DIR)
    {
      cli_shell_printf(shell, "%s\r\n", info.name);
    }
    else if (info.type == LFS_TYPE_REG)
    {
      cli_shell_printf(shell, "%s\r\n", info.name);
    }
  }
  lfs_dir_close(CONFIG_LITTLEFS, &dir);
  return 0;
}
int cli_read_file(ShellContext_t *shell, int argc, char *argv[]){
  if(argc < 2){
      cli_shell_printf(shell,"Usage: cat <filename>\n");
      return -1;
  }
  const char * filename = argv[1];
  FILE * f = fopen(filename, "r");
  if(f == NULL){
      cli_shell_printf(shell,"Open file %s failed\n",filename);
      return -1;
  }
  char buffer[129];
  size_t n;
  while((n = fread(buffer,1,sizeof(buffer)-1,f)) > 0){
      buffer[n] = '\0';
      cli_shell_printf(shell,"%s",buffer);
  }
  fclose(f);
  cli_shell_printf(shell,"\n");
  return 0;
}
int cli_write_file(ShellContext_t *shell, int argc, char *argv[]){
  if(argc < 3){
      cli_shell_printf(shell,"Usage: write <filename> <data>\n");
      return -1;
  }
  const char * filename = argv[1];
  const char * data = argv[2];
  FILE * f = fopen(filename, "a+");
  if(f == NULL){
      cli_shell_printf(shell,"Open file %s failed\n",filename);
      return -1;
  }
  fwrite(data,1,strlen(data),f);
  fwrite("\n",1,1,f);
  fclose(f);
  cli_shell_printf(shell,"Write to file %s success\n",filename);
  return 0;
}
int cli_cd_direct(ShellContext_t *shell,int argc,char *argv[]){
  if(argc != 2){
      cli_shell_printf(shell,"Usage: cd <directory>\n");
      return -1;
  }
  return 0;
}
const Cli_Shell_Cmd *const g_shell_commands = s_shell_commands;
const size_t g_num_shell_commands = ARRAY_SIZE(s_shell_commands);