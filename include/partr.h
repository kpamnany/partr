/*  partr -- parallel tasks runtime
 */

#ifndef PARTR_H
#define PARTR_H

#include <stdint.h>
#include <stdio.h>

#include "log.h"

/* tasks, pools, multiq controls */
#define TASK_STACK_SIZE                 (1024*4)
#define TASKS_PER_POOL                  1024
#define MULTIQ_HEAP_C                   4
#define MULTIQ_TASKS_PER_HEAP           129

/* logging control (debug, info, warn, err, critical) */
#define LOG_LEVEL_NAME                  "PARTR_LOG_LEVEL_NAME"
#define DEFAULT_LOG_LEVEL               "debug"

/* controls for when threads sleep */
#define THREAD_SLEEP_THRESHOLD_NAME     "PARTR_THREAD_SLEEP_THRESHOLD"
#define DEFAULT_THREAD_SLEEP_THRESHOLD  1e9    /* cycles (1e9==1sec@1GHz) */

/* defaults for # threads */
#define NUM_THREADS_NAME                "PARTR_NUM_THREADS"
#define DEFAULT_NUM_THREADS             4

/* affinitization behavior */
#define MACHINE_EXCLUSIVE_NAME          "PARTR_EXCLUSIVE"
#define DEFAULT_MACHINE_EXCLUSIVE       0

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
partr_t partr_spawn(void *(*f)(void *), void *arg);
void *partr_sync(partr_t l);
partr_t partr_parfor(void *(*f)(void *), void *arg);


#endif /* PARTR_H */

