#ifndef QUEUE_H
#define QUEUE_H

#define _GNU_SOURCE
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

#define INITIAL_QUEUE_CAPACITY 4

typedef struct {
    uint8_t  type;
    uint16_t hash;
    uint8_t  size;
    uint8_t  data[260];
} Message;

typedef struct {
    Message**    buffer;
    int          capacity;
    int          head;
    int          tail;
    int          count;
    int          added_count;
    int          removed_count;
    pthread_mutex_t mutex;
    sem_t        sem_empty;
    sem_t        sem_full;
} Queue;

Queue*   queue_init();
int     queue_push(Queue* q, Message* msg);
Message* queue_pop(Queue* q);
void     queue_resize_increase(Queue* q);
void     queue_resize_decrease(Queue* q);
void     queue_destroy(Queue* q);
uint16_t calculate_hash(Message* msg);

#endif
