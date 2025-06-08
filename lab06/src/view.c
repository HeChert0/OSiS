#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h> // PRIu64
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

struct index_s {
    double   time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t       records;
    struct index_s idx[];
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    const char *filename = argv[1];

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }
    size_t filesize = st.st_size;
    if (filesize < sizeof(uint64_t)) {
        fprintf(stderr, "File too small to be valid index\n");
        close(fd);
        return 1;
    }

    void *map = mmap(NULL, filesize, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);

    struct index_hdr_s *hdr = map;
    uint64_t records = hdr->records;
    size_t expected = sizeof(uint64_t) + records * sizeof(struct index_s);
    if (filesize < expected) {
        fprintf(stderr,
                "File size %zu is too small for %" PRIu64 " records (need %zu)\n",
                filesize, records, expected);
        munmap(map, filesize);
        return 1;
    }

    struct index_s *entries = hdr->idx;
    for (uint64_t i = 0; i < records; i++) {
        printf("%4" PRIu64 ": time = %.6f, recno = %" PRIu64 "\n",
               i, entries[i].time_mark, entries[i].recno);
    }

    if (munmap(map, filesize) < 0) {
        perror("munmap");
        return 1;
    }
    return 0;
}
