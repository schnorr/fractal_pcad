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
#ifndef __MPI_COMM_H
#define __MPI_COMM_H
#include <mpi.h>
#include "fractal.h"

#define FRACTAL_MPI_PAYLOAD_REQUEST 0
#define FRACTAL_MPI_PAYLOAD_DATA 1

#define FRACTAL_MPI_RESPONSE_REQUEST 2
#define FRACTAL_MPI_RESPONSE_DATA 3

payload_t *mpi_payload_receive (int target);
void mpi_payload_send (payload_t *payload, int worker);
response_t *mpi_response_receive (int worker);
void mpi_response_send (response_t *response);
#endif
