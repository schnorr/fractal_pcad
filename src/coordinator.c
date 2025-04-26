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
#include "fractal.h"
#include "connection.h"
#include "queue.h"
#include "mpi_comm.h"

//payload from the grafica
static payload_t *newest_payload = NULL;
static int latest_generation = 0;
static pthread_mutex_t newest_payload_mutex;
static pthread_cond_t new_payload;

//many small payloads to the workers
static queue_t payload_to_workers_queue;

static queue_t response_queue;

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
      perror("malloc failed.\n");
      pthread_exit(NULL);
    }

    // Get payload data from connection
    if (recv_all(connection, payload, sizeof(*payload), 0) <= 0) {
      perror("Receive failed. Killing thread...\n");
      free(payload);
      pthread_exit(NULL);
    }

#ifdef PAYLOAD_DEBUG
    payload_print(__func__, "received payload", payload);
#endif

    pthread_mutex_lock(&newest_payload_mutex);
    // Checking if there's a previous payload that hasn't been used in compute_create_blocks yet
    if (newest_payload != NULL) {
      free(newest_payload);
    }
    newest_payload = payload; // Update newest payload, signal compute_create_blocks
    latest_generation = newest_payload->generation;
    payload = NULL;
    pthread_cond_signal(&new_payload);
    pthread_mutex_unlock(&newest_payload_mutex);
  }
  pthread_exit(NULL);
}

/*
  compute_create_blocks: after signaling, this function discretizes
  the newest payload so we have numerous blocks to compute. The
  "sub-blocks" will be queued in the =payload_to_workers_queue= so our
  main thread (main_thread_function) can distribute them accordingly
  to the workers.
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
    for (i = 0; i < length; i++){
#ifdef PAYLOAD_DEBUG
      printf("(%d) %s: Enqueueing discretized payload %d\n", payload_vector[i]->generation, __func__, i);
#endif
      queue_enqueue(&payload_to_workers_queue, payload_vector[i]);
      payload_vector[i] = NULL; //transfer ownership to the queue
    }
    free(payload_vector); //no longer necessary as all members have been queued
    newest_payload = NULL; // Ownership transferred to queue
    pthread_mutex_unlock(&newest_payload_mutex);
  }
  pthread_exit(NULL);
}

/*
  main_thread_mpi_recv: receive responses from our workers.
*/
void *main_thread_mpi_recv_responses ()
{
  while(1) {
    // Might have to rethink - queue-dequeue() currently locks
    // thread. If this thread needs to wait for worker answers, maybe
    // a queue_peek() that doesn't block thread would be appropriate?

    int worker;
    {
      // verify who wants to send us a response
      MPI_Recv(&worker, 1, MPI_INT,
	       MPI_ANY_SOURCE, // receive request from any worker
	       FRACTAL_MPI_RESPONSE_REQUEST,
	       MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      // receive response from this worker
      response_t *response = mpi_response_receive (worker);
#ifdef RESPONSE_DEBUG
      response_print(__func__, "Enqueueing response", response);
#endif
      // only queue responses that we are waiting for
      if (response->payload.generation == latest_generation) {
	queue_enqueue(&response_queue, response);
      }else{
	response_print(__func__, "Discard response", response);
	free(response);
      }
      response = NULL; // Transferred ownership to queue
    }
  }
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

  pthread_exit(NULL);
}

/*
  main_thread_function: distribute discretized payloads to our workers.
*/
void *main_thread_mpi_send_payloads ()
{
  while(1) {
    // Might have to rethink - queue-dequeue() currently locks
    // thread. If this thread needs to wait for worker answers, maybe
    // a queue_peek() that doesn't block thread would be appropriate?

    int worker;
    {
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
    }
  }
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

  pthread_exit(NULL);
}

void *net_thread_send_response(void *arg)
{
  int connection = *(int *)arg;
  while(1) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);
#ifdef RESPONSE_DEBUG
    response_print(__func__, "preparating for sending the response", response);
#endif

    uint8_t *buffer;
    // Serialize the response so response->values is sent
    size_t buffer_size = response_serialize(response, &buffer);

    // First, send buffer size in bytes since response size is variable
    if (send(connection, &buffer_size, sizeof(buffer_size), 0) <= 0) {
      perror("Send failed. Killing thread...\n");
      free(response->values);
      free(response);
      free(buffer);
      pthread_exit(NULL);
    }

    // Then, send actual buffer
    if (send(connection, buffer, buffer_size, 0) <= 0) {
      perror("Send failed. Killing thread...\n");
      free(response->values);
      free(response);
      free(buffer);
      pthread_exit(NULL);
    }

#ifdef RESPONSE_DEBUG
    response_print(__func__, "response sent", response);
#endif

    free(response->values);
    free(response);
    free(buffer);
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

  //Launch the paralellism mechanism!
  
  struct sockaddr_in client_addr; // client ip address after connect
  socklen_t client_len = sizeof(client_addr);
  int socket = open_server_socket(atoi(argv[1]));
  int connection = accept(socket, (struct sockaddr *)&client_addr, &client_len);
  printf("Accepted connection.\n");

  queue_init(&payload_to_workers_queue, 256, free);
  queue_init(&response_queue, 256, free_response);
  pthread_mutex_init(&newest_payload_mutex, NULL);
  pthread_cond_init(&new_payload, NULL);
  
  pthread_t main_mpi_send = 0;
  pthread_t main_mpi_recv = 0;
  pthread_t compute_thread = 0;
  pthread_t payload_receive_thread = 0;
  pthread_t response_send_thread = 0;

  pthread_create(&main_mpi_send, NULL, main_thread_mpi_send_payloads, NULL);
  pthread_create(&main_mpi_recv, NULL, main_thread_mpi_recv_responses, NULL);
  pthread_create(&compute_thread, NULL, compute_create_blocks, NULL);
  pthread_create(&payload_receive_thread, NULL, net_thread_receive_payload, &connection);
  pthread_create(&response_send_thread, NULL, net_thread_send_response, &connection);

  pthread_join(main_mpi_send, NULL);
  pthread_join(main_mpi_recv, NULL);
  pthread_join(compute_thread, NULL);
  pthread_join(payload_receive_thread, NULL);
  pthread_join(response_send_thread, NULL);

  shutdown(socket, SHUT_RDWR);
  close(socket);
  close(connection);
  
  queue_destroy(&payload_to_workers_queue);
  queue_destroy(&response_queue);
  return 0;
}

double get_time_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

int main_worker(int argc, char* argv[])
{
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  printf("%s: Worker (rank %d) with %d arguments\n", argv[0], rank, argc);

  // execute forever
  while(1) {
    payload_t *payload = NULL;
    {
      // send our rank to the coordinator so it can send us jobs
      MPI_Ssend(&rank, 1, MPI_INT,
		0, // to our coordinator (rank zero)
		FRACTAL_MPI_PAYLOAD_REQUEST,
		MPI_COMM_WORLD);

      // receive the payload from the coordinator
      payload = mpi_payload_receive(0);
    }

    {
      // compute the response
      response_t *response = create_response_for_payload (payload);
      response->max_worker_id = size;
      response->worker_id = rank;

      // send our rank to the coordinator so it can wait for our response
      MPI_Ssend(&rank, 1, MPI_INT,
		0, // to our coordinator (rank zero)
		FRACTAL_MPI_RESPONSE_REQUEST,
		MPI_COMM_WORLD);

      // send the response back to the coordinator
      mpi_response_send(response);

      // free stuff for this round
      free(response->values);
      free(response);
      free(payload);
    }
  }
  return 0;
}

int main(int argc, char* argv[])
{
  int provided;
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
