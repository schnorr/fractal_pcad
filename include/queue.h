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
#ifndef __QUEUE_H_
#define __QUEUE_H_

#include <pthread.h>

/* queue is generic, so receives and returns void*. memory management has to be done on 
   caller, with a malloc before enqueue and free after dequeue.
   queue is also unbounded, doubling in size when the limit is reached. */
typedef struct queue {
  void **queue;
  size_t buffer_size; // buffer size is capacity + 1 (to distinguish full and empty)
  size_t front;
  size_t back;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  void (*free_function)(void *); // destructor for items.
} queue_t;

void queue_init(queue_t *q, size_t starting_capacity, void (*free_function)(void *));

/* Queue takes ownership of the pointer. If size limit reached, doubles the size of the queue. */
void queue_enqueue(queue_t *q, void *item);

/* Blocking dequeue. Thread will be signaled when it can dequeue. 
   Returns a NULL pointer on shutdown. */
void* queue_dequeue(queue_t *q);

/* Non-blocking dequeue. Returns NULL if queue is empty. */
void* queue_try_dequeue(queue_t *q);

size_t queue_size(queue_t *q);
void queue_clear(queue_t *q);
void queue_destroy(queue_t *q);

#endif 