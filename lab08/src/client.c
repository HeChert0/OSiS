#include "common.h"
#include <libgen.h>
#include <signal.h>
#include <sys/stat.h>

#define MAXLINE 1024

static volatile sig_atomic_t sigint_received = 0;
static FILE *client_sockfp = NULL;

void interactive_mode(FILE *sock_fp);
void process_file_commands(FILE *sock_fp, const char *filename);


void handle_sigint(int signo) {
    if (client_sockfp && !sigint_received) {
        sigint_received = 1;

        fputs("QUIT\r\n", client_sockfp);
        fflush(client_sockfp);
        fclose(client_sockfp);
         _exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip:port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *sep = strchr(argv[1], ':');
    if (!sep) {
        fprintf(stderr, "Invalid server address, expected ip:port\n");
        return EXIT_FAILURE;
    }
    char ip[INET_ADDRSTRLEN];
    size_t ip_len = sep - argv[1];
    if (ip_len >= sizeof(ip)) ip_len = sizeof(ip) - 1;
    memcpy(ip, argv[1], ip_len);
    ip[ip_len] = '\0';
    int port = atoi(sep + 1);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) die("socket");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
        die("inet_pton");

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        die("connect");

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) < 0)
        die("sigaction");

    FILE *sock_fp = fdopen(sockfd, "r+");
    if (!sock_fp) {
        perror("fdopen");
        close(sockfd);
        return EXIT_FAILURE;
    }

     client_sockfp = sock_fp;

    char line[MAXLINE];
    while (fgets(line, sizeof(line), sock_fp)) {
        if (strcmp(line, "\r\n") == 0)
            break;
        fputs(line, stdout);
    }

    interactive_mode(sock_fp);

    fclose(sock_fp);
    return 0;
}

void process_file_commands(FILE *sock_fp, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file '%s': %s\n", filename, strerror(errno));
        return;
    }

    char sendbuf[MAXLINE];
    char recvbuf[MAXLINE];
    int should_exit = 0;

    while (fgets(sendbuf, sizeof(sendbuf), file) && !should_exit) {
        sendbuf[strcspn(sendbuf, "\r\n")] = '\0';
        if (strlen(sendbuf) == 0) continue;

        fprintf(sock_fp, "%s\r\n", sendbuf);
        fflush(sock_fp);

        if (strcmp(sendbuf, "QUIT") == 0) {
            if (fgets(recvbuf, sizeof(recvbuf), sock_fp)) {
                fputs(recvbuf, stdout);
                if (strstr(recvbuf, "BYE") != NULL) {
                    should_exit = 1;
                }
            }
        } else if (strcmp(sendbuf, "INFO") == 0 || strcmp(sendbuf, "LIST") == 0) {
            while (fgets(recvbuf, sizeof(recvbuf), sock_fp)) {
                if (strcmp(recvbuf, "\r\n") == 0) break;
                fputs(recvbuf, stdout);
            }
        } else {
            if (fgets(recvbuf, sizeof(recvbuf), sock_fp)) {
                fputs(recvbuf, stdout);
            }
        }
    }

    fclose(file);

    if (should_exit) {
        fclose(sock_fp);
        exit(EXIT_SUCCESS);
    }
}

void interactive_mode(FILE *sock_fp) {
    char sendbuf[MAXLINE];
    char recvbuf[MAXLINE];
    int should_exit = 0;

    while (!should_exit) {
        fputs("> ", stdout);
        fflush(stdout);

        if (!fgets(sendbuf, sizeof(sendbuf), stdin)) break;

        sendbuf[strcspn(sendbuf, "\r\n")] = '\0';

        if (sendbuf[0] == '@') {
            process_file_commands(sock_fp, sendbuf + 1);
            continue;
        }

        fprintf(sock_fp, "%s\r\n", sendbuf);
        fflush(sock_fp);

        if (strcmp(sendbuf, "QUIT") == 0) {
            if (fgets(recvbuf, sizeof(recvbuf), sock_fp)) {
                fputs(recvbuf, stdout);
                if (strstr(recvbuf, "BYE") != NULL) {
                    should_exit = 1;
                }
            }
        } else if (strcmp(sendbuf, "INFO") == 0 || strcmp(sendbuf, "LIST") == 0) {
            while (fgets(recvbuf, sizeof(recvbuf), sock_fp)) {
                if (strcmp(recvbuf, "\r\n") == 0) break;
                fputs(recvbuf, stdout);
            }
        } else {
            if (fgets(recvbuf, sizeof(recvbuf), sock_fp)) {
                fputs(recvbuf, stdout);
            }
        }
    }

    fclose(sock_fp);
    exit(EXIT_SUCCESS);
}
