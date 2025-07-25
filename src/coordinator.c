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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <arpa/inet.h> 
#include <sys/socket.h>   
#include <netinet/in.h>
#include <pthread.h>
#include <mpi.h>
#include <sys/time.h>
#include <signal.h>
#include <stdatomic.h>
#include "fractal.h"
#include "connection.h"
#include "queue.h"
#include "mpi_comm.h"
#include "timing.h"
#include "logging.h"

// Raw payload from the client
static payload_t *newest_payload = NULL;
static atomic_int latest_generation = ATOMIC_VAR_INIT(-1);
static pthread_mutex_t newest_payload_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t new_payload = PTHREAD_COND_INITIALIZER;

// Discretized payloads to the workers
static queue_t payload_to_workers_queue;
// Computed responses to be sent to the client
static queue_t response_queue;

#if LOG_LEVEL >= LOG_BASIC
// These variables measure time spent on each payload.
// It is assumed that the user will not interrupt a payload 
// with another during processing when logging times
static struct timespec payload_received_time;
static struct timespec first_response_sent_time;
static struct timespec last_response_sent_time;
static struct timespec payload_discretized_time;
static struct timespec first_response_received_time;
static struct timespec last_response_received_time;
int expected_payloads;
int responses_received_from_workers = 0;
int payloads_sent_to_workers = 0;
int responses_sent_to_client = 0;
#endif

/*
  net_thread_receive_payload: the sole goal is to receive one payload
  from the grafica client. Then, it updates the "newest_payload"
  global variable to contain the latest work that should be
  distributed to workers (once discretized). This action is carried
  out by another thread, compute_create_blocks.
*/
void *net_thread_receive_payload(void *arg)
{
  int connection = *(int *)arg;

  while(1) {
    // Allocate new payload
    payload_t *payload = malloc(sizeof(payload_t)); 
    if (payload == NULL) {
      fprintf(stderr, "malloc failed.\n");
      pthread_exit(NULL);
    }

    // Get payload data from connection
    if (recv_all(connection, payload, sizeof(*payload), 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      free(payload);
      pthread_exit(NULL);
    }

#ifdef PAYLOAD_DEBUG
    payload_print(__func__, "received payload", payload);
#endif

#if LOG_LEVEL >= LOG_BASIC
    clock_gettime(CLOCK_MONOTONIC, &payload_received_time);
#endif

    if (payload->generation == PAYLOAD_GENERATION_SHUTDOWN) {
      free(payload);
      exit(0); // Not cleaning up for now
    }

    pthread_mutex_lock(&newest_payload_mutex);
    // Checking if there's a previous payload that hasn't been used in compute_create_blocks yet
    if (newest_payload != NULL) {
      free(newest_payload);
    }
    newest_payload = payload; // Update newest payload, signal compute_create_blocks
    atomic_store(&latest_generation, payload->generation);
    payload = NULL;
    pthread_cond_signal(&new_payload);
    pthread_mutex_unlock(&newest_payload_mutex);
  }
  pthread_exit(NULL);
}

/*
  compute_create_blocks: after being signaled, this thread discretizes
  the newest payload so we have numerous blocks to compute. The
  "sub-blocks" will be queued in the =payload_to_workers_queue= so our
  mpi thread can distribute them accordingly to the workers.
*/
void *compute_create_blocks()
{
  while(1) {
    pthread_mutex_lock(&newest_payload_mutex);
    while (newest_payload == NULL) { // Wait for new payload to arrive
      pthread_cond_wait(&new_payload, &newest_payload_mutex);
    }

#ifdef PAYLOAD_DEBUG
    payload_print(__func__, "received newest_payload", newest_payload);
#endif

    // new payload, so clear obsolete payloads to workers
    queue_clear(&payload_to_workers_queue);

    //payload consumer: get the payload, discretize in blocks
    //Call Ana Laura function
    int length = 0, i;
    payload_t **payload_vector = discretize_payload(newest_payload, &length);

#if LOG_LEVEL >= LOG_BASIC
    clock_gettime(CLOCK_MONOTONIC, &payload_discretized_time);
    fprintf(stdout, "[DISCRETIZED]: %.9f\n", 
            timespec_to_double(timespec_diff(payload_received_time, payload_discretized_time)));
    expected_payloads = length;
    responses_received_from_workers = 0;
    responses_sent_to_client = 0;
    payloads_sent_to_workers = 0;
#endif

    for (i = 0; i < length; i++){
#ifdef PAYLOAD_DEBUG
      printf("(%d) %s: Enqueueing discretized payload %d\n", payload_vector[i]->generation, __func__, i);
#endif
      queue_enqueue(&payload_to_workers_queue, payload_vector[i]);
      payload_vector[i] = NULL; //transfer ownership to the queue
    }
    free(payload_vector); //no longer necessary as all members have been queued
    free(newest_payload);
    newest_payload = NULL; // Ownership transferred to queue
    pthread_mutex_unlock(&newest_payload_mutex);
  }
  pthread_exit(NULL);
}

void *main_thread_mpi_recv_responses ()
{
  while(1) {
    int worker;
    
    // verify who wants to send us a response
    MPI_Recv(&worker, 1, MPI_INT,
	    MPI_ANY_SOURCE, // receive request from any worker
	    FRACTAL_MPI_RESPONSE_REQUEST,
	    MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // receive response from this worker
    response_t *response = mpi_response_receive (worker);

#if LOG_LEVEL >= LOG_BASIC
    responses_received_from_workers++;
    if (responses_received_from_workers == 1) {
      clock_gettime(CLOCK_MONOTONIC, &first_response_received_time);
      fprintf(stdout, "[MPI_RECV_FIRST]: %.9f\n", 
        timespec_to_double(timespec_diff(payload_received_time, first_response_received_time)));
    } else if (responses_received_from_workers == expected_payloads) {
      clock_gettime(CLOCK_MONOTONIC, &last_response_received_time);
      fprintf(stdout, "[MPI_RECV_ALL]: %.9f\n", 
        timespec_to_double(timespec_diff(payload_received_time, last_response_received_time)));
    }
#endif

#ifdef RESPONSE_DEBUG
      response_print(__func__, "Enqueueing response", response);
#endif

    // only queue responses that we are waiting for
    if (response->payload.generation == atomic_load(&latest_generation)) {
	    queue_enqueue(&response_queue, response);
    }else{
	    //response_print(__func__, "Discard response", response);
      free(response->values);
	    free(response);
    }
    response = NULL; // Transferred ownership to queue
  }
  pthread_exit(NULL);
}

/*
  main_thread_function: distribute discretized payloads to our workers.
*/
void *main_thread_mpi_send_payloads ()
{
#if LOG_LEVEL >= LOG_BASIC
  payload_t done_flag = {0};
  done_flag.generation = PAYLOAD_GENERATION_DONE;
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
#endif

  while(1) {
    int worker;

    // Get the payload (the queue-dequeue blocks this thread)
    payload_t *payload = (payload_t *)queue_dequeue(&payload_to_workers_queue);

    // after getting a payload to do, check which worker is available
    MPI_Recv(&worker, 1, MPI_INT,
	     MPI_ANY_SOURCE, // receive request from any worker
	     FRACTAL_MPI_PAYLOAD_REQUEST,
	     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // send the work to this worker
    mpi_payload_send (payload, worker);

    // free the payload
    free(payload);
    payload = NULL;    

#if LOG_LEVEL >= LOG_BASIC
    payloads_sent_to_workers++;
    if (payloads_sent_to_workers == expected_payloads) {
      for (int i = 1; i < world_size; i++) {
        MPI_Recv(&worker, 1, MPI_INT,
	        i, // receive request from worker i
	        FRACTAL_MPI_PAYLOAD_REQUEST,
	        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        mpi_payload_send(&done_flag, i); // Signal it to print times
      }
    }
#endif

  }
  pthread_exit(NULL);
}


void *net_thread_send_response(void *arg)
{
  int connection = *(int *)arg;
  while(1) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);
    if (response == NULL) break; // poison pill

#ifdef RESPONSE_DEBUG
    response_print(__func__, "preparating for sending the response", response);
#endif

    size_t buffer_size = response->payload.granularity * response->payload.granularity;
    buffer_size *= sizeof(int);

    // First, send response
    if (send(connection, response, sizeof(response_t), 0) <= 0) {
      fprintf(stderr, "Send failed. Killing thread...\n");
      free(response->values);
      free(response);
      pthread_exit(NULL);
    }

    // Then, send response values
    if (send(connection, response->values, buffer_size, 0) <= 0) {
      fprintf(stderr, "Send failed. Killing thread...\n");
      free(response->values);
      free(response);
      pthread_exit(NULL);
    }
  
#if LOG_LEVEL >= LOG_BASIC
    responses_sent_to_client++;
    if (responses_sent_to_client == 1) {
      clock_gettime(CLOCK_MONOTONIC, &first_response_sent_time);
      fprintf(stdout, "[NET_SEND_FIRST]: %.9f\n", 
        timespec_to_double(timespec_diff(payload_received_time, first_response_sent_time)));
    } else if (responses_sent_to_client == expected_payloads) {
      clock_gettime(CLOCK_MONOTONIC, &last_response_sent_time);
      fprintf(stdout, "[NET_SEND_ALL]: %.9f\n", 
        timespec_to_double(timespec_diff(payload_received_time, last_response_sent_time)));
    }
#endif 

#ifdef RESPONSE_DEBUG
    response_print(__func__, "response sent", response);
#endif

    free(response->values);
    free(response);
  }

  pthread_exit(NULL);
}

int main_coordinator(int argc, char* argv[])
{
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (argc == 1) {
    printf("Missing arguments. Format: %s <port>\n", argv[0]);
    return 1;
  }
  printf("%s: Coordinator (rank %d) with %d arguments\n", argv[0], rank, argc);
  printf("%s: \t There are %d workers\n", argv[0], size-1);

  signal(SIGPIPE, SIG_IGN); // ignoring SIGPIPE (failed send)

  MPI_Barrier(MPI_COMM_WORLD); // Wait for all mpi workers to be ready before accepting connections
  
  struct sockaddr_in client_addr; // client ip address after connect
  socklen_t client_len = sizeof(client_addr);
  int socket = open_server_socket(atoi(argv[1]));

  queue_init(&response_queue, 65536, free_response);
  queue_init(&payload_to_workers_queue, 65536, free);
  
  pthread_t compute_thread = 0;
  pthread_t mpi_send = 0;
  pthread_t mpi_recv = 0;
  pthread_t payload_receive_thread = 0;
  pthread_t response_send_thread = 0;

  pthread_create(&compute_thread, NULL, compute_create_blocks, NULL);
  pthread_create(&mpi_send, NULL, main_thread_mpi_send_payloads, NULL);
  pthread_create(&mpi_recv, NULL, main_thread_mpi_recv_responses, NULL);

  while(true) { // No shutdown logic yet, loop infinitely
    int connection = accept(socket, (struct sockaddr *)&client_addr, &client_len);
    printf("Accepted client connection.\n");
    
    queue_clear(&response_queue);
    queue_clear(&payload_to_workers_queue);
    atomic_store(&latest_generation, -1);

    pthread_create(&payload_receive_thread, NULL, net_thread_receive_payload, &connection);
    pthread_create(&response_send_thread, NULL, net_thread_send_response, &connection);

    pthread_join(payload_receive_thread, NULL);
    // Receiving thread died, meaning we can shut down the sending thread. 
    // Enqueueing "poison pill" NULL response to kill it
    queue_enqueue(&response_queue, NULL);
    pthread_join(response_send_thread, NULL);
    printf("TCP connection lost. Awaiting new connection...\n");

    close(connection);
  }

  pthread_join(compute_thread, NULL);
  pthread_join(mpi_recv, NULL);
  pthread_join(mpi_send, NULL);

  shutdown(socket, SHUT_RDWR);
  close(socket);
  
  queue_destroy(&response_queue);
  queue_destroy(&payload_to_workers_queue);

  return 0;
}

int main_worker(int argc, char* argv[])
{
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  printf("%s: Worker (rank %d) with %d arguments\n", argv[0], rank, argc);

  MPI_Barrier(MPI_COMM_WORLD); // Sync with coordinator before starting

#if LOG_LEVEL >= LOG_BASIC
  long long total_iterations = 0;
  long long total_pixels = 0;
  struct timespec total_compute_time = {0};
  struct timespec compute_start_time, compute_end_time;
#endif

  while (1) {    
    MPI_Ssend(&rank, 1, MPI_INT, 0, FRACTAL_MPI_PAYLOAD_REQUEST, MPI_COMM_WORLD);
    payload_t *payload = mpi_payload_receive(0);

#if LOG_LEVEL >= LOG_BASIC
    if (payload->generation == PAYLOAD_GENERATION_DONE) {
      fprintf(stdout, "[WORKER_%d_TOTAL]: %.9f, %lld, %lld\n", 
              rank, 
              timespec_to_double(total_compute_time),
              total_pixels,
              total_iterations);
      fflush(stdout);
      free(payload);
      total_iterations = 0;
      total_pixels = 0;
      total_compute_time = (struct timespec) {0};
      continue;
    }
    clock_gettime(CLOCK_MONOTONIC, &compute_start_time);
#endif
    create_response_return_t response_result = create_response_for_payload(payload);

#if LOG_LEVEL >= LOG_BASIC
    clock_gettime(CLOCK_MONOTONIC, &compute_end_time);
#endif

    response_t *response = response_result.response;

#if LOG_LEVEL >= LOG_BASIC
#if LOG_LEVEL >= LOG_FULL
      printf("[WORKER_%d_PAYLOAD]: %.9f, %d, %lld\n", 
             rank,
             timespec_to_double(timespec_diff(compute_start_time, compute_end_time)),
             response->payload.granularity * response->payload.granularity,
             response_result.total_iterations);
#endif // LOG_FULL

    total_compute_time = timespec_add(total_compute_time, 
                                      timespec_diff(compute_start_time, compute_end_time));
    total_iterations += response_result.total_iterations;
    total_pixels += response->payload.granularity * response->payload.granularity;

#endif // LOG_BASIC

    response->max_worker_id = size;
    response->worker_id = rank;

    MPI_Ssend(&rank, 1, MPI_INT, 0, FRACTAL_MPI_RESPONSE_REQUEST, MPI_COMM_WORLD);
    mpi_response_send(response);

    free(response->values);
    free(response);
    free(payload);
  }
  return 0;
}

int main(int argc, char* argv[])
{
  int provided;
  putenv("OMPI_MCA_pml=ob1");
  putenv("OMPI_MCA_mtl=^ofi");
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  if (provided < MPI_THREAD_MULTIPLE) {
    // Tratamento de erro: seu código pode não funcionar corretamente
    printf("MPI implementation does not support MPI_THREAD_MULTIPLE\n");
    MPI_Abort(MPI_COMM_WORLD, 1);
  }
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == 0){
    main_coordinator(argc, argv);
  }else{
    main_worker(argc, argv);
  }
  MPI_Finalize();
  return 0;
}
