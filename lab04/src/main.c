#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include "queue.h"

#define MAX_PROCS 4

volatile sig_atomic_t running = 1;
int prod_count = 0, cons_count = 0;
pid_t producers[MAX_PROCS], consumers[MAX_PROCS];
Queue* queue = NULL;

void handle_signal(int sig);
void sigchld_handler(int sig);
void setup_signals();
void consumer_task();
void create_producer();
void create_consumer();
void remove_last_producer();
void remove_last_consumer();
void remove_all_procs();
void print_status();

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGCHLD, sigchld_handler);

    queue = queue_init();
    if (!queue) {
        fprintf(stderr, "Queue initialization failed\n");
        return 1;
    }

    printf("Commands:\n p=add producer\n c=add consumer\n P=remove producer\n C=remove consumer\n k=kill all\n s=status\n q=quit\n");
    while (running) {
        char cmd;
        if (scanf(" %c", &cmd) != 1) break;
        switch (cmd) {
            case 'p': create_producer();   break;
            case 'c': create_consumer();   break;
            case 'P': remove_last_producer(); break;
            case 'C': remove_last_consumer(); break;
            case 'k': remove_all_procs();  break;
            case 's': print_status();      break;
            case 'q': running = 0;         break;
        }
    }

    remove_all_procs();
    while (wait(NULL) > 0);
    queue_destroy(queue);
    printf("Program terminated\n");
    return 0;
}




void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_signals() {
    struct sigaction sa = { .sa_flags = SA_RESTART };
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTERM, &sa, NULL);
}

void producer_task() {
    srand(getpid());
    while (running) {
        uint8_t size = (uint8_t)(rand() % 256 + 1);

        Message msg;
        memset(&msg, 0, sizeof(msg));

        msg.type = (uint8_t)(rand() % 256);
        msg.size = size;

        for (size_t i = 0; i < size; ++i) {
            msg.data[i] = (uint8_t)(rand() % 256 + 1);
        }
        msg.hash = calculate_hash(&msg);

        queue_push(queue, &msg);
        printf("[Producer %lld] Added |type:%llu hash:%llu size:%llu| Total:%lld\n",
               (long long)getpid(),
               (unsigned long long)msg.type,
               (unsigned long long)msg.hash,
               (unsigned long long)msg.size,
               (long long)queue->added_count);

        sleep(1 + rand() % 2);
    }
}

void consumer_task() {
    while (running) {
        Message msg;
        queue_pop(queue, &msg);

        printf("[Consumer %lld] Removed |type:%llu hash:%llu size:%llu| Total:%lld\n",
               (long long)getpid(),
               (unsigned long long)msg.type,
               (unsigned long long)msg.hash,
               (unsigned long long)msg.size,
               (long long)queue->removed_count);

        uint16_t expected = calculate_hash(&msg);
        if (expected != msg.hash) {
            fprintf(stderr, "[Consumer %lld] Hash mismatch: expected %llu, got %llu\n",
                    (long long)getpid(),
                    (unsigned long long)expected,
                    (unsigned long long)msg.hash);
        }

        sleep(2 + rand() % 3);
    }
}

void create_producer() {
    if (prod_count >= MAX_PROCS) {
        printf("Max producers reached!\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setup_signals();
        producer_task();
        exit(0);
    }
    if (pid == -1) {
        perror("fork");
        return;
    }
    else {
        producers[prod_count++] = pid;
        printf("Producer %d created\n", pid);
    }
}

void create_consumer() {
    if (cons_count >= MAX_PROCS) {
        printf("Max consumers reached!\n");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setup_signals();
        consumer_task();
        exit(0);
    }
    if (pid == -1) {
        perror("fork");
        return;
    }
    else {
        consumers[cons_count++] = pid;
        printf("Consumer %d created\n", pid);
    }
}

void remove_last_producer() {
    if (prod_count == 0) return;

    pid_t pid = producers[--prod_count];
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    printf("Producer %d removed\n", pid);
}

void remove_last_consumer() {
    if (cons_count == 0) return;

    pid_t pid = consumers[--cons_count];
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    printf("Consumer %d removed\n", pid);
}


void remove_all_procs() {
    while (prod_count)
        remove_last_producer();
    while (cons_count)
        remove_last_consumer();
}

void print_status() {
    printf("Queue: %d/%d used/free\n added=%d removed=%d\n producers=%d\n consumers=%d\n",
           MAX_QUEUE_SIZE - queue->free_slots,
           queue->free_slots,
           queue->added_count,
           queue->removed_count,
           prod_count,
           cons_count);
}

