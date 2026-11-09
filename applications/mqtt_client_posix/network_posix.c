#define _POSIX_C_SOURCE 200112L

#include "network_posix.h"

#include "logger.h"

#include <string.h>
#include "lwip/errno.h"
#include "FreeRTOS.h"
#include "task.h"
#include <unistd.h>
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip.h"
#include "lwip/ip4_addr.h"

#include <mbedtls/error.h>

#define NETWORK_POSIX_LOG_TAG "MQTT_NET"
#define NETWORK_POLL_TIMEOUT_MS 1000U
#define NETWORK_DEFAULT_CONNECT_TIMEOUT_MS 10000U

bool network_posix_is_ready(void)
{
    return netif_is_up(&gnetif) && netif_is_link_up(&gnetif) &&
           !ip4_addr_isany(netif_ip4_addr(&gnetif));
}

static uint32_t network_elapsed_ms(TickType_t start)
{
    TickType_t elapsed_ticks = xTaskGetTickCount() - start;
    return (uint32_t)(elapsed_ticks * portTICK_PERIOD_MS);
}

static bool network_is_receive_timeout(void)
{
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}

static int posix_net_send(void *ctx,
                          const unsigned char *buf,
                          size_t len)
{
    int fd = *(int *)ctx;

    if (len > INT_MAX) {
        len = INT_MAX;
    }

    ssize_t ret = send(fd, buf, len, MSG_NOSIGNAL);

    if (ret >= 0) {
        return (int)ret;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    if (errno == EINTR) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int posix_net_recv(void *ctx,
                          unsigned char *buf,
                          size_t len)
{
    int fd = *(int *)ctx;

    if (len > INT_MAX) {
        len = INT_MAX;
    }

    ssize_t ret = recv(fd, buf, len, 0);

    if (ret > 0) {
        return (int)ret;
    }

    if (ret == 0) {
        return 0;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK ||
        errno == EINTR) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int tcp_connect(const char *host,
                       const char *port,
                       uint32_t timeout_ms,
                       TickType_t start)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp;
    int fd = -1;
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    if (timeout_ticks == 0U) {
        timeout_ticks = 1U;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &result) != 0)
    {
        LOGE(NETWORK_POSIX_LOG_TAG, "DNS resolution failed for %s", host);
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        int nonblocking = 1;
        int socket_error = 0;
        bool connected = false;
        socklen_t socket_error_length = sizeof(socket_error);

        if ((uint32_t)(xTaskGetTickCount() - start) >=
            (uint32_t)timeout_ticks) {
            break;
        }

        fd = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);

        if (fd < 0)
            continue;

        if (lwip_ioctl(fd, FIONBIO, &nonblocking) != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        if (connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) {
            connected = true;
        } else {
            if (errno != EINPROGRESS && errno != EWOULDBLOCK) {
                close(fd);
                fd = -1;
                continue;
            }

            while ((uint32_t)(xTaskGetTickCount() - start) <
                   (uint32_t)timeout_ticks) {
                fd_set write_set;
                fd_set error_set;
                struct timeval wait_time;
                uint32_t elapsed_ms = network_elapsed_ms(start);
                uint32_t remaining_ms = timeout_ms > elapsed_ms
                                            ? timeout_ms - elapsed_ms
                                            : 0U;
                int select_result;

                if (remaining_ms == 0U) {
                    break;
                }

                FD_ZERO(&write_set);
                FD_ZERO(&error_set);
                FD_SET(fd, &write_set);
                FD_SET(fd, &error_set);
                wait_time.tv_sec = (long)(remaining_ms / 1000U);
                wait_time.tv_usec = (long)((remaining_ms % 1000U) * 1000U);

                select_result = lwip_select(fd + 1,
                                            NULL,
                                            &write_set,
                                            &error_set,
                                            &wait_time);
                if (select_result < 0 && errno == EINTR) {
                    continue;
                }
                if (select_result <= 0) {
                    break;
                }
                if (lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR,
                                    &socket_error,
                                    &socket_error_length) == 0 &&
                    socket_error == 0) {
                    connected = true;
                    break;
                }
                break;
            }
        }

        if (!connected ||
            (uint32_t)(xTaskGetTickCount() - start) >=
                (uint32_t)timeout_ticks) {
            close(fd);
            fd = -1;
            continue;
        }

        nonblocking = 0;
        if (lwip_ioctl(fd, FIONBIO, &nonblocking) != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        {
            const int socket_timeout_ms = (int)NETWORK_POLL_TIMEOUT_MS;
            if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                           &socket_timeout_ms,
                           sizeof(socket_timeout_ms)) != 0 ||
                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                           &socket_timeout_ms,
                           sizeof(socket_timeout_ms)) != 0) {
                close(fd);
                fd = -1;
                continue;
            }
        }

        break;
    }

    freeaddrinfo(result);
    return fd;
}

int network_posix_connect_mode(
    NetworkContext_t *ctx,
    const char *host,
    const char *port,
    NetworkSecurityMode_t security_mode,
    const char *ca,
    uint32_t ca_len,
    uint32_t timeout_ms)
{
    int ret = -1;
    TickType_t connection_start = xTaskGetTickCount();

    if (ctx == NULL || host == NULL || host[0] == '\0' ||
        port == NULL || port[0] == '\0' ||
        (security_mode != NETWORK_SECURITY_TLS &&
         security_mode != NETWORK_SECURITY_PLAIN) ||
        (security_mode == NETWORK_SECURITY_TLS &&
         (ca == NULL || ca[0] == '\0')))
    {
        return -1;
    }

    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->socket_fd = -1;
    ctx->security_mode = security_mode;

    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_x509_crt_init(&ctx->ca_chain);

    if (timeout_ms == 0U) {
        timeout_ms = NETWORK_DEFAULT_CONNECT_TIMEOUT_MS;
    }

    LOGI(NETWORK_POSIX_LOG_TAG, "Resolving and connecting to %s:%s",
         host, port);
    ctx->socket_fd = tcp_connect(host, port, timeout_ms, connection_start);
    if (ctx->socket_fd < 0)
    {
        LOGE(NETWORK_POSIX_LOG_TAG,
             "TCP connection to %s:%s failed or timed out", host, port);
        goto fail;
    }
    LOGI(NETWORK_POSIX_LOG_TAG, "TCP connection established");

    if (security_mode == NETWORK_SECURITY_PLAIN)
    {
        return 0;
    }

    ret = mbedtls_ctr_drbg_seed(
        &ctx->ctr_drbg,
        mbedtls_entropy_func,
        &ctx->entropy,
        (const unsigned char *)"coreHTTP-posix",
        sizeof("coreHTTP-posix") - 1);
    
    if (ret != 0)
        goto fail;

    ret = mbedtls_x509_crt_parse(
        &ctx->ca_chain, (const unsigned char*)ca,(size_t) ca_len);
    LOGD(NETWORK_POSIX_LOG_TAG,"mbedtls parse result : %d",ret);
    if (ret < 0)
        goto fail;

    ret = mbedtls_ssl_config_defaults(
        &ctx->conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    LOGD(NETWORK_POSIX_LOG_TAG,"mbedtls_ssl_config_defaults result : %d",ret);   
    if (ret != 0)
        goto fail;

    mbedtls_ssl_conf_authmode(
        &ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);

    mbedtls_ssl_conf_ca_chain(
        &ctx->conf, &ctx->ca_chain, NULL);

    mbedtls_ssl_conf_rng(
        &ctx->conf,
        mbedtls_ctr_drbg_random,
        &ctx->ctr_drbg);

    ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);

    if (ret != 0)
        goto fail;

    /* Set SNI and hostname verification target. */
    ret = mbedtls_ssl_set_hostname(&ctx->ssl, host);

    if (ret != 0)
        goto fail;

    mbedtls_ssl_set_bio(
        &ctx->ssl,
        &ctx->socket_fd,
        posix_net_send,
        posix_net_recv,
        NULL);

    LOGI(NETWORK_POSIX_LOG_TAG, "Starting TLS handshake");
    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            goto fail;
        if (network_elapsed_ms(connection_start) >= timeout_ms) {
            ret = MBEDTLS_ERR_SSL_TIMEOUT;
            goto fail;
        }
    }
    LOGI(NETWORK_POSIX_LOG_TAG, "TLS handshake completed");

    uint32_t flags =
        mbedtls_ssl_get_verify_result(&ctx->ssl);
    LOGD(NETWORK_POSIX_LOG_TAG,"mbedtls_ssl_get_verify_result flag = %d",flags);
    if (flags != 0)
    {
        char message[256];

        mbedtls_x509_crt_verify_info(
            message, sizeof(message), "  ! ", flags);

        LOGE(NETWORK_POSIX_LOG_TAG, "Certificate verification failed:\n%s",
             message);

        ret = -1;
        goto fail;
    }

    return 0;

fail:
    if (ret != 0)
    {
        char message[256];

        mbedtls_strerror(ret, message, sizeof(message));

        LOGE(NETWORK_POSIX_LOG_TAG, "%s connection error: %s (%d)",
             security_mode == NETWORK_SECURITY_TLS ? "TLS" : "TCP",
             message, ret);
    }

    network_posix_disconnect(ctx);
    return -1;
}

int network_posix_connect(
    NetworkContext_t *ctx,
    const char *host,
    const char *port,
    const char *ca,
    uint32_t ca_len)
{
    return network_posix_connect_mode(ctx,
                                      host,
                                      port,
                                      NETWORK_SECURITY_TLS,
                                      ca,
                                      ca_len,
                                      NETWORK_DEFAULT_CONNECT_TIMEOUT_MS);
}

int32_t network_posix_send(
    NetworkContext_t *ctx,
    const void *buffer,
    size_t length)
{
    if (ctx == NULL || (buffer == NULL && length != 0U))
    {
        return -1;
    }

    if (ctx->security_mode == NETWORK_SECURITY_PLAIN)
    {
        size_t send_length = length > (size_t)INT_MAX ? (size_t)INT_MAX : length;
        ssize_t sent = send(ctx->socket_fd, buffer, send_length, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR)
        {
            return 0;
        }
        return sent >= 0 ? (int32_t)sent : -1;
    }

    int ret = mbedtls_ssl_write(&ctx->ssl,
                                (const unsigned char *)buffer,
                                length);

    if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
        ret == MBEDTLS_ERR_SSL_TIMEOUT) {
        return 0;
    }

    return ret > 0 ? ret : -1;
}

int32_t network_posix_recv(
    NetworkContext_t *ctx,
    void *buffer,
    size_t length)
{
    if (ctx == NULL || (buffer == NULL && length != 0U))
    {
        return -1;
    }

    if (ctx->security_mode == NETWORK_SECURITY_PLAIN)
    {
        size_t receive_length = length > (size_t)INT_MAX ? (size_t)INT_MAX : length;
        ssize_t received = recv(ctx->socket_fd, buffer, receive_length, 0);
        if (received < 0 && network_is_receive_timeout())
        {
            return 0;
        }
        return received > 0 ? (int32_t)received : -1;
    }

    int ret = mbedtls_ssl_read(&ctx->ssl,
                               (unsigned char *)buffer,
                               length);

    if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        return -1;

    if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
        ret == MBEDTLS_ERR_SSL_TIMEOUT) {
        return 0;
    }

    return ret > 0 ? ret : -1;
}

int32_t network_posix_recv_poll(
    NetworkContext_t *ctx,
    void *buffer,
    size_t length)
{
    if (ctx == NULL || (buffer == NULL && length != 0U))
    {
        return -1;
    }

    if (ctx->security_mode == NETWORK_SECURITY_PLAIN) {
        size_t receive_length =
            length > (size_t)INT_MAX ? (size_t)INT_MAX : length;
        ssize_t result = recv(ctx->socket_fd, buffer, receive_length, 0);

        if (result > 0) {
            return (int32_t)result;
        }
        if (result == 0) {
            return -1;
        }
        return network_is_receive_timeout() ? 0 : -1;
    }

    int ret = mbedtls_ssl_read(&ctx->ssl,
                               (unsigned char *)buffer,
                               length);

    if (ret > 0) {
        return ret;
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
        ret == MBEDTLS_ERR_SSL_TIMEOUT) {
        return 0;
    }
    return -1;
}

void network_posix_disconnect(NetworkContext_t *ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->socket_fd >= 0)
    {
        if (ctx->security_mode == NETWORK_SECURITY_TLS)
        {
            (void)mbedtls_ssl_close_notify(&ctx->ssl);
        }
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }

    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
    mbedtls_x509_crt_free(&ctx->ca_chain);
}