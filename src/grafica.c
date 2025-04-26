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
#include <signal.h>
#include <stdatomic.h>

#include "fractal.h"
#include "connection.h"
#include "queue.h"
#include "colors.h"

atomic_int shutdown_requested;

// We need a multi-threaded Raylib app, where one thread handles
// rendering, and another handles the UI or logic.  Shared pixel
// buffer between the two threads (the render and the drawing).  To
// avoid race conditions while one thread writes and the other reads
// from the shared Color * buffer we use the pixelMutex.

#define TOTAL_COLORS 3
Color *g_pallete_color = NULL;
Color *g_mandelbrot_color = NULL;
Color *g_worker_color = NULL;
Color *g_both_color = NULL; // mandelbrot with lower alpha + pallete
bool g_show_workers = false;

int g_actual_color = 0;

pthread_mutex_t pixelMutex;

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

bool g_selecting = false;
Vector2 g_box_origin = {0, 0};
Vector2 g_box_attr = {0, 0};

// Currently sending random packets then ending threads.

/* Shutdown function. Shuts down the TCP connection and unblocks all queues.*/
void request_shutdown(int connection){
  // Atomic exchange tests the variable and sets it atomically
  if (atomic_exchange(&shutdown_requested, 1) == 1){
    return; // If already shutting down
  }
  printf("Shutdown requested.\n");
  shutdown(connection, SHUT_RDWR); 
  queue_shutdown(&payload_queue);
  queue_shutdown(&response_queue);
}

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

  static fractal_coord_t actual_ll = {-2, -1.5};
  static fractal_coord_t actual_ur = {2, 1.5};

   // Placeholder: Currently simulating user input with random payloads every 5-10 seconds
  // Input handling function would be here instead

  double screen_width = 0.0f;
  double screen_height = 0.0f;
  double pixel_coord_ratio = 0;

  Vector2 mouse = {0};
  Vector2 first_point_screen = {0, 0};

  fractal_coord_t first_point_fractal = {0, 0};
  fractal_coord_t second_point_fractal = {0, 0};
    
  bool interaction = false; 
  bool initial = true;
  
  bool clicked = false;
  // Primeiro caso enviar a tela inteira
  // Parar de desenhar o quadrado quando o payload 

  while(!atomic_load(&shutdown_requested)) {

    if(initial && IsWindowReady()){ // If window is ready, config UI needs and send a initial payload
      screen_width = (double)GetScreenWidth();
      screen_height = (double)GetScreenHeight();
      
      pixel_coord_ratio = (actual_ur.real - actual_ll.real)/screen_width;
      actual_ur.imag = ((actual_ur.real - actual_ll.real) * (screen_height/screen_width))/2;
      actual_ll.imag = actual_ur.imag * -1;
      
      initial = false;
      interaction = true;

      g_box_origin = (Vector2) {0,0};
      g_box_attr = (Vector2) {screen_width, screen_height};
    }

    /* ENTER starts user interaction */
    if(!g_selecting && (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))){
      g_selecting = true;
      g_box_origin = (Vector2) {screen_width/4, screen_height/4};
      g_box_attr = (Vector2) {screen_width/2, screen_height/2};
      WaitTime(0.1);
    }

    if(g_selecting && (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))){
      g_selecting = false;
      interaction = true;
      WaitTime(0.1);
    }    

    if(IsKeyPressed(KEY_BACKSPACE)){
      g_selecting = false;
    }
    

    if(g_selecting){
      mouse.x = GetMouseX();
      mouse.y = GetMouseY();

      if(mouse.x > g_box_origin.x && mouse.y > g_box_origin.y &&
	 mouse.x < g_box_origin.x + g_box_attr.x && mouse.y < g_box_origin.y + g_box_attr.y){

	float zoom = GetFrameTime()*GetMouseWheelMove();
	WaitTime(0.001);
	
	g_box_attr.x = g_box_attr.x + zoom*screen_width;
	g_box_attr.y = g_box_attr.y + zoom*screen_height;
	g_box_origin.x = g_box_origin.x - zoom*screen_width/2;
	g_box_origin.y = g_box_origin.y - zoom*screen_height/2;

	if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
	  first_point_screen.x = GetMouseX();
	  first_point_screen.y = GetMouseY();
	  clicked = true;
	}
	
      } else {
	clicked = true;
      }
      
      if(clicked && IsMouseButtonDown(MOUSE_BUTTON_LEFT)){ 

	  Vector2 distance = (Vector2) {g_box_origin.x - first_point_screen.x, g_box_origin.y - first_point_screen.y};
	
                                                         	  /* mouse delta */
	  g_box_origin.x = max(0, mouse.x + distance.x + (mouse.x - first_point_screen.x)*GetFrameTime());
	  g_box_origin.y = max(0, mouse.y + distance.y + (mouse.y - first_point_screen.y)*GetFrameTime());

	
	  g_box_origin.x = min(g_box_origin.x, screen_width - g_box_attr.x);
	  g_box_origin.y = min(g_box_origin.y, screen_height - g_box_attr.y);
	
	  first_point_screen.x = mouse.x;
	  first_point_screen.y = mouse.y;
      
      }
    }    

    // Changing mandelbrot color
    if(IsKeyPressed(KEY_SPACE)){
      g_actual_color = (g_actual_color + 1 >= TOTAL_COLORS) ? 0 : g_actual_color + 1; // Using colors defined by function get_color in colors.c
      WaitTime(0.1);
    }
    if(IsKeyPressed(KEY_W)){
      g_show_workers = !g_show_workers;
      WaitTime(0.1);
    }
    
    
    if(interaction == true){
      interaction = false;
      
      payload_t *payload = calloc(1, sizeof(payload_t));
      if (payload == NULL) {
	fprintf(stderr, "malloc failed.\n");
	pthread_exit(NULL);
      }


      /* ratio from pixels to coordinates based on the x axis */
      pixel_coord_ratio = (actual_ur.real - actual_ll.real)/screen_width;

      /* Transforming from screen coordinates to fractal coordinates */
      first_point_fractal.real = (g_box_origin.x)*pixel_coord_ratio + actual_ll.real;
      first_point_fractal.imag = (g_box_origin.y)*pixel_coord_ratio + actual_ll.imag;

      second_point_fractal.real = (g_box_origin.x + g_box_attr.x)*pixel_coord_ratio + actual_ll.real;
      second_point_fractal.imag = (g_box_origin.y + g_box_attr.y)*pixel_coord_ratio + actual_ll.imag;
      
      payload->generation = generation++;
      payload->granularity = 10; // placeholder values
      payload->fractal_depth = 1000; // <-/

      payload->ll.real = (float) min(first_point_fractal.real, second_point_fractal.real); 
      payload->ll.imag = (float) min(first_point_fractal.imag, second_point_fractal.imag); 
      payload->ur.real = (float) max(first_point_fractal.real, second_point_fractal.real);
      payload->ur.imag = (float) max(first_point_fractal.imag, second_point_fractal.imag);

      actual_ll = payload->ll;
      actual_ur = payload->ur;
      
      payload->s_ll.x = 0;
      payload->s_ll.y = 0;
      payload->s_ur.x = screen_width;
      payload->s_ur.y = screen_height;

      payload_print(__func__, "Enqueueing payload", payload);

      queue_enqueue(&payload_queue, payload);
      payload = NULL;
      WaitTime(0.1);
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

  static int generation = 0;

  int screen_width = GetScreenWidth();
  int screen_height = GetScreenHeight();

  // Placeholder: Currently printing random values that come from coordinator
  // Rendering function that takes response would be here instead
  while(!atomic_load(&shutdown_requested)) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);
    if (response == NULL) break; // Queue shutdown

    if(response->payload.generation > generation){
      generation = response->payload.generation;
    }
    //    response_print(__func__, "dequeued response", response);
    if(response->payload.generation == generation){
      pthread_mutex_lock(&pixelMutex); //lock
      int p = 0;
      for (int i = response->payload.s_ll.x; i < response->payload.s_ur.x; i++){
	for (int j = response->payload.s_ll.y; j < response->payload.s_ur.y; j++){
	  if (i < screen_width && j < screen_height) {
	  // must find a better way to map response->values[p] to a color
	  // the maximum value of response->values[p] is available at
	  // response->payload.fractal_depth
	    cor_t cor = {0};
	    
	    cor = get_color(response->values[p],
			    response->payload.fractal_depth);
	    
	    struct Color color = {cor.r, cor.g, cor.b, 255};
	    g_mandelbrot_color[j * screen_width + i] = color;

	    cor = get_color(response->worker_id,
			    response->max_worker_id);

	    color = (struct Color){cor.r, cor.g, cor.b, 255};
	    g_worker_color[j * screen_width + i] = color;
	    
	    color = (struct Color){cor.r, cor.g, cor.b, 100};
	    g_both_color[j * screen_width + i] = color;
	    
	    cor = get_color_viridis(response->values[p],
				    response->payload.fractal_depth);
	    
	    color = (struct Color){cor.r, cor.g, cor.b, 255};
	    g_pallete_color[j * screen_width + i] = color;
	    
	  }
	  p++;
	}
      }
      pthread_mutex_unlock(&pixelMutex); //unlock
    }
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
  while(!atomic_load(&shutdown_requested)) {
    payload_t *payload = (payload_t *)queue_dequeue(&payload_queue);
    if (payload == NULL) break; // Queue shutdown
    
    if (send(connection, payload, sizeof(*payload), 0) <= 0) {
      fprintf(stderr, "Send failed. Killing thread...\n");
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
  while(!atomic_load(&shutdown_requested)){
    size_t response_size;

    // Receive size of response. Done since response can be arbitrarily large due to *values
    if (recv_all(connection, &response_size, sizeof(response_size), 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      pthread_exit(NULL);
    }

    uint8_t *buffer = malloc(response_size);
    if (buffer == NULL) {
      fprintf(stderr, "malloc failed.\n");
      pthread_exit(NULL);
    }

    // Receive actual response, put in buffer
    if (recv_all(connection, buffer, response_size, 0) <= 0) {
      fprintf(stderr, "Receive failed. Killing thread...\n");
      free(buffer);
      pthread_exit(NULL);
    }

    response_t *response = response_deserialize(&buffer);
    free(buffer); 
    buffer = NULL;

    /* printf("(%d) %s: received response.\n", response->generation, __func__); */
    /* printf("\t[%d, %d]\n", */
    /* 	   response->granularity, response->worker_id); */

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

  int screen_width = 1000;
  int screen_height = 800;
  /* int screen_width = GetScreenWidth(); */
  /* int screen_height = GetScreenHeight(); */
  
  InitWindow(screen_width, screen_height, "Fractal @ PCAD");
  //  InitWindow(GetScreenWidth(), GetScreenHeight(), "Fractal @ PCAD");
  SetTargetFPS(60);

  // create a CPU-side "image" that we can draw on top when needed
  Image img = GenImageColor(screen_width, screen_height, RAYWHITE);
  g_mandelbrot_color = LoadImageColors(img); //global
  Texture2D texture_mandelbrot = LoadTextureFromImage(img);
  UnloadImage(img);

  img = GenImageColor(screen_width, screen_height, RAYWHITE);
  g_pallete_color = LoadImageColors(img); //global
  Texture2D texture_pallete = LoadTextureFromImage(img);
  UnloadImage(img);

  img = GenImageColor(screen_width, screen_height, RAYWHITE);
  g_worker_color = LoadImageColors(img); //global
  Texture2D texture_worker = LoadTextureFromImage(img);
  UnloadImage(img);

  img = GenImageColor(screen_width, screen_height, RAYWHITE);
  g_both_color = LoadImageColors(img); //global
  Texture2D texture_both = LoadTextureFromImage(img);
  UnloadImage(img);

  pthread_mutex_init(&pixelMutex, NULL);

  srand(0);

  queue_init(&payload_queue, MAX_QUEUE_SIZE, free);
  queue_init(&response_queue, MAX_QUEUE_SIZE, free_response);

  int connection = open_connection(argv[1], atoi(argv[2]));

  pthread_t ui_thread = 0;
  pthread_t render_thread = 0;
  pthread_t payload_thread = 0;
  pthread_t response_thread = 0;

  pthread_create(&ui_thread, NULL, ui_thread_function, NULL);
  pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  //  ToggleFullscreen();
  while (!WindowShouldClose()) { // Closed with ESC or manually closing window
    // get the mutex so we can read safely from sharedPixels
    pthread_mutex_lock(&pixelMutex);
    UpdateTexture(texture_mandelbrot, g_mandelbrot_color);
    UpdateTexture(texture_pallete, g_pallete_color);
    UpdateTexture(texture_worker, g_worker_color);
    UpdateTexture(texture_both, g_both_color);
    pthread_mutex_unlock(&pixelMutex);

    // do the drawing (only here we actually do the drawing)
    BeginDrawing();
    ClearBackground(RAYWHITE);

    
    if(g_actual_color == 0)
      DrawTexture(texture_mandelbrot, 0, 0, WHITE);
    else if(g_actual_color == 1)
      DrawTexture(texture_pallete, 0, 0, WHITE);
    else if(g_actual_color == 2)
      DrawTexture(texture_worker, 0, 0, WHITE);
    if(g_show_workers){
      DrawTexture(texture_both, 0, 0, WHITE);
    }
    
    if(g_selecting){
      DrawRectangleV(g_box_origin, g_box_attr, (Color){1.0f, 1.0f, 255.0f, 100.0f});
    }

      
    EndDrawing();
  }

  // Window closed, request shutdown
  request_shutdown(connection);
  
  pthread_join(ui_thread, NULL);
  pthread_join(render_thread, NULL);
  pthread_join(payload_thread, NULL);
  pthread_join(response_thread, NULL);

  UnloadTexture(texture_mandelbrot);
  UnloadTexture(texture_pallete);
  UnloadTexture(texture_worker);
  UnloadTexture(texture_both);
  
  UnloadImageColors(g_mandelbrot_color);
  UnloadImageColors(g_pallete_color);
  UnloadImageColors(g_worker_color);
  UnloadImageColors(g_both_color);
  CloseWindow(); // Close OpenGL context 

  pthread_mutex_destroy(&pixelMutex);

  close(connection);

  queue_destroy(&payload_queue);
  queue_destroy(&response_queue);

  return 0;
}
