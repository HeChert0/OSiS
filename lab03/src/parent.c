#define _GNU_SOURCE
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define CAPACITY 8

typedef struct {
    pid_t pid;
    char name[CAPACITY * 2];
    bool is_running;
} ProcessInfo;

size_t child_processes_size = 0;
size_t child_processes_capacity = CAPACITY;
ProcessInfo *child_processes = NULL;
const char *child_name = "./build/debug/child";

void InitSignals();
void HandleSignal(int signo, siginfo_t *info, void *context);
void HandleExitChild(int signo);
void CreateChild();
void DeleteLastChild();
void ListChild();
void DeleteAllChild();
void CleanExit();
void WaitChild();
void print_menu();
void StartChild(size_t index);
void StopChild(size_t index);



int main() {
    srand(time(NULL));
    InitSignals();
    child_processes = (ProcessInfo *)calloc(child_processes_capacity, sizeof(ProcessInfo));
    if (!child_processes) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    print_menu();
    char input[CAPACITY];
    while (true) {
        printf("> ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL)
            continue;

        char option = input[0];
        int index = -1;
        if (strlen(input) > 1 && (input[1] >= '0' && input[1] <= '9')) {
            index = input[1] - '0';
        }

        switch (option) {
            case '+':
                CreateChild();
                break;
            case '-':
                DeleteLastChild();
                break;
            case 'l':
                ListChild();
                break;
            case 'k':
                DeleteAllChild();
                break;
            case 's':
                StopChild(index);
                break;
            case 'g':
                StartChild(index);
                break;
            case 'q':
                CleanExit();
                break;
            default:
                printf("Invalid option. Type 'm' for menu.\n");
                break;
        }
    }
    return 0;
}

void StartChild(size_t index) {
    if (index >= child_processes_size) {
        printf("Invalid index\n");
        return;
    }
    pid_t pid = child_processes[index].pid;
    union sigval info = { .sival_int = 0 };
    info.sival_int = 0;

    sigqueue(pid, SIGUSR2, info);
    child_processes[index].is_running = true;
    printf("Started child %s, PID %d\n", child_processes[index].name, pid);
}


void StopChild(size_t index) {
    if (index >= child_processes_size) {
        printf("Invalid index\n");
        return;
    }
    pid_t pid = child_processes[index].pid;
    union sigval info = { .sival_int = 0 };
    info.sival_int = 0;

    sigqueue(pid, SIGUSR1, info);
    child_processes[index].is_running = false;
    printf("Stopped child %s, PID %d\n", child_processes[index].name, pid);
}

void WaitChild() {
    while (child_processes_size > 0) {
        pid_t pid = child_processes[child_processes_size - 1].pid;
        int status;
        waitpid(pid, &status, 0);
        printf("Child %s, PID %d has exited\n", child_processes[child_processes_size - 1].name, pid);
        child_processes_size--;
    }
}

void InitSignals() {
    struct sigaction action = {0};
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGCHLD);

    action.sa_flags = SA_SIGINFO;
    action.sa_mask = set;
    action.sa_sigaction = HandleSignal;
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    action.sa_handler = HandleExitChild;
    sigaction(SIGCHLD, &action, NULL);
}

void HandleSignal(int signo, siginfo_t *info, void *context) {
    (void)context;
    if (signo == SIGUSR1) {
        pid_t child_pid = info->si_value.sival_int;
        printf("Parent: Received SIGUSR1 from child %d\n", child_pid);

        union sigval info_resume = { .sival_int = 0 };
        info_resume.sival_int = 0;
        sigqueue(child_pid, SIGUSR2, info_resume);
    } else if (signo == SIGUSR2) {
        pid_t child_pid = info->si_value.sival_int;
        printf("Parent: Child %d has finished output\n", child_pid);
    }
}

void HandleExitChild(int signo) {
    (void)signo;
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (size_t i = 0; i < child_processes_size; i++) {
            if (child_processes[i].pid == pid) {
                printf("Child %s, PID %d has exited\n", child_processes[i].name, pid);
                for (size_t j = i; j < child_processes_size - 1; j++) {
                    child_processes[j] = child_processes[j + 1];
                }
                child_processes_size--;
                break;
            }
        }
    }
}

void CreateChild() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        return;
    }
    if (pid == 0) {
        execl(child_name, child_name, NULL);
        perror("Failed to exec");
        exit(EXIT_FAILURE);
    } else {
        if (child_processes_size >= child_processes_capacity) {
            child_processes_capacity *= 2;
            ProcessInfo *tmp = (ProcessInfo *)realloc(child_processes, child_processes_capacity * sizeof(ProcessInfo));
            if (!tmp) {
                perror("Failed to reallocate memory");
                exit(EXIT_FAILURE);
            }
            child_processes = tmp;
        }
        snprintf(child_processes[child_processes_size].name, CAPACITY * 2, "C_%02d", (int)child_processes_size);
        child_processes[child_processes_size].pid = pid;

        child_processes[child_processes_size].is_running = false;
        child_processes_size++;
        printf("Created child %s, PID %d\n", child_processes[child_processes_size - 1].name, pid);
    }
}


void DeleteLastChild() {
    if (child_processes_size == 0) {
        printf("No children to delete\n");
        return;
    }
    pid_t pid = child_processes[child_processes_size - 1].pid;
    kill(pid, SIGTERM);
    printf("Deleted child %s, PID %d\n", child_processes[child_processes_size - 1].name, pid);
    child_processes_size--;
}


void ListChild() {
    printf("Parent PID: %d\n", getpid());
    if (child_processes_size == 0) {
        printf("No children running.\n");
    } else {
        for (size_t i = 0; i < child_processes_size; i++) {
            printf("Child %s, PID %d is %s\n", child_processes[i].name, child_processes[i].pid,
                   child_processes[i].is_running ? "running" : "stopped");
        }
    }
}


void DeleteAllChild() {
    sigset_t block_mask, old_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    while (child_processes_size > 0) {
        DeleteLastChild();
    }

    sigprocmask(SIG_SETMASK, &old_mask, NULL);
    printf("All children deleted\n");
}

void CleanExit() {
    DeleteAllChild();
    WaitChild();
    free(child_processes);
    printf("Exiting...\n");
    exit(EXIT_SUCCESS);
}

void print_menu() {
    printf("\nOptions:\n");
    printf("+ - Create new child\n");
    printf("- - Delete last child\n");
    printf("l - List all children\n");
    printf("k - Delete all children\n");
    printf("s<ind> - Stop child at index \n");
    printf("g<ind> - Start child at index \n");
    printf("q - Quit\n");
}
