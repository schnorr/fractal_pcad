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
#ifndef __CONNECTION_H_
#define __CONNECTION_H_

#include <stdint.h>
#include <sys/types.h>

int open_connection(char host[], uint16_t port);
int open_server_socket(uint16_t port);

/* recv that blocks until getting a certain amount of data */
ssize_t recv_all(int socket, void *buffer, size_t length, int flags);

#endif