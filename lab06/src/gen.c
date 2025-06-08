#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <math.h>


struct index_s {
    double   time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t    records;
    struct index_s idx[];
};


static double current_mjd(void) {
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);

    int Y = tm.tm_year + 1900;
    int M = tm.tm_mon + 1;
    int D = tm.tm_mday;
    int A = (14 - M) / 12;
    int y = Y + 4800 - A;
    int m = M + 12 * A - 3;
    int64_t JDN = D + (153*m + 2)/5 + 365LL*y + y/4 - y/100 + y/400 - 32045;
    double day_frac = (tm.tm_hour*3600 + tm.tm_min*60 + tm.tm_sec) / 86400.0;
    double JD = JDN + day_frac;
    return JD - 2400000.5;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <records (multiple of 256)> <filename>\n", argv[0]);
        return 1;
    }

    errno = 0;
    uint64_t records = strtoull(argv[1], NULL, 10);
    if (errno || records == 0 || (records % 256) != 0) {
        fprintf(stderr, "Error: records must be a positive multiple of 256\n");
        return 1;
    }

    const char *filename = argv[2];
    size_t filesize = sizeof(uint64_t) + records * sizeof(struct index_s);

    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    if (ftruncate(fd, filesize) < 0) {
        perror("ftruncate");
        close(fd);
        return 1;
    }

    void *map = mmap(NULL, filesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);

    struct index_hdr_s *hdr = map;
    hdr->records = records;

    unsigned seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    double mjd_today = current_mjd();
    double mjd_max = floor(mjd_today) - 1.0;
    double mjd_min = 15020.0;

    struct index_s *entries = hdr->idx;
    for (uint64_t i = 0; i < records; i++) {
        double int_part = mjd_min + (rand_r(&seed) / (double)RAND_MAX) * (mjd_max - mjd_min);
        double frac_part = (rand_r(&seed) / (double)RAND_MAX);
        entries[i].time_mark = floor(int_part) + frac_part;
        entries[i].recno     = i + 1;
    }

    if (msync(map, filesize, MS_SYNC) < 0) perror("msync");
    if (munmap(map, filesize)  < 0) perror("munmap");

    return 0;
}
