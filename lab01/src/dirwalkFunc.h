#ifndef DIRWALK_H
#define DIRWALK_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <limits.h>

typedef struct {
    int show_links;
    int show_dirs;
    int show_files;
    int sort_output;
} Options;

void walk_directory(const char *path, const Options *options, int filter);

#endif

