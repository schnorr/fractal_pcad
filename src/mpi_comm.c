#include "mpi_comm.h"

payload_t *mpi_payload_receive (void)
{
  // use FRACTAL_MPI_PAYLOAD_DATA tag
  payload_t *ret = NULL;
  // TODO
  return ret;
}

void mpi_payload_send (payload_t *payload, int worker)
{
  // use FRACTAL_MPI_PAYLOAD_DATA tag 
  // TODO
  free(payload);
  payload = NULL;
}

response_t *mpi_response_receive (int worker)
{
  // use FRACTAL_MPI_RESPONSE_DATA tag
  response_t *ret = NULL;
  // TODO
  return ret;

}

void mpi_response_send (response_t *response)
{
  int target = 0; // rank 0 is always our target here
  // use FRACTAL_MPI_RESPONSE_DATA tag  
  if (!response) return;
  // TODO
  // free the response as it has already been sent
  free(response->values);
  free(response);
  response = NULL;
}

