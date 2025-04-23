#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdint.h>

#define QUEUE_KEY 0x1234
#define MAX_QUEUE_SIZE 4
#define SEM_KEY 0x5678
#define DATA_ALIGNMENT 4

typedef struct {
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    uint8_t data[];
} Message;

typedef struct {
    Message* buffer[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int added_count;
    int removed_count;
    int free_slots;
    int sem_id;
} Queue;

Queue* queue_init();
void queue_push(Queue* q, Message* msg);
Message* queue_pop(Queue* q);
void queue_destroy(Queue* q);
uint16_t calculate_hash(Message* msg);

#endif
