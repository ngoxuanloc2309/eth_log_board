#include "cli_shell.h"
#include "socketserver.h"


/**
 * @brief Initializes the TCP shell interface.
 *
 * This function sets up the necessary resources and configurations
 * required to enable shell communication over TCP.
 *
 * Call this function during system initialization before using
 * any shell TCP features.
 */
void shell_tcp_init(SocketServer *shell);