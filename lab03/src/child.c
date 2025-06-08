#define _GNU_SOURCE
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int first;
    int second;
} Pair;

void InitSignalsHandling();
void UsrSignHandler(int signo);
void AlrSignHandler(int signo);
void UpdateStat();

bool can_print = false;
bool received_signal = false;

Pair occurrence;
size_t c00 = 0, c01 = 0, c10 = 0, c11 = 0;


void UpdateStat() {
    static int counter = 0;
    switch (counter) {
        case 0:
            occurrence.first = 0;
            occurrence.second = 0;
            break;
        case 1:
            occurrence.first = 1;
            occurrence.second = 0;
            break;
        case 2:
            occurrence.first = 0;
            occurrence.second = 1;
            break;
        case 3:
            occurrence.first = 1;
            occurrence.second = 1;
            break;
        default:
            counter = -1;
            break;
    }
    counter++;
}

int main() {
    srand(time(NULL));

    InitSignalsHandling();

    alarm(rand() % 1 + 1);

    for (int i = 0;; i++) {
        sleep(1);
        UpdateStat();

        received_signal = false;

        if (i >= 6 && can_print) {
            alarm(0);

            union sigval info = { .sival_int = getpid() };
            info.sival_int = getpid();
            while (!received_signal) {
                sigqueue(getppid(), SIGUSR1, info);
                sleep(1);
            }

            alarm(rand() % 1 + 1);
            if (!can_print) {
                i = 0;
                can_print = true;
                continue;
            }
            printf("-------------------------------------------\n");
            printf("ppid - %5d\tpid  - %5d\t", (int)getppid(), (int)getpid());
            printf("00   - %5zu; 01   - %5zu; 10   - %5zu; 11   - %5zu\n", c00, c01, c10, c11);
            i = 0;

            sigqueue(getppid(), SIGUSR2, info);
        }
    }
    return 0;
}


void InitSignalsHandling() {
    struct sigaction action = {0};
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    action.sa_flags = 0;
    action.sa_mask = set;
    action.sa_handler = UsrSignHandler;
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

    action.sa_handler = AlrSignHandler;
    sigaction(SIGALRM, &action, NULL);
}

void UsrSignHandler(int signo) {
    if (signo == SIGUSR1) {
        can_print = false;
        received_signal = true;
    } else if (signo == SIGUSR2) {
        can_print = true;
        received_signal = true;
    }
}


void AlrSignHandler(int signo) {
    if (occurrence.first == 0 && occurrence.second == 0)
        c00++;
    else if (occurrence.first == 1 && occurrence.second == 0)
        c01++;
    else if (occurrence.first == 0 && occurrence.second == 1)
        c10++;
    else if (occurrence.first == 1 && occurrence.second == 1)
        c11++;

    alarm(rand() % 1 + 1);
}
