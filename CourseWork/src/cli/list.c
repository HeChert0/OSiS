#include "trashctl.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

int cmd_list(const char *base) {
    char info_dir[PATH_MAX];
    snprintf(info_dir, sizeof(info_dir), "%s/.trash/info", base);

    DIR *d = opendir(info_dir);
    if (!d) {
        perror("opendir info");
        return 1;
    }

    printf("%-20s %-20s %-8s %s\n",
           "ID", "Deleted Time", "Size", "Original Path");

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".info") != 0)
            continue;

        char id[64];
        strncpy(id, entry->d_name, len - 5);
        id[len - 5] = '\0';

        char path[PATH_MAX*2];
        snprintf(path, sizeof(path), "%s/%s", info_dir, entry->d_name);
        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        char original[PATH_MAX] = "";
        char deleted[32] = "";
        long size = 0;
        char line[256];

        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "original_path=%[^\n]", original) == 1) {}
            else if (sscanf(line, "deleted_time=%[^\n]", deleted) == 1) {}
            else if (sscanf(line, "size=%ld", &size) == 1) {}
        }
        fclose(fp);

        printf("%-20s %-20s %-8ld %s\n",
               id, deleted, size, original);
    }
    closedir(d);
    return 0;
}
