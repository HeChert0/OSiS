#include "dirwalkFunc.h"

int main(int argc, char *argv[]) {
	Options options = {0, 0, 0, 0};
	int filter = 0;
	const char *path = ".";

	setlocale(LC_COLLATE, "");

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (int j = 1; argv[i][j] != '\0'; j++) {
				switch (argv[i][j]) {
					case 'l': options.show_links = 1; filter = 1; break;
					case 'd': options.show_dirs = 1; filter = 1; break;
					case 'f': options.show_files = 1; filter = 1; break;
					case 's': options.sort_output = 1; break;
					default:
						fprintf(stderr, "Usage: %s [dir] [-l] [-d] [-f] [-s]\n", argv[0]);
						exit(EXIT_FAILURE);
				}
			}
		} else {
			path = argv[i];
		}
	}

	if (options.show_dirs || !filter) {
		printf("%s\n", path);
	}

	walk_directory(path, &options, filter);

	return 0;
}
