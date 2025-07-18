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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fractal.h"
#include "mandelbrot.h"

void free_response(void* ptr) {
  response_t *response = (response_t*) ptr; 
  free(response->values);
  free(response);
}

#ifdef EMBARALHAR
// Função para embaralhar o vetor
static void embaralhar(payload_t **vetor, int tamanho) {
    for (int i = tamanho - 1; i > 0; i--) {
        int j = rand() % (i + 1); // índice aleatório de 0 até i
        // troca vetor[i] com vetor[j]
        payload_t *temp = vetor[i];
        vetor[i] = vetor[j];
        vetor[j] = temp;
    }
}
#endif

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
  double x_ratio = (origin->ur.real - origin->ll.real) / screen_width;
  double y_ratio = (origin->ur.imag - origin->ll.imag) / screen_height;
  double real_step = origin->granularity * x_ratio;
  double imag_step = origin->granularity * y_ratio;

  /* Define the (corresponding) screen space we need to cover */
  int x_step = origin->granularity;
  int y_step = origin->granularity;

  payload_t **ret = (payload_t**)calloc(*length, sizeof(payload_t*));
  int i, j, p = 0;
  for (i = 0; i < amount_x; i++){
    for (j = 0; j < amount_y; j++){
      fractal_coord_t fractal_current = origin->ll;
      fractal_current.real += real_step * i;
      fractal_current.imag += imag_step * j;

      screen_coord_t screen_current = origin->s_ll;
      screen_current.x += x_step * i;
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

#ifdef PAYLOAD_DEBUG
      payload_print(__func__, "discretized payload", ret[p]);
#endif

      //next discretized payload
      p++;
    }
  }
#ifdef EMBARALHAR
  embaralhar(ret, *length);
#endif
  return ret;
}

create_response_return_t create_response_for_payload (payload_t *payload)
{
  if (!payload) return (create_response_return_t) {0};
  response_t *ret = calloc(1, sizeof(response_t));
  if (!ret) {
    return (create_response_return_t) {0};
  }
  ret->payload = *payload;
  int screen_width = payload->s_ur.x - payload->s_ll.x;
  int screen_height = payload->s_ur.y - payload->s_ll.y;

  /* Define the fractal space we need to cover */
  double real_step = (payload->ur.real - payload->ll.real) / screen_width;
  double imag_step = (payload->ur.imag - payload->ll.imag) / screen_height;

  ret->values = calloc((screen_width * screen_height), // payload size
		       sizeof(int)); // space required for each signal

  // TODO: sequencial solution for now
  //  payload_print(__func__, "compute", payload);
  long long total_iterations = 0;
  int r = 0;
  for (int y = 0; y < screen_height; y++){
    for (int x = 0; x < screen_width; x++){
      fractal_coord_t fractal_current = payload->ll;
      fractal_current.imag += imag_step * y;
      fractal_current.real += real_step * x;

      ret->values[r] =
	mandelbrot (fractal_current.real,
		    fractal_current.imag,
		    ret->payload.fractal_depth);
      total_iterations += ret->values[r];
      r++;
    }
  }
  return (create_response_return_t) {
    .response = ret,
    .total_iterations = total_iterations
  };
}

void payload_print (const char *func, const char *message, const payload_t *p)
{
  printf("(%d) %s: %s.\n", p->generation, func, message);
  printf("\t[%d, %d, f_ll(%6.3Lf, %6.3Lf) -> f_ur(%6.3Lf, %6.3Lf)]\n"
	 "\t[        s_ll(%6d, %6d) -> s_ur(%6d, %6d)]\n",
	 p->granularity, p->fractal_depth,
	 p->ll.real, p->ll.imag,
	 p->ur.real, p->ur.imag,
	 p->s_ll.x, p->s_ll.y,
	 p->s_ur.x, p->s_ur.y);
}

long long compute_total_load (const response_t *r)
{
  int number_of_values = r->payload.granularity * r->payload.granularity;
  long long total_cost = 0;
  for (int i = 0; i < number_of_values; i++){
    total_cost += r->values[i];
  }
  return total_cost;
}

void response_print (const char *func, const char *message, const response_t *r)
{
  payload_print(func, message, &r->payload);
  //compute cost of this response
  long long total_cost = compute_total_load (r);
  printf("\tTotal cost: %lld\n", total_cost);
}
