/** 
  ******************************************************************************
  * File Name       : net_sockets.c.h
  * Description     : TCP/IP or UDP/IP networking functions implementation based
                    on LwIP API see the file "mbedTLS/library/net_socket_template.c"
                    for the standard implmentation
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2019 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/net_sockets.h"

#include <limits.h>
#include <string.h>
#include <sys/time.h>

#include "cmsis_os2.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static int net_would_block(int error)
{
#if defined(EAGAIN)
    if (error == EAGAIN) {
        return 1;
    }
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || EWOULDBLOCK != EAGAIN)
    if (error == EWOULDBLOCK) {
        return 1;
    }
#endif
    return 0;
}

static int net_valid_context(const mbedtls_net_context *ctx)
{
    return ctx != NULL && ctx->fd >= 0;
}

static int net_socket_type(int proto, int *type, int *protocol)
{
    if (proto == MBEDTLS_NET_PROTO_TCP) {
        *type = SOCK_STREAM;
        *protocol = IPPROTO_TCP;
        return 0;
    }
    if (proto == MBEDTLS_NET_PROTO_UDP) {
        *type = SOCK_DGRAM;
        *protocol = IPPROTO_UDP;
        return 0;
    }
    return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
}

void mbedtls_net_init(mbedtls_net_context *ctx)
{
    if (ctx != NULL) {
        ctx->fd = -1;
    }
}

int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host,
                        const char *port, int proto)
{
    int ret;
    int type;
    int protocol;
    struct addrinfo hints;
    struct addrinfo *addr_list = NULL;
    struct addrinfo *cur;

    if (ctx == NULL || host == NULL || port == NULL || ctx->fd >= 0 ||
        net_socket_type(proto, &type, &protocol) != 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;

    if (lwip_getaddrinfo(host, port, &hints, &addr_list) != 0) {
        return MBEDTLS_ERR_NET_UNKNOWN_HOST;
    }

    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        int fd = lwip_socket(cur->ai_family, cur->ai_socktype,
                             cur->ai_protocol);
        if (fd < 0) {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (lwip_connect(fd, cur->ai_addr, (socklen_t) cur->ai_addrlen) == 0) {
            ctx->fd = fd;
            ret = 0;
            break;
        }

        (void) lwip_close(fd);
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    lwip_freeaddrinfo(addr_list);
    return ret;
}

int mbedtls_net_bind(mbedtls_net_context *ctx, const char *bind_ip,
                     const char *port, int proto)
{
    int ret;
    int type;
    int protocol;
    struct addrinfo hints;
    struct addrinfo *addr_list = NULL;
    struct addrinfo *cur;

    if (ctx == NULL || port == NULL || ctx->fd >= 0 ||
        net_socket_type(proto, &type, &protocol) != 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;
    if (bind_ip == NULL) {
        hints.ai_flags = AI_PASSIVE;
    }

    if (lwip_getaddrinfo(bind_ip, port, &hints, &addr_list) != 0) {
        return MBEDTLS_ERR_NET_UNKNOWN_HOST;
    }

    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        int reuse = 1;
        socklen_t reuse_len = (socklen_t) sizeof(reuse);
        int fd = lwip_socket(cur->ai_family, cur->ai_socktype,
                             cur->ai_protocol);
        if (fd < 0) {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                            reuse_len) != 0) {
            (void) lwip_close(fd);
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if (lwip_bind(fd, cur->ai_addr, (socklen_t) cur->ai_addrlen) != 0) {
            (void) lwip_close(fd);
            ret = MBEDTLS_ERR_NET_BIND_FAILED;
            continue;
        }

        if (proto == MBEDTLS_NET_PROTO_TCP &&
            lwip_listen(fd, MBEDTLS_NET_LISTEN_BACKLOG) != 0) {
            (void) lwip_close(fd);
            ret = MBEDTLS_ERR_NET_LISTEN_FAILED;
            continue;
        }

        ctx->fd = fd;
        ret = 0;
        break;
    }

    lwip_freeaddrinfo(addr_list);
    return ret;
}

int mbedtls_net_accept(mbedtls_net_context *bind_ctx,
                       mbedtls_net_context *client_ctx,
                       void *client_ip, size_t buf_size, size_t *cip_len)
{
    int type;
    socklen_t type_len = (socklen_t) sizeof(type);
    struct sockaddr_storage remote_addr;
    socklen_t remote_addr_len = (socklen_t) sizeof(remote_addr);
    int fd;

    if (!net_valid_context(bind_ctx) || client_ctx == NULL ||
        bind_ctx == client_ctx || client_ctx->fd >= 0 ||
        (client_ip != NULL && cip_len == NULL)) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }

    if (lwip_getsockopt(bind_ctx->fd, SOL_SOCKET, SO_TYPE, &type,
                        &type_len) != 0 ||
        (type != SOCK_STREAM && type != SOCK_DGRAM)) {
        return MBEDTLS_ERR_NET_ACCEPT_FAILED;
    }

    if (type == SOCK_STREAM) {
        fd = lwip_accept(bind_ctx->fd, (struct sockaddr *) &remote_addr,
                         &remote_addr_len);
        if (fd < 0) {
            return net_would_block(errno) || errno == EINTR ?
                   MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }
    } else {
        struct sockaddr_storage local_addr;
        socklen_t local_addr_len = (socklen_t) sizeof(local_addr);
        char peek_byte;
        int reuse = 1;
        int replacement_fd;

        if (lwip_recvfrom(bind_ctx->fd, &peek_byte, sizeof(peek_byte), MSG_PEEK,
                          (struct sockaddr *) &remote_addr,
                          &remote_addr_len) < 0) {
            return net_would_block(errno) || errno == EINTR ?
                   MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }

        if (lwip_getsockname(bind_ctx->fd, (struct sockaddr *) &local_addr,
                             &local_addr_len) != 0) {
            return MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }

        replacement_fd = lwip_socket(local_addr.ss_family, SOCK_DGRAM,
                                     IPPROTO_UDP);
        if (replacement_fd < 0) {
            return MBEDTLS_ERR_NET_SOCKET_FAILED;
        }
        if (lwip_setsockopt(replacement_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                            (socklen_t) sizeof(reuse)) != 0 ||
            lwip_bind(replacement_fd, (struct sockaddr *) &local_addr,
                      local_addr_len) != 0) {
            (void) lwip_close(replacement_fd);
            return MBEDTLS_ERR_NET_BIND_FAILED;
        }

        if (lwip_connect(bind_ctx->fd, (struct sockaddr *) &remote_addr,
                         remote_addr_len) != 0) {
            (void) lwip_close(replacement_fd);
            return MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }

        fd = bind_ctx->fd;
        bind_ctx->fd = replacement_fd;
    }

    client_ctx->fd = fd;
    if (client_ip != NULL) {
        const void *address;
        size_t address_len;

        if (remote_addr.ss_family == AF_INET) {
            const struct sockaddr_in *addr4 =
                (const struct sockaddr_in *) &remote_addr;
            address = &addr4->sin_addr;
            address_len = sizeof(addr4->sin_addr);
#if LWIP_IPV6
        } else if (remote_addr.ss_family == AF_INET6) {
            const struct sockaddr_in6 *addr6 =
                (const struct sockaddr_in6 *) &remote_addr;
            address = &addr6->sin6_addr;
            address_len = sizeof(addr6->sin6_addr);
#endif
        } else {
            mbedtls_net_close(client_ctx);
            return MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }

        *cip_len = address_len;
        if (buf_size < address_len) {
            mbedtls_net_close(client_ctx);
            return MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
        }
        memcpy(client_ip, address, address_len);
    }

    return 0;
}

int mbedtls_net_poll(mbedtls_net_context *ctx, uint32_t rw, uint32_t timeout)
{
    fd_set read_fds;
    fd_set write_fds;
    fd_set *read_set = NULL;
    fd_set *write_set = NULL;
    struct timeval tv;
    struct timeval *tv_ptr = NULL;
    int ret;

    if (!net_valid_context(ctx)) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }
    if ((rw & ~(MBEDTLS_NET_POLL_READ | MBEDTLS_NET_POLL_WRITE)) != 0 ||
        rw == 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }
    if (ctx->fd >= FD_SETSIZE) {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    if ((rw & MBEDTLS_NET_POLL_READ) != 0) {
        FD_ZERO(&read_fds);
        FD_SET(ctx->fd, &read_fds);
        read_set = &read_fds;
    }
    if ((rw & MBEDTLS_NET_POLL_WRITE) != 0) {
        FD_ZERO(&write_fds);
        FD_SET(ctx->fd, &write_fds);
        write_set = &write_fds;
    }
    if (timeout != UINT32_MAX) {
        tv.tv_sec = (long) (timeout / 1000U);
        tv.tv_usec = (long) ((timeout % 1000U) * 1000U);
        tv_ptr = &tv;
    }

    ret = lwip_select(ctx->fd + 1, read_set, write_set, NULL, tv_ptr);
    if (ret < 0) {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    ret = 0;
    if (read_set != NULL && FD_ISSET(ctx->fd, read_set)) {
        ret |= MBEDTLS_NET_POLL_READ;
    }
    if (write_set != NULL && FD_ISSET(ctx->fd, write_set)) {
        ret |= MBEDTLS_NET_POLL_WRITE;
    }
    return ret;
}

int mbedtls_net_set_block(mbedtls_net_context *ctx)
{
    int nonblocking = 0;

    if (!net_valid_context(ctx)) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }
    return lwip_ioctl(ctx->fd, FIONBIO, &nonblocking);
}

int mbedtls_net_set_nonblock(mbedtls_net_context *ctx)
{
    int nonblocking = 1;

    if (!net_valid_context(ctx)) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }
    return lwip_ioctl(ctx->fd, FIONBIO, &nonblocking);
}

void mbedtls_net_usleep(unsigned long usec)
{
    unsigned long delay_ms = usec / 1000UL;

    if (usec % 1000UL != 0) {
        delay_ms++;
    }

    while (delay_ms > 0) {
        uint32_t current_delay = delay_ms >= (unsigned long) osWaitForever ?
                                 (uint32_t) osWaitForever - 1U :
                                 (uint32_t) delay_ms;
        (void) osDelay(current_delay);
        delay_ms -= current_delay;
    }
}

int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    mbedtls_net_context *net_ctx = (mbedtls_net_context *) ctx;
    int ret;

    if (!net_valid_context(net_ctx)) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }
    if (buf == NULL && len != 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }
    if (len == 0) {
        return 0;
    }

    if (len > (size_t) INT_MAX) {
        len = (size_t) INT_MAX;
    }
    ret = (int) lwip_recv(net_ctx->fd, buf, len, 0);
    if (ret >= 0) {
        return ret;
    }
    if (net_would_block(errno) || errno == EINTR) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    if (errno == EPIPE || errno == ECONNRESET) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
    return MBEDTLS_ERR_NET_RECV_FAILED;
}

int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    mbedtls_net_context *net_ctx = (mbedtls_net_context *) ctx;
    int ret;

    if (!net_valid_context(net_ctx)) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }
    if (buf == NULL && len != 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }
    if (len == 0) {
        return 0;
    }

    if (len > (size_t) INT_MAX) {
        len = (size_t) INT_MAX;
    }
    ret = (int) lwip_send(net_ctx->fd, buf, len, 0);
    if (ret >= 0) {
        return ret;
    }
    if (net_would_block(errno) || errno == EINTR) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    if (errno == EPIPE || errno == ECONNRESET) {
        return MBEDTLS_ERR_NET_CONN_RESET;
    }
    return MBEDTLS_ERR_NET_SEND_FAILED;
}

int mbedtls_net_recv_timeout(void *ctx, unsigned char *buf, size_t len,
                             uint32_t timeout)
{
    mbedtls_net_context *net_ctx = (mbedtls_net_context *) ctx;
    int ret;

    if (!net_valid_context(net_ctx)) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    ret = mbedtls_net_poll(net_ctx, MBEDTLS_NET_POLL_READ,
                           timeout == 0 ? UINT32_MAX : timeout);
    if (ret == 0) {
        return MBEDTLS_ERR_SSL_TIMEOUT;
    }
    if (ret < 0) {
        return errno == EINTR ? MBEDTLS_ERR_SSL_WANT_READ : ret;
    }
    return mbedtls_net_recv(net_ctx, buf, len);
}

void mbedtls_net_close(mbedtls_net_context *ctx)
{
    if (ctx != NULL && ctx->fd >= 0) {
        (void) lwip_close(ctx->fd);
        ctx->fd = -1;
    }
}

void mbedtls_net_free(mbedtls_net_context *ctx)
{
    mbedtls_net_close(ctx);
}
