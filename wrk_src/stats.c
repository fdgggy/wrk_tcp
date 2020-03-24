#include <inttypes.h>
#include <stdlib.h>

#include "stats.h"
#include "zmalloc.h"

stats *stats_alloc(uint64_t max) {
    uint64_t limit = max + 1;
    stats *s = zcalloc(sizeof(stats) + sizeof(uint64_t) * limit);
    s->limit = limit;
    s->min   = UINT64_MAX;
    s->max  = 0;
    s->connects = 0;
    return s;
}

void stats_free(stats *stats) {
    zfree(stats);
}

int stats_record(stats *stats, uint64_t n) {
    if (n >= stats->limit) return 0;
    __sync_fetch_and_add(&stats->data[n], 1);
    __sync_fetch_and_add(&stats->count, 1);
    uint64_t min = stats->min;
    uint64_t max = stats->max;
    while (n < min) min = __sync_val_compare_and_swap(&stats->min, min, n);
    while (n > max) max = __sync_val_compare_and_swap(&stats->max, max, n);
    return 1;
}

void stats_connect(stats *stats) {
    __sync_fetch_and_add(&stats->connects, 1);
}

long double stats_mean(stats *stats) {
    if (stats->count == 0) return 0.0;

    uint64_t sum = 0;
    for (uint64_t i = stats->min; i <= stats->max; i++) {
        sum += stats->data[i] * i;
    }
    return sum / (long double) stats->count;
}
