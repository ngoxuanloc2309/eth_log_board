#ifndef MMB_SERIAL_H
#define MMB_SERIAL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "nanomodbus.h"
#include "board.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define MMB_BYTES_TIMEOUT_mS 100
#define MMB_TIMEOUT_mS 300

typedef struct MMBCLient{
    nmbs_t *client;
    SemaphoreHandle_t mutex;
    int port;
}MMBCLient_t;

void mmb_init(MMBCLient_t *client ,nmbs_t *mmb, nmbs_platform_conf *conf,int port);

int mmb_read_holding_registers(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint16_t *dest);
int mmb_write_single_register(MMBCLient_t *client ,uint8_t slave_id, uint16_t reg_addr, uint16_t value);
int mmb_write_multiple_registers(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, const uint16_t *values);
int mmb_read_input_registers(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint16_t *dest);
int mmb_read_coils(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *dest);
int mmb_write_single_coil(MMBCLient_t *client ,uint8_t slave_id, uint16_t coil_addr, bool value);
int mmb_write_multiple_coils(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, const uint8_t *values);
int mmb_read_discrete_inputs(MMBCLient_t *client ,uint8_t slave_id, uint16_t start_addr, uint16_t quantity, uint8_t *dest);

#ifdef __cplusplus
}
#endif
#endif /* MMB_SERIAL_H */