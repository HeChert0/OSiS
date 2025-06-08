#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

#define MAXLINE 1024

static char *info_buf = NULL;
static size_t info_len = 0;
static int listen_fd = -1;
static const char *root_dir;

static struct PendingFD {
    int *fd;
    struct PendingFD *next;
} *pending_fds = NULL;

static pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t server_running = 1;

void handle_sigint(int signo) {
    server_running = 0;

    close(listen_fd);

    pthread_mutex_lock(&list_mutex);
    struct PendingFD *current = pending_fds;
    while (current) {
        if (*(current->fd) >= 0) {
            close(*(current->fd));
            *(current->fd) = -1;
        }
        struct PendingFD *tmp = current;
        current = current->next;
        free(tmp->fd);
        free(tmp);
    }
    pending_fds = NULL;
    pthread_mutex_unlock(&list_mutex);

    if (info_buf) free(info_buf);
    fprintf(stderr, "\nServer terminated gracefully\n");
    exit(EXIT_SUCCESS);
}

void *handle_client(void *arg) {
    int *p_client_fd = (int*)arg;
    int client_fd = *p_client_fd;

    pthread_mutex_lock(&list_mutex);
    struct PendingFD **pp = &pending_fds;
    while (*pp) {
        if ((*(*pp)->fd) == client_fd) {
            struct PendingFD *tmp = *pp;
            *pp = (*pp)->next;
            free(tmp);
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&list_mutex);
    free(p_client_fd);

    char buffer[MAXLINE];
    ssize_t n;

    send(client_fd, "Welcome to myserver\r\n", 22, 0);
    if (info_buf && info_len > 0) {
        send(client_fd, info_buf, info_len, 0);
        send(client_fd, "\r\n", 2, 0);
    }

    char curr_dir[PATH_MAX] = "";
    while ((n = recv(client_fd, buffer, MAXLINE-1, 0)) > 0) {
        buffer[n] = '\0';

        char *cmd = strtok(buffer, "\r\n");
        if (!cmd || *cmd == '\0') continue;

        if (strncmp(cmd, "ECHO ", 5) == 0) {
            const char *payload = cmd + 5;
            send(client_fd, payload, strlen(payload), 0);
            send(client_fd, "\r\n", 2, 0);
        }
        else if (strcmp(cmd, "QUIT") == 0) {
            send(client_fd, "BYE\r\n", 6, 0);
            break;
        }
        else if (strcmp(cmd, "INFO") == 0) {
            if (info_buf && info_len > 0) {
                send(client_fd, info_buf, info_len, 0);
                send(client_fd, "\r\n", 2, 0);
            } else {
                send(client_fd, "(no INFO available)\r\n", 23, 0);
            }
        }
        else if (strncmp(cmd, "CD ", 3) == 0) {
            char *arg = cmd + 3;
            char tmp[PATH_MAX];
            if (arg[0]=='/') {
                snprintf(tmp, sizeof(tmp), "%s/%s", root_dir, arg);
            } else {
                snprintf(tmp, sizeof(tmp), "%s/%s/%s", root_dir,
                         curr_dir[0] ? curr_dir : "", arg);
            }
            char real[PATH_MAX];
            if (realpath(tmp, real) && strncmp(real, root_dir, strlen(root_dir))==0) {
                const char *p = real + strlen(root_dir);
                if (*p=='/') p++;
                strncpy(curr_dir, p, sizeof(curr_dir));
            }
            send(client_fd, "\r\n", 2, 0);
            continue;
        }
        else if (strcmp(cmd, "LIST") == 0) {
            char dirpath[PATH_MAX];
            snprintf(dirpath, sizeof(dirpath), "%s/%s",
                     root_dir, curr_dir[0]? curr_dir : "");
            DIR *d = opendir(dirpath);
            if (!d) {
                send(client_fd, "(cannot open dir)\r\n", 18, 0);
                send(client_fd, "\r\n", 2, 0);
                continue;
            }
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (strcmp(ent->d_name, ".")==0) continue;
                char full[PATH_MAX];
                snprintf(full, sizeof(full), "%s/%s", dirpath, ent->d_name);
                struct stat st;
                if (lstat(full, &st) < 0) continue;
                char line[MAXLINE];
                if (S_ISDIR(st.st_mode)) {
                    snprintf(line, sizeof(line), "%s/\r\n", ent->d_name);
                }
                else if (S_ISLNK(st.st_mode)) {
                    char target[PATH_MAX];
                    ssize_t L = readlink(full, target, sizeof(target)-1);
                    if (L<0) continue;
                    target[L]='\0';
                    struct stat st2;
                    char linkfull[PATH_MAX];
                    snprintf(linkfull, sizeof(linkfull), "%s/%s", dirpath, target);
                    if (stat(linkfull, &st2)==0 && S_ISREG(st2.st_mode))
                        snprintf(line, sizeof(line), "%s --> %s\r\n", ent->d_name, target);
                    else
                        snprintf(line, sizeof(line), "%s -->> %s\r\n", ent->d_name, target);
                }
                else {
                    snprintf(line, sizeof(line), "%s\r\n", ent->d_name);
                }
                send(client_fd, line, strlen(line), 0);
            }
            closedir(d);
            send(client_fd, "\r\n", 2, 0);
            continue;
        }
        else {
            send(client_fd, "Unknown command\r\n", 17, 0);
        }
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <root_dir> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    root_dir = argv[1];
    int port = atoi(argv[2]);

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    char info_path[PATH_MAX];
    snprintf(info_path, sizeof(info_path), "%s/info.txt", root_dir);
    FILE *f = fopen(info_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        info_len = ftell(f);
        rewind(f);
        info_buf = malloc(info_len + 1);
        if (info_buf) {
            fread(info_buf, 1, info_len, f);
            info_buf[info_len] = '\0';
        }
        fclose(f);
    } else {
        fprintf(stderr, "Warning: could not open %s: %s\n", info_path, strerror(errno));
    }

    if (chdir(root_dir) != 0) {
        die("chdir root_dir");
    }

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        die("bind");

    if (listen(listen_fd, BACKLOG) < 0)
        die("listen");

    printf("Server listening on port %d, root dir '%s'\n", port, root_dir);

    while (server_running) {
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) die("malloc");
        *client_fd = -1;

        struct PendingFD *new = malloc(sizeof(struct PendingFD));
        new->fd = client_fd;
        pthread_mutex_lock(&list_mutex);
        new->next = pending_fds;
        pending_fds = new;
        pthread_mutex_unlock(&list_mutex);

        *client_fd = accept(listen_fd, NULL, NULL);
        if (*client_fd < 0) {
             free(client_fd);
            if (errno == EINTR) break;
            continue;
        }
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            close(*client_fd);
            free(client_fd);
        }
        pthread_detach(tid);
    }

    if (listen_fd >= 0) close(listen_fd);
    if (info_buf) free(info_buf);

    return 0;
}
