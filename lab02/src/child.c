#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ENV_FILENAME "env.txt"

int main(int argc, char *argv[], char *envp[]) {

    printf("Имя процесса: %s\n", argv[0]);
    printf("PID: %d\n", getpid());
    printf("PPID: %d\n", getppid());
    printf("\n");


    if (argc > 1 && strcmp(argv[1], "env") == 0) {
        printf("Режим: чтение переменных из файла %s через getenv()\n", ENV_FILENAME);
        FILE *env_file = fopen(ENV_FILENAME, "r");
        if (!env_file) {
            perror("Ошибка открытия файла env");
            exit(EXIT_FAILURE);
        }
        char line[256];
        while (fgets(line, sizeof(line), env_file)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '\0')
                continue;
            char *value = getenv(line);
            printf("%s=%s\n", line, value ? value : "");
        }
        fclose(env_file);
    } else {
        printf("Режим: вывод переменных из переданного окружения (envp):\n");
        for (int i = 0; envp[i] != NULL; i++) {
            printf("%s\n", envp[i]);
        }
    }
    return 0;
}
