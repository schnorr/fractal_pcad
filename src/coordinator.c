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
#include "fractal.h"
#include "connection.h"
#include "queue.h"
#include "mpi_comm.h"
#include "timing.h"

static queue_t incoming_payload_queue;  // Raw payloads from network
static queue_t payload_to_workers_queue; // Discretized payloads for workers
static queue_t response_queue;

pthread_mutex_t latest_generation_mutex = PTHREAD_MUTEX_INITIALIZER;
int latest_generation = PAYLOAD_GENERATION_DONE;


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

    bool coordinator_shutdown = (payload->generation == PAYLOAD_GENERATION_SHUTDOWN);
    if (coordinator_shutdown) continue; // TODO: shutdown sequence

    queue_clear(&payload_to_workers_queue);
    queue_clear(&incoming_payload_queue);
    queue_enqueue(&incoming_payload_queue, payload);
  }
  pthread_exit(NULL);
}

/*
  compute_create_blocks: this function discretizes
  the newest payload so we have numerous blocks to compute. The
  "sub-blocks" will be queued in the =payload_to_workers_queue= so our
  mpi thread can distribute them accordingly to the workers.
*/
void compute_create_blocks(payload_t *payload, 
                           queue_t *payload_to_workers_queue)
{
  int length = 0;
  payload_t **payload_vector = discretize_payload(payload, &length);
  
  for (int i = 0; i < length; i++) {
    queue_enqueue(payload_to_workers_queue, payload_vector[i]);
  }
  free(payload_vector);
}

void *main_thread_mpi_recv_responses ()
{
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  int num_workers = world_size - 1;

  while (1) { // Loop through rounds of work
    int workers_done = 0;
    while (workers_done < num_workers) { // Loop until all workers have received their payloads
      int worker;
      MPI_Recv(&worker, 1, MPI_INT, MPI_ANY_SOURCE, FRACTAL_MPI_RESPONSE_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      response_t *response = mpi_response_receive(worker);
      if (response->payload.generation == latest_generation) {
        queue_enqueue(&response_queue, response);
      } else {
        if (response->payload.generation == PAYLOAD_GENERATION_DONE) {
          workers_done++;
        }
        free(response->values);
        free(response);
      }
    }
  }

  pthread_exit(NULL);
}

void *main_thread_mpi_send_payloads ()
{
  payload_t done_flag = {0}; // This payload will be sent to workers to signal end of current round
  done_flag.generation = PAYLOAD_GENERATION_DONE;

  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  int num_workers = world_size - 1;

  while (1) { // Loop through rounds of work
    payload_t *new_payload = (payload_t *)queue_dequeue(&incoming_payload_queue);
    if (new_payload == NULL) break; // shutdown

    compute_create_blocks(new_payload, &payload_to_workers_queue);

    MPI_Barrier(MPI_COMM_WORLD);

    int workers_done = 0;
    while (workers_done < num_workers) { // Loop until all workers have received their payloads
      int worker;
      MPI_Recv(&worker, 1, MPI_INT, MPI_ANY_SOURCE, FRACTAL_MPI_PAYLOAD_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      payload_t *payload = queue_try_dequeue(&payload_to_workers_queue); // Non blocking dequeue
      if (payload){
        latest_generation = payload->generation;
        mpi_payload_send(payload, worker);
        free(payload);
      } else {
        workers_done++;
        mpi_payload_send(&done_flag, worker);
      }
    }
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
  
  struct sockaddr_in client_addr; // client ip address after connect
  socklen_t client_len = sizeof(client_addr);
  int socket = open_server_socket(atoi(argv[1]));

  queue_init(&incoming_payload_queue, 1, free);
  queue_init(&payload_to_workers_queue, 65536, free);
  queue_init(&response_queue, 65536, free_response);
  
  pthread_t mpi_send = 0;
  pthread_t mpi_recv = 0;
  pthread_t payload_receive_thread = 0;
  pthread_t response_send_thread = 0;

  pthread_create(&mpi_send, NULL, main_thread_mpi_send_payloads, NULL);
  pthread_create(&mpi_recv, NULL, main_thread_mpi_recv_responses, NULL);

  while(true) { // No shutdown logic yet, loop infinitely
    int connection = accept(socket, (struct sockaddr *)&client_addr, &client_len);
    printf("Accepted client connection.\n");

    pthread_create(&payload_receive_thread, NULL, net_thread_receive_payload, &connection);
    pthread_create(&response_send_thread, NULL, net_thread_send_response, &connection);

    pthread_join(payload_receive_thread, NULL);
    // Receiving thread died, meaning we can shut down the sending thread. 
    // Enqueueing "poison pill" NULL response to kill it
    queue_enqueue(&response_queue, NULL);
    pthread_join(response_send_thread, NULL);
    printf("TCP connection lost. Awaiting new connection...\n");

    queue_clear(&incoming_payload_queue);
    queue_clear(&payload_to_workers_queue);
    queue_clear(&response_queue);

    close(connection);
  }

  pthread_join(mpi_recv, NULL);
  pthread_join(mpi_send, NULL);

  shutdown(socket, SHUT_RDWR);
  close(socket);
  
  queue_destroy(&incoming_payload_queue);
  queue_destroy(&payload_to_workers_queue);
  queue_destroy(&response_queue);
  return 0;
}

int main_worker(int argc, char* argv[])
{
  response_t done_flag = {0};
  done_flag.payload.generation = PAYLOAD_GENERATION_DONE;

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  printf("%s: Worker (rank %d) with %d arguments\n", argv[0], rank, argc);

  while (1) { // Loop through rounds of work
    MPI_Barrier(MPI_COMM_WORLD); // Synchronize all workers and coordinator
    struct timespec total_time = {0}, total_compute_time = {0};
    struct timespec start_time, end_time, compute_start_time, compute_end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    while (1) { // Loop through individual payloads
      // Send rank and receive payload
      MPI_Ssend(&rank, 1, MPI_INT, 0, FRACTAL_MPI_PAYLOAD_REQUEST, MPI_COMM_WORLD);
      payload_t *payload = mpi_payload_receive(0);

      if (payload->generation == -1) { // Poison pill, signal worker done
        free(payload);
        MPI_Ssend(&rank, 1, MPI_INT, 0, FRACTAL_MPI_RESPONSE_REQUEST, MPI_COMM_WORLD);
        mpi_response_send(&done_flag);
        break;
      }

      clock_gettime(CLOCK_MONOTONIC, &compute_start_time);
      create_response_return_t response_result = create_response_for_payload(payload);
      clock_gettime(CLOCK_MONOTONIC, &compute_end_time);

      response_t *response = response_result.response;
      printf("[WORKER %d PAYLOAD_LOG]: Computed %d depth values in %lld iterations in %0.9fs]\n", 
             rank,
             response->payload.granularity * response->payload.granularity,
             response_result.total_iterations, 
             timespec_to_double(timespec_diff(compute_start_time, compute_end_time)));

      total_compute_time = timespec_add(total_compute_time, 
                                        timespec_diff(compute_start_time, compute_end_time));

      response->max_worker_id = size;
      response->worker_id = rank;

      MPI_Ssend(&rank, 1, MPI_INT, 0, FRACTAL_MPI_RESPONSE_REQUEST, MPI_COMM_WORLD);
      mpi_response_send(response);

      free(response->values);
      free(response);
      free(payload);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    total_time = timespec_diff(start_time, end_time);
    
    printf("[WORKER %d FINAL_LOG]: Total elapsed time for payload = %.9fs [Total compute time: %.9fs]\n", 
           rank, 
           timespec_to_double(total_time), 
           timespec_to_double(total_compute_time));
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
