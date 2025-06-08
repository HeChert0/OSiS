#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <locale.h>

extern char **environ;

#define MAX_ENV_VARS 100
#define CHILD_NAME_FORMAT "child_%02d"
#define ENV_FILENAME "env.txt"

int EvcCmp(const void *a, const void *b) {
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

void PrintEnvSorted() {
    char *env_vars[MAX_ENV_VARS];
    int count = 0;
    for (char **env = environ; *env != NULL && count < MAX_ENV_VARS; env++) {
        env_vars[count++] = *env;
    }
    qsort(env_vars, count, sizeof(char *), EvcCmp);
    printf("Родительское окружение (отсортированное):\n");
    for (int i = 0; i < count; i++) {
        puts(env_vars[i]);
    }
    printf("\n");
}

char **CreateChildEnv() {
    FILE *env_file = fopen(ENV_FILENAME, "r");
    if (!env_file) {
        perror("Ошибка открытия файла env.txt");
        exit(EXIT_FAILURE);
    }

    char *env_entries[MAX_ENV_VARS];
    int count = 0;
    char line[256];

    while (fgets(line, sizeof(line), env_file) != NULL && count < MAX_ENV_VARS) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        char *value = getenv(line);
        if (!value) value = "";

        size_t len = strlen(line) + strlen(value) + 2;
        env_entries[count] = malloc(len);
        if (!env_entries[count]) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        snprintf(env_entries[count], len, "%s=%s", line, value);
        count++;
    }
    fclose(env_file);

    if (getenv("CHILD_PATH")) {
        size_t len = strlen("CHILD_PATH=") + strlen(getenv("CHILD_PATH")) + 1;
        env_entries[count] = malloc(len);
        snprintf(env_entries[count], len, "CHILD_PATH=%s", getenv("CHILD_PATH"));
        count++;
    }

    env_entries[count] = NULL;
    char **child_env = malloc((count + 1) * sizeof(char *));
    if (!child_env) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < count; i++) {
        child_env[i] = env_entries[i];
    }
    child_env[count] = NULL;

    return child_env;
}

char *FindChildPath(const char mode, char **child_env) {
    char *child_path = NULL;

    if (mode == '+') {
        child_path = getenv("CHILD_PATH");
        printf("Режим '+': Получаем CHILD_PATH через getenv() -> %s\n", child_path ? child_path : "не найден");
    } else if (mode == '*') {
        for (int i = 0; child_env[i] != NULL; i++) {
            if (strncmp(child_env[i], "CHILD_PATH=", 11) == 0) {
                child_path = child_env[i] + 11;
                printf("Режим '*': Найден CHILD_PATH в envp -> %s\n", child_path);
                break;
            }
        }
    } else if (mode == '&') {
        for (int i = 0; environ[i] != NULL; i++) {
            if (strncmp(environ[i], "CHILD_PATH=", 11) == 0) {
                child_path = environ[i] + 11;
                printf("Режим '&': Найден CHILD_PATH в глобальном окружении -> %s\n", child_path);
                break;
            }
        }
    }

    return child_path;
}

void StartChild(char **child_env, char mode) {
    static int child_count = 0;
    char child_name[16];
    snprintf(child_name, sizeof(child_name), CHILD_NAME_FORMAT, child_count++);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        char *child_prog_path = FindChildPath(mode, child_env);
        if (!child_prog_path) {
            fprintf(stderr, "Ошибка: CHILD_PATH не найден\n");
            exit(EXIT_FAILURE);
        }
        if (access(child_prog_path, X_OK) == -1) {
            perror("access");
            exit(EXIT_FAILURE);
        }

        char *argv_child[3];
        argv_child[0] = child_name;
        if (mode == '+')
            argv_child[1] = "env";
        else
            argv_child[1] = NULL;

        argv_child[2] = NULL;
        execve(child_prog_path, argv_child, child_env);
        perror("execve");
        exit(EXIT_FAILURE);
        wait(NULL);
    }
}

int main(int argc, char *argv[], char *envp[]) {

    if (setenv("LC_COLLATE", "C", 1) != 0) {
        perror("Ошибка установки LC_COLLATE");
        exit(EXIT_FAILURE);
    }

    char hostname[256];
    if (!getenv("HOSTNAME")) {
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            setenv("HOSTNAME", hostname, 1);
        } else {
            perror("Ошибка получения HOSTNAME");
        }
    }

    setlocale(LC_ALL, "C");
    PrintEnvSorted();

    char **child_env = CreateChildEnv();

    printf("Введите команду:\n");
    printf("  '+' : запустить child с чтением переменных из файла\n");
    printf("  '*' : запустить child с выводом окружения, переданного через envp\n");
    printf("  '&' : запустить child с поиском CHILD_PATH в глобальном окружении\n");
    printf("  'q' : завершить работу программы\n");

    char input;
    while (scanf(" %c", &input) == 1 && input != 'q') {
        switch (input) {
            case '+':
            case '*':
            case '&':
                StartChild(child_env, input);
                break;
            default:
                printf("Неизвестная команда\n");
                break;
        }
        printf("Введите команду (+, *, &, q): ");
    }

    for (int i = 0; child_env[i] != NULL; i++) {
        free(child_env[i]);
    }
    free(child_env);

    return 0;
}
