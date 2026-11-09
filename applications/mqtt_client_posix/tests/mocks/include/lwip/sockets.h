#ifndef MQTT_TEST_LWIP_SOCKETS_H
#define MQTT_TEST_LWIP_SOCKETS_H

#include <sys/socket.h>

int mqtt_test_setsockopt(int socket_fd,
                         int level,
                         int option,
                         const void *value,
                         socklen_t value_length);
int mqtt_test_shutdown(int socket_fd, int how);

#define setsockopt mqtt_test_setsockopt
#define shutdown mqtt_test_shutdown

#endif
