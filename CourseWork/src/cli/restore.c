#include "trashctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int cmd_restore(const char *base, const char *id) {
    char info_path[PATH_MAX], file_path[PATH_MAX];
    snprintf(info_path, sizeof(info_path),
             "%s/.trash/info/%s.info", base, id);
    snprintf(file_path, sizeof(file_path),
             "%s/.trash/files/%s", base, id);

    FILE *fp = fopen(info_path, "r");
    if (!fp) {
        fprintf(stderr, "No such entry: %s\n", id);
        return 1;
    }
    char original[PATH_MAX] = "";
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "original_path=%[^\n]", original) == 1)
            break;
    }
    fclose(fp);

    if (original[0] == '\0') {
        fprintf(stderr, "Corrupt info for %s\n", id);
        return 1;
    }

    char *dup = strdup(original);
    char *dir = dirname(dup);
    if (access(dir, F_OK) != 0) {
        fprintf(stderr, "Target directory does not exist: %s\n", dir);
        free(dup);
        return 1;
    }
    free(dup);

    if (rename(file_path, original) != 0) {
        perror("restore failed");
        return 1;
    }

    if (unlink(info_path) != 0) {
        perror("remove info failed");
    }

    printf("Restored %s → %s\n", id, original);
    return 0;
}
