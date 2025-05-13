#define _GNU_SOURCE
#include "trashctl.h"
#include <locale.h>
#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define PANEL_WIDTH  40

static WINDOW *win_list, *win_info, *win_status;
static char cwd[PATH_MAX];
static int rows, cols;

static void draw_status() {
    wbkgd(win_status, COLOR_PAIR(1));
    werase(win_status);
    mvwprintw(win_status, 0, 1,
              " Arrows:Move  Enter:Open  Backspace:Up  Del:Trash  F1:Help  q:Quit ");
    wrefresh(win_status);
}

static int draw_list(int highlight) {
    werase(win_list);
    box(win_list, 0, 0);
    DIR *d = opendir(cwd);
    if (!d) {
        mvwprintw(win_list, 1,1, "Cannot open %s", cwd);
        wrefresh(win_list);
        return 0;
    }
    struct dirent *e;
    int y = 1, idx = 0;
    while ((e = readdir(d))) {
        if (idx == highlight) wattron(win_list, A_REVERSE);
        mvwprintw(win_list, y, 1, "%s", e->d_name);
        if (idx == highlight) wattroff(win_list, A_REVERSE);
        y++; idx++;
    }
    closedir(d);
    wrefresh(win_list);
    return idx;
}

static void draw_info(int highlight) {
    werase(win_info);
    box(win_info, 0, 0);
    DIR *d = opendir(cwd);
    if (!d) { wrefresh(win_info); return; }

    struct dirent *e;
    int idx = 0;
    while ((e = readdir(d))) {
        if (idx++ == highlight) {
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", cwd, e->d_name);
            struct stat st;
            if (stat(full, &st)==0) {
                mvwprintw(win_info, 1, 1, "%s", e->d_name);
                mvwprintw(win_info, 2, 1, "%s", S_ISDIR(st.st_mode) ? "<DIR>" : "");
                mvwprintw(win_info, 3, 1, "Size: %ld", (long)st.st_size);
                mvwprintw(win_info, 4, 1, "Mode: %o", st.st_mode & 0777);
            }
            break;
        }
    }
    closedir(d);
    wrefresh(win_info);
}

static int build_items_count() {
    DIR *d = opendir(cwd);
    if (!d) return 0;
    int cnt = 0;
    while (readdir(d)) cnt++;
    closedir(d);
    return cnt;
}

int cmd_browse(const char *base, const char *start_dir) {
    setlocale(LC_ALL, "");

    strncpy(cwd, start_dir, sizeof(cwd));

    initscr();
    noecho();
    curs_set(FALSE);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_CYAN);
    }
    keypad(stdscr, TRUE);
    getmaxyx(stdscr, rows, cols);
    refresh();

    win_list   = newwin(rows-1, PANEL_WIDTH,       0, 0);
    win_info   = newwin(rows-1, cols-PANEL_WIDTH, 0, PANEL_WIDTH);
    win_status = newwin(1,        cols,          rows-1, 0);

    int highlight = 0;
    draw_list(highlight);
    draw_info(highlight);
    draw_status();

    int ch;
    while ((ch = wgetch(stdscr)) != 'q') {
        if (ch == KEY_F(1)) {
            int h = 10, w = 50;
            int y = (rows - h) / 2, x = (cols - w) / 2;
            WINDOW *win_help = newwin(h, w, y, x);
            wbkgd(win_help, COLOR_PAIR(2));
            box(win_help, 0, 0);
            mvwprintw(win_help, 1, 2, "Help - trashctl browse");
            mvwprintw(win_help, 3, 2, "Arrows    - move cursor");
            mvwprintw(win_help, 4, 2, "Enter     - open directory");
            mvwprintw(win_help, 5, 2, "Backspace - go up");
            mvwprintw(win_help, 6, 2, "Del       - delete (to trash)");
            mvwprintw(win_help, 7, 2, "F1        - this help");
            mvwprintw(win_help, 8, 2, "q or Esc  - quit browser");
            wrefresh(win_help);
            wgetch(win_help);
            delwin(win_help);

            draw_list(highlight);
            draw_info(highlight);
            draw_status();
            continue;
        }

        int item_count = build_items_count();
        switch (ch) {
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case KEY_DOWN:
                if (highlight < item_count-1) highlight++;
                break;
            case 10: {
                DIR *d = opendir(cwd);
                if (d) {
                    struct dirent *e;
                    int idx=0;
                    while ((e = readdir(d))) {
                        if (idx++ == highlight) {
                            char full[PATH_MAX];
                            snprintf(full, sizeof(full), "%s/%s", cwd, e->d_name);
                            struct stat st;
                            if (stat(full, &st)==0 && S_ISDIR(st.st_mode)) {
                                strncpy(cwd, full, sizeof(cwd));
                                highlight = 0;
                            }
                            break;
                        }
                    }
                    closedir(d);
                }
                break;
            }
            case KEY_BACKSPACE:
            case 8:
            case 127: {
                char *p = strrchr(cwd, '/');
                if (p && p != cwd) *p = '\0';
                else strcpy(cwd, "/");
                highlight = 0;
                break;
            }
            case KEY_DC: {
                DIR *d = opendir(cwd);
                if (d) {
                    struct dirent *e;
                    int idx=0;
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
                break;
            }
            default:
                break;
        }
        item_count = build_items_count();
        if (highlight >= item_count) highlight = item_count>0 ? item_count-1 : 0;
        draw_list(highlight);
        draw_info(highlight);
        draw_status();
    }

    delwin(win_list);
    delwin(win_info);
    delwin(win_status);
    endwin();
    return 0;
}
