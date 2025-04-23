#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};

int safe_semop(int sem_id, struct sembuf *sops, size_t nsops) {
    int ret;
    do {
        ret = semop(sem_id, sops, nsops);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

Queue* queue_init() {
    int shmid = shmget(QUEUE_KEY, sizeof(Queue), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1) {
        if (errno == EEXIST) {
            shmid = shmget(QUEUE_KEY, sizeof(Queue), 0666);
        } else {
            perror("shmget");
            return NULL;
        }
    }

    Queue* q = shmat(shmid, NULL, 0);
    if (q == (void*)-1) {
        perror("shmat failed");
        return NULL;
    }

    q->sem_id = semget(SEM_KEY, 3, IPC_CREAT | IPC_EXCL | 0666);
    if (q->sem_id == -1) {
        if (errno == EEXIST) {
            q->sem_id = semget(SEM_KEY, 3, 0666);
        } else {
            perror("semget failed");
            return NULL;
        }
    } else {
        union semun arg;
        unsigned short values[3] = {MAX_QUEUE_SIZE, 0, 1}; //efm
        arg.array = values;
        if (semctl(q->sem_id, 0, SETALL, arg) == -1) {
            perror("semctl SETALL failed");
            return NULL;
        }
    }

    q->head = 0;
    q->tail = 0;
    q->added_count = 0;
    q->removed_count = 0;
    q->free_slots = MAX_QUEUE_SIZE;
    memset(q->buffer, 0, sizeof(q->buffer));


    return q;
}

void queue_push(Queue* q, Message* msg) {
    struct sembuf ops_push[2] = {
        {0, -1, 0},
        {2, -1, 0}
    };
    if (safe_semop(q->sem_id, ops_push, 2) == -1) {
        perror("semop push wait/lock failed");
        exit(EXIT_FAILURE);
    }

    q->buffer[q->tail] = msg;
    q->tail = (q->tail + 1) % MAX_QUEUE_SIZE;
    q->added_count++;
    q->free_slots--;

    struct sembuf ops_push_finish[2] = {
        {2, 1, 0},
        {1, 1, 0}
    };
    if (safe_semop(q->sem_id, ops_push_finish, 2) == -1) {
        perror("semop push unlock/signal failed");
        exit(EXIT_FAILURE);
    }
}

Message* queue_pop(Queue* q) {
    struct sembuf ops[] = {
        {1, -1, 0},
        {2, -1, 0}
    };
    safe_semop(q->sem_id, ops, 2);

    Message* msg = q->buffer[q->head];
    q->buffer[q->head] = NULL;
    q->head = (q->head + 1) % MAX_QUEUE_SIZE;
    q->removed_count++;
    q->free_slots++;

    struct sembuf ops2[] = {
        {2, 1, 0},
        {0, 1, 0}
    };
    safe_semop(q->sem_id, ops2, 2);

    return msg;
}

void queue_destroy(Queue* q) {
    semctl(q->sem_id, 0, IPC_RMID);

    shmdt(q);
    shmctl(QUEUE_KEY, IPC_RMID, NULL);
}

uint16_t calculate_hash(Message* msg) {
    uint16_t hash = 0;
    uint8_t* bytes = (uint8_t*)msg;
    uint16_t saved_hash = msg->hash;
    msg->hash = 0;

    size_t bytes_to_hash = sizeof(Message) + msg->size;

    for (size_t i = 0; i < bytes_to_hash; i++) {
        hash += bytes[i];
    }

    msg->hash = saved_hash;
    return hash;
}
