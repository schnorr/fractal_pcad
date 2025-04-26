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
  int shutdown;
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

/* Function intended to clean up the program when exiting. Once called, threads blocked at
   enqueue/dequeue will be signaled and will return. Enqueuing threads will have their enqueued 
   item cleaned up, and dequeuing threads will return NULL . */
void queue_shutdown(queue_t *q);

#endif 