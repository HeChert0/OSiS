#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "trashctl.h"

int main(int argc, char **argv) {
    const char *base = getenv("TRASH_BASE");
    if (!base) base = ".";

    if (argc < 2) {
        fprintf(stderr, "Usage: trashctl <list|restore|purge> [...]\n");
        return 1;
    }

    if (strcmp(argv[1], "list") == 0) {
        return cmd_list(base);
    }
    else if (strcmp(argv[1], "browse") == 0) {
            const char *start = (argc >= 3 ? argv[2] : ".");
            return cmd_browse(base, start);
    }
    else if (strcmp(argv[1], "restore") == 0 && argc == 3) {
        return cmd_restore(base, argv[2]);
    }
    else if (strcmp(argv[1], "purge") == 0 && argc == 3) {
        if (strcmp(argv[2], "--all") == 0)
            return cmd_purge_all(base);
        else
            return cmd_purge(base, argv[2]);
    }


    fprintf(stderr, "Unknown command or wrong args\n");
    return 1;
}
