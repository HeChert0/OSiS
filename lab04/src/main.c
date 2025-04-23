#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "queue.h"

#define MAX_PROCS 6

volatile sig_atomic_t running = 1;
pid_t producers[MAX_PROCS] = {0};
pid_t consumers[MAX_PROCS] = {0};
int prod_count = 0;
int cons_count = 0;

Queue* queue = NULL;

void handle_signal(int sig);
void producer_task();
void consumer_task();
void create_producer();
void create_consumer();
void remove_last_producer();
void remove_last_consumer();
void remove_all_procs();
void print_status();
void setup_signals();
void sigchld_handler(int sig);

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGCHLD, sigchld_handler); 

    queue = queue_init();
    if (!queue) {
        fprintf(stderr, "Queue initialization failed\n");
        return 1;
    }

    printf("Controls:\n"
    "  p - Add producer\n"
    "  c - Add consumer\n"
    "  P - Remove last producer\n"
    "  C - Remove last consumer\n"
    "  k - Remove all processes\n"
    "  s - Show status\n"
    "  q - Quit\n");

    while (running) {
        char cmd;
        scanf(" %c", &cmd);

        switch (cmd) {
            case 'p': create_producer(); break;
            case 'c': create_consumer(); break;
            case 'P': remove_last_producer(); break;
            case 'C': remove_last_consumer(); break;
            case 'k': remove_all_procs(); break;
            case 's': print_status(); break;
            case 'q': running = 0; break;
            default: printf("Unknown command\n");
        }
    }


    remove_all_procs();
    while (wait(NULL) > 0);
    queue_destroy(queue);
    printf("\nProgram terminated\n");

    return 0;
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;  // Автоматический перезапуск системных вызовов
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTERM, &sa, NULL);
}


void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


void producer_task() {
    srand(getpid());
    while (1) {

        uint8_t size = rand() % 256;
        size_t data_alloc = ((size + DATA_ALIGNMENT - 1) / DATA_ALIGNMENT) * DATA_ALIGNMENT;
        size_t total_size = sizeof(Message) + data_alloc;
        Message* msg = aligned_alloc(DATA_ALIGNMENT, total_size);
        if (!msg) {
            perror("aligned_alloc failed");
            exit(EXIT_FAILURE);
        }

        msg->type = rand() % 256;
        msg->size = size;

        for (int i = 0; i < size; i++) {
            msg->data[i] = rand() % 256;
        }
        msg->hash = 0;
        msg->hash = calculate_hash(msg);

        queue_push(queue, msg);
        printf("[Producer %d] Added. |type: %u, hash: %u, size: %u| Total: %d\n", getpid(), msg->type, msg->hash, msg->size, queue->added_count);

        sleep(1 + rand() % 2);
    }
}

void consumer_task() {
    while (1) {
        Message* msg = queue_pop(queue);
        printf("[Consumer %d] Removed. |type: %u, hash: %u, size: %u| Total: %d\n", getpid(), msg->type, msg->hash, msg->size, queue->removed_count);

        uint16_t expected_hash = calculate_hash(msg);
        if (expected_hash != msg->hash) {
            fprintf(stderr, "[Consumer %d] Hash mismatch: expected %hu, got %hu\n",
                    getpid(), expected_hash, msg->hash);
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
    while (prod_count > 0) {
        remove_last_producer();
    }

    while (cons_count > 0) {
        remove_last_consumer();
    }
}

void print_status() {
    printf("\n--- Status ---\n");
    printf("Queue: %d/%d (used/free)\n",
           MAX_QUEUE_SIZE - queue->free_slots,
           queue->free_slots);
    printf("Messages: added=%d, removed=%d\n",
           queue->added_count,
           queue->removed_count);
    printf("Active producers: %d\n", prod_count);
    printf("Active consumers: %d\n\n", cons_count);
}

