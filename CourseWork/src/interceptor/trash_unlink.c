#define _GNU_SOURCE
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>

#define TRASH_DIR_FMT   "%s/.trash"
#define TRASH_FILES_FMT "%s/.trash/files"
#define TRASH_INFO_FMT  "%s/.trash/info"

typedef int (*orig_unlink_fn)(const char *);
typedef int (*orig_unlinkat_fn)(int, const char *, int);

static orig_unlink_fn   real_unlink   = NULL;
static orig_unlinkat_fn real_unlinkat = NULL;

static void init_real_funcs(void) {
    if (!real_unlink) {
        real_unlink = (orig_unlink_fn)dlsym(RTLD_NEXT, "unlink");
    }
    if (!real_unlinkat) {
        real_unlinkat = (orig_unlinkat_fn)dlsym(RTLD_NEXT, "unlinkat");
    }
}

static const char *get_trash_base(void) {
    const char *env = getenv("TRASH_BASE");
    return env ? env : ".";
}

static void ensure_directories(void) {
    char path[PATH_MAX];
    const char *base = get_trash_base();

    snprintf(path, sizeof(path), TRASH_DIR_FMT, base);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), TRASH_FILES_FMT, base);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), TRASH_INFO_FMT, base);
    mkdir(path, 0755);
}

static char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return NULL;
    return strdup(dot);
}

static char *generate_unique_name(const char *orig_name) {
    static char buf[256];
    time_t now = time(NULL);
    int r = rand() % 100000;
    char *ext = get_file_extension(orig_name);

    if (ext) {
        snprintf(buf, sizeof(buf), "%ld_%05d%s", now, r, ext);
        free(ext);
    } else {
        snprintf(buf, sizeof(buf), "%ld_%05d", now, r);
    }
    return buf;
}

static int write_info_file(const char *newname, const char *orig_path, off_t size) {
    char info_path[PATH_MAX];
    const char *base = get_trash_base();

    snprintf(info_path, sizeof(info_path),
             TRASH_INFO_FMT "/%s.info", base, newname);

    FILE *fp = fopen(info_path, "w");
    if (!fp) return -1;

    time_t now = time(NULL);
    char timestr[32];
    strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", localtime(&now));

    fprintf(fp, "original_path=%s\n", orig_path);
    fprintf(fp, "deleted_time=%s\n", timestr);
    fprintf(fp, "size=%ld\n", (long)size);

    fclose(fp);
    return 0;
}

static int move_to_trash(const char *abs_path) {
    ensure_directories();

    struct stat st;
    char orig_real[PATH_MAX];
    if (realpath(abs_path, orig_real) == NULL) {

        strncpy(orig_real, abs_path, sizeof(orig_real));
        orig_real[sizeof(orig_real)-1] = '\0';
    }

    if (stat(abs_path, &st) != 0) {
        return -1;
    }

    {
        char trash_root[PATH_MAX];
        const char *base = get_trash_base();
        snprintf(trash_root, sizeof(trash_root), TRASH_DIR_FMT, base);
        if (strncmp(abs_path, trash_root, strlen(trash_root)) == 0) {
            return real_unlink(abs_path);
        }
    }

    const char *base_name = basename((char *)abs_path);
    char *newname = generate_unique_name(base_name);

    char dest_path[PATH_MAX];
    const char *base = get_trash_base();
    snprintf(dest_path, sizeof(dest_path),
             TRASH_FILES_FMT "/%s", base, newname);

    if (rename(abs_path, dest_path) != 0) {
        perror("rename to trash failed");
        return -1;
    }

    write_info_file(newname, orig_real, st.st_size);
    return 0;
}

static int build_abs_path(int dirfd, const char *pathname, char *out, size_t outlen) {
    if (pathname[0] == '/') {
        strncpy(out, pathname, outlen);
        return 0;
    }
    if (dirfd == AT_FDCWD) {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) return -1;
        snprintf(out, outlen, "%s/%s", cwd, pathname);
        return 0;
    }

    char linkpath[64];
    snprintf(linkpath, sizeof(linkpath), "/proc/self/fd/%d", dirfd);
    char dirpath[PATH_MAX];
    ssize_t len = readlink(linkpath, dirpath, sizeof(dirpath)-1);
    if (len <= 0) return -1;
    dirpath[len] = '\0';
    snprintf(out, outlen, "%s/%s", dirpath, pathname);
    return 0;
}

int unlink(const char *pathname) {
    init_real_funcs();

    char abs_path[PATH_MAX];
    if (build_abs_path(AT_FDCWD, pathname, abs_path, sizeof(abs_path)) == 0) {
        if (move_to_trash(abs_path) == 0) {
            return 0;
        }
    }
    return real_unlink(pathname);
}

int unlinkat(int dirfd, const char *pathname, int flags) {
    init_real_funcs();
    if (!(flags & AT_REMOVEDIR)) {
        char abs_path[PATH_MAX];
        if (build_abs_path(dirfd, pathname, abs_path, sizeof(abs_path)) == 0) {
            if (move_to_trash(abs_path) == 0) {
                return 0;
            }
        }
    }
    return real_unlinkat(dirfd, pathname, flags);
}
