#ifndef NETWORK_POSIX_H
#define NETWORK_POSIX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/x509_crt.h>

#include "transport_interface.h"

typedef enum
{
    NETWORK_SECURITY_TLS,
    NETWORK_SECURITY_PLAIN
} NetworkSecurityMode_t;

typedef struct NetworkContext
{
    int socket_fd;
    NetworkSecurityMode_t security_mode;

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt ca_chain;

} NetworkContext_t;

/* Reports whether the lwIP Ethernet interface has link and an IPv4 address. */
bool network_posix_is_ready(void);

int network_posix_connect(
    NetworkContext_t *ctx,
    const char *host,
    const char *port,
    const char *ca,
    uint32_t ca_len
);

int network_posix_connect_mode(
    NetworkContext_t *ctx,
    const char *host,
    const char *port,
    NetworkSecurityMode_t security_mode,
    const char *ca,
    uint32_t ca_len,
    uint32_t timeout_ms);

void network_posix_disconnect(NetworkContext_t *ctx);

int32_t network_posix_send(
    NetworkContext_t *ctx,
    const void *buffer,
    size_t length);

int32_t network_posix_recv(
    NetworkContext_t *ctx,
    void *buffer,
    size_t length);

int32_t network_posix_recv_poll(
    NetworkContext_t *ctx,
    void *buffer,
    size_t length);

#endif