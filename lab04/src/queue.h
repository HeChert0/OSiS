#ifndef QUEUE_H
#define QUEUE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define QUEUE_KEY       0x1234
#define MAX_QUEUE_SIZE  10
#define SEM_KEY         0x5678

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint16_t hash;
    uint8_t size;
    uint8_t data[260];
} Message;

typedef struct {
    Message   buffer[MAX_QUEUE_SIZE];
    int       head;
    int       tail;
    int       added_count;
    int       removed_count;
    int       free_slots;
    int       sem_id;
} Queue;

Queue*   queue_init();
void     queue_push(Queue* q, const Message* msg);
void     queue_pop(Queue* q, Message* out_msg);
void     queue_destroy(Queue* q);
uint16_t calculate_hash(const Message* msg);

#endif
