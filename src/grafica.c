#include "fractal.h"
#include "connection.h"
#include "queue.h"

#include <raylib.h>
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


#define max(a,b)				\
({ __typeof__ (a) _a = (a);			\
__typeof__ (b) _b = (b);			\
_a > _b ? _a : _b; })

#define min(a,b)				\
({ __typeof__ (a) _a = (a);			\
__typeof__ (b) _b = (b);			\
_a < _b ? _a : _b; })

#define MAX_QUEUE_SIZE 100 // Should probably be much higher

static queue_t payload_queue = {0};
static queue_t response_queue = {0};

// Currently sending random packets then ending threads.

// Main thread manages the window and user interaction

/*
  ui_thread_function: Every time a user selects
  a new area to compute, a payload must be created with a
  new generation. This payload must then be queued in the
  payload_queue using the queue_enqueue function.
*/
void *ui_thread_function () {
  static int generation = 0;

  //This action is guided by the user

  // Placeholder: Currently simulating user input with random payloads every 5-10 seconds
  // Input handling function would be here instead

  double screen_width = 0.0f;
  double screen_height = 0.0f;
  
  Vector2 first_click = {0, 0};
  Vector2 second_click = {0, 0};

  bool interaction = false; 
  bool initial = true;
  
  while(true) {
    
    if(initial && IsWindowReady()){ // If window is ready, send a intial payload
      screen_width = (double)GetMonitorWidth(GetCurrentMonitor());
      screen_height = (double)GetMonitorHeight(GetCurrentMonitor());

      second_click.x = screen_width;
      second_click.y = screen_height;

      initial = false;
      interaction = true;
    }
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){ 
      first_click.x = GetMouseX();
      first_click.y = GetMouseY();
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) // User interaction creates a new payload
	&& GetMouseX() != first_click.x){ // If mouse has moved from last click
     
      second_click.x = GetMouseX();
      second_click.y = GetMouseY();
      interaction = true;
    }

    if(interaction == true){
      interaction = false;
      
      payload_t *payload = malloc(sizeof(payload_t));
      if (payload == NULL) {
	perror("malloc failed.\n");
	pthread_exit(NULL);
      }

      payload->generation = generation++;
      payload->granularity = 10; // placeholder values
      payload->fractal_depth = 255; // <-/
      payload->ll.x = (float) min(first_click.x, second_click.x); 
      payload->ll.y = (float) max(first_click.y, second_click.y); // in Raylib, origin (0, 0) is in the upper-right corner
      payload->ur.x = (float) max(first_click.x, second_click.x);
      payload->ur.y = (float) min(first_click.y, second_click.y);
      payload->screen_width = screen_width;
      payload->screen_height = screen_height;

      printf("%s: Enqueueing payload (%d).\n", __func__, payload->generation);
      /* : [%d, %d, (%lf, %lf), (%lf, %lf), %d, %d]\n", */
      /* __func__, */
      /* payload->generation, payload->granularity, payload->fractal_depth,  */
      /* payload->ll.x, payload->ll.y, payload->ur.x, payload->ur.y,  */
      /* payload->screen_width, payload->screen_height); */

      queue_enqueue(&payload_queue, payload);
      payload = NULL;
      sleep(1); // to send just one payload per interaction
    }
  }

  pthread_exit(NULL);
}

/*
  render_thread_function: this function is responsible for
  drawing the responses in the raylib canvas, so it probably
  must be initilized with access to the raylib canvas. The
  function queue_dequeue blocks until a new response is obtained
  so we must be very reactive. As soon as it receives a new
  response, the drawing must be fast.
*/
void *render_thread_function () {
  // This action is guided by the responses from the coordinator

  // Placeholder: Currently printing random values that come from coordinator
  // Rendering function that takes response would be here instead
  while(true) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);

    printf("(%d) %s: dequeued response.\n", response->generation, __func__);
           /* : [%d, %d, %d, %d, (%d, %d)]\n", */
           /* response->generation, response->granularity, response->worker_id, */
           /* response->max_worker_id, response->ll.x, response->ll.y); */

    // Do the drawing in the canvas using the response coordinates
    /* printf("Values:" ); */
    /* for (int i = 0; i < response->granularity * response->granularity; i++) { */
    /*   printf(" %d", response->values[i]); */
    /* } */
    /* printf("\n"); */
    
    // Freeing response after using it
    free(response->values);
    free(response);
  }

  pthread_exit(NULL);
}

/*
  net_thread_send_payload: this function is responsible for
  sending the payload to the coordinator.
*/
void *net_thread_send_payload (void *arg)
{
  int connection = *(int *)arg;
  while(1) {
    payload_t *payload = (payload_t *)queue_dequeue(&payload_queue);
    
    if (send(connection, payload, sizeof(*payload), 0) <= 0) {
      perror("Send failed. Killing thread...\n");
      free(payload);
      pthread_exit(NULL);
    }else{
      printf("(%d) %s: payload sent.\n", payload->generation, __func__);
      free(payload);
    }
  }

  pthread_exit(NULL);
}

/*
  net_thread_receive_response: this function is responsible for
  receiving response. As the responses can be numerous (thousands), it
  is important to receive them as soon as possible. Algorithm: 1/ we
  receive the response size (sent by the coordinator); 2/ we mallocate
  the space needed to receive the response; 3/ we receive the actual data
  with the recv_all function; 4/ we deserialize the function with the
  response_deserialize function; 5/ and then we enqueue this function
  so the render_thread can do its work.
*/
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

    response_t *response = response_deserialize(&buffer);
    free(buffer); 
    buffer = NULL;

    printf("(%d) %s: received response.\n", response->generation, __func__);

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

  pthread_t ui_thread = 0;
  pthread_t render_thread = 0;
  pthread_t payload_thread = 0;
  pthread_t response_thread = 0;

  pthread_create(&ui_thread, NULL, ui_thread_function, NULL);
  pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  /* raylib program goes here */

  int screen_width = GetMonitorWidth(GetCurrentMonitor());
  int screen_height = GetMonitorHeight(GetCurrentMonitor());
  
  InitWindow(screen_width, screen_height, "Fractal @ PCAD");
  SetTargetFPS(60);

  ToggleFullscreen();
  while (!WindowShouldClose()) {
    
    BeginDrawing();

    ClearBackground(RAYWHITE);

    DrawText(TextFormat("screen size: %d, %d\n", GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor())), 0, 0, 32, BLACK);
    DrawText(TextFormat("screen size: %d, %d\n", GetScreenWidth(), GetScreenHeight()), 0, 100, 32, BLACK);
    
    EndDrawing();
  
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
