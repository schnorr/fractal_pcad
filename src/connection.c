/*
This file is part of "Fractal @ PCAD".

"Fractal @ PCAD" is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

"Fractal @ PCAD" is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with "Fractal @ PCAD". If not, see
<https://www.gnu.org/licenses/>.
*/
#include "connection.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <arpa/inet.h>     
#include <sys/socket.h>   
#include <netinet/in.h> 
#include <netdb.h> 

int open_connection(char host[], uint16_t port) {
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_STREAM;

  int status = getaddrinfo(host, NULL, &hints, &res);
  if (status != 0) {
    perror("Error getting address info");
    exit(1);
  }
  
  int client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (client_socket < 0) {
    perror("Error creating socket.");
    exit(1);
  }

  struct sockaddr_in *server_address = (struct sockaddr_in *) res->ai_addr;
  server_address->sin_port = htons(port);

  if (connect(client_socket, (struct sockaddr *) server_address, sizeof(*server_address)) < 0) {
    perror("Error connecting to server.");
    freeaddrinfo(res);
    exit(1);
  }
  freeaddrinfo(res);

  printf("Connected to server %s:%d\n", host, port);
  
  return client_socket;
}

int open_server_socket(uint16_t port) {
  int server_fd;
  struct sockaddr_in server_addr;
  int opt = 1;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Error creating socket.\n");
    exit(1);
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("Error on setsockopt.\n");
    exit(1);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Error binding socket.\n");
    exit(1);
  }

  if (listen(server_fd, 1) < 0) { // connection buffer size 1, should be fine
    perror("Error listening.\n");
    exit(1);
  }

  printf("Server listening on port %d, waiting for connection...\n", port);

  return server_fd;
}

ssize_t recv_all(int socket, void *buffer, size_t length, int flags) {
  size_t total_read = 0;
  while (total_read < length) {
    ssize_t r = recv(socket, buffer + total_read, length - total_read, flags);
    if (r <= 0) { // if r <= 0, error
        return r;
    }
    total_read += r;
  }
  return (ssize_t)total_read;
}