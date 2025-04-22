#include "fractal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

size_t response_serialize(response_t *response, uint8_t **buffer){
    size_t num_values = response->payload.granularity * response->payload.granularity;
    size_t size_values = sizeof(int) * num_values;
    size_t buffer_size = sizeof(response_t) + size_values;

    *buffer = malloc(buffer_size);
    if (*buffer == NULL) {
        perror("malloc failed.\n");
        exit(1);
    }

    memcpy(*buffer, response, sizeof(response_t)); // Copy response to buffer 
    memcpy(*buffer + sizeof(response_t), response->values, size_values); // Append values

    return buffer_size;
}

response_t* response_deserialize(uint8_t **data){
    response_t* response = malloc(sizeof(response_t));
    if (response == NULL) {
        perror("malloc failed.\n");
        exit(1);
    }
    
    memcpy(response, *data, sizeof(response_t)); // Copy base response (pointer is garbage)
    size_t num_values = response->payload.granularity * response->payload.granularity;
    size_t size_values = sizeof(int) * num_values;

    response->values = malloc(size_values); // Overwrite garbage pointer
    if (response->values == NULL) {
        perror("malloc failed.\n");
        exit(1);
    }

    memcpy(response->values, *data + sizeof(response_t), size_values); // Add values to pointer
    return response;
}

payload_t **discretize_payload (payload_t *origin, int *length)
{
  if (!origin || !length){
    return NULL;
  }
  int screen_width = origin->s_ur.x - origin->s_ll.x;
  int screen_height = origin->s_ur.y - origin->s_ll.y;

  int amount_x = (screen_width  + origin->granularity-1) / origin->granularity;
  int amount_y = (screen_height + origin->granularity-1) / origin->granularity;
  // This is the number of squared blocks we need to create
  *length = amount_x * amount_y;

  /* Define the fractal space we need to cover */
  double real_step = (origin->ur.real - origin->ll.real) / amount_x;
  double imag_step = (origin->ur.imag - origin->ll.imag) / amount_y;

  /* Define the (corresponding) screen space we need to cover */
  int x_step = screen_width / amount_x;
  int y_step = screen_height / amount_y;

  payload_t **ret = (payload_t**)calloc(*length, sizeof(payload_t*));
  int i, j, p = 0;
  for (i = 0; i < amount_x; i++){
    fractal_coord_t fractal_current = origin->ll;
    fractal_current.real += real_step * i;

    screen_coord_t screen_current = origin->s_ll;
    screen_current.x += x_step * i;

    for (j = 0; j < amount_y; j++){
      fractal_current.imag += imag_step * j;
      screen_current.y += y_step * j;

      ret[p] = (payload_t*) calloc(1, sizeof(payload_t));
      ret[p]->generation = origin->generation;
      ret[p]->granularity = origin->granularity;
      ret[p]->fractal_depth = origin->fractal_depth;

      ret[p]->ll = fractal_current;
      ret[p]->ur = fractal_current;
      ret[p]->ur.real += real_step;
      ret[p]->ur.imag += imag_step;

      ret[p]->s_ll = screen_current;
      ret[p]->s_ur = screen_current;
      ret[p]->s_ur.x += x_step;
      ret[p]->s_ur.y += y_step;

      payload_print(__func__, "discretized payload", ret[p]);

      //next discretized payload
      p++;
    }
  }
  return ret;
}

response_t *create_response_for_payload (payload_t *payload)
{
  if (!payload) return NULL;
  response_t *ret = malloc(sizeof(response_t));
  if (!ret) {
    return NULL;
  }
  ret->payload = *payload;
  ret->max_worker_id = 0; // TODO
  ret->worker_id = 0; // TODO
  int screen_width = payload->s_ur.x - payload->s_ll.x;
  int screen_height = payload->s_ur.y - payload->s_ll.y;

  ret->values = calloc((screen_width * screen_height) * // payload size
		       3,  // RGB colors
		       sizeof(unsigned short)); // space required for each signal
  return ret;
}

void payload_print (const char *func, const char *message, const payload_t *p)
{
  printf("(%d) %s: %s.\n", p->generation, func, message);
  printf("\t[%d, %d, f_ll(%6.3lf, %6.3lf) -> f_ur(%6.3lf, %6.3lf)]\n"
	 "\t[        s_ll(%6d, %6d) -> s_ur(%6d, %6d)]\n",
	 p->granularity, p->fractal_depth,
	 p->ll.real, p->ll.imag,
	 p->ur.real, p->ur.imag,
	 p->s_ll.x, p->s_ll.y,
	 p->s_ur.x, p->s_ur.y);
}

void response_print (const char *func, const char *message, const response_t *r)
{
  payload_print(func, message, &r->payload);
}
