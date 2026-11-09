#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Queue startup of the single-client lwIP raw TCP web server.
 *
 * Call after lwIP initialization from task context.
 *
 * @return true when the startup callback was queued successfully.
 */
bool web_server_init(void);

/**
 * @brief Dispatch queued web requests and refresh application-owned snapshots.
 *
 * Call regularly from the application task that owns the radio API.
 */
void web_server_process(void);

#ifdef __cplusplus
}
#endif

#endif
