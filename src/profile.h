/*  partr -- parallel tasks runtime

    thread-aware performance profiling
 */

#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>
#include <stdlib.h>
#include "perfutil.h"

#ifdef PERF_PROFILE
enum {
    PERF_SPAWN, PERF_SYNC, PERF_PARFOR, NTIMES
};

char *times_names[] = {
    "spawn", "sync", "parfor", "run", ""
};

typedef struct thread_timing_tag {
    uint64_t last, min, max, total, count;
} thread_timing_t;

thread_timing_t **thread_times;

#define PROFILE_SETUP()                                                       \
    thread_times = (thread_timing_t **)                                       \
            _mm_malloc(nthreads * sizeof (thread_timing_t *), 64);

#define PROFILE_INIT_THREAD()                                                 \
    thread_times[tid] = (thread_timing_t *)                                   \
        _mm_malloc(NTIMES * sizeof (thread_timing_t), 64);                    \
    for (int i = 0;  i < NTIMES;  i++) {                                      \
        thread_times[tid][i].last = thread_times[tid][i].max =                \
                thread_times[tid][i].total = thread_times[tid][i].count = 0;  \
        thread_times[tid][i].min = UINT64_MAX;                                \
    }

#define PROFILE_START(w)                                                      \
    thread_times[tid][(w)].last = rdtscp()

#define PROFILE_STAMP(w)                                                      \
{                                                                             \
    uint64_t l = thread_times[tid][(w)].last =                                \
            rdtscp() - thread_times[tid][(w)].last;                           \
    if (l < thread_times[tid][(w)].min) thread_times[tid][(w)].min = l;       \
    if (l > thread_times[tid][(w)].max) thread_times[tid][(w)].max = l;       \
    thread_times[tid][(w)].total += l;                                        \
    ++thread_times[tid][(w)].count;                                           \
}

#define PROFILE_PRINT()                                                       \
{                                                                             \
    thread_timing_t coll_times[NTIMES];                                       \
    for (int i = 0;  i < NTIMES;  i++) {                                      \
        memset(&coll_times[i], 0, sizeof (thread_timing_t));                  \
        coll_times[i].min = UINT64_MAX;                                       \
    }                                                                         \
    for (int tnum = 0;  tnum < nthreads;  tnum++) {                           \
        for (int i = 0;  i < NTIMES;  i++) {                                  \
            coll_times[i].total += thread_times[tnum][i].total;               \
            coll_times[i].count += thread_times[tnum][i].count;               \
            if (thread_times[tnum][i].max > coll_times[i].max)                \
                coll_times[i].max = thread_times[tnum][i].max;                \
            if (thread_times[tnum][i].min < coll_times[i].min)                \
                coll_times[i].min = thread_times[tnum][i].min;                \
        }                                                                     \
    }                                                                         \
    printf("partr profile: #calls, mean ticks, [min, max]\n");                \
    for (int i = 0;  i < NTIMES;  i++) {                                      \
        uint64_t m = 0;                                                       \
        if (coll_times[i].count > 0)                                          \
            m = coll_times[i].total / (double)coll_times[i].count;            \
        printf("%s: %llu, %llu, [%llu, %llu]\n",                              \
               times_names[i], coll_times[i].count, m,                        \
               coll_times[i].min, coll_times[i].max);                         \
    }                                                                         \
}


#else  /* !PERF_PROFILE */

#define PROFILE_SETUP()
#define PROFILE_INIT_THREAD()
#define PROFILE_START(w)
#define PROFILE_STAMP(w)
#define PROFILE_PRINT()

#endif /* PERF_PROFILE */

#endif /* PROFILE_H */

