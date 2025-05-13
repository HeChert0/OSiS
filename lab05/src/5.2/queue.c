#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

extern volatile sig_atomic_t running;


Queue* queue_init() {
    Queue* q = malloc(sizeof(*q));
    if (!q) { perror("malloc queue"); exit(1); }

    q->capacity      = INITIAL_QUEUE_CAPACITY;
    q->buffer        = malloc(q->capacity * sizeof(Message*));
    q->head = q->tail = q->count = 0;
    q->added_count   = q->removed_count = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init (&q->not_full,  NULL);
    pthread_cond_init (&q->not_empty, NULL);

    return q;
}

int queue_push(Queue* q, Message* msg) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    pthread_mutex_lock(&q->mutex);
    while (q->count == q->capacity && running)
        pthread_cond_wait(&q->not_full, &q->mutex);

    if (!running) {
        pthread_mutex_unlock(&q->mutex);
        pthread_setcancelstate(oldstate, NULL);
        return -1;
    }

    q->buffer[q->tail] = msg;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    q->added_count++;
    pthread_cond_signal(&q->not_empty);

    pthread_mutex_unlock(&q->mutex);
    pthread_setcancelstate(oldstate, NULL);
    return 0;
}


Message* queue_pop(Queue* q) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    pthread_mutex_lock(&q->mutex);
    while (q->count == 0 && running)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    if (!running && q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        pthread_setcancelstate(oldstate, NULL);
        return NULL;
    }

    Message* msg = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    q->removed_count++;
    pthread_cond_signal(&q->not_full);

    pthread_mutex_unlock(&q->mutex);
    pthread_setcancelstate(oldstate, NULL);
    return msg;
}


void queue_resize_increase(Queue* q) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    pthread_mutex_lock(&q->mutex);

    int oldcap = q->capacity;
    int newcap = oldcap + 1;
    Message** newbuf = malloc(newcap * sizeof(Message*));

    for (int i = 0; i < q->count; ++i)
        newbuf[i] = q->buffer[(q->head + i) % oldcap];

    free(q->buffer);
    q->buffer   = newbuf;
    q->capacity = newcap;
    q->head = 0;
    q->tail = q->count;

    pthread_cond_broadcast(&q->not_full);

    pthread_mutex_unlock(&q->mutex);
    pthread_setcancelstate(oldstate, NULL);
    printf("[Main] Queue increased: capacity = %d\n", newcap);
}

void queue_resize_decrease(Queue* q) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    pthread_mutex_lock(&q->mutex);

    if (q->capacity <= 1) {
        printf("[Main] Cannot decrease: minimum capacity = 1\n");
        pthread_mutex_unlock(&q->mutex);
        pthread_setcancelstate(oldstate, NULL);
        return;
    }

    int oldcap = q->capacity;
    int newcap = oldcap - 1;

    if (q->count >= newcap) {
        printf("[Main] Cannot decrease: %d elements ≥ new capacity %d. Remove some first.\n",
               q->count, newcap);
        pthread_mutex_unlock(&q->mutex);
        pthread_setcancelstate(oldstate, NULL);
        return;
    }

    Message** newbuf = malloc(newcap * sizeof(Message*));
    for (int i = 0; i < q->count; ++i) {
        newbuf[i] = q->buffer[(q->head + i) % oldcap];
    }
    free(q->buffer);

    q->buffer   = newbuf;
    q->capacity = newcap;
    q->head     = 0;
    q->tail     = q->count;

    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    pthread_setcancelstate(oldstate, NULL);

    printf("[Main] Queue decreased: capacity = %d\n", newcap);
}


void queue_destroy(Queue* q) {
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    pthread_mutex_lock(&q->mutex);

    while (q->count > 0) {
        Message* msg = q->buffer[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;
        free(msg);
    }
    pthread_mutex_unlock(&q->mutex);
    pthread_setcancelstate(oldstate, NULL);

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    free(q->buffer);
    free(q);
}

uint16_t calculate_hash(Message* msg) {
    uint16_t h = 0;
    h += msg->type;
    h += msg->size;
    for (int i = 0; i < msg->size; ++i)
        h += msg->data[i];
    return h;
}
