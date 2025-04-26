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
void *ui_thread_function () {
  static int generation = 0;
  //This action is guided by the user

  // Placeholder: Currently simulating user input with random payloads every 5-10 seconds
  // Input handling function would be here instead
  while(!atomic_load(&shutdown_requested)) {
    int wait_seconds = 0;

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

void *net_thread_send_payload (void *arg)
{
  int connection = *(int *)arg;
  while(!atomic_load(&shutdown_requested)) {
    payload_t *payload = (payload_t *)queue_dequeue(&payload_queue);
    if (payload == NULL) break; // Poison pill
    
    if (send(connection, payload, sizeof(*payload), 0) <= 0) {
      fprintf(stderr, "Send failed. Killing thread...\n");
      free(payload);
      break;
    }

#ifdef PAYLOAD_DEBUG
    printf("Sent payload.\n");
#endif

    free(payload);
  }

  pthread_exit(NULL);
}

void *net_thread_receive_response (void *arg)
{
  int connection = *(int *)arg;
  while(!atomic_load(&shutdown_requested)){
    size_t response_size;

    // Receive size of response. Done since response can be arbitrarily large due to *values
    if (recv_all(connection, &response_size, sizeof(response_size), 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      break;
    }
  
    uint8_t *buffer = malloc(response_size);
    if (buffer == NULL) {
      fprintf(stderr, "malloc failed.\n");
      break;
    }

    // Receive actual response, put in buffer
    if (recv_all(connection, buffer, response_size, 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      free(buffer);
      break;
    }

    response_t *response = response_deserialize(&buffer);
    free(buffer); 
    buffer = NULL;

    queue_enqueue(&response_queue, response);
    response = NULL; // Transferred ownership to queue
  }
  pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
  if (argc != 3) {
    printf("Missing arguments. Format:\n%s <host> <port>\n", argv[0]);
    return 1;
  }

  atomic_init(&shutdown_requested, 0);
  signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE (failed send)

  queue_init(&payload_queue, 1, free);
  queue_init(&response_queue, 256, free_response);

  int connection = open_connection(argv[1], atoi(argv[2]));

  pthread_t ui_thread = 0;
  pthread_t render_thread = 0;
  pthread_t payload_thread = 0;
  pthread_t response_thread = 0;

  pthread_create(&ui_thread, NULL, ui_thread_function, NULL);
  pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  char input[64];
  while (!atomic_load(&shutdown_requested)){
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0';
    if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
      request_shutdown(connection);
    }
  }

  pthread_join(ui_thread, NULL);
  pthread_join(render_thread, NULL);
  pthread_join(payload_thread, NULL);
  pthread_join(response_thread, NULL);

  close(connection);

  queue_destroy(&payload_queue);
  queue_destroy(&response_queue);

  return 0;
}
