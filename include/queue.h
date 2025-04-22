#ifndef __QUEUE_H_
#define __QUEUE_H_

#include <pthread.h>

/* queue is generic, so receives and returns void*. memory management has to be done on 
   caller, with a malloc before enqueue and free after dequeue.*/
typedef struct queue {
  void **queue;
  size_t buffer_size; // buffer size is capacity + 1 (to distinguish full and empty)
  size_t front;
  size_t back;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  pthread_cond_t not_full;
  void (*free_function)(void *); // destructor for items.
} queue_t;

void queue_init(queue_t *q, size_t capacity, void (*free_function)(void *));
/* Blocking enqueue. Thread will be signaled when it can enqueue. */
void queue_enqueue(queue_t *q, void *item);
/* Non-blocking enqueue. Returns 1 on success, 0 on failure. */
int queue_try_enqueue(queue_t *q, void *item);
/* Blocking dequeue. Thread will be signaled when it can dequeue. */
void* queue_dequeue(queue_t *q);
/* Non-blocking dequeue. Returns NULL on failure. */
void* queue_try_dequeue(queue_t *q);
size_t queue_size(queue_t *q);
void queue_clear(queue_t *q);
void queue_destroy(queue_t *q);

#endif 