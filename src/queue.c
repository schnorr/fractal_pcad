
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>

void queue_init(queue_t *q, size_t capacity, void (*free_function)(void *)) {
    // Extra slot needed to distinguish full and empty queue, allowing for capacity 1 queues
    q->buffer_size = capacity + 1;
    q->queue = malloc(sizeof(void*) * q->buffer_size);
    if (q->queue == NULL) {
        perror("malloc failed.");
        exit(1);
    }
    q->front = 0;
    q->back = 0;
    q->shutdown = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    q->free_function = free_function;
}

void queue_enqueue(queue_t *q, void *item) {
    pthread_mutex_lock(&q->mutex);

    while ((q->back + 1) % q->buffer_size == q->front && !q->shutdown) { // Queue at max, wait for dequeue
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    if (q->shutdown) {
        if (q->free_function){
            q->free_function(item);
        }
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    q->queue[q->back] = item; // shallow copy
    q->back = (q->back + 1) % q->buffer_size;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int queue_try_enqueue(queue_t *q, void *item) {
    pthread_mutex_lock(&q->mutex);

    if ((q->back + 1) % q->buffer_size == q->front) { // Queue at max, return 0
        pthread_mutex_unlock(&q->mutex);
        return 0;
    }

    q->queue[q->back] = item; // shallow copy
    q->back = (q->back + 1) % q->buffer_size;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}

void* queue_dequeue(queue_t *q) {
    pthread_mutex_lock(&q->mutex);

    while (q->front == q->back && !q->shutdown) { // Queue empty, wait for enqueue
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (q->shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return NULL; // Queue shutdown, return NULL
    }

    void *item = q->queue[q->front];
    q->front = (q->front + 1) % q->buffer_size;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return item;
}

void* queue_try_dequeue(queue_t *q) {
    pthread_mutex_lock(&q->mutex);

    if (q->front == q->back) { // Queue empty, return NULL
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    void *item = q->queue[q->front];
    q->front = (q->front + 1) % q->buffer_size;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return item;
}

size_t queue_size(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    size_t size = (q->back - q->front + q->buffer_size) % q->buffer_size;
    pthread_mutex_unlock(&q->mutex);
    return size;
}

void queue_clear(queue_t *q) {
    pthread_mutex_lock(&q->mutex);

    // Free the items still in the queue.
    // We don't free any other items potentially in **queue since there could be references outside
    // from a previous dequeue    
    while (q->front != q->back) {
        void *item = q->queue[q->front];
        if (q->free_function){
            q->free_function(item); // Free items using function
        }
        q->queue[q->front] = NULL;
        q->front = (q->front + 1) % q->buffer_size;
    }

    q->front = 0;
    q->back = 0;
    pthread_cond_broadcast(&q->not_full); // Broadcast to all waiting threads
    pthread_mutex_unlock(&q->mutex);
}

void queue_destroy(queue_t *q) {
    queue_clear(q);
    free(q->queue);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void queue_shutdown(queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    q->shutdown = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}