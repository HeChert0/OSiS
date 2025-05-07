#include "trashctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

// Удалить одну запись
int cmd_purge(const char *base, const char *id) {
    char info_path[PATH_MAX], file_path[PATH_MAX];
    snprintf(info_path, sizeof(info_path),
             "%s/.trash/info/%s.info", base, id);
    snprintf(file_path, sizeof(file_path),
             "%s/.trash/files/%s", base, id);

    int ok = 0;
    if (unlink(file_path) != 0) {
        perror("remove file failed");
    } else ok++;

    if (unlink(info_path) != 0) {
        perror("remove info failed");
    } else ok++;

    if (ok == 2) {
        printf("Purged %s\n", id);
        return 0;
    }
    return 1;
}

// Удалить всё
int cmd_purge_all(const char *base) {
    char info_dir[PATH_MAX], file_dir[PATH_MAX];
    snprintf(info_dir, sizeof(info_dir), "%s/.trash/info", base);
    snprintf(file_dir, sizeof(file_dir), "%s/.trash/files", base);

    DIR *d = opendir(info_dir);
    if (!d) {
        perror("opendir info");
        return 1;
    }
    struct dirent *entry;
    int status = 0;

    while ((entry = readdir(d)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".info") != 0)
            continue;

        // id без .info
        char id[64];
        strncpy(id, entry->d_name, len - 5);
        id[len - 5] = '\0';

        if (cmd_purge(base, id) != 0) {
            status = 1;
        }
    }
    closedir(d);
    return status;
}
