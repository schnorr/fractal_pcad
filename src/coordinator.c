#include "fractal.h"
#include "connection.h"
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <arpa/inet.h> 
#include <sys/socket.h>   
#include <netinet/in.h>
#include <pthread.h>

#define MAX_QUEUE_SIZE 100 // Should probably be much higher

//payload from the grafica
static payload_t *newest_payload = NULL;
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

    printf("Received payload: [%d, %d, %d, (%lf, %lf), (%lf, %lf), %d, %d]\n",
           payload->generation, payload->granularity, payload->fractal_depth, 
           payload->ll.x, payload->ll.y, payload->ur.x, payload->ur.y, 
           payload->screen_width, payload->screen_height);
    
    pthread_mutex_lock(&newest_payload_mutex);
    // Checking if there's a previous payload that hasn't been used in compute_create_blocks yet
    if (newest_payload != NULL) {
      free(newest_payload);
    }
    newest_payload = payload; // Update newest payload, signal compute_create_blocks
    payload = NULL;
    pthread_cond_signal(&new_payload);
    pthread_mutex_unlock(&newest_payload_mutex);
  }
  pthread_exit(NULL);
}

/*
  compute_create_blocks:
*/
void *compute_create_blocks()
{
  while(1) {
    pthread_mutex_lock(&newest_payload_mutex);
    while (newest_payload == NULL) { // Wait for new payload to arrive
      pthread_cond_wait(&new_payload, &newest_payload_mutex);
    }

    //payload consumer: get the payload, discretize in blocks
    //Call Ana Laura function

    queue_clear(&payload_to_workers_queue); // new payload, so clear obsolete payloads to workers

    // Placeholder: simply passing along payload
    queue_enqueue(&payload_to_workers_queue, newest_payload);
    newest_payload = NULL; // Ownership transferred to queue

    pthread_mutex_unlock(&newest_payload_mutex);
  }
  pthread_exit(NULL);
}

void *main_thread_function()
{
  while(1) {
    // Might have to rethink - queue_dequeue() currently locks thread. If this thread needs to wait
    // for worker answers, maybe a queue_peek() that doesn't block thread would be appropriate?

    // Placeholder: Gets payload and generates random response after 5-10 secs delay
    payload_t *payload = (payload_t *)queue_dequeue(&payload_to_workers_queue);
    free(payload); // simply free payload
    payload = NULL;

    response_t *response = malloc(sizeof(response_t));
    if (response == NULL) {
      perror("malloc failed.\n");
      pthread_exit(NULL);
    }

    response->values = malloc(sizeof(int) * 4);
    if (response->values == NULL) {
      perror("malloc failed.\n");
      pthread_exit(NULL);
    }

    // random vals
    response->generation = rand() % 100;
    response->granularity = 2; // granularity 2 so that there's always 4 values
    response->ll.x = rand() % 100;
    response->ll.y = rand() % 100;
    response->max_worker_id = rand() % 100;
    response->worker_id = rand() % 100;
    response->values[0] = rand() % 100;
    response->values[1] = rand() % 100;
    response->values[2] = rand() % 100;
    response->values[3] = rand() % 100;

    printf("Enqueueing response: [%d, %d, %d, %d, (%d, %d)]\n",
           response->generation, response->granularity, response->worker_id,
           response->max_worker_id, response->ll.x, response->ll.y);
    printf("Values:" );
    for (int i = 0; i < response->granularity * response->granularity; i++) {
      printf(" %d", response->values[i]);
    }
    printf("\n");

    queue_enqueue(&response_queue, response);
    response = NULL; // Transferred ownership to queue
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

    printf("Sent response.\n");

    free(response->values);
    free(response);
    free(buffer);
  }

  pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
  if (argc == 1) {
    printf("Missing arguments. Format: %s <port>\n", argv[0]);
    return 1;
  }

  //Launch the paralellism mechanism!
  
  struct sockaddr_in client_addr; // client ip address after connect
  socklen_t client_len = sizeof(client_addr);
  int socket = open_server_socket(atoi(argv[1]));
  int connection = accept(socket, (struct sockaddr *)&client_addr, &client_len);
  printf("Accepted connection.\n");

  queue_init(&payload_to_workers_queue, MAX_QUEUE_SIZE);
  queue_init(&response_queue, MAX_QUEUE_SIZE);
  pthread_mutex_init(&newest_payload_mutex, NULL);
  pthread_cond_init(&new_payload, NULL);
  
  pthread_t main_thread = 0;
  pthread_t compute_thread = 0;
  pthread_t payload_receive_thread = 0;
  pthread_t response_send_thread = 0;

  pthread_create(&main_thread, NULL, main_thread_function, NULL);
  pthread_create(&compute_thread, NULL, compute_create_blocks, NULL);
  pthread_create(&payload_receive_thread, NULL, net_thread_receive_payload, &connection);
  pthread_create(&response_send_thread, NULL, net_thread_send_response, &connection);

  pthread_join(main_thread, NULL);
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
