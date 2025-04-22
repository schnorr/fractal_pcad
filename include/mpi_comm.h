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
