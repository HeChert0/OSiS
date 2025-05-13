#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

Queue* queue_init() {
    Queue* q = malloc(sizeof(Queue));
    if (!q) {
        perror("malloc queue");
        exit(1);
    }

    q->capacity    = INITIAL_QUEUE_CAPACITY;
    q->buffer      = malloc(q->capacity * sizeof(Message*));
    q->head = q->tail = q->count = 0;
    q->added_count = q->removed_count = 0;

    pthread_mutex_init(&q->mutex, NULL);
    sem_init(&q->sem_empty, 0, q->capacity);
    sem_init(&q->sem_full,  0, 0);
    return q;
}

void queue_push(Queue* q, Message* msg) {
    sem_wait(&q->sem_empty);
    pthread_mutex_lock(&q->mutex);

    q->buffer[q->tail] = msg;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    q->added_count++;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->sem_full);
}

Message* queue_pop(Queue* q) {
    sem_wait(&q->sem_full);
    pthread_mutex_lock(&q->mutex);

    Message* msg = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    q->removed_count++;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->sem_empty);
    return msg;
}

void queue_resize_increase(Queue* q) {
    pthread_mutex_lock(&q->mutex);

    int oldcap = q->capacity;
    int newcap = oldcap + 1;
    Message** newbuf = malloc(newcap * sizeof(Message*));

    for (int i = 0; i < q->count; ++i)
        newbuf[i] = q->buffer[(q->head + i) % oldcap];
    free(q->buffer);

    q->buffer   = newbuf;
    q->capacity = newcap;
    q->head = 0; q->tail = q->count;

    pthread_mutex_unlock(&q->mutex);
    sem_post(&q->sem_empty);
    printf("[Main] Queue increased: capacity = %d\n", newcap);
}

void queue_resize_decrease(Queue* q) {
    pthread_mutex_lock(&q->mutex);
    if (q->capacity <= 1) {
        printf("[Main] Cannot decrease: already at minimum capacity = 1\n");
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    if (q->count == q->capacity) {
        printf("[Main] Cannot decrease: queue is full (%d/%d). Remove at least one message first.\n",
        q->count, q->capacity);
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    int oldcap = q->capacity;
    int newcap = oldcap - 1;
    Message** newbuf = malloc(newcap * sizeof(Message*));

    for (int i = 0; i < q->count; ++i) {
        newbuf[i] = q->buffer[(q->head + i) % oldcap];
    }
    free(q->buffer);

    q->buffer = newbuf;
    q->capacity = newcap;
    q->head = 0;
    q->tail = q->count;

    pthread_mutex_unlock(&q->mutex);
    sem_wait(&q->sem_empty);
    printf("[Main] Queue decreased: capacity = %d\n", newcap);
}

void queue_destroy(Queue* q) {
    pthread_mutex_lock(&q->mutex);

    while (q->count > 0) {
        Message* msg = q->buffer[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;
        free(msg);
    }

    pthread_mutex_unlock(&q->mutex);

    pthread_mutex_destroy(&q->mutex);
    sem_destroy(&q->sem_empty);
    sem_destroy(&q->sem_full);

    free(q->buffer);
    free(q);
}

uint16_t calculate_hash(Message* msg) {
    uint16_t hash = 0;
    hash += msg->type;
    hash += msg->size;

    for (int i = 0; i < msg->size; ++i)
        hash += msg->data[i];

    return hash;
}

