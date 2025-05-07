#include "binCommands.h"
#include "logActions.h"

#define BASKET_FOLDER "/home/hechert/.local/bin"
#define MAX_FILENAME_LENGTH 1024

static void mkdir_p(const char* path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0777);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0777);
}

void addToBasket(const char* filename) {
    if (access(filename, F_OK) != 0) {
        fprintf(stderr, "Файл '%s' не существует!\n", filename);
        return;
    }

    const char* basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;

    char newFilename[MAX_FILENAME_LENGTH];
    snprintf(newFilename, sizeof(newFilename), "%s/%s", BASKET_FOLDER, basename);

    if (rename(filename, newFilename) != 0) {
        fprintf(stderr, "Ошибка перемещения файла: %s\n", strerror(errno));
        return;
    }

    FILE* basketInfo = fopen(BASKET_FOLDER "/bin.info", "a");
    if (!basketInfo) {
        fprintf(stderr, "Ошибка открытия bin.info: %s\n", strerror(errno));
        return;
    }

    fprintf(basketInfo, "%s|%s\n", filename, newFilename); // Изменён разделитель
    fclose(basketInfo);
    logAction("Добавлено в корзину", filename);
}

void restoreFromBasket() {
    if (access(BASKET_FOLDER "/bin.info", F_OK) != 0) {
        printf("Корзина пуста\n");
        return;
    }

    FILE* basketInfo = fopen(BASKET_FOLDER "/bin.info", "r");
    if (!basketInfo) {
        fprintf(stderr, "Ошибка открытия bin.info: %s\n", strerror(errno));
        return;
    }

    char line[2048];
    while (fgets(line, sizeof(line), basketInfo)) {
        line[strcspn(line, "\n")] = 0; // Удаляем перевод строки

        char* original = strtok(line, "|");
        char* new_path = strtok(NULL, "|");

        if (!original || !new_path) {
            fprintf(stderr, "Ошибка формата записи\n");
            continue;
        }

        // Создаём родительские директории
        char* dir = strdup(original);
        mkdir_p(dirname(dir));
        free(dir);

        if (rename(new_path, original) != 0) {
            fprintf(stderr, "Ошибка восстановления %s: %s\n", original, strerror(errno));
        } else {
            logAction("Восстановлено", original);
        }
    }

    fclose(basketInfo);
    remove(BASKET_FOLDER "/bin.info");
}

void clearBasket() {
    if (access(BASKET_FOLDER "/bin.info", F_OK) != 0) {
        printf("Корзина пуста\n");
        return;
    }

    FILE* basketInfo = fopen(BASKET_FOLDER "/bin.info", "r");
    if (!basketInfo) {
        fprintf(stderr, "Ошибка открытия bin.info: %s\n", strerror(errno));
        return;
    }

    char line[2048];
    while (fgets(line, sizeof(line), basketInfo)) {
        line[strcspn(line, "\n")] = 0;
        char* new_path = strtok(line, "|");
        new_path = strtok(NULL, "|"); // Получаем второй элемент

        if (new_path && unlink(new_path) != 0) {
            fprintf(stderr, "Ошибка удаления %s: %s\n", new_path, strerror(errno));
        }
    }

    fclose(basketInfo);
    remove(BASKET_FOLDER "/bin.info");
    logAction("Корзина очищена", "");
}

void displayBasketInfo() {
    if (access(BASKET_FOLDER "/bin.info", F_OK) != 0) {
        printf("Корзина пуста\n");
        return;
    }

    FILE* basketInfo = fopen(BASKET_FOLDER "/bin.info", "r");
    if (!basketInfo) {
        fprintf(stderr, "Ошибка открытия bin.info: %s\n", strerror(errno));
        return;
    }

    char line[2048];
    printf("Содержимое корзины:\n");
    while (fgets(line, sizeof(line), basketInfo)) {
        line[strcspn(line, "\n")] = 0;
        char* original = strtok(line, "|");
        printf("• %s\n", original);
    }

    fclose(basketInfo);
}


void autoCleanBasket() {
    while (1) {
        sleep(AUTO_CLEAN_INTERVAL);
        printf("Auto cleaning basket...\n");
        clearBasket();
        logAction("Корзина очищена автоматически", "");
    }
}
