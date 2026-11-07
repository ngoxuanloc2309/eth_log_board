#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H
#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initializes the HTTP server.
 *
 * This function sets up all necessary resources and configurations
 * required to start the HTTP server. It should be called once during
 * system initialization before handling any HTTP requests.
 */
void http_server_init(void);


#ifdef __cplusplus
}
#endif
#endif // HTTP_SERVER_H