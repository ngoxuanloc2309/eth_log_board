#ifndef SHELL_SERIAL_H
#define SHELL_SERIAL_H
#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initializes the serial interface for the shell.
 *
 * This function sets up the necessary hardware and software resources
 * required for serial communication in the shell application.
 * It should be called before any serial shell operations are performed.
 */
void shell_serial_init(void);


#ifdef _cplusplus
}
#endif
#endif // SHELL_SERIAL_H