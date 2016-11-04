/*  partr -- parallel tasks runtime

    thread-aware performance profiling
 */

#include "profile.h"

/* 0-based thread ID */
extern __thread int16_t ltid;

#ifdef PROFILE_PERF
enum {
    TIME_GETWORK, TIME_MPIWAIT, TIME_MPISEND, TIME_RUN, NTIMES
};

char *times_names[] = {
    "getwork", "mpiwait", "mpisend", "run", ""
};

typedef struct thread_timing_tag {
    uint64_t last, min, max, total, count;
} thread_timing_t;

thread_timing_t **thread_times;

#define PROFILE_INIT(nthreads)                                          \
    thread_times = (thread_timing_t **)                                 \
            aligned_alloc(64, nthreads * sizeof (thread_timing_t *));   \
    #pragma omp parallel
    {
        int tnum = omp_get_thread_num();
        times[tnum] = (Threadtiming *)
                _mm_malloc(NTIMES * sizeof (Threadtiming), 64);
        for (int i = 0;  i < NTIMES;  i++) {
            times[tnum][i].last = times[tnum][i].max =
                times[tnum][i].total = times[tnum][i].count = 0;
            times[tnum][i].min = UINT64_MAX;
        }
    }

#define PROFILE_SHUTDOWN(
#define PROFILE_START(w)                                                \
    times[ltid][(w)].last = _rdtsc()

#define PROFILE_STAMP(w)                                                \
{                                                                       \
    uint64_t l = times[ltid][(w)].last = _rdtsc()-times[ltid][(w)].last;\
    if (l < times[ltid][(w)].min) times[ltid][(w)].min = l;             \
    if (l > times[ltid][(w)].max) times[ltid][(w)].max = l;             \
    times[ltid][(w)].total += l;                                        \
    ++times[ltid][(w)].count;                                           \
}

#else  /* !PROFILE_PERF */

#define PROFILE_START(w)
#define PROFILE_STAMP(w)

#endif /* PROFILE_PERF */

#endif /* PROFILE_H */
