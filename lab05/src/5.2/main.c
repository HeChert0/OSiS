#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include "queue.h"

#define MAX_PRODS 6

volatile sig_atomic_t running = 1;
pthread_t producers[MAX_PRODS], consumers[MAX_PRODS];
int prod_count = 0, cons_count = 0;
Queue* queue;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

void* producer_task(void* arg) {
    (void)arg;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    srand((unsigned)time(NULL) ^ (uintptr_t)pthread_self());

    while (running) {
        Message* msg = malloc(sizeof(Message));
        msg->type = (uint8_t)(rand() % 256);
        msg->size = (uint8_t)(rand() % 256 + 1);
        for (int i = 0; i < msg->size; ++i)
            msg->data[i] = (uint8_t)(rand() % 256 + 1);
        msg->hash = calculate_hash(msg);

        if (queue_push(queue, msg) == -1) {
            free(msg);
            break;
        }
        printf("[Producer %lu] Added |type:%u hash:%u size:%u| Total:%d\n",
               pthread_self(),
               msg->type, msg->hash, msg->size,
               queue->added_count);

        sleep(1 + rand() % 2);
    }
    return NULL;
}

void* consumer_task(void* arg) {
    (void)arg;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (running) {
        Message* msg = queue_pop(queue);
        if (!msg) break;
        printf("[Consumer %lu] Removed |type:%u hash:%u size:%u| Total:%d\n",
               pthread_self(),
               msg->type, msg->hash, msg->size,
               queue->removed_count);

        uint16_t expected = calculate_hash(msg);
        if (expected != msg->hash) {
            fprintf(stderr, "[Consumer %lu] Hash mismatch: expected %hu, got %hu\n",
                    pthread_self(), expected, msg->hash);
        }
        free(msg);
        sleep(2 + rand() % 3);
    }
    return NULL;
}

void create_producer() {
    if (prod_count >= MAX_PRODS) {
        printf("Max producers reached!\n");
        return;
    }
    pthread_create(&producers[prod_count], NULL, producer_task, NULL);
    printf("[Main] Producer thread created (id=%lu)\n", producers[prod_count]);
    prod_count++;
}

void create_consumer() {
    if (cons_count >= MAX_PRODS) {
        printf("Max consumers reached!\n");
        return;
    }
    pthread_create(&consumers[cons_count], NULL, consumer_task, NULL);
    printf("[Main] Consumer thread created (id=%lu)\n", consumers[cons_count]);
    cons_count++;
}

void remove_last_producer() {
    if (prod_count == 0) return;

    pthread_mutex_lock(&queue->mutex);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    pthread_cancel(producers[--prod_count]);
    pthread_join(producers[prod_count], NULL);
    printf("[Main] Producer thread removed (id=%lu)\n", producers[prod_count]);
}

void remove_last_consumer() {
    if (cons_count == 0) return;

    pthread_mutex_lock(&queue->mutex);
    pthread_cond_broadcast(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    pthread_cancel(consumers[--cons_count]);
    pthread_join(consumers[cons_count], NULL);
    printf("[Main] Consumer thread removed (id=%lu)\n", consumers[cons_count]);
}

void remove_all() {
    pthread_mutex_lock(&queue->mutex);
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    while (prod_count > 0) remove_last_producer();
    while (cons_count > 0) remove_last_consumer();
}


void print_status() {
    pthread_mutex_lock(&queue->mutex);
    int used = queue->count;
    int cap  = queue->capacity;
    pthread_mutex_unlock(&queue->mutex);

    printf("\n--- Status ---\n");
    printf("Queue: %d/%d (used/free)\n", used, cap - used);
    printf("Messages: added=%d, removed=%d\n",
           queue->added_count, queue->removed_count);
    printf("Active producers: %d\n", prod_count);
    printf("Active consumers: %d\n\n", cons_count);
}

int main() {
    signal(SIGINT, handle_signal);

    queue = queue_init();

    printf("Controls:\n"
    "  p - Add producer\n"
    "  c - Add consumer\n"
    "  P - Remove last producer\n"
    "  C - Remove last consumer\n"
    "  + - Increase queue size\n"
    "  - - Decrease queue size\n"
    "  k - Kill all threads\n"
    "  s - Show status\n"
    "  q - Quit\n");

    while (running) {
        char cmd;
        if (scanf(" %c", &cmd) != 1) break;
        switch (cmd) {
            case 'p': create_producer();               break;
            case 'c': create_consumer();               break;
            case 'P': remove_last_producer();          break;
            case 'C': remove_last_consumer();          break;
            case '+': queue_resize_increase(queue);    break;
            case '-': queue_resize_decrease(queue);    break;
            case 'k': remove_all();                    break;
            case 's': print_status();                  break;
            case 'q': running = 0;                     break;
            default:  printf("Unknown command\n");
        }
    }
    pthread_mutex_lock(&queue->mutex);
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);


    remove_all();
    queue_destroy(queue);
    printf("Program terminated\n");
    return 0;
}
