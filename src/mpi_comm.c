#include <stdio.h>
#include "mpi_comm.h"

static payload_t *_mpi_payload_receive (int source, int tag)
{
  payload_t *payload = calloc(1, sizeof(payload_t));
  MPI_Recv(&payload->generation, 1, MPI_INT,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  MPI_Recv(&payload->granularity, 1, MPI_INT,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  MPI_Recv(&payload->fractal_depth, 1, MPI_INT,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  //coord lower-left
  MPI_Recv(&payload->ll, 2, MPI_DOUBLE,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  //coord upper-right
  MPI_Recv(&payload->ur, 2, MPI_DOUBLE,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  //screen coord lower-left
  MPI_Recv(&payload->s_ll, 2, MPI_INT,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  //screeen coord upper-right
  MPI_Recv(&payload->s_ur, 2, MPI_INT,
	   source,
	   tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  return payload;
}

static void _mpi_payload_send (payload_t *payload, int target, int tag)
{
  MPI_Send(&payload->generation, 1, MPI_INT,
	   target,
	   tag, MPI_COMM_WORLD);
  MPI_Send(&payload->granularity, 1, MPI_INT,
	   target,
	   tag, MPI_COMM_WORLD);
  MPI_Send(&payload->fractal_depth, 1, MPI_INT,
	   target,
	   tag, MPI_COMM_WORLD);
  //coord lower-left
  MPI_Send(&payload->ll, 2, MPI_DOUBLE,
	   target,
	   tag, MPI_COMM_WORLD);
  //coord upper-right
  MPI_Send(&payload->ur, 2, MPI_DOUBLE,
	   target,
	   tag, MPI_COMM_WORLD);
  //screen coord lower-left
  MPI_Send(&payload->s_ll, 2, MPI_INT,
	   target,
	   tag, MPI_COMM_WORLD);
  //screeen coord upper-right
  MPI_Send(&payload->s_ur, 2, MPI_INT,
	   target,
	   tag, MPI_COMM_WORLD);
}

payload_t *mpi_payload_receive (int source)
{
  return _mpi_payload_receive (source, FRACTAL_MPI_PAYLOAD_DATA);
}

void mpi_payload_send (payload_t *payload, int target)
{
  _mpi_payload_send (payload, target, FRACTAL_MPI_PAYLOAD_DATA);
}

response_t *mpi_response_receive (int worker_source)
{
  //receive the payload from that worker
  //(the payload that corresponds to the response)
  payload_t *payload = _mpi_payload_receive (worker_source,
					     FRACTAL_MPI_RESPONSE_DATA);
  response_t *response = calloc(1, sizeof(response_t));
  response->payload = *payload;
  int n_values;
  n_values = response->payload.granularity * response->payload.granularity;
  free(payload);
  payload = NULL;
  MPI_Recv(&response->worker_id, 1, MPI_INT,
	   worker_source,
	   FRACTAL_MPI_RESPONSE_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  MPI_Recv(&response->max_worker_id, 1, MPI_INT,
	   worker_source,
	   FRACTAL_MPI_RESPONSE_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  response->values = (int*)calloc(n_values, sizeof(int));
  MPI_Recv(response->values, n_values, MPI_INT,
	   worker_source,
	   FRACTAL_MPI_RESPONSE_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  return response;
}

void mpi_response_send (response_t *response)
{
  if (!response) return;
  int target = 0; // rank 0 is always our target here
  _mpi_payload_send (&response->payload, target, FRACTAL_MPI_RESPONSE_DATA);
  MPI_Send(&response->worker_id, 1, MPI_INT,
	   target,
	   FRACTAL_MPI_RESPONSE_DATA, MPI_COMM_WORLD);
  MPI_Send(&response->max_worker_id, 1, MPI_INT,
	   target,
	   FRACTAL_MPI_RESPONSE_DATA, MPI_COMM_WORLD);
  int n_values;
  n_values = response->payload.granularity * response->payload.granularity;
  MPI_Send(response->values, n_values, MPI_INT,
	   target,
	   FRACTAL_MPI_RESPONSE_DATA, MPI_COMM_WORLD);
}

