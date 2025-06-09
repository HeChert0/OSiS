#include <ncurses.h>
#include <stdlib.h>

int main() {
    SCREEN *screen = newterm(NULL, stdout, stdin);
    if (!screen) {
        fprintf(stderr, "newterm failed\n");
        return 1;
    }
    set_term(screen);
    noecho();
    curs_set(FALSE);

    WINDOW *win = newwin(10, 50, 5, 5);
    box(win, 0, 0);
    mvwprintw(win, 1, 1, "This is a minimal leak test...");
    wrefresh(win);

    wgetch(win);

    delwin(win);
    endwin();
    delscreen(screen);

    printf("Cleanup finished.\n");

    return 0;
}
