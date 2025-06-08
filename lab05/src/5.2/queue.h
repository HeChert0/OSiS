#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <pthread.h>

#define INITIAL_QUEUE_CAPACITY 4

typedef struct {
    uint8_t  type;
    uint16_t hash;
    uint8_t  size;
    uint8_t  data[260];
} Message;

typedef struct {
    Message**       buffer;
    int             capacity;
    int             head, tail;
    int             count;
    int             added_count;
    int             removed_count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} Queue;

Queue*   queue_init();
void     queue_destroy(Queue* q);
int     queue_push(Queue* q, Message* msg);
Message* queue_pop (Queue* q);
void cleanup_mutex(void *m);
void     queue_resize_increase(Queue* q);
void     queue_resize_decrease(Queue* q);
uint16_t calculate_hash(Message* msg);

#endif
