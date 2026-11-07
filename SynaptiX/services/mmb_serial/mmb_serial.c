#include "mmb_serial.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "semphr.h"

static const char *TAG = "MMBSerial";

#define mmb_lock(x)  xSemaphoreTake(x->mutex,portMAX_DELAY)
#define mmb_unlock(x) xSemaphoreGive(x->mutex)

void mmb_init(MMBCLient_t *client ,nmbs_t *mmb,nmbs_platform_conf *conf,int port){
    client->client = mmb;
    nmbs_error status = nmbs_client_create(client->client,conf);
    if(status != NMBS_ERROR_NONE){
        log_error(TAG,"Can't create modbus");
        return;
    }
    client->mutex = xSemaphoreCreateMutex();
    if(client->mutex == NULL){
        log_error(TAG,"Can't create master modbus mutex");
    }
    nmbs_set_byte_timeout(client->client, MMB_BYTES_TIMEOUT_mS);
    nmbs_set_read_timeout(client->client, MMB_TIMEOUT_mS);
    client->port = port;
    //log_debug(TAG,"Modbus initialized");
}

int mmb_read_holding_registers(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint16_t *dest){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_read_holding_registers(client->client,start_addr,quantity,dest);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Read holding registers failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Read holding registers start: %u, quantity: %u success",slave_id ,start_addr,quantity);
    log_print_hex(LOGGER_DEBUG,TAG,(uint8_t*)dest,quantity*2);
    return 0;
}
int mmb_write_single_register(MMBCLient_t *client ,uint8_t slave_id, uint16_t reg_addr, uint16_t value){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_write_single_register(client->client,reg_addr,value);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Write single register failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Write single register addr: %u, value: %u success",slave_id,reg_addr,value);
    return 0;
}
int mmb_write_multiple_registers(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, const uint16_t *values){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_write_multiple_registers(client->client,start_addr,quantity,values);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Write multiple registers failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Write multiple registers start: %u, quantity: %u success",slave_id,start_addr,quantity);
    return 0;
}
int mmb_read_input_registers(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint16_t *dest){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_read_input_registers(client->client,start_addr,quantity,dest);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Read input registers failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Read input registers start: %u, quantity: %u success",slave_id,start_addr,quantity);
    log_print_hex(LOGGER_DEBUG,TAG,(uint8_t*)dest,quantity * 2);
    return 0;
}
int mmb_read_coils(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *dest){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_read_coils(client->client,start_addr,quantity,dest);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Read coils failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Read coils start: %u, quantity: %u success",slave_id,start_addr,quantity);
    return 0;
}
int mmb_write_single_coil(MMBCLient_t *client ,uint8_t slave_id, uint16_t coil_addr, bool value){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_write_single_coil(client->client,coil_addr,value);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Write single coil failed: %d",slave_id,status);
        return status;
    }
    log_debug(TAG,"Slave %d Write single coil addr: %u, value: %u success",slave_id,coil_addr,value);
    return 0;
}
int mmb_write_multiple_coils(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, const uint8_t *values){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_write_multiple_coils(client->client,start_addr,quantity,values);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Write multiple coils failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Write multiple coils start: %u, quantity: %u success",slave_id,start_addr,quantity);
    return 0;
}
int mmb_read_discrete_inputs(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *dest){
    mmb_lock(client);
    nmbs_set_destination_rtu_address(client->client,slave_id);
    nmbs_error status = nmbs_read_discrete_inputs(client->client,start_addr,quantity,dest);
    mmb_unlock(client);
    if(status != NMBS_ERROR_NONE){
        uart_flush(client->port);
        log_error(TAG,"Slave %d Read discrete inputs failed: %d",slave_id,status);
        return -1;
    }
    log_debug(TAG,"Slave %d Read discrete inputs start: %u, quantity: %u success",slave_id,start_addr,quantity);
    return 0;
}