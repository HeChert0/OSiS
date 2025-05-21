#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>

struct index_s {
    double   time_mark;
    uint64_t recno;
};

typedef struct {
    unsigned thread_id;
    size_t   block_size;
} thread_arg_t;

static size_t   pagesize;
static size_t   memsize;
static size_t   blocks;
static size_t   block_size;
static unsigned threads;
static uint64_t records;
static const char *filename;

static void   *map_base;
static size_t  map_len;
static off_t   data_offset;
static size_t  page_offset;

static pthread_barrier_t barrier;
static unsigned         *block_map;
static unsigned         *merge_map;
static pthread_mutex_t   map_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t    *tids;
static thread_arg_t *targs;

static void  usage(const char *prog);
static void *worker_thread(void *arg);
static int   cmp_index_s(const void *a, const void *b);
static void  do_merge(size_t idx, size_t span, struct index_s *base);


int main(int argc, char *argv[]) {
    pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize == (size_t)-1) {
        perror("sysconf");
        return 1;
    }
    if (argc != 5) usage(argv[0]);

    long ms = strtol(argv[1], NULL, 10);
    if (ms <= 0 || ms % pagesize) usage(argv[0]);
    memsize = ms;

    long bl = strtol(argv[2], NULL, 10);
    if (bl <= 0 || (bl & (bl-1))) usage(argv[0]);
    blocks = bl;

    long th = strtol(argv[3], NULL, 10);
    if (th <= 0) usage(argv[0]);
    threads = th;

    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (threads < cores || threads > 8*cores) usage(argv[0]);
    if (blocks < 4*threads) usage(argv[0]);

    filename = argv[4];

    int fd = open(filename, O_RDWR);
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
        fprintf(stderr, "File too small\n");
        close(fd);
        return 1;
    }
    if (pread(fd, &records, sizeof(records), 0) != sizeof(records)) {
        perror("pread");
        close(fd);
        return 1;
    }
    size_t entry_size = sizeof(double) + sizeof(uint64_t);
    size_t expected = sizeof(uint64_t) + records * entry_size;
    if (filesize < expected) {
        fprintf(stderr, "File size %zu < expected %zu\n", filesize, expected);
        close(fd);
        return 1;
    }

    printf("OK: memsize=%zu, blocks=%zu, threads=%u, records=%" PRIu64 "\n",
           memsize, blocks, threads, records);

    block_size = memsize / blocks;

    tids  = malloc(threads * sizeof(*tids));
    targs = malloc(threads * sizeof(*targs));
    if (!tids || !targs) {
        perror("malloc");
        return 1;
    }

    data_offset = sizeof(uint64_t);
    while (data_offset < (off_t)expected) {
        size_t page = data_offset & ~(pagesize - 1);
        page_offset = data_offset - page;
        size_t want = page_offset + memsize;
        if (page + want > expected) want = expected - page;
        map_len  = want;
        map_base = mmap(NULL, map_len,
                        PROT_READ|PROT_WRITE,
                        MAP_SHARED, fd, page);
        if (map_base == MAP_FAILED) {
            perror("mmap"); close(fd); return 1;
        }

        printf("\n--- Chunk | offset=%jd (page=%zu,page_offset=%zu,len=%zu) ---\n",
               (intmax_t)data_offset, page, page_offset, map_len);

        pthread_barrier_init(&barrier, NULL, threads);
        block_map = calloc(blocks, sizeof *block_map);
        merge_map = calloc(blocks/2, sizeof *merge_map);
        if (!block_map || !merge_map) { perror("calloc"); return 1; }
        for (unsigned i = 0; i < threads; i++) block_map[i] = 1;

        for (unsigned t = 0; t < threads; t++) {
            targs[t].thread_id  = t;
            targs[t].block_size = block_size;
        }

        for (unsigned t = 1; t < threads; t++) {
            if (pthread_create(&tids[t], NULL, worker_thread, &targs[t]) != 0) {
                perror("pthread_create"); return 1;
            }
        }
        worker_thread(&targs[0]);

        for (unsigned t = 1; t < threads; t++) {
            pthread_join(tids[t], NULL);
        }

        if (msync(map_base, map_len, MS_SYNC) < 0) perror("msync");
        munmap(map_base, map_len);

        free(block_map);
        free(merge_map);
        pthread_barrier_destroy(&barrier);

        printf(">>> Chunk | offset=%jd sorted and written back <<<\n",
               (intmax_t)data_offset);

        data_offset += memsize;
    }

    close(fd);
    free(tids);
    free(targs);
    return 0;
}


static void *worker_thread(void *arg) {
    thread_arg_t *t = arg;
    unsigned id = t->thread_id;

    struct index_s *base = (struct index_s*)((char*)map_base + page_offset);
    size_t per = t->block_size / sizeof(*base);

    pthread_barrier_wait(&barrier);

    qsort(base + per*id, per, sizeof(*base), cmp_index_s);
    while (1) {
        unsigned next = blocks;
        pthread_mutex_lock(&map_mutex);
        for (unsigned i = 0; i < blocks; i++) {
            if (!block_map[i]) {
                block_map[i] = 1;
                next = i;
                break;
            }
        }
        pthread_mutex_unlock(&map_mutex);
        if (next >= blocks) break;
        qsort(base + per*next, per, sizeof(*base), cmp_index_s);
    }
    printf("[T%2u] local sort done\n", id);

    pthread_barrier_wait(&barrier);

    size_t levels = 0;
    for (size_t tmp = blocks; tmp > 1; tmp >>= 1) levels++;
    for (size_t s = 0; s < levels; s++) {
        size_t span      = (1ULL<<s) * per;
        size_t num_pairs = blocks >> (s+1);

        if (id == 0) {
            printf("[T%2u] start merge round %zu (pairs=%zu)\n",
                   id, s, num_pairs);
            memset(merge_map, 0, num_pairs*sizeof *merge_map);
        }
        pthread_barrier_wait(&barrier);

        if (id < num_pairs) {
            do_merge(id, span, base);
        }

        while (1) {
            unsigned j = num_pairs;
            pthread_mutex_lock(&map_mutex);
            for (unsigned k = 0; k < num_pairs; k++) {
                if (!merge_map[k]) { merge_map[k] = 1; j = k; break; }
            }
            pthread_mutex_unlock(&map_mutex);
            if (j >= num_pairs) break;
            do_merge(j, span, base);
        }

        pthread_barrier_wait(&barrier);
        if (id == 0) printf("[T%2u] merge round %zu done\n", id, s);
    }

    return NULL;
}


static int cmp_index_s(const void *a, const void *b) {
    const struct index_s *ia = a, *ib = b;
    if (ia->time_mark < ib->time_mark) return -1;
    if (ia->time_mark > ib->time_mark) return  1;
    if (ia->recno    < ib->recno)    return -1;
    if (ia->recno    > ib->recno)    return  1;
    return 0;
}

static void do_merge(size_t idx, size_t span, struct index_s *base) {
    struct index_s *left  = base + idx*2*span;
    struct index_s *right = left + span;
    size_t total = span*2;

    struct index_s *tmp = malloc(total * sizeof *tmp);
    if (!tmp) { perror("malloc"); exit(1); }

    size_t l=0, r=0, i=0;
    while (l < span && r < span) {
        if (cmp_index_s(&left[l], &right[r]) <= 0) tmp[i++] = left[l++];
        else                                      tmp[i++] = right[r++];
    }
    while (l < span) tmp[i++] = left[l++];
    while (r < span) tmp[i++] = right[r++];

    memcpy(left, tmp, total * sizeof *tmp);
    free(tmp);
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s memsize blocks threads filename\n"
            " memsize – multiple of page size (%zu)\n"
            " blocks  – power of two, ≥ 4*threads\n"
            " threads – [%u..%u]\n",
            prog, pagesize,
            (unsigned)sysconf(_SC_NPROCESSORS_ONLN),
            (unsigned)sysconf(_SC_NPROCESSORS_ONLN)*8u);
    exit(1);
}
