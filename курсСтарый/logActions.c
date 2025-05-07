#include "logActions.h"

void logAction(const char* action, const char* filename) {
    FILE* logFile = fopen("log.txt", "a");
    if (!logFile) {
        fprintf(stderr, "Ошибка открытия файла журнала: %s\n", strerror(errno));
        return;
    }

    time_t now = time(NULL);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(logFile, "[%s] %s: %s\n", timestamp, action, filename);
    fclose(logFile);
}
