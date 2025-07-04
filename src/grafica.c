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

#define max(a,b)				\
({ __typeof__ (a) _a = (a);			\
__typeof__ (b) _b = (b);			\
_a > _b ? _a : _b; })

#define min(a,b)				\
({ __typeof__ (a) _a = (a);			\
__typeof__ (b) _b = (b);			\
_a < _b ? _a : _b; })

atomic_int shutdown_requested;

// We need a multi-threaded Raylib app, where one thread handles
// rendering, and another handles the UI or logic.  Shared pixel
// buffer between the two threads (the render and the drawing).  To
// avoid race conditions while one thread writes and the other reads
// from the shared Color * buffer we use the pixelMutex.
pthread_mutex_t pixelMutex;
int *g_pixel_depth = NULL;// Depths for all screen coordinates.
Color *g_pixels = NULL; // Stores current pallete texture in RAM
Color *g_worker_pixels = NULL; // Stores worker texture in RAM
bool g_pixels_changed = false;
bool g_show_workers = false;
int g_current_color = 0;

#define MAX_DEPTH 256*256*256
int g_granularity = 10;
bool g_gchanged = false;

int g_depth = 256;
bool g_dchanged = false;

int g_show_changes_timer = 0;

/* Selection box (blue box) related globals */
bool g_selecting = false;
Vector2 g_box_origin = {0, 0};
Vector2 g_box_attr = {0, 0}; // x = width, y = height

static queue_t payload_queue = {0};
static queue_t response_queue = {0};
payload_t *payload_history = NULL;
int payload_count = 0;

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

void update_pixels(response_t *response) {
  int screen_width = GetScreenWidth();
  int screen_height = GetScreenHeight();

  // Compute bounds before loop so we don't have to check whether a pixel escapes screen
  int x0 = max(response->payload.s_ll.x, 0);
  int y0 = max(response->payload.s_ll.y, 0);
  int x1 = min(response->payload.s_ur.x, screen_width);
  int y1 = min(response->payload.s_ur.y, screen_height);
  int x_offset = x0 - response->payload.s_ll.x;
  int y_offset = y0 - response->payload.s_ll.y;
  int p = y_offset * response->payload.granularity + x_offset;

  pthread_mutex_lock(&pixelMutex); //lock
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      g_pixel_depth[y * screen_width + x] = response->values[p];

      Color color = get_current_pallette_color(g_current_color, response->values[p], response->payload.fractal_depth);
	    g_pixels[y * screen_width + x] = color;

      color = get_current_pallette_color(0, response->worker_id, response->max_worker_id);
	    g_worker_pixels[y * screen_width + x] = color;
      p++;
	  }
    p += (response->payload.granularity - (x1 - x0)); // Skip to next row in case there are values left over
  }
  g_pixels_changed = true;
  pthread_mutex_unlock(&pixelMutex); //unlock
}

void swap_pallete() {
  g_current_color = (g_current_color + 1 > TOTAL_COLORS) ? 0 : g_current_color + 1;
  if (g_current_color >= TOTAL_COLORS) return;

  int screen_width = GetScreenWidth();
  int screen_height = GetScreenHeight();
  pthread_mutex_lock(&pixelMutex);
  for (int y = 0; y < screen_height; y++) {
    for (int x = 0; x < screen_width; x++) {
      int max_depth = payload_history[payload_count-1].fractal_depth;
      Color color = get_current_pallette_color(g_current_color, g_pixel_depth[y * screen_width + x], max_depth);
	    g_pixels[y * screen_width + x] = color;
    }
  }
  g_pixels_changed = true;
  pthread_mutex_unlock(&pixelMutex);
}

/*
  ui_thread_function: Every time a user selects
  a new area to compute, a payload must be created with a
  new generation. This payload must then be queued in the
  payload_queue using the queue_enqueue function.
*/
void *ui_thread_function () {
  /* This action is guided by the user */

  static int generation = 0;
  static fractal_coord_t actual_ll = {-2, -1.5};
  static fractal_coord_t actual_ur = {2, 1.5};
  long double screen_width = 0.0f;
  long double screen_height = 0.0f;
  long double pixel_coord_ratio = 0.0f;
  float zoom = 0.0f;

  Vector2 mouse = {0};
  Vector2 first_point_screen = {0, 0};

  fractal_coord_t first_point_fractal = {0, 0};
  fractal_coord_t second_point_fractal = {0, 0};

  bool interaction = false;
  bool initial = true;
  bool clicked = false;
  bool back = false;

  float moviment_speed = 0.0f;
  float zoom_speed = 0.0f;

  while(!atomic_load(&shutdown_requested)) {

    if(initial && IsWindowReady()){ /* Send the initial payload when the window gets ready */

      screen_width = (long double)GetScreenWidth();
      screen_height = (long double)GetScreenHeight();

      pixel_coord_ratio = (actual_ur.real - actual_ll.real)/screen_width;

      actual_ur.imag = ((actual_ur.real - actual_ll.real) * (screen_height/screen_width))/2;
      actual_ll.imag = actual_ur.imag * -1;

      g_box_origin = (Vector2) {0,0};
      g_box_attr = (Vector2) {screen_width, screen_height};

      initial = false;
      interaction = true;
    }



    /* Starting selecting by pressing enter */
    if(!g_selecting && IsKeyPressed(KEY_ENTER)){
      g_selecting = true;
      g_box_origin = (Vector2) {screen_width/4, screen_height/4};
      g_box_attr = (Vector2) {screen_width/2, screen_height/2};
      WaitTime(0.1); /* This is needed so this can work with the visual interface */
    }
    if(g_selecting && IsKeyPressed(KEY_ENTER)){
      g_selecting = false;
      interaction = true;
      WaitTime(0.1);
    }
    if(IsKeyPressed(KEY_BACKSPACE)){
      g_selecting = false;
    }


    /* add or sub fractal depth */
    if(IsKeyDown(KEY_P)){
      if(IsKeyDown(KEY_MINUS)){
	g_dchanged = true;
	g_show_changes_timer = 50;
	  
	g_depth *= 0.9;
	if(g_depth < 2){
	  g_depth = 2;
	}
	WaitTime(0.1);
      }
      if(IsKeyDown(KEY_EQUAL)){
	g_dchanged = true;
	g_show_changes_timer = 50;
		
  if (g_depth < 10) g_depth++;
	else g_depth *= 1.1;

	if(g_depth > MAX_DEPTH){
	  g_depth = MAX_DEPTH;
	}
	WaitTime(0.1);
      }
    }



    /* add or sub granularity  */
    if(IsKeyDown(KEY_G)){
      if(IsKeyDown(KEY_MINUS)){
	g_gchanged = true;
	g_show_changes_timer = 50;

	g_granularity *= 0.9;
	if(g_granularity < 1){
	  g_granularity = 1;
	}
	WaitTime(0.1);
      }
      if(IsKeyDown(KEY_EQUAL)){
	g_gchanged = true;
	g_show_changes_timer = 50;

  if(g_granularity < 10) g_granularity++;
  else g_granularity *= 1.1;

 	if(g_granularity > 500){
	  g_granularity = 500;
	}
	WaitTime(0.1);
      }
    }



    /* Go back with C-z */
    if(IsKeyPressed(KEY_Z) && IsKeyDown(KEY_LEFT_CONTROL)){
      // Is 1, because gen 0, is the 0 position
      if(payload_count > 1){
	back = true;
	interaction = true;
	WaitTime(0.1);
      }
    }



    /* Zoom with mouse */
    if(GetMouseWheelMove()){
      zoom = GetFrameTime()*GetMouseWheelMove();
      WaitTime(0.001); /* This defines how fast the box will inscrease or decrease */

      g_box_attr.x = max(1, g_box_attr.x - zoom*screen_width);
      g_box_attr.y = max(1,g_box_attr.y - zoom*screen_height);

      if(g_box_attr.x > 1 && g_box_attr.y > 1){
	g_box_origin.x = g_box_origin.x + zoom*screen_width/2;
	g_box_origin.y = g_box_origin.y + zoom*screen_height/2;
      }
    }



    /* Selection box related */
    if(g_selecting){
      /* Mouse interaction */
      mouse.x = GetMouseX();
      mouse.y = GetMouseY();
      if(mouse.x > g_box_origin.x && mouse.y > g_box_origin.y &&
	 mouse.x < g_box_origin.x + g_box_attr.x && mouse.y < g_box_origin.y + g_box_attr.y){

	if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){

	  first_point_screen.x = GetMouseX();
	  first_point_screen.y = GetMouseY();
	  clicked = true;
	}
      } else { clicked = true; }

      if(clicked && IsMouseButtonDown(MOUSE_BUTTON_LEFT)){

	Vector2 distance = (Vector2) {g_box_origin.x - first_point_screen.x, g_box_origin.y - first_point_screen.y};
	/*                                               mouse delta                         */
	  g_box_origin.x = mouse.x + distance.x + (mouse.x - first_point_screen.x)*GetFrameTime();
	  g_box_origin.y = mouse.y + distance.y + (mouse.y - first_point_screen.y)*GetFrameTime();

	  first_point_screen.x = mouse.x;
	  first_point_screen.y = mouse.y;
      }



      moviment_speed = IsKeyDown(KEY_LEFT_CONTROL) ? 0.1 : 1.5/screen_width; /* Left control sets precise moviment */
      /* Moving box with keyboard */
      if(!IsKeyDown(KEY_LEFT_SHIFT)){
	if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)){
	  g_box_origin.y = g_box_origin.y - 1;
	  WaitTime(moviment_speed);
	}
	if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)){
	  g_box_origin.y = g_box_origin.y + 1;
	  WaitTime(moviment_speed);
	}
	if(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)){
	  g_box_origin.x = g_box_origin.x - 1;
	  WaitTime(moviment_speed);
	}
	if(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)){
	  g_box_origin.x = g_box_origin.x + 1;
	  WaitTime(moviment_speed);
	}
      }



      /* Shift enables zoom mode */
      if(IsKeyDown(KEY_LEFT_SHIFT)){
	zoom = IsKeyDown(KEY_LEFT_CONTROL) ? GetFrameTime()/10 : GetFrameTime();
	zoom_speed = IsKeyDown(KEY_LEFT_CONTROL) ? 0.1 : 0.01;

	int zoom_direction = 0;
	if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)){
	  zoom_direction = 1;
	}
	if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)){
	  zoom_direction = -1;
	}

	zoom *= zoom_direction;

	g_box_attr.x = max(1, g_box_attr.x + zoom*screen_width);
	g_box_attr.y = max(1, g_box_attr.y + zoom*screen_height);

	if(g_box_attr.x > 1 && g_box_attr.y > 1){
	  g_box_origin.x = g_box_origin.x - zoom*screen_width/2;
	  g_box_origin.y = g_box_origin.y - zoom*screen_height/2;
	}
	WaitTime(zoom_speed);
      }



      /* Checking the screen limits*/
      g_box_origin.x = max(-g_box_attr.x/4, g_box_origin.x);
      g_box_origin.y = max(-g_box_attr.y/4, g_box_origin.y);
      g_box_origin.x = min(g_box_origin.x, screen_width - g_box_attr.x + g_box_attr.x/4);
      g_box_origin.y = min(g_box_origin.y, screen_height - g_box_attr.y + g_box_attr.y/4);
    }



    /* Colors related keys */
    if(IsKeyPressed(KEY_SPACE)){
      swap_pallete();
      WaitTime(0.1);
    }
    if(IsKeyPressed(KEY_C)){
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

      if(back == true){
	back = false;
	payload_count--;
	*payload = payload_history[payload_count-1];
	payload->generation = generation++;
	actual_ll = payload->ll;
	actual_ur = payload->ur;

	if (payload_count > 0) {
	  payload_history = realloc(payload_history, payload_count * sizeof(payload_t));
	  if (payload_history == NULL && payload_count > 0) {
	    perror("Realloc failed");
	    }
	}
      }
      else{
      /* Generating the payload */
      payload->generation = generation++; /* The generation is always increasing */
      payload->granularity = g_granularity;
      payload->fractal_depth = g_depth;
      payload->ll.real = min(first_point_fractal.real, second_point_fractal.real);
      payload->ll.imag = min(first_point_fractal.imag, second_point_fractal.imag);
      payload->ur.real = max(first_point_fractal.real, second_point_fractal.real);
      payload->ur.imag = max(first_point_fractal.imag, second_point_fractal.imag);

      actual_ll = payload->ll;
      actual_ur = payload->ur;

      payload->s_ll.x = 0;
      payload->s_ll.y = 0;
      payload->s_ur.x = screen_width;
      payload->s_ur.y = screen_height;

      payload_history = realloc(payload_history, (payload_count + 1)*sizeof(payload_t));
      if(payload_history == NULL){
	perror("Malloc faild");
	pthread_exit(NULL);
      }

      payload_history[payload_count] = *payload;
      payload_count++;
    }

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
  /* This action is guided by the responses from the coordinator */

  static int generation = 0;

  while(!atomic_load(&shutdown_requested)) {
    response_t *response = (response_t *)queue_dequeue(&response_queue);
    if (response == NULL) break; // Poison pill

    if(response->payload.generation > generation){
      generation = response->payload.generation;
    }
    //    response_print(__func__, "dequeued response", response);
    update_pixels(response);
    
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
    if (payload == NULL) break; // Poison pill

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
  mallocate a response struct and receive it from the coordinator with
  recv_all; 2/ we mallocate the space needed to receive the values; 
  3/ we receive the values; 4/ and then we enqueue this response
  so the render_thread can do its work.
*/
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

    /* printf("(%d) %s: received response.\n", response->generation, __func__); */
    /* printf("\t[%d, %d]\n", */
    /*	   response->granularity, response->worker_id); */

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

  int connection = open_connection(argv[1], atoi(argv[2]));

  atomic_init(&shutdown_requested, 0);
  signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE (failed send)

  int screen_width;
  int screen_height;
  SetConfigFlags(FLAG_FULLSCREEN_MODE);
  InitWindow(0, 0, "Fractal @ PCAD");
  screen_width = GetScreenWidth();
  screen_height = GetScreenHeight();
  SetWindowSize(screen_width, screen_height);
  ToggleFullscreen();
  SetTargetFPS(60);

  g_pixel_depth = calloc(screen_width * screen_height, sizeof(int));
  
  // create a CPU-side "image" that we can draw on top when needed
  Image img = GenImageColor(screen_width, screen_height, RAYWHITE);
  g_pixels = LoadImageColors(img);
  g_worker_pixels = LoadImageColors(img);
  Texture2D texture_mandelbrot = LoadTextureFromImage(img);
  Texture2D texture_worker = LoadTextureFromImage(img);
  UnloadImage(img);

  pthread_mutex_init(&pixelMutex, NULL);
  srand(0);

  queue_init(&payload_queue, 1, free);
  queue_init(&response_queue, 256, free_response);

  pthread_t ui_thread = 0;
  pthread_t render_thread = 0;
  pthread_t payload_thread = 0;
  pthread_t response_thread = 0;

  pthread_create(&ui_thread, NULL, ui_thread_function, NULL);
  pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  while (!WindowShouldClose()) { // Closed with ESC or manually closing window
    // get the mutex so we can read safely from global pixel colors
    pthread_mutex_lock(&pixelMutex);
    if (g_pixels_changed) { // Only update if pixels changed
      UpdateTexture(texture_mandelbrot, g_pixels);
      UpdateTexture(texture_worker, g_worker_pixels);
    }
    pthread_mutex_unlock(&pixelMutex);

    // do the drawing (only here we actually do the drawing)
    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawTexture(texture_mandelbrot, 0, 0, WHITE);
    if(g_current_color == TOTAL_COLORS) DrawTexture(texture_worker, 0, 0, WHITE);
    // If overlaying workers on top, draw with alpha
    if(g_show_workers) DrawTexture(texture_worker, 0, 0, (Color){ 255, 255, 255, 128 });

    if(!g_selecting && g_show_changes_timer > 0){
      if(g_gchanged == true){
	DrawText(TextFormat("Granularity:  %d", g_granularity), screen_width/4, screen_height/2 - 50, 100, WHITE);
      }
      if(g_dchanged == true){
	DrawText(TextFormat("Depth:  %d", g_depth), screen_width/4, screen_height/2 + 50, 100, WHITE);
      }
      g_show_changes_timer -= GetFrameTime();
    } else {
      g_dchanged = false;
      g_gchanged = false;
    }
    
    if(g_selecting){
      DrawRectangleV(g_box_origin, g_box_attr, (Color){1.0f, 1.0f, 255.0f, 100.0f});
      DrawText(TextFormat("Granularity:  %d", g_granularity), 10, 10, 20, DARKGRAY);
      DrawText(TextFormat("Depth:  %d", g_depth), 10, 30, 20, DARKGRAY);
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
  UnloadTexture(texture_worker);

  UnloadImageColors(g_pixels);
  UnloadImageColors(g_worker_pixels);
  CloseWindow(); // Close OpenGL context

  free(g_pixel_depth);

  pthread_mutex_destroy(&pixelMutex);

  close(connection);

  queue_destroy(&payload_queue);
  queue_destroy(&response_queue);

  return 0;
}
