/*  partr -- parallel tasks runtime
 */

#ifndef PARTR_H
#define PARTR_H

#include <stdint.h>
#include <stdio.h>

#include "log.h"


/* tasks */
#define TASK_STACK_SIZE                 (1024*4)

/* pools */
#define TASKS_PER_POOL                  1024
    /* number allocated = TASKS_PER_POOL * nthreads */

/* multiq */
#define MULTIQ_HEAP_C                   4
    /* number of heaps = MULTIQ_HEAP_C * nthreads */
#define MULTIQ_TASKS_PER_HEAP           129
    /* how many in each heap */

/* parfor */
#define GRAIN_K                         4
    /* tasks = niters / (GRAIN_K * nthreads) */

/* synchronization */
#define ARRIVERS_P                      2
    /* narrivers = ((GRAIN_K * nthreads) ^ ARRIVERS_P) + 1
       limit for number of recursive parfors */
#define REDUCERS_FRAC                   1
    /* nreducers = narrivers * REDUCERS_FRAC */

/* logging (debug, info, warn, err, critical, none) */
#define LOG_LEVEL_NAME                  "PARTR_LOG_LEVEL"
    /* environment variable name */
#define DEFAULT_LOG_LEVEL               "debug"

/* controls for when threads sleep */
#define THREAD_SLEEP_THRESHOLD_NAME     "PARTR_THREAD_SLEEP_THRESHOLD"
    /* environment variable name */
#define DEFAULT_THREAD_SLEEP_THRESHOLD  1e9
    /* in cycles (1e9 == 1sec@1GHz) */

/* defaults for # threads */
#define NUM_THREADS_NAME                "PARTR_NUM_THREADS"
    /* environment variable name */
#define DEFAULT_NUM_THREADS             4

/* affinitization behavior */
#define MACHINE_EXCLUSIVE_NAME          "PARTR_EXCLUSIVE"
    /* environment variable name */
#define DEFAULT_MACHINE_EXCLUSIVE       0
    /* don't assume we own the machine */

/* performance profiling */
#define PERF_PROFILE                    1
    /* comment to disable profiling */


/* externally visible globals */
extern log_t plog;                      /* message logger */
extern int16_t nthreads;                /* number of threads */


/* externally visible thread-local globals */
extern __thread int16_t tid;            /* 0-based thread ID */
extern __thread uint64_t rngseed;       /* per-thread RNG seed */


/* external interface */
typedef void *partr_t;

void partr_init();
void partr_shutdown();
int  partr_start(void **ret, void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t start, int64_t end);
int  partr_spawn(partr_t *t, void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t start, int64_t end, int8_t sticky,
        int8_t detach);
int  partr_sync(void **r, partr_t t, int done_with_task);
int  partr_parfor(partr_t *t, void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t count, void *(*rf)(void *, void *));


#endif /* PARTR_H */

