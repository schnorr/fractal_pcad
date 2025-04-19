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
#include <netdb.h> 
#include <sys/types.h>

#define MAX_QUEUE_SIZE 100 // Should probably be much higher

static queue_t payload_queue;
static queue_t response_queue;

// Currently sending random packets then ending threads.

// Main thread manages the window and user interaction
void *ui_thread_function () {
  //This action is guided by the user

  // Placeholder: Currently simulating user input with random payloads every 5-10 seconds
  // Input handling function would be here instead
  while(1) {
    int wait_seconds = (rand() % 6) + 5;
    sleep(wait_seconds);

    payload_t *payload = malloc(sizeof(payload_t));
    if (payload == NULL) {
      perror("malloc failed.\n");
      pthread_exit(NULL);
    }

    payload->generation = rand() % 100;
    payload->granularity = rand() % 100;
    payload->fractal_depth = rand() % 100;
    payload->ll.x = (float)(rand() % 100) / 100.0f;
    payload->ll.y = (float)(rand() % 100) / 100.0f;
    payload->ur.x = (float)(rand() % 100) / 100.0f;
    payload->ur.y = (float)(rand() % 100) / 100.0f;
    payload->screen_width = rand() % 1000;
    payload->screen_height = rand() % 1000;

    printf("Enqueueing payload: [%d, %d, %d, (%lf, %lf), (%lf, %lf), %d, %d]\n",
           payload->generation, payload->granularity, payload->fractal_depth, 
           payload->ll.x, payload->ll.y, payload->ur.x, payload->ur.y, 
           payload->screen_width, payload->screen_height);

    queue_enqueue(&payload_queue, payload);
    payload = NULL;
  }

  pthread_exit(NULL);
}

void *render_thread_function () {
  // This action is guided by the responses from the coordinator

  // Placeholder: Currently printing random values that come from coordinator
  // Rendering function that takes response would be here instead
  while(1) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);

    printf("Dequeued response: [%d, %d, %d, %d, (%d, %d)]\n",
           response->generation, response->granularity, response->worker_id,
           response->max_worker_id, response->ll.x, response->ll.y);

    printf("Values:" );
    for (int i = 0; i < response->granularity * response->granularity; i++) {
      printf(" %d", response->values[i]);
    }
    printf("\n");
    
    // Freeing response after using it
    free(response->values);
    free(response);
  }

  pthread_exit(NULL);
}

void *net_thread_send_payload (void *arg)
{
  int connection = *(int *)arg;
  while(1) {
    payload_t *payload = (payload_t *)queue_dequeue(&payload_queue);
    
    if (send(connection, payload, sizeof(*payload), 0) <= 0) {
      perror("Send failed. Killing thread...\n");
      free(payload);
      pthread_exit(NULL);
    }

    printf("Sent payload.\n");

    free(payload);
  }

  pthread_exit(NULL);
}

void *net_thread_receive_response (void *arg)
{
  int connection = *(int *)arg;
  while(1){
    size_t response_size;

    // Receive size of response. Done since response can be arbitrarily large due to *values
    if (recv_all(connection, &response_size, sizeof(response_size), 0) <= 0) {
      perror("Receive failed. Killing thread...\n");
      pthread_exit(NULL);
    }
  
    uint8_t *buffer = malloc(response_size);
    if (buffer == NULL) {
      perror("malloc failed.\n");
      pthread_exit(NULL);
    }

    // Receive actual response, put in buffer
    if (recv_all(connection, buffer, response_size, 0) <= 0) {
      perror("Receive failed. Killing thread...\n");
      free(buffer);
      pthread_exit(NULL);
    }

    printf("Received response.\n");
    
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

  queue_init(&payload_queue, MAX_QUEUE_SIZE);
  queue_init(&response_queue, MAX_QUEUE_SIZE);

  int connection = open_connection(argv[1], atoi(argv[2]));

  pthread_t ui_thread, render_thread, payload_thread, response_thread;

  pthread_create(&ui_thread, NULL, ui_thread_function, NULL);
  pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  pthread_join(ui_thread, NULL);
  pthread_join(render_thread, NULL);
  pthread_join(payload_thread, NULL);
  pthread_join(response_thread, NULL);

  close(connection);

  queue_destroy(&payload_queue);
  queue_destroy(&response_queue);

  return 0;
}
