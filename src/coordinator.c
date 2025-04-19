#include "fractal.h"

//payload from the grafica
static payload_t payload_queue;

//many small payloads to the workers
static payload_t *payload_to_workers;
static int payload_to_workers_amount;

static response_t *response_queue;
static int response_amount;

void net_thread_receive_payload()
{
  //block waiting from the grafica  
  //lock - payload_queue
  //payload producer: overwrite the payload in the payload_queue
  //signal the compute-create-blocks
  //unlock
}

void compute_create_blocks()
{
  //wait
  //lock - payload_queue;
  //payload consumer: get the payload, discretize in blocks
  //Call Ana Laura function
  //lock - payload_to_workers
  //if there is something in payload_to_workers, remove them all
  //payload_to_workers producer: fill the payload_to_workers with
  //   the response from ana Laura function
  //unlock - payload_to_workers
  //unlock - payload_queue
}

void main_thread()
{
  //keep looking to the payload_to_workers queue
  //lock payload_to_workers
  //consume payload_to_workers and distribute each payload
  //to the queue of workers
  //unlock

  //wait for any worker to answer (mpi-test and if true, mpi-wait-any)
  //lock response_queue  
  //while there is a response from a worker
  //  - receive from the worker
  //  - producer: put in that queue
  //signal response-queue (net-thread-send-response)
  //unlock
}

void net_thread_send_response()
{
  //wait
  //lock response_queue
  //send all through the socket to the client the response
  //unlock
}

int main()
{
  //Launch the paralellism mechanism!
  
  //create and open the socket to wait for a client
  
  //launch all threads

  //join
}
