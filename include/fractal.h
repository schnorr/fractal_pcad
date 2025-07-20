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
#ifndef __FRACTAL_H_
#define __FRACTAL_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* a fractal coordinate */
typedef struct {
  long double real;
  long double imag;
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

// Special poison pill payloads
#define PAYLOAD_GENERATION_DONE -1 // Signals to workers that the current round is done
#define PAYLOAD_GENERATION_SHUTDOWN -2 // Signals that the coordinator is shutting down

/* the response obtained from the coordinator */
typedef struct {
  payload_t payload; // the origin of this response
  int worker_id; // between [0, n-1]
  int max_worker_id; // maximum is n-1, n is the number of workers
  int *values; // there are granularity * granularity elements
} response_t;

typedef struct {
  response_t *response; 
  long long total_iterations;
} create_response_return_t;

void free_response(void* ptr); // custom free function for use in queue

/* discretized a payload in several pieces, block-wise */
payload_t **discretize_payload (payload_t *origin, int *length);

/* encapsulate a response for a given payload */
create_response_return_t create_response_for_payload (payload_t *payload);

/* print a payload */
void payload_print (const char *func, const char *message, const payload_t *p);

/* print a response */
void response_print (const char *func, const char *message, const response_t *p);
#endif
