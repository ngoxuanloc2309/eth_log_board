#ifndef FILE_SYSTEM_PORT_H
#define FILE_SYSTEM_PORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the board W25Q device and mount its existing filesystem.
 *
 * Must be called from task context after the scheduler starts. This function
 * never formats flash; a failed mount is reported through errno.
 *
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_port_init(void);

/**
 * @brief Report whether the LittleFS volume is currently mounted.
 *
 * This accessor does not initialize, mount, or format storage.
 *
 * @return true when the volume is mounted.
 */
bool file_system_port_is_mounted(void);

/**
 * @brief Explicitly format the board flash as LittleFS.
 *
 * This destructive operation must be requested intentionally from task
 * context. It leaves the filesystem unmounted; call
 * file_system_port_init() afterward to mount it.
 *
 * @return 0 on success; -1 on failure with errno set.
 */
int32_t file_system_port_format(void);

#ifdef __cplusplus
}
#endif

#endif
