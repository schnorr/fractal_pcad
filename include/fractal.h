#ifndef __FRACTAL_H_
#define __FRACTAL_H_

#include <stdint.h>
#include <stdlib.h>

/* a fractal coordinate */
typedef struct {
  float x;
  float y;
} fractal_coordinate_t;

/* a screen coordinate (in pixels) */
typedef struct {
  int x;
  int y;
} screen_coordinate_t;

/* the payload sent to the coordinator at every user interaction */
typedef struct {
  int generation; // generation of the user interaction
  int granularity; // size of the square blocks
  int fractal_depth; // the depth of the fractal
  
  fractal_coordinate_t ll; // lower-left corner 
  fractal_coordinate_t ur; // upper-right corner
  int screen_width;
  int screen_height;
} payload_t;

/* the response obtained from the coordinator */
typedef struct {
  int generation;
  int granularity;
  int worker_id; // between [0, n-1]
  int max_worker_id; // maximum is n-1, n is the number of workers
  
  screen_coordinate_t ll;
  int *values; // there are granularity * granularity elements
} response_t;

/* serializes response into a buffer to send over TCP connection
   response object is followed by response->values
   returns size of the buffer, in bytes */
size_t response_serialize(response_t *response, uint8_t **buffer);
/* deserializes buffer into response object. Must free later. */
response_t* response_deserialize(uint8_t **data);

#endif
