#define _GNU_SOURCE
#include "trashctl.h"
#include <locale.h>
#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>

#define PANEL_W_MIN 20
#define PANEL_W_MAX 60

#ifndef NAME_MAX
#  define NAME_MAX 255
#endif

typedef enum { MODE_FS, MODE_TRASH } BrowseMode;
typedef struct {
    char id[NAME_MAX+1];
    char disp[NAME_MAX+1];
    bool is_dir;
} BrowserEntry;

static SCREEN   *screen      = NULL;
static WINDOW   *win_list    = NULL;
static WINDOW   *win_info    = NULL;
static WINDOW   *win_status  = NULL;
static BrowseMode mode       = MODE_FS;
static volatile sig_atomic_t winch_flag = 0;
static int      rows, cols;
static int      panel_w;
static int      highlight    = 0;
static int      scroll_offset= 0;
static char     cwd[PATH_MAX];
static char     trash_base[PATH_MAX];
static char     feedback[80] = "";
static void get_trash_paths(char *files_dir, char *info_dir);

static void on_winch(int sig) {
    (void)sig;
    winch_flag = 1;
}

static void on_sigint(int sig) {
    (void)sig;
    if (win_list)   { delwin(win_list);   win_list   = NULL; }
    if (win_info)   { delwin(win_info);   win_info   = NULL; }
    if (win_status) { delwin(win_status); win_status = NULL; }
    endwin();
    if (screen) {
        delscreen(screen);
        screen = NULL;
    }
    _exit(1);
}


static int cmp_entry(const void *a, const void *b) {
    const BrowserEntry *ea = a, *eb = b;
    if (ea->is_dir != eb->is_dir) {
        return (eb->is_dir ? 1 : 0) - (ea->is_dir ? 1 : 0);
    }
    return strcasecmp(ea->disp, eb->disp);
}

static size_t build_trash_entries(BrowserEntry **out_arr) {
    char files_dir[PATH_MAX], info_dir[PATH_MAX];
    get_trash_paths(files_dir, info_dir);

    DIR *d = opendir(files_dir);
    if (!d) {
        *out_arr = NULL;
        return 0;
    }

    BrowserEntry *arr = NULL;
    size_t cnt = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        if (cnt == cap) {
            cap = cap ? cap * 2 : 16;
            arr = realloc(arr, cap * sizeof(*arr));
        }
        strncpy(arr[cnt].id, e->d_name, NAME_MAX);
        arr[cnt].id[NAME_MAX] = '\0';

        arr[cnt].disp[0] = '\0';
        char info_path[PATH_MAX];
        snprintf(info_path, sizeof(info_path),
                 "%s/%s.info", info_dir, e->d_name);
        FILE *fp = fopen(info_path, "r");
        if (fp) {
            char line[PATH_MAX];
            while (fgets(line, sizeof(line), fp)) {
                if (!strncmp(line, "original_path=", 14)) {
                    char *p = strrchr(line + 14, '/');
                    if (p) strncpy(arr[cnt].disp, p+1, NAME_MAX);
                    else strncpy(arr[cnt].disp, line+14, NAME_MAX);
                    arr[cnt].disp[strcspn(arr[cnt].disp, "\n")] = '\0';
                    break;
                }
            }
            fclose(fp);
        }

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", files_dir, e->d_name);
        struct stat st;
        arr[cnt].is_dir = (stat(full, &st)==0 && S_ISDIR(st.st_mode));

        cnt++;
    }
    closedir(d);

    qsort(arr, cnt, sizeof(*arr), cmp_entry);

    *out_arr = arr;
    return cnt;
}


static int build_items_count(void) {
     if (mode == MODE_FS) {
         DIR *d = opendir(cwd);
         if (!d) return 0;
         int cnt = 0;
         struct dirent *e;
         while ((e = readdir(d))) {
             cnt++;
         }
         closedir(d);
         return cnt;
     } else {
         char files_dir[PATH_MAX], info_dir[PATH_MAX];
         get_trash_paths(files_dir, info_dir);
        DIR *d = opendir(files_dir);
         if (!d) return 0;
         int cnt = 0;
         struct dirent *e;
         while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) {
                continue;
            }
             cnt++;
         }
         closedir(d);
         return cnt;
     }
}
static void get_trash_paths(char *files_dir, char *info_dir) {
    snprintf(files_dir, PATH_MAX, "%s/.trash/files", trash_base);
    snprintf(info_dir,  PATH_MAX, "%s/.trash/info",  trash_base);
}

static int visible_items(void) {
    return (rows - 1) - 2;
}



static void draw_list(void) {
    werase(win_list);
    box(win_list, 0, 0);

    if (mode == MODE_FS) {
        wmove(win_list, 0, 2);
        waddstr(win_list, "FS: ");
        waddstr(win_list, cwd);

        DIR *d = opendir(cwd);
        if (!d) {
            wmove(win_list, 1, 1);
            waddstr(win_list, "Cannot open");
            wrefresh(win_list);
            return;
        }

        int vis = visible_items();
        int idx = 0, y = 1;
        struct dirent *e;
        while ((e = readdir(d))) {
            if (idx < scroll_offset) { idx++; continue; }
            if (y > vis)             break;
            if (idx == highlight)    wattron(win_list, A_REVERSE);
            wmove(win_list, y, 1);
            waddstr(win_list, e->d_name);
            if (idx == highlight)    wattroff(win_list, A_REVERSE);
            idx++;  y++;
        }
        closedir(d);
        wrefresh(win_list);
        return;
    }

    wmove(win_list, 0, 2);
    waddstr(win_list, "Trash");

    BrowserEntry *arr;
    size_t cnt = build_trash_entries(&arr);
    if (cnt == 0) {
        wmove(win_list, 1, 1);
        waddstr(win_list, "Empty");
        free(arr);
        wrefresh(win_list);
        return;
    }

    int vis = visible_items();
    if (highlight < scroll_offset)          scroll_offset = highlight;
    else if (highlight >= scroll_offset+vis) scroll_offset = highlight - vis + 1;

    int y = 1;
    for (size_t i = scroll_offset; i < cnt && y <= vis; i++, y++) {
        if ((int)i == highlight) wattron(win_list, A_REVERSE);
        wmove(win_list, y, 1);
        waddstr(win_list, arr[i].disp);
        int idcol = panel_w - (int)strlen(arr[i].id) - 2;
        if (idcol > (int)strlen(arr[i].disp) + 2) {
            wmove(win_list, y, idcol);
            waddstr(win_list, arr[i].id);
        }
        if ((int)i == highlight) wattroff(win_list, A_REVERSE);
    }

    free(arr);
    wrefresh(win_list);
}


static void draw_info(void) {
    werase(win_info);
    box(win_info, 0, 0);

    if (mode == MODE_FS) {
        DIR *d = opendir(cwd);
        if (!d) {
            wrefresh(win_info);
            return;
        }
        struct dirent *e;
        int idx = 0;
        while ((e = readdir(d))) {
            if (idx++ == highlight) {
                char full[PATH_MAX];
                snprintf(full, sizeof(full), "%s/%s", cwd, e->d_name);
                struct stat st;
                if (stat(full, &st) == 0) {
                    wmove(win_info, 1, 1);
                    waddstr(win_info, e->d_name);
                    if (S_ISDIR(st.st_mode)) {
                        wmove(win_info, 2, 1);
                        waddstr(win_info, "<DIR>");
                    }
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Size: %ld", (long)st.st_size);
                    wmove(win_info, 3, 1);
                    waddstr(win_info, buf);
                    snprintf(buf, sizeof(buf), "Mode: %o", st.st_mode & 0777);
                    wmove(win_info, 4, 1);
                    waddstr(win_info, buf);
                }
                break;
            }
        }
        closedir(d);
    } else {
        BrowserEntry *arr;
        size_t cnt = build_trash_entries(&arr);

        if (cnt == 0 || highlight < 0 || (size_t)highlight >= cnt) {
            wmove(win_info, 1, 1);
            waddstr(win_info, "Empty or invalid");
            free(arr);
            wrefresh(win_info);
            return;
        }

        char info_path[PATH_MAX];
        snprintf(info_path, sizeof(info_path),
                 "%s/.trash/info/%s.info", trash_base, arr[highlight].id);

        FILE *fp = fopen(info_path, "r");
        if (!fp) {
            wmove(win_info, 1, 1);
            waddstr(win_info, "Cannot open info");
            free(arr);
            wrefresh(win_info);
            return;
        }

        char line[PATH_MAX];
        int row = 1;
        while (fgets(line, sizeof(line), fp)) {
            if (!strncmp(line, "original_path=", 14)) {
                wmove(win_info, row, 1);
                waddstr(win_info, "Orig: ");
                waddstr(win_info, line + 14);
                row++;
            }
            else if (!strncmp(line, "deleted_time=", 13)) {
                wmove(win_info, row, 1);
                waddstr(win_info, "Del:  ");
                waddstr(win_info, line + 13);
                row++;
            }
            else if (!strncmp(line, "size=", 5)) {
                wmove(win_info, row, 1);
                waddstr(win_info, "Size: ");
                waddstr(win_info, line + 5);
                row++;
            }
        }
        fclose(fp);
        free(arr);
    }

    wrefresh(win_info);
}

static void draw_status(void) {
    werase(win_status);
    wbkgd(win_status, COLOR_PAIR(1));

    if (mode == MODE_FS) {
        wmove(win_status, 0, 1);
        waddstr(win_status,
                "Arrows Move Enter Open Backspace Up Del Trash  T TrashMode F1 Help q Quit");
    } else {
        wmove(win_status, 0, 1);
        waddstr(win_status,
                "Arrows Move r Restore d Purge one  D Purge all  T FSMode F1 Help q Quit");
    }

    int len = strlen(feedback);
    if (len > 0 && len < cols - 2) {
        wmove(win_status, 0, cols - len - 1);
        waddstr(win_status, feedback);
    }

    wrefresh(win_status);
}

int cmd_browse(const char *base, const char *start_dir) {
    setlocale(LC_ALL, "");

    struct sigaction sa_w = { .sa_handler = on_winch,  .sa_flags = SA_RESTART };
    struct sigaction sa_i = { .sa_handler = on_sigint, .sa_flags = SA_RESTART };
    sigaction(SIGWINCH, &sa_w, NULL);
    sigaction(SIGINT,  &sa_i, NULL);

    strncpy(trash_base, base, sizeof(trash_base) - 1);
    trash_base[sizeof(trash_base)-1] = '\0';
    strncpy(cwd, start_dir, sizeof(cwd) - 1);
    cwd[sizeof(cwd)-1] = '\0';


    screen = newterm(NULL, stdout, stdin);
    if (!screen) {
        fprintf(stderr, "Error: cannot initialize terminal\n");
        return 1;
    }
    set_term(screen);

    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
    }
    clearok(stdscr, TRUE);
    refresh();


    getmaxyx(stdscr, rows, cols);
    panel_w = cols / 3;
    if (panel_w < PANEL_W_MIN) panel_w = PANEL_W_MIN;
    if (panel_w > PANEL_W_MAX) panel_w = PANEL_W_MAX;

    win_list   = newwin(rows - 1, panel_w,        0, 0);
    win_info   = newwin(rows - 1, cols - panel_w, 0, panel_w);
    win_status = newwin(1,        cols,          rows - 1, 0);

    clearok(win_list,   TRUE);
    clearok(win_info,   TRUE);
    clearok(win_status, TRUE);

    draw_list();
    draw_info();
    draw_status();

    int ch;
    while ((ch = wgetch(stdscr)) != 'q') {
        if (winch_flag || ch == KEY_RESIZE) {
            winch_flag = 0;
            resizeterm(0, 0);
            getmaxyx(stdscr, rows, cols);
            panel_w = cols / 3;
            if (panel_w < PANEL_W_MIN) panel_w = PANEL_W_MIN;
            if (panel_w > PANEL_W_MAX) panel_w = PANEL_W_MAX;

            wresize(win_list,   rows - 1, panel_w);
            mvwin(  win_info,   0, panel_w);
            wresize(win_info,   rows - 1, cols - panel_w);
            mvwin(  win_status, rows - 1, 0);
            wresize(win_status, 1, cols);

            draw_list();
            draw_info();
            draw_status();
            continue;
        }

        feedback[0] = '\0';

        int cnt = (mode == MODE_FS)
        ? build_items_count()
        : build_items_count();

        switch (ch) {
            case 'T':
            case 't':
                mode = (mode == MODE_FS ? MODE_TRASH : MODE_FS);
                highlight = scroll_offset = 0;
                break;

            case KEY_UP:
                if (highlight > 0) {
                    highlight--;
                }
                break;

            case KEY_DOWN:
                if (highlight < cnt - 1) {
                    highlight++;
                }
                break;

            case 10:
                if (mode == MODE_FS) {
                    DIR *d = opendir(cwd);
                    if (d) {
                        struct dirent *e;
                        int idx = 0;
                        while ((e = readdir(d))) {
                            if (idx++ == highlight) {
                                char full[PATH_MAX];
                                snprintf(full, sizeof(full), "%s/%s", cwd, e->d_name);
                                struct stat st;
                                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                                    strncpy(cwd, full, sizeof(cwd) - 1);
                                    cwd[sizeof(cwd)-1] = '\0';
                                    highlight = scroll_offset = 0;
                                }
                                break;
                            }
                        }
                        closedir(d);
                    }
                }
                break;

            case KEY_BACKSPACE:
            case 8:
            case 127:
                if (mode == MODE_FS) {
                    char *p = strrchr(cwd, '/');
                    if (p && p != cwd) {
                        *p = '\0';
                    } else {
                        strcpy(cwd, "/");
                    }
                    highlight = scroll_offset = 0;
                }
                break;

            case KEY_DC:
                if (mode == MODE_FS) {
                    DIR *d = opendir(cwd);
                    if (d) {
                        struct dirent *e;
                        int idx = 0;
                        while ((e = readdir(d))) {
                            if (idx++ == highlight) {
                                char full[PATH_MAX];
                                snprintf(full, sizeof(full), "%s/%s", cwd, e->d_name);
                                unlink(full);
                                break;
                            }
                        }
                        closedir(d);
                    }
                }
                break;

            case 'R':
            case 'r':
                if (mode == MODE_TRASH) {
                    BrowserEntry *arr;
                    size_t cnt = build_trash_entries(&arr);
                    if (highlight >= 0 && (size_t)highlight < cnt) {
                        char *id = arr[highlight].id;

                        int fd = open("/dev/null", O_WRONLY);
                        int so = dup(STDOUT_FILENO), se = dup(STDERR_FILENO);
                        dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd);

                        int ret = cmd_restore(trash_base, id);

                        dup2(so, STDOUT_FILENO); dup2(se, STDERR_FILENO);
                        close(so); close(se);

                        if (ret == 0) snprintf(feedback, sizeof(feedback),
                            "Restored %s", id);
                        else          snprintf(feedback, sizeof(feedback),
                            "Restore failed");
                    }
                    free(arr);
                    draw_list(); draw_info(); draw_status();
                }
                break;

            case 'd':
                if (mode == MODE_TRASH) {
                    BrowserEntry *arr;
                    size_t cnt = build_trash_entries(&arr);
                    if (highlight >= 0 && (size_t)highlight < cnt) {
                        char *id = arr[highlight].id;
                        int fd = open("/dev/null", O_WRONLY);
                        int so = dup(STDOUT_FILENO), se = dup(STDERR_FILENO);
                        dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd);

                        int ret = cmd_purge(trash_base, id);

                        dup2(so, STDOUT_FILENO); dup2(se, STDERR_FILENO);
                        close(so); close(se);

                        if (ret == 0) {
                            snprintf(feedback, sizeof(feedback),
                                     "Purged %s", id);
                            if (--highlight < 0) highlight = 0;
                        }
                        else {
                            snprintf(feedback, sizeof(feedback),
                                     "Purge failed");
                        }
                    }
                    free(arr);
                    draw_list(); draw_info(); draw_status();
                }
                break;

            case 'D':
                if (mode == MODE_TRASH) {
                    int fd = open("/dev/null", O_WRONLY);
                    int so = dup(STDOUT_FILENO), se = dup(STDERR_FILENO);
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);

                    int ret = cmd_purge_all(trash_base);

                    dup2(so, STDOUT_FILENO);
                    dup2(se, STDERR_FILENO);
                    close(so);
                    close(se);

                    if (ret == 0) {
                        snprintf(feedback, sizeof(feedback),
                                 "Trash emptied");
                    } else {
                        snprintf(feedback, sizeof(feedback),
                                 "Failed to empty trash");
                    }
                    highlight = scroll_offset = 0;
                }
                break;

            case KEY_F(1):
            {
                int h = 12, w = 50;
                int y = (rows - h) / 2;
                int x = (cols - w) / 2;
                WINDOW *wh = newwin(h, w, y, x);
                wbkgd(wh, COLOR_PAIR(1));
                box(wh, 0, 0);
                wmove(wh, 1, 2); waddstr(wh, "Help - trashctl browse");
                wmove(wh, 3, 2); waddstr(wh, "Arrows    - move cursor");
                wmove(wh, 4, 2); waddstr(wh, "Enter     - open directory");
                wmove(wh, 5, 2); waddstr(wh, "Backspace - go up");
                wmove(wh, 6, 2); waddstr(wh, "Del       - delete (to trash)");
                wmove(wh, 7, 2); waddstr(wh, "T         - toggle Trash mode");
                wmove(wh, 8, 2); waddstr(wh, "r/d/D     - restore/delete/delete all");
                wmove(wh, 9, 2); waddstr(wh, "q or Esc  - quit browser");
                wrefresh(wh);
                wgetch(wh);
                delwin(wh);
            }
            break;

            default:
                break;
        }

        draw_list();
        draw_info();
        draw_status();
    }

    delwin(win_list);
    delwin(win_info);
    delwin(win_status);
    endwin();
    delscreen(screen);
    screen = NULL;
    return 0;
}
