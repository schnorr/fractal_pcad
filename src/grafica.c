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
// Using float for smooth granularity and depth input, cast to int when actually used
float g_granularity = 10;
bool g_gchanged = false;

float g_depth = 256;
bool g_dchanged = false;

float g_show_changes_timer = 0;

/* Selection box (blue box) related globals */
bool g_selecting = false;
Rectangle g_box = {0, 0, 0, 0};

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
  handle_input: Handles user input. Every time a user selects
  a new area to compute, a payload must be created with a
  new generation. This payload must then be queued in the
  payload_queue using the queue_enqueue function.
*/
void handle_input() {
  /* This action is guided by the user */
  static int generation = 0;
  static fractal_coord_t actual_ll = {-2, -1.5};
  static fractal_coord_t actual_ur = {2, 1.5};

  static float zoom = 0.0f;

  static fractal_coord_t first_point_fractal = {0, 0};
  static fractal_coord_t second_point_fractal = {0, 0};

  static bool interaction = false;
  static bool initial = true;
  static bool back = false;

  long double screen_width = (long double) GetScreenWidth();
  long double screen_height = (long double) GetScreenHeight();
  long double pixel_coord_ratio = (actual_ur.real - actual_ll.real) / screen_width;

  float dt = GetFrameTime();

  if(initial){ /* Send the initial payload when the window gets ready */
    actual_ur.imag = ((actual_ur.real - actual_ll.real) * (screen_height/screen_width))/2;
    actual_ll.imag = actual_ur.imag * -1;

    g_box = (Rectangle){.x = 0, .y = 0, .width = screen_width, .height = screen_height};

    initial = false;
    interaction = true;
  }

  /* Starting selecting by pressing enter */
  if(IsKeyPressed(KEY_ENTER)){
    if (g_selecting) {
      g_selecting = false;
      interaction = true;
    } else {
      g_selecting = true;
      g_box = (Rectangle){
        .x = screen_width/4, 
        .y = screen_height/4, 
        .width = screen_width/2, 
        .height = screen_height/2
      };
    }
  }
  if(IsKeyPressed(KEY_BACKSPACE)){
    g_selecting = false;
  }

  /* add or sub fractal depth */
  if(IsKeyDown(KEY_P)){
    // Increase depth based on frame time, scaling faster the higher it is
    float change = g_depth * dt;
    if(IsKeyDown(KEY_MINUS)){
	    g_dchanged = true;
	    g_show_changes_timer = 1.0f;
	    g_depth -= change;
    }
    if(IsKeyDown(KEY_EQUAL)){
	    g_dchanged = true;
	    g_show_changes_timer = 1.0f;
	    g_depth += change;
    }
    g_depth = min(g_depth, MAX_DEPTH);
    g_depth = max(g_depth, 2);
  }

  /* add or sub granularity  */
  if(IsKeyDown(KEY_G)){
    float change = g_granularity * dt;
    if(IsKeyDown(KEY_MINUS)){
	    g_gchanged = true;
	    g_show_changes_timer = 1.0f;
	    g_granularity -= change;
    }
    if(IsKeyDown(KEY_EQUAL)){
	    g_gchanged = true;
	    g_show_changes_timer = 1.0f;
      g_granularity += change;
    }
    g_granularity = min(g_granularity, 500);
    g_granularity = max(g_granularity, 1);
  }

  /* Go back with C-z */
  if(IsKeyPressed(KEY_Z) && IsKeyDown(KEY_LEFT_CONTROL)){
    // Is 1, because gen 0, is the 0 position
    if(payload_count > 1){
	    back = true;
	    interaction = true;
    }
  }

  /* Zoom with mouse */
  if(GetMouseWheelMove()){
    zoom = dt * GetMouseWheelMove() * 3.0;

    g_box.width = max(1, g_box.width - zoom*screen_width);
    g_box.height = max(1, g_box.height - zoom*screen_height);

    if(g_box.width > 1 && g_box.height > 1){
	    g_box.x = g_box.x + zoom*screen_width/2;
	    g_box.y = g_box.y + zoom*screen_height/2;
    }
  }

  /* Selection box related */
  if(g_selecting){
    /* Mouse interaction */
    Vector2 mouse = GetMousePosition();
    if(mouse.x > g_box.x && mouse.y > g_box.y &&
	    mouse.x < g_box.x + g_box.width && mouse.y < g_box.y + g_box.height){
	    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
        Vector2 delta = GetMouseDelta();
	      g_box.x += delta.x;
	      g_box.y += delta.y;
	    }
    }

    /* Left control sets precise moviment */
    float movement_speed = IsKeyDown(KEY_LEFT_CONTROL) ? screen_width / 10 : screen_width / 3;
    /* Moving box with keyboard */
    if(!IsKeyDown(KEY_LEFT_SHIFT)){
	    if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)){
	      g_box.y = g_box.y - movement_speed * dt;
	    }
	    if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)){
	      g_box.y = g_box.y + movement_speed * dt;
	    }
	    if(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)){
	      g_box.x = g_box.x - movement_speed * dt;
	    }
	    if(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)){
	      g_box.x = g_box.x + movement_speed * dt;
	    }
    }

    /* Shift enables zoom mode */
    if(IsKeyDown(KEY_LEFT_SHIFT)){
	    zoom = IsKeyDown(KEY_LEFT_CONTROL) ? dt / 10 : dt;

	    int zoom_direction = 0;
	    if(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)){
	      zoom_direction = 1;
	    }
	    if(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)){
	      zoom_direction = -1;
	    }

	    zoom *= zoom_direction;

	    g_box.width = max(1, g_box.width + zoom*screen_width);
	    g_box.height = max(1, g_box.height + zoom*screen_height);

	    if(g_box.width > 1 && g_box.height > 1){
	      g_box.x = g_box.x - zoom * screen_width/2;
	      g_box.y = g_box.y - zoom * screen_height/2;
	    }
    }

    /* Checking the screen limits*/
    g_box.x = max(-g_box.width/4, g_box.x);
    g_box.y = max(-g_box.height/4, g_box.y);
    g_box.x = min(g_box.x, screen_width - g_box.width + g_box.width/4);
    g_box.y = min(g_box.y, screen_height - g_box.height + g_box.height/4);
  }

  /* Colors related keys */
  if(IsKeyPressed(KEY_SPACE)){
    swap_pallete();
  }
  if(IsKeyPressed(KEY_C)){
    g_show_workers = !g_show_workers;
  }

  if(interaction == true){
    interaction = false;

    payload_t *payload = calloc(1, sizeof(payload_t));
    if (payload == NULL) {
	    fprintf(stderr, "malloc failed.\n");
      exit(1);
    }

    /* ratio from pixels to coordinates based on the x axis */
    pixel_coord_ratio = (actual_ur.real - actual_ll.real)/screen_width;

    /* Transforming from screen coordinates to fractal coordinates */
    first_point_fractal.real = (g_box.x)*pixel_coord_ratio + actual_ll.real;
    first_point_fractal.imag = (g_box.y)*pixel_coord_ratio + actual_ll.imag;

    second_point_fractal.real = (g_box.x + g_box.width)*pixel_coord_ratio + actual_ll.real;
    second_point_fractal.imag = (g_box.y + g_box.height)*pixel_coord_ratio + actual_ll.imag;

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
	        fprintf(stderr, "Realloc failed.\n");
          exit(1);
	      }
	    }
    } else {
      /* Generating the payload */
      payload->generation = generation++; /* The generation is always increasing */
      payload->granularity = (int) g_granularity;
      payload->fractal_depth = (int) g_depth;
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
	      fprintf(stderr, "malloc failed.\n");
        exit(1);
      }

      payload_history[payload_count] = *payload;
      payload_count++;
    }

    payload_print(__func__, "Enqueueing payload", payload);
    queue_enqueue(&payload_queue, payload);
    payload = NULL;
  }
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

  pthread_t render_thread = 0;
  pthread_t payload_thread = 0;
  pthread_t response_thread = 0;

  pthread_create(&render_thread, NULL, render_thread_function, NULL);
  pthread_create(&payload_thread, NULL, net_thread_send_payload, &connection);
  pthread_create(&response_thread, NULL, net_thread_receive_response, &connection);

  while (!WindowShouldClose()) { // Closed with ESC or manually closing window
    handle_input(); // Handle user input

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

    if(!g_selecting && g_show_changes_timer > 0.0f){
      if(g_gchanged == true){
	      DrawText(TextFormat("Granularity:  %d", (int)g_granularity), screen_width/4, screen_height/2 - 50, 100, WHITE);
      }
      if(g_dchanged == true){
	      DrawText(TextFormat("Depth:  %d", (int)g_depth), screen_width/4, screen_height/2 + 50, 100, WHITE);
      }
      g_show_changes_timer -= GetFrameTime();
    } else {
      g_dchanged = false;
      g_gchanged = false;
    }
    
    if(g_selecting){
      DrawRectangleRec(g_box, (Color){1.0f, 1.0f, 255.0f, 100.0f});
      DrawText(TextFormat("Granularity:  %d", (int)g_granularity), 10, 10, 20, DARKGRAY);
      DrawText(TextFormat("Depth:  %d", (int)g_depth), 10, 30, 20, DARKGRAY);
    }

    EndDrawing();
  }

  // Window closed, request shutdown
  request_shutdown(connection);

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
