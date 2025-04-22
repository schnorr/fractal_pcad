#ifndef __QUEUE_H_
#define __QUEUE_H_

#include "fractal.h"
#include <pthread.h>

/* queue is generic, so receives and returns void*. memory management has to be done on 
   caller, with a malloc before enqueue and free after dequeue.*/
typedef struct queue {
    void **queue;
    size_t buffer_size; // buffer size is capacity + 1
    size_t front;
    size_t back;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} queue_t;

void queue_init(queue_t *q, size_t capacity);
void queue_enqueue(queue_t *q, void *item);
void* queue_dequeue(queue_t *q);
size_t queue_size(queue_t *q);

/* TODO: currently, clear() and destroy() below can leak memory if queue contains pointers to 
   dynamically allocated data. Not necessarily an issue, as we're only clearing a queue with 
   payloads at the moment. Maybe solved by passing a cleanup function? */
void queue_clear(queue_t *q);
void queue_destroy(queue_t *q);

#endif 