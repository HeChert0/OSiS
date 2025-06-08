#define _GNU_SOURCE
#include "trashctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ftw.h>
#include <errno.h>

static const char *TRASH_INFO_SUB = ".trash/info";
static const char *TRASH_FILES_SUB = ".trash/files";

static char info_path[PATH_MAX];
static char file_path[PATH_MAX];

static int remove_tree_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)ftwbuf;
    int ret = 0;
    if (typeflag == FTW_DP) {
        ret = rmdir(fpath);
    } else {
        ret = remove(fpath);
    }
    if (ret != 0) {
        fprintf(stderr, "remove_tree: failed on %s: %s\n", fpath, strerror(errno));
    }
    return ret;
}

static int remove_tree(const char *path) {
    return nftw(path, remove_tree_cb, 64, FTW_DEPTH | FTW_PHYS);
}

int cmd_purge(const char *base, const char *id) {
    snprintf(info_path, sizeof(info_path),
             "%s/%s/%s.info", base, TRASH_INFO_SUB, id);
    snprintf(file_path, sizeof(file_path),
             "%s/%s/%s",      base, TRASH_FILES_SUB, id);

    struct stat st;
    int ok = 0;

    if (stat(file_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (remove_tree(file_path) == 0) {
                ok++;
            } else {
                fprintf(stderr, "remove file failed: %s is a directory\n", file_path);
            }
        } else {
            if (unlink(file_path) == 0) {
                ok++;
            } else {
                perror("remove file failed");
            }
        }
    } else {
        perror("stat failed on file_path");
    }

    if (unlink(info_path) == 0) {
        ok++;
    } else {
        perror("remove info failed");
    }

    if (ok == 2) {
        printf("Purged %s\n", id);
        return 0;
    }
    return 1;
}

int cmd_purge_all(const char *base) {
    char info_dir[PATH_MAX];
    snprintf(info_dir, sizeof(info_dir), "%s/%s", base, TRASH_INFO_SUB);

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
