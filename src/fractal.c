#include "fractal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

size_t response_serialize(response_t *response, uint8_t **buffer){
    size_t num_values = response->granularity * response->granularity;
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
    size_t num_values = response->granularity * response->granularity;
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
  int amount_x = (origin->screen.width  + origin->granularity-1) / origin->granularity;
  int amount_y = (origin->screen.height + origin->granularity-1) / origin->granularity;
  *length = amount_x * amount_y;

  payload_t **ret = (payload_t**)calloc(*length, sizeof(payload_t*));
  int i, j, p = 0;
  for (i = 0; i < amount_x; i++){
    for (j = 0; j < amount_y; j++){
      ret[p] = (payload_t*) calloc(1, sizeof(payload_t));
      ret[p]->generation = origin->generation;
      ret[p]->granularity = origin->granularity;
      ret[p]->fractal_depth = origin->fractal_depth;
      ret[p]->ll = origin->ll; // TODO
      ret[p]->ur = origin->ur; // TODO
      ret[p]->screen.width = origin->granularity;
      ret[p]->screen.height = origin->granularity;

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
  ret->generation = payload->generation;
  ret->granularity = payload->granularity;
  ret->ll.x = 0; // TODO
  ret->ll.y = 0; // TODO
  ret->max_worker_id = 0; // TODO
  ret->worker_id = 0; // TODO
  ret->values = calloc((payload->screen.width * payload->screen.height) * // payload size
		       3,  // RGB colors
		       sizeof(unsigned short)); // space required for each signal
  return ret;
}
