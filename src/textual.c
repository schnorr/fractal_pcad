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
#include <netdb.h> 
#include <sys/types.h>
#include <signal.h>
#include <stdatomic.h>
#include "timing.h"
#include "fractal.h"
#include "connection.h"
#include "queue.h"

atomic_int shutdown_requested;

static queue_t payload_queue = {0};
static queue_t response_queue = {0};

/* Shutdown function. Shuts down the TCP connection and sends "poison pills" to queues.*/
void request_shutdown(int connection){
  // Atomic exchange tests the variable and sets it atomically
  if (atomic_exchange(&shutdown_requested, 1) == 1){
    return; // If already shutting down
  }
  printf("Shutdown requested.\n");
  shutdown(connection, SHUT_RDWR); 

  // Send "poison pills" to queues, making threads dequeuing them quit
  queue_enqueue(&payload_queue, NULL);
  queue_enqueue(&response_queue, NULL);
}

/*
  ui_thread_function: this function is a placeholder
  to simulate user interaction. Every time a user selects
  a new area to compute, a payload must be created with a
  new generation. This payload must then be queued in the
  payload_queue using the queue_enqueue function.
*/
/*
void *ui_thread_function () {
  static int generation = 0;
  //This action is guided by the user

  // Placeholder: Currently simulating user input with random payloads every 5-10 seconds
  // Input handling function would be here instead
  while(!atomic_load(&shutdown_requested)) {
    int wait_seconds = (rand() % 6) + 5;

    // User interaction creates a new payload
    payload_t *payload = calloc(1, sizeof(payload_t));
    if (payload == NULL) {
      fprintf(stderr, "malloc failed.\n");
      break;
    }

    payload->generation = generation++;
    payload->granularity = 10;
    payload->ll.real = -2.0;
    payload->ll.imag = -1.5;
    payload->ur.real =  2.0;
    payload->ur.imag =  1.5;
    payload->fractal_depth = 2048*2;

    payload->s_ll.x = 0;
    payload->s_ll.y = 0;
    payload->s_ur.x = 1920;
    payload->s_ur.y = 1080;

#ifdef PAYLOAD_DEBUG
    payload_print(__func__, "enqueueing payload", payload);
#endif

    queue_enqueue(&payload_queue, payload);
    payload = NULL;
    sleep(wait_seconds);
  }

  pthread_exit(NULL);
}
*/
/*
void *render_thread_function () {
  // This action is guided by the responses from the coordinator

  // Placeholder: Currently printing random values that come from coordinator
  // Rendering function that takes response would be here instead
  while(!atomic_load(&shutdown_requested)) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);
    if (response == NULL) break; // Poison pill

#ifdef RESPONSE_DEBUG
    response_print(__func__, "Dequeued response", response);
#endif

    // Freeing response after using it
    free(response->values);
    free(response);
  }

  pthread_exit(NULL);
}
*/

void *net_thread_send_payload (void *arg)
{
  int connection = *(int *)arg;
  while(!atomic_load(&shutdown_requested)) {
    payload_t *payload = (payload_t *)queue_dequeue(&payload_queue);
    if (payload == NULL) break; 

    bool shutdown_coordinator = (payload->generation == PAYLOAD_GENERATION_SHUTDOWN);
    
    if (send(connection, payload, sizeof(*payload), 0) <= 0) {
      fprintf(stderr, "Send failed. Killing thread...\n");
      free(payload);
      break;
    }

#ifdef PAYLOAD_DEBUG
    printf("Sent payload.\n");
#endif

    free(payload);

    if (shutdown_coordinator) {
      printf("Sent shutdown payload to coordinator.\n");
      request_shutdown(connection);
      break;
    }
  }

  pthread_exit(NULL);
}

void *net_thread_receive_response (void *arg)
{
  int connection = *(int *)arg;
  while(!atomic_load(&shutdown_requested)){
    response_t *response = malloc(sizeof(response_t)); 
    if (response == NULL) {
      fprintf(stderr, "malloc failed.\n");
      pthread_exit(NULL);
    }

    // Receive base response struct. Pointer to values is garbage.
    if (recv_all(connection, response, sizeof(response_t), 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      free(response);
      pthread_exit(NULL);
    }

    size_t buffer_size = response->payload.granularity * response->payload.granularity;
    buffer_size *= sizeof(int);
    int *buffer = malloc(buffer_size);

    if (buffer == NULL) {
      fprintf(stderr, "malloc failed.\n");
      free(response);
      pthread_exit(NULL);
    }

    // Receive values
    if (recv_all(connection, buffer, buffer_size, 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      free(response);
      free(buffer);
      pthread_exit(NULL);
    }

    response->values = buffer;

    queue_enqueue(&response_queue, response);
    response = NULL; // Transferred ownership to queue
  }
  pthread_exit(NULL);
}

payload_t *parse_args(int argc, char *argv[])
{
  if (argc != 11) {
    fprintf(stderr, "Wrong format. Format:\n"
      "%s <host> <port> <granularity> <fractal_depth> "
      "<screen width> <screen height> "
      "<ll_real> <ll_imag> <ur_real> <ur_imag>\n", argv[0]);
    exit(1);
  }

  payload_t *payload = malloc(sizeof(payload_t));
  if (payload == NULL) {
    fprintf(stderr, "malloc failed.\n");
    exit(1);
  }
  payload->generation = 1;
  payload->granularity = atoi(argv[3]);
  payload->fractal_depth = atoi(argv[4]);
  payload->s_ll.x = 0;
  payload->s_ll.y = 0;
  payload->s_ur.x = atoi(argv[5]);
  payload->s_ur.y = atoi(argv[6]);
  payload->ll.real = strtold(argv[7], NULL);
  payload->ll.imag = strtold(argv[8], NULL);
  payload->ur.real = strtold(argv[9], NULL);
  payload->ur.imag = strtold(argv[10], NULL);

  return payload;
}

int main(int argc, char* argv[])
{
  payload_t *payload = parse_args(argc, argv);

  atomic_init(&shutdown_requested, 0);
  signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE (failed send)

  queue_init(&payload_queue, 1, free);
  queue_init(&response_queue, 256, free_response);

  int connection = open_connection(argv[1], atoi(argv[2]));

  //pthread_t ui_thread = 0;
  //pthread_t render_thread = 0;
  pthread_t payload_thread = 0;
  pthread_t response_thread = 0;

  //pthread_create(&ui_thread, NULL, ui_thread_function, NULL);
  //pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  // send payload to coordinator, wait for all responses
  // then send poison pill to coordinator, shut down

  int amount_x = (payload->s_ur.x - 1  + payload->granularity-1) / payload->granularity;
  int amount_y = (payload->s_ur.y - 1  + payload->granularity-1) / payload->granularity;
  int expected_responses = amount_x * amount_y;

  struct timespec start_time, first_response_time, end_time;
  
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  queue_enqueue(&payload_queue, payload);
  payload = NULL;

  response_t *response = (response_t *)queue_dequeue(&response_queue);
  free_response(response);
  clock_gettime(CLOCK_MONOTONIC, &first_response_time);


  for (int i = 1; i < expected_responses; i++) {
    response = (response_t *)queue_dequeue(&response_queue);
    free_response(response);
  }
  clock_gettime(CLOCK_MONOTONIC, &end_time);

  printf("[CLIENT_LOG] Time to dequeue first response: %.9fs\n", 
         timespec_to_double(timespec_diff(start_time, first_response_time)));
  printf("[CLIENT_LOG] Time to dequeue all responses: %.9fs\n", 
         timespec_to_double(timespec_diff(start_time, end_time)));

  payload_t *poison = calloc(1, sizeof(payload_t));
  poison->generation = PAYLOAD_GENERATION_SHUTDOWN;

  queue_enqueue(&payload_queue, poison); // shut down coordinator and client
  poison = NULL;

  //pthread_join(ui_thread, NULL);
  //pthread_join(render_thread, NULL);
  pthread_join(payload_thread, NULL);
  pthread_join(response_thread, NULL);

  close(connection);

  queue_destroy(&payload_queue);
  queue_destroy(&response_queue);

  return 0;
}
