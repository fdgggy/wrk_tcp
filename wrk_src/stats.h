#ifndef STATS_H
#define STATS_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t connect;
    uint32_t read;
    uint32_t write;
    uint32_t status;
    uint32_t timeout;
} errors;

typedef struct {
    uint64_t count;
    uint64_t limit;
    uint64_t min;
    uint64_t max;
    uint64_t connects;
    uint64_t data[];
} stats;

stats *stats_alloc(uint64_t);
void stats_free(stats *);

void stats_connect(stats *stats);
int stats_record(stats *, uint64_t);
long double stats_mean(stats *);
#endif /* STATS_H */
