/*
 * cli_shell.h
 *
 *  Created on: Sep 10, 2022
 *      Author: Admin
 */

#ifndef SERVICE_CLI_CLI_SHELL_H_
#define SERVICE_CLI_CLI_SHELL_H_

#include <stddef.h>
#include "app_config.h"

#define SHELL_RX_BUFFER_SIZE (256)
#define SHELL_MAX_ARGS (16)

#ifndef SHELL_PROMPT
#define SHELL_PROMPT "shell> "
#endif
typedef struct Cli_Shell_Cmd_t Cli_Shell_Cmd;
typedef struct Cli_ShellImpl sCliShellImpl_t;
typedef struct ShellContext  ShellContext_t;

struct Cli_Shell_Cmd_t{
      const char *command;
      int (*handler)(ShellContext_t *shell,int argc, char *argv[]);
      const char *help;
};

struct Cli_ShellImpl {
  //! Function to call whenever a character needs to be sent out.
  void *arg;
  int (*send_char)(void *arg,char c);
  int (*receive_char)(void *arg); // Optional, can be NULL
  int (*send_str)(void *arg,char *str);
};

struct ShellContext {
  sCliShellImpl_t impl;
  size_t rx_size;
  char rx_buffer[SHELL_RX_BUFFER_SIZE];
};



/**
 * @brief Boots or initializes the CLI shell.
 *
 * This function sets up the command-line interface (CLI) shell environment
 * using the provided shell context. It should be called during system startup
 * or when the shell needs to be (re)initialized.
 *
 * @param shell Pointer to a ShellContext_t structure containing the shell state and configuration.
 */
void cli_shell_boot(ShellContext_t *shell);

//! Call this when a character is received. The character is processed synchronously.
/**
 * @brief Receives a single character input for the CLI shell.
 *
 * This function processes an incoming character from the user input and updates
 * the shell context accordingly. It is typically called whenever a new character
 * is received from the input stream (e.g., UART, USB, etc.).
 *
 * @param shell Pointer to the shell context structure.
 * @param c     The character received from the input.
 */
void cli_shell_receive_char(ShellContext_t *shell,char c);

//! Print help command
/**
 * @brief Handler function for the 'help' command in the CLI shell.
 *
 * This function processes the 'help' command, typically displaying a list of available commands
 * or providing usage information to the user.
 *
 * @param shell Pointer to the ShellContext_t structure representing the current shell context.
 * @param argc Number of arguments passed to the command.
 * @param argv Array of argument strings.
 * @return int Status code (0 for success, non-zero for error).
 */
int cli_shell_help_handler(ShellContext_t *shell,int argc, char *argv[]);

//! Prints a line then a newline
/**
 * @brief Outputs a line of text to the shell interface.
 *
 * This function sends the specified string as a line to the shell associated with the given context.
 *
 * @param shell Pointer to the ShellContext_t structure representing the shell instance.
 * @param str   Null-terminated string to be output to the shell.
 */
void cli_shell_put_line(ShellContext_t *shell,const char *str);


/**
 * @brief Prints formatted output to the shell interface.
 *
 * This function sends a formatted string to the specified shell context,
 * similar to printf. It supports variable arguments for formatting.
 *
 * @param shell Pointer to the shell context where the output will be sent.
 * @param fmt Format string (as in printf).
 * @param ... Additional arguments to be formatted according to fmt.
 */
void cli_shell_printf(ShellContext_t *shell, const char *fmt, ...);

/**
 * @brief Externally defined array of shell command structures.
 *
 * This constant pointer references the list of available shell commands
 * for the CLI (Command Line Interface) shell. Each entry in the array
 * is a pointer to a Cli_Shell_Cmd structure, which defines a command
 * and its associated handler.
 *
 * @note The array is expected to be terminated appropriately (e.g., with a NULL pointer).
 */
extern const Cli_Shell_Cmd *const g_shell_commands;
extern const size_t g_num_shell_commands;

#endif /* SERVICE_CLI_CLI_SHELL_H_ */