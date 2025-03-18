#include "dirwalkFunc.h"

void walk_directory(const char *path, const Options *options, int filter) {
    struct dirent **namelist;
    int n = scandir(path, &namelist, NULL, options->sort_output ? alphasort : NULL);

    if (n < 0) {
        perror("scandir");
        return;
    }

    for (int i = 0; i < n; i++) {
        struct dirent *entry = namelist[i];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(entry);
            continue;
        }

        char full_path[PATH_MAX];
        size_t len = strlen(path);
        if (strcmp(path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "./%s", entry->d_name);
        } else {
            if (len > 0 && path[len - 1] == '/')
                snprintf(full_path, sizeof(full_path), "%s%s", path, entry->d_name);
            else
                snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }

        struct stat st;
        if (lstat(full_path, &st) < 0) {
            perror("lstat");
            free(entry);
            continue;
        }

        int is_symlink = S_ISLNK(st.st_mode);
        int is_dir     = S_ISDIR(st.st_mode);
        int is_file    = S_ISREG(st.st_mode);

        if (filter ?
            ((options->show_links && is_symlink) ||
            (options->show_dirs && is_dir) ||
            (options->show_files && is_file))
            : 1)
        {
            printf("%s\n", full_path);
        }

        if (is_dir && !is_symlink &&
            strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            walk_directory(full_path, options, filter);
        }

        free(entry);
    }
    free(namelist);
}
