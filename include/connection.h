#ifndef __CONNECTION_H_
#define __CONNECTION_H_

#include <stdint.h>
#include <sys/types.h>

int open_connection(char host[], uint16_t port);
int open_server_socket(uint16_t port);

/* recv that blocks until getting a certain amount of data */
ssize_t recv_all(int socket, void *buffer, size_t length, int flags);

#endif