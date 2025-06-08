#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#define DATA_FILE "data.bin"
#define MAX_CMD_LEN 128
#define RECORD_COUNT 10

struct record_s {
    char name[80];
    char address[80];
    uint8_t semester;
};

void cmd_lst();
void cmd_get(int rec_no);
void cmd_put(int rec_no);
int lock_record(int fd, int rec_no, short type);
int unlock_record(int fd, int rec_no);
ssize_t read_record(int fd, int rec_no, struct record_s *rec);
ssize_t write_record(int fd, int rec_no, const struct record_s *rec);
void init_file();
void handle_sigint(int sig);
void cleanup_and_exit();

int main(int argc, char *argv[]) {
    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }

    if (argc < 2) {
        char buf[MAX_CMD_LEN];
        printf("Интерактивный режим. Доступные команды: LST, GET, PUT, INIT, QUIT\n");
        while (1) {
            printf("> ");
            if (!fgets(buf, sizeof(buf), stdin)) break;

            buf[strcspn(buf, "\n")] = 0;

            char *cmd = strtok(buf, " ");
            if (!cmd) continue;
            if (strcasecmp(cmd, "LST") == 0) {
                cmd_lst();
            } else if (strcasecmp(cmd, "GET") == 0) {
                char *arg = strtok(NULL, " ");
                if (arg) cmd_get(atoi(arg));
                else      printf("Usage: GET <Rec_No>\n");
            } else if (strcasecmp(cmd, "PUT") == 0) {
                char *arg = strtok(NULL, " ");
                if (arg) cmd_put(atoi(arg));
                else      printf("Usage: PUT <Rec_No>\n");
            } else if (strcasecmp(cmd, "INIT") == 0) {
                init_file();
            } else if (strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "EXIT") == 0) {
                break;
            } else {
                printf("Неизвестная команда: %s\n", cmd);
            }
        }
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "LST") == 0) {
        cmd_lst();
    }
    else if (strcmp(argv[1], "GET") == 0 && argc == 3) {
        cmd_get(atoi(argv[2]));
    }
    else if (strcmp(argv[1], "PUT") == 0 && argc == 3) {
        cmd_put(atoi(argv[2]));
    }
    else {
        fprintf(stderr, "Неверная команда или параметры\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\nПрограмма прервана (Ctrl+C). Очистка ресурсов...\n");
    cleanup_and_exit();
}

void cleanup_and_exit() {
    exit(0);
}

void init_file() {
    int fd = open(DATA_FILE, O_CREAT|O_TRUNC|O_RDWR, 0666);
    if (fd < 0) { perror("init_file: open"); return; }
    struct record_s rec = {0};
    for (int i = 0; i < RECORD_COUNT; i++) {
        snprintf(rec.name, sizeof(rec.name), "Student%02d", i);
        snprintf(rec.address, sizeof(rec.address), "Address%02d", i);
        rec.semester = (i % 8) + 1;
        if (write(fd, &rec, sizeof(rec)) != sizeof(rec)) {
            perror("init_file: write");
            break;
        }
    }
    close(fd);
    printf("Инициализирован файл '%s' с %d записями\n", DATA_FILE, RECORD_COUNT);
}

void cmd_lst() {
    int fd = open(DATA_FILE, O_RDONLY);
    if (fd < 0) { perror("open"); return; }

    struct record_s r;
    int idx = 0;
    while (read(fd, &r, sizeof(r)) == sizeof(r)) {
        printf("%2d: %s, %s, sem=%u\n", idx,
               r.name, r.address, r.semester);
        idx++;
    }
    close(fd);
}

void cmd_get(int rec_no) {
    int fd = open(DATA_FILE, O_RDONLY);
    if (fd < 0) { perror("open"); return; }

    struct record_s r;
    if (lock_record(fd, rec_no, F_RDLCK) < 0) {
        perror("lock_record");
        close(fd);
        return;
    }
    if (read_record(fd, rec_no, &r) == sizeof(r)) {
        printf("REC[%d]: %s, %s, sem=%u\n", rec_no, r.name, r.address, r.semester);
    } else {
        fprintf(stderr, "Ошибка чтения записи %d\n", rec_no);
    }
    unlock_record(fd, rec_no);
    close(fd);
}

void cmd_put(int rec_no) {
    int fd = open(DATA_FILE, O_RDWR);
    if (fd < 0) { perror("open"); return; }

    struct record_s orig, work, fresh;
    Again:

    if (read_record(fd, rec_no, &orig) != sizeof(orig)) {
        fprintf(stderr, "Не удалось прочитать запись %d\n", rec_no);
        close(fd);
        return;
    }
    memcpy(&work, &orig, sizeof(orig));

    printf("Текущие данные:\n");
    printf("Name: %s\nAddress: %s\nSemester: %u\n",
           work.name, work.address, work.semester);

    printf("Введите новое ФИО (или Enter для пропуска): ");
    fgets(work.name, sizeof(work.name), stdin);
    work.name[strcspn(work.name, "\n")] = 0;

    printf("Введите новый адрес (или Enter): ");
    fgets(work.address, sizeof(work.address), stdin);
    work.address[strcspn(work.address, "\n")] = 0;

    printf("Введите семестр (0–255, Enter для пропуска): ");
    char buf[16];
    fgets(buf, sizeof(buf), stdin);
    if (buf[0] != '\n') work.semester = (uint8_t)atoi(buf);

    if (memcmp(&orig, &work, sizeof(orig)) == 0) {
        printf("Изменений нет, выход.\n");
        close(fd);
        return;
    }

    if (lock_record(fd, rec_no, F_WRLCK) < 0) {
        perror("lock_record");
        close(fd);
        return;
    }

    if (read_record(fd, rec_no, &fresh) != sizeof(fresh)) {
        perror("read_record");
        unlock_record(fd, rec_no);
        close(fd);
        return;
    }
    if (memcmp(&orig, &fresh, sizeof(orig)) != 0) {
        printf("!!! Конфликт при сохранении: запись %d уже изменена. Новые данные: \n", rec_no);
        printf("   %s, %s, sem=%u\n", fresh.name, fresh.address, fresh.semester);

        unlock_record(fd, rec_no);
        orig = fresh;
        memcpy(&work, &orig, sizeof(orig));

        printf("Желаете заменить новыми?(+/-)");
        char c = getchar();
        getchar();
        if(c == '-') {
            close(fd);
            return;
        }
        goto Again;
    }

    if (write_record(fd, rec_no, &work) != sizeof(work)) {
        perror("write_record");
    } else {
        printf("Запись %d обновлена.\n", rec_no);
    }

    unlock_record(fd, rec_no);
    close(fd);
}

int lock_record(int fd, int rec_no, short type) {
    struct flock fl = {0};
    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = rec_no * sizeof(struct record_s);
    fl.l_len = sizeof(struct record_s);
    fl.l_pid = 0;
    return fcntl(fd, F_OFD_SETLKW, &fl);
}

int unlock_record(int fd, int rec_no) {
    struct flock fl = {0};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = rec_no * sizeof(struct record_s);
    fl.l_len = sizeof(struct record_s);
    return fcntl(fd, F_OFD_SETLK, &fl);
}

ssize_t read_record(int fd, int rec_no, struct record_s *rec) {
    off_t off = lseek(fd, rec_no * sizeof(*rec), SEEK_SET);
    if (off < 0) return -1;
    return read(fd, rec, sizeof(*rec));
}

ssize_t write_record(int fd, int rec_no, const struct record_s *rec) {
    off_t off = lseek(fd, rec_no * sizeof(*rec), SEEK_SET);
    if (off < 0) return -1;
    return write(fd, rec, sizeof(*rec));
}
