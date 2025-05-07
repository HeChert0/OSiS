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
#include <signal.h>

// Минимальная и максимальная ширина левой панели
#define PANEL_WIDTH_MIN 20
#define PANEL_WIDTH_MAX 60

static WINDOW *win_list, *win_info, *win_status;
static char cwd[PATH_MAX];
static int rows, cols;
static int panel_w;              // текущая ширина левой панели
static int highlight = 0;        // текущий индекс выделения
static volatile sig_atomic_t winch_flag = 0;  // флаг SIGWINCH

// Обработчик изменения размера
static void on_winch(int sig) {
    (void)sig;
    winch_flag = 1;
}

// Подсказочная строка внизу
static void draw_status() {
    wbkgd(win_status, COLOR_PAIR(1));
    werase(win_status);
    mvwprintw(win_status, 0, 1,
              " Arrows:Move  Enter:Open  Backspace:Up  Del:Trash  F1:Help  q:Quit ");
    wrefresh(win_status);
}

// Сосчитать число элементов в cwd
static int build_items_count() {
    DIR *d = opendir(cwd);
    if (!d) return 0;
    int cnt = 0;
    while (readdir(d)) cnt++;
    closedir(d);
    return cnt;
}

// Левая панель
static void draw_list() {
    werase(win_list);
    box(win_list, 0, 0);
    DIR *d = opendir(cwd);
    if (!d) {
        mvwprintw(win_list, 1,1, "Cannot open %s", cwd);
        wrefresh(win_list);
        return;
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
}

// Правая панель
static void draw_info() {
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
                mvwprintw(win_info, 2, 1, "%s",
                          S_ISDIR(st.st_mode) ? "<DIR>" : "");
                mvwprintw(win_info, 3, 1, "Size: %ld", (long)st.st_size);
                mvwprintw(win_info, 4, 1, "Mode: %o", st.st_mode & 0777);
            }
            break;
        }
    }
    closedir(d);
    wrefresh(win_info);
}

// Пересоздать окна и подстроить их под новые размеры
static void resize_windows() {
    // Получить новый размер экрана
    getmaxyx(stdscr, rows, cols);
    // Вычислить ширину левой панели
    panel_w = cols / 3;
    if (panel_w < PANEL_WIDTH_MIN) panel_w = PANEL_WIDTH_MIN;
    if (panel_w > PANEL_WIDTH_MAX) panel_w = PANEL_WIDTH_MAX;
    // Корректируем highlight
    int cnt = build_items_count();
    if (highlight >= cnt) highlight = cnt > 0 ? cnt - 1 : 0;

    clear();                     // полностью очистить фон
    refresh();

    // Уничтожаем старые
    delwin(win_list);
    delwin(win_info);
    delwin(win_status);

    // Создаём новые
    win_list   = newwin(rows - 1, panel_w,    0,           0);
    win_info   = newwin(rows - 1, cols - panel_w, 0,      panel_w);
    win_status = newwin(1,         cols,      rows - 1,    0);

    // Перерисовываем
    draw_list();
    draw_info();
    draw_status();
}

// Основная функция browse
int cmd_browse(const char *base, const char *start_dir) {
    // Локаль и сигналы
    setlocale(LC_ALL, "");
    struct sigaction sa = { .sa_handler = on_winch, .sa_flags = SA_RESTART };
    sigaction(SIGWINCH, &sa, NULL);

    // Стартовый каталог
    strncpy(cwd, start_dir, sizeof(cwd));

    // ncurses
    initscr();
    clear();                     // чистим фон
    refresh();                   // обновляем пустой экран
    clearok(stdscr, TRUE);
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_CYAN);
    }

    // Первичная установка окон
    resize_windows();
    refresh();

    int ch;
    while ((ch = wgetch(stdscr)) != 'q') {
        // Размер терминала изменился?
        if (winch_flag || ch == KEY_RESIZE) {
            winch_flag = 0;
            resizeterm(0, 0);
            clear();                     // сбрасываем старое изображение
            refresh();                   // фантомные остатки исчезнут
            resize_windows();
            resize_windows();
            continue;
        }

        // F1 - помощь
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
            resize_windows();
            continue;
        }

        int cnt = build_items_count();
        switch (ch) {
            case KEY_UP:
                if (highlight > 0) highlight--;
                break;
            case KEY_DOWN:
                if (highlight < cnt - 1) highlight++;
                break;
            case 10: {  // Enter
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
            case KEY_DC: { // Delete
                DIR *d = opendir(cwd);
                if (d) {
                    struct dirent *e;
                    int idx = 0;
                    while ((e = readdir(d))) {
                        if (idx++ == highlight) {
                            char full[PATH_MAX];
                            snprintf(full, sizeof(full), "%s/%s", cwd, e->d_name);
                            unlink(full);  // переместится в .trash
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

        // Перерисовать экраны
        resize_windows();
    }

    // Выход
    delwin(win_list);
    delwin(win_info);
    delwin(win_status);
    endwin();
    return 0;
}
