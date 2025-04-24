#ifndef __FRACTAL_H_
#define __FRACTAL_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* a fractal coordinate */
typedef struct {
  double real;
  double imag;
} fractal_coord_t;

/* a screen coordinate (in pixels) */
typedef struct {
  int x;
  int y;
} screen_coord_t;

/* the payload sent to the coordinator at every user interaction */
typedef struct {
  int generation; // generation of the user interaction
  int granularity; // size of the squared blocks
  int fractal_depth; // the depth of the fractal
  
  fractal_coord_t ll; // lower-left corner
  fractal_coord_t ur; // upper-right corner

  screen_coord_t s_ll; // the screen lower-left corner
  screen_coord_t s_ur; // the screen upper-right corner
} payload_t;

/* the response obtained from the coordinator */
typedef struct {
  payload_t payload; // the origin of this response
  int worker_id; // between [0, n-1]
  int max_worker_id; // maximum is n-1, n is the number of workers
  int *values; // there are granularity * granularity elements
} response_t;

/* serializes response into a buffer to send over TCP connection
   response object is followed by response->values
   returns size of the buffer, in bytes */
size_t response_serialize(response_t *response, uint8_t **buffer);
/* deserializes buffer into response object. Must free later. */
response_t* response_deserialize(uint8_t **data);

void free_response(void* ptr); // custom free function for use in queue

/* discretized a payload in several pieces, block-wise */
payload_t **discretize_payload (payload_t *origin, int *length);

/* encapsulate a response for a given payload */
response_t *create_response_for_payload (payload_t *payload);

/* print a payload */
void payload_print (const char *func, const char *message, const payload_t *p);

/* print a response */
void response_print (const char *func, const char *message, const response_t *p);
#endif
