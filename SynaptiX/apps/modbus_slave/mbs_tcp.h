#ifndef __MODBUSTCPSV_H__
#define __MODBUSTCPSV_H__
#ifdef __cplusplus
extern "C"
{
#endif

#include "socketserver.h"

#define MODBUS_TCP_PORT 502

typedef struct {
    SocketServer server;
    int port;
} ModbusTCPServer;

/**
 * @brief Initializes the Modbus TCP server.
 *
 * @param server Pointer to the ModbusTCPServer structure.
 * @param port Port number to listen on.
 * @return int 0 on success, -1 on failure.
 */
/**
 * @brief Initializes a Modbus TCP server on the specified port.
 *
 * This function sets up the ModbusTCPServer structure and prepares it to accept
 * incoming Modbus TCP connections on the given port.
 *
 * @param server Pointer to a ModbusTCPServer structure to be initialized.
 * @param port The TCP port number on which the server will listen for connections.
 * @return 0 on success, or a negative error code on failure.
 */
int modbus_tcp_server_init(ModbusTCPServer* server, int port);

#ifdef __cplusplus
}
#endif
#endif // __MODBUSTCPSV_H__