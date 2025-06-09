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

// Минимальные/максимальные ширины
#define PANEL_W_MIN 20
#define PANEL_W_MAX 60

// Состояние: файловая система или корзина
typedef enum { MODE_FS, MODE_TRASH } BrowseMode;

static SCREEN *screen = NULL;
static WINDOW *win_list, *win_info, *win_status, *win_help;
static int rows, cols, panel_w;
static int highlight = 0, scroll = 0;
static volatile sig_atomic_t winch_flag = 0;
static BrowseMode mode = MODE_FS;
static char cwd[PATH_MAX];
static char trash_base[PATH_MAX];

static char feedback[80] = "";


static void on_sigint(int sig) {
    (void)sig;
    if (win_list)   {
        delwin(win_list);
        win_list = NULL;
    }
    if (win_info) { delwin(win_info);
      win_info   = NULL; }
    if (win_status) { delwin(win_status); win_status = NULL; }
    if (win_help) { delwin(win_help); win_help = NULL; }

    endwin();

    if (screen) {
        delscreen(screen);
        screen = NULL;
    }
        _exit(1);
}

// SIGWINCH
static void on_winch(int sig) {
    (void)sig;
    winch_flag = 1;
}


static int visible_items(void) {
    return (rows - 1) - 2;
}

// Получить абсолютный путь из TRASH_BASE
static void get_trash_paths(char *files_dir, char *info_dir) {
    snprintf(files_dir, PATH_MAX, "%s/.trash/files", trash_base);
    snprintf(info_dir, PATH_MAX,  "%s/.trash/info",  trash_base);
}

// Считает количество элементов в текущем просмотре
static int build_items_count(void) {
    DIR *d;
    if (mode == MODE_FS) {
        d = opendir(cwd);
    } else {
        char files_dir[PATH_MAX], info_dir[PATH_MAX];
        get_trash_paths(files_dir, info_dir);
        d = opendir(files_dir);
    }
    if (!d) return 0;
    int cnt = 0;
    while (readdir(d)) cnt++;
    closedir(d);
    return cnt;
}

// Отрисовка левой панели
static void draw_list(void) {
    werase(win_list);
    box(win_list, 0,0);

    DIR *d;
    if (mode == MODE_FS) {
        mvwprintw(win_list, 0,2, " FS: %s", cwd);
        d = opendir(cwd);
    } else {
        char files_dir[PATH_MAX], info_dir[PATH_MAX];
        get_trash_paths(files_dir, info_dir);
        mvwprintw(win_list, 0,2, " Trash");
        d = opendir(files_dir);
    }
    if (!d) {
        mvwprintw(win_list,1,1,"Cannot open");
        wrefresh(win_list);
        return;
    }
    struct dirent *e;
    int y=1, idx=0;
    while ((e=readdir(d))) {
        if (idx==highlight) wattron(win_list,A_REVERSE);
        mvwprintw(win_list,y,1,"%s", e->d_name);
        if (idx==highlight) wattroff(win_list,A_REVERSE);
        y++; idx++;
    }
    closedir(d);
    wrefresh(win_list);
}

// Отрисовка правой панели
static void draw_info(void) {
    werase(win_info);
    box(win_info, 0,0);

    if (mode == MODE_FS) {
        // как раньше: stat и вывод name/dir/size/mode
        DIR *d = opendir(cwd);
        if (!d) { wrefresh(win_info); return; }
        struct dirent *e; int idx=0;
        while ((e=readdir(d))) {
            if (idx++==highlight) {
                char full[PATH_MAX];
                snprintf(full,sizeof(full),"%s/%s",cwd,e->d_name);
                struct stat st;
                if (stat(full,&st)==0) {
                    mvwprintw(win_info,1,1,"%s",e->d_name);
                    if (S_ISDIR(st.st_mode)) mvwprintw(win_info,2,1,"<DIR>");
                    mvwprintw(win_info,3,1,"Size: %ld",(long)st.st_size);
                    mvwprintw(win_info,4,1,"Mode: %o",st.st_mode&0777);
                }
                break;
            }
        }
        closedir(d);
    } else {
        // Режим корзины: читаем .info
        char files_dir[PATH_MAX], info_dir[PATH_MAX];
        get_trash_paths(files_dir, info_dir);
        DIR *d = opendir(files_dir);
        if (!d) { wrefresh(win_info); return; }
        struct dirent *e; int idx=0;
        while ((e=readdir(d))) {
            if (idx++==highlight) {
                char id[PATH_MAX], info[PATH_MAX];
                snprintf(id,sizeof(id),"%s",e->d_name);
                // info file = e->d_name + ".info"
                snprintf(info,sizeof(info),"%s/%s.info",info_dir,id);
                FILE *fp = fopen(info,"r");
                if (fp) {
                    char line[256];
                    while (fgets(line,sizeof(line),fp)) {
                        if (strncmp(line,"original_path=",14)==0)
                            mvwprintw(win_info,1,1,"Orig: %s", line+14);
                        else if (strncmp(line,"deleted_time=",13)==0)
                            mvwprintw(win_info,2,1,"Del: %s", line+13);
                        else if (strncmp(line,"size=",5)==0)
                            mvwprintw(win_info,3,1,"Size: %s", line+5);
                    }
                    fclose(fp);
                }
                break;
            }
        }
        closedir(d);
    }
    wrefresh(win_info);
}

// Нижняя строка подсказок
static void draw_status(void) {
    werase(win_status);
    wbkgd(win_status, COLOR_PAIR(1));
    if (mode==MODE_FS) {
        mvwprintw(win_status,0,1,
                  "Arrows Move Enter Open Backspace Up Del Trash  T TrashMode F1 Help q Quit");
    } else {
        mvwprintw(win_status,0,1,
                  "Arrows Move r Restore D Purge  T FSMode F1 Help q Quit");
    }

    mvwprintw(win_status, 0, cols - strlen(feedback) - 2, "%s", feedback);
    wrefresh(win_status);
}

// Инициализация и главный цикл
int cmd_browse(const char *base, const char *start_dir) {
    // локаль и сигнал
    setlocale(LC_ALL,"");
    struct sigaction sa={.sa_handler=on_winch,.sa_flags=SA_RESTART};
    sigaction(SIGWINCH,&sa,NULL);
    struct sigaction sa_int = {
        .sa_handler = on_sigint,
        .sa_flags   = SA_RESTART
    };
    sigaction(SIGINT, &sa_int, NULL);

    // запомним TRASH_BASE
    snprintf(trash_base, sizeof(trash_base), "%s", base);
    snprintf(cwd, sizeof(cwd), "%s", start_dir);

    // ncurses init
    screen = newterm(NULL, stdout, stdin);
    if (!screen) {
        fprintf(stderr, "Error: cannot initialize terminal\n");
        return 1;
    }
    set_term(screen);
    noecho();
    curs_set(FALSE);
    keypad(stdscr,TRUE);
    if (has_colors()) {
        start_color();
        init_pair(1,COLOR_WHITE,COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
    }
    clearok(stdscr,TRUE);
    refresh();
    // создаём окна
    getmaxyx(stdscr,rows,cols);
    panel_w = cols/3; if (panel_w<PANEL_W_MIN) panel_w=PANEL_W_MIN;
    if (panel_w>PANEL_W_MAX) panel_w=PANEL_W_MAX;
    win_list   = newwin(rows-1,panel_w,0,0);
    win_info   = newwin(rows-1,cols-panel_w,0,panel_w);
    win_status = newwin(1,cols,rows-1,0);
    clearok(win_list,TRUE); clearok(win_info,TRUE); clearok(win_status,TRUE);

    // первая отрисовка
    draw_list(); draw_info(); draw_status();

    int ch;
    while ((ch=wgetch(stdscr))!='q') {
        if (winch_flag||ch==KEY_RESIZE) {
            winch_flag=0; resizeterm(0,0);
            getmaxyx(stdscr,rows,cols);
            panel_w=cols/3; if(panel_w<PANEL_W_MIN)panel_w=PANEL_W_MIN;
            if(panel_w>PANEL_W_MAX)panel_w=PANEL_W_MAX;
            wresize(win_list,rows-1,panel_w);
            mvwin(win_info,0,panel_w);
            wresize(win_info,rows-1,cols-panel_w);
            mvwin(win_status,rows-1,0);
            wresize(win_status,1,cols);
            draw_list(); draw_info(); draw_status();
            continue;
        }
        switch(ch) {
            case 'T':
            case 't':  // переключить режим
                mode = (mode==MODE_FS ? MODE_TRASH : MODE_FS);
                highlight = 0;
                draw_list(); draw_info(); draw_status();
                break;
            case KEY_UP:
                if (highlight>0) highlight--;
                draw_list(); draw_info(); break;
            case KEY_DOWN: {
                int cnt=build_items_count();
                if (highlight<cnt-1) highlight++;
                draw_list(); draw_info();
                break;
            }
            case 10:  // Enter: в FS-режиме заходить в папку
                if (mode==MODE_FS) {
                    DIR *d=opendir(cwd);
                    if (d) {
                        struct dirent *e; int idx=0;
                        while((e=readdir(d))) {
                            if(idx++==highlight) {
                                char full[PATH_MAX];
                                snprintf(full,sizeof(full),"%s/%s",cwd,e->d_name);
                                struct stat st;
                                if(stat(full,&st)==0 && S_ISDIR(st.st_mode)) {
                                    strncpy(cwd,full,sizeof(cwd));
                                    highlight=0;
                                }
                                break;
                            }
                        }
                        closedir(d);
                    }
                    draw_list(); draw_info();
                }
                break;
            case KEY_BACKSPACE: case 8: case 127:
                if(mode==MODE_FS) {
                    char *p=strrchr(cwd,'/');
                    if(p&&p!=cwd)*p='\0'; else strcpy(cwd,"/");
                    highlight=0; draw_list(); draw_info();
                }
                break;
            case KEY_DC: // Del
                if (mode==MODE_FS) {
                    // переместить в корзину
                    DIR *d=opendir(cwd);
                    if(d) {
                        struct dirent *e; int idx=0;
                        while((e=readdir(d))) {
                            if(idx++==highlight) {
                                char full[PATH_MAX];
                                snprintf(full,sizeof(full),"%s/%s",cwd,e->d_name);
                                unlink(full);
                                break;
                            }
                        }
                        closedir(d);
                    }
                    draw_list(); draw_info();
                }
                break;
            case 'R':
            case 'r':  // в корзине — restore
                if (mode == MODE_TRASH) {
                    // *найти* запись под highlight
                    char id[64] = {0};
                    char files_dir[PATH_MAX], info_dir[PATH_MAX];
                    get_trash_paths(files_dir, info_dir);
                    DIR *d = opendir(files_dir);
                    if (d) {
                        struct dirent *e;
                        int idx = 0;
                        while ((e = readdir(d))) {
                            if (idx++ == highlight) {
                                snprintf(id, sizeof(id), "%s", e->d_name);
                                break;
                            }
                        }
                        closedir(d);
                    }
                    if (id[0] == '\0') break;

                    int fd = open("/dev/null", O_WRONLY);
                    int so = dup(STDOUT_FILENO), se = dup(STDERR_FILENO);
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                    int ret = cmd_restore(trash_base, id);
                    // восстанавливаем вывод
                    dup2(so, STDOUT_FILENO); dup2(se, STDERR_FILENO);
                    close(so); close(se);
                    // формируем сообщение
                    if (ret==0) snprintf(feedback, sizeof(feedback), "Restored %s", id);
                    else snprintf(feedback, sizeof(feedback), "Restore failed");
                    draw_list(); draw_info(); draw_status();
                }
                break;
            case 'd':
            case 'D':  // в корзине — purge
                if (mode==MODE_TRASH) {
                    // *найти* запись под highlight
                    char id2[64] = {0};
                    char files_dir2[PATH_MAX], info_dir2[PATH_MAX];
                    get_trash_paths(files_dir2, info_dir2);
                    DIR *d2 = opendir(files_dir2);
                    if (d2) {
                        struct dirent *e2;
                        int idx2 = 0;
                        while ((e2 = readdir(d2))) {
                            if (idx2++ == highlight) {
                                snprintf(id2, sizeof(id2), "%s", e2->d_name);
                                break;
                            }
                        }
                        closedir(d2);
                    }
                    if (id2[0] == '\0') break;

                    int fd = open("/dev/null", O_WRONLY);
                    int so = dup(STDOUT_FILENO), se = dup(STDERR_FILENO);
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                    int ret2 = cmd_purge(trash_base, id2);
                    // восстанавливаем вывод
                    dup2(so, STDOUT_FILENO); dup2(se, STDERR_FILENO);
                    close(so); close(se);
                    // формируем сообщение
                    if (ret2==0) snprintf(feedback, sizeof(feedback), "Purged %s ", id2);
                    else         snprintf(feedback, sizeof(feedback), "Purge failed");
                    draw_list(); draw_info(); draw_status();
                }
                break;
            case KEY_F(1):
                int h = 15, w = 50;
                int y = (rows - h) / 2, x = (cols - w) / 2;
                win_help = newwin(h, w, y, x);
                wbkgd(win_help, COLOR_PAIR(2));
                box(win_help, 0, 0);
                mvwprintw(win_help, 1, 2, "Help      - trashctl browse");
                mvwprintw(win_help, 3, 2, "Arrows    - move cursor");
                mvwprintw(win_help, 4, 2, "Enter     - open directory");
                mvwprintw(win_help, 5, 2, "Backspace - go up");
                mvwprintw(win_help, 6, 2, "Del       - delete (to trash)");
                mvwprintw(win_help, 7, 2, "F1        - this help");
                mvwprintw(win_help, 8, 2, "t         - open trash");
                mvwprintw(win_help, 9, 2, "r         - restore in trash");
                mvwprintw(win_help, 10, 2, "d         - delete in trash");
                mvwprintw(win_help, 11, 2, "q or Esc - quit browser");
                wrefresh(win_help);
                wgetch(win_help);
                delwin(win_help);

                draw_list();
                draw_info();
                draw_status();
                continue;
                    break;
                default: break;
        }
        draw_status();
    }

    delwin(win_list);
    delwin(win_info);
    delwin(win_status);
    delwin(win_help);
    endwin();         // завершить текущее окно
    delscreen(screen); // освободить все внутренние буферы ncurses
    return 0;
}
