#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

typedef struct {
    char id[64];
    char original_path[PATH_MAX];
    char deleted_time[32];
    off_t size;
} TrashEntry;


int load_trash_entries(const char *base, TrashEntry **out, size_t *count);
int restore_entry(const char *base, const char *id);
int purge_entry(const char *base, const char *id);
int purge_all(const char *base);

int cmd_list(const char *base);
int cmd_restore(const char *base, const char *id);
int cmd_purge(const char *base, const char *id);
int cmd_purge_all(const char *base);
int cmd_browse(const char *base, const char *start_dir);
