#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "tap.h"
#include "partr.h"
#include "taskpools.h"

log_t plog;
int16_t nthreads;
__thread int16_t tid;

#define NTASKS_PER_POOL 1024
static ptask_t *tasks[NTASKS_PER_POOL];

/* thread barrier */
static int volatile barcnt;
static int volatile barsense = 1;

#define BARRIER_INIT()          barcnt=nthreads

#define BARRIER() do {                                                  \
    mysense = !mysense;                                                 \
    if (!__atomic_sub_fetch(&barcnt, 1, __ATOMIC_SEQ_CST)) {            \
        barcnt = nthreads;                                              \
        barsense = mysense;                                             \
    } else while (barsense != mysense);                                 \
} while(0)

typedef struct pthread_arg_tag {
    int16_t tid;
} pthread_arg_t;


static void *threadfun(void *targ)
{
    int success;
    int mysense = 1; /* for the barrier */

    pthread_arg_t *arg = (pthread_arg_t *)targ;
    tid = arg->tid;
    free(targ);

    if (tid == 0) {
        taskpools_init();

        /* single-thread tests */
        success = 1;
        for (int16_t i = 0;  i < NTASKS_PER_POOL;  ++i)
            if ((tasks[i] = task_alloc()) == NULL)
                success = 0;
        ok(success, "single-thread allocation");
        ok(task_alloc() == NULL, "expected number of tasks");
        for (int16_t i = 0;  i < NTASKS_PER_POOL;  ++i)
            task_free(tasks[i]);
        success = 1;
        for (int16_t i = 0;  i < NTASKS_PER_POOL;  ++i)
            if ((tasks[i] = task_alloc()) == NULL)
                success = 0;
        ok(success, "free and alloc again");
    }

    BARRIER();

    int each = NTASKS_PER_POOL/nthreads, start = tid * each, end = start + each;
    if (tid == nthreads-1) end = NTASKS_PER_POOL;

    /* parallel free tests */
    for (int i = start;  i < end;  ++i) {
        task_free(tasks[i]);
        __atomic_store_n(&tasks[i], NULL, __ATOMIC_SEQ_CST);
    }

    BARRIER();

    if (tid == 0) {
        /* verify that all tasks were freed */
        int success = 1;
        for (int16_t i = 0;  i < NTASKS_PER_POOL;  ++i) {
            if (tasks[i] != NULL)
                success = 0;
            tasks[i] = task_alloc();
        }
        ok(success, "all tasks freed");

        /* verify that tasks were freed out of order */
        success = 0;
        int16_t last_index = tasks[0]->index;
        for (int16_t i = 1;  i < NTASKS_PER_POOL;  ++i) {
            if (tasks[i]->index != last_index++)
                success = 1;
            task_free(tasks[i]);
        }
        ok(success, "frees happened in parallel");
    }

    BARRIER();

    /* parallel alloc tests */
    for (int i = start;  i < end;  ++i)
        tasks[i] = task_alloc();

    BARRIER();

    if (tid == 0) {
        /* verify that tasks were allocated concurrently */
        success = 0;
        int16_t last_index = tasks[0]->index;
        for (int16_t i = 1;  i < NTASKS_PER_POOL;  ++i) {
            if (tasks[i]->index != last_index++) {
                success = 1;
                break;
            }
        }
        ok(success, "concurrent allocs");
    }

    BARRIER();

    /* TODO: parallel alloc and free tests */

    if (tid == 0) {
        todo("parallel allocs/frees");
        ok(0);
        end_todo;
    }

    BARRIER();

    if (tid == 0)
        taskpools_destroy();

    return NULL;
}

int main(int argc, char **argv)
{
    LOG_SETUP(plog, LOG_LEVEL_INFO, stdout);
    LOG_INFO(plog, "taskpools test\n");

    nthreads = DEFAULT_NUM_THREADS;
    char *cp = getenv(NUM_THREADS_NAME);
    if (cp) nthreads = strtol(cp, NULL, 10);
    LOG_INFO(plog, "  %d threads\n", nthreads);

    BARRIER_INIT();

    /* create threads */
    pthread_t pthread_id;
    pthread_attr_t pthread_attr;
    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
    for (int16_t i = 1;  i < nthreads;  ++i) {
        pthread_arg_t *targ = (pthread_arg_t *)calloc(1, sizeof(pthread_arg_t));
        targ->tid = i;
        pthread_create(&pthread_id, &pthread_attr, threadfun, targ);
    }
    pthread_attr_destroy(&pthread_attr);

    /* thread 0 enters the thread function too */
    pthread_arg_t *targ = (pthread_arg_t *)calloc(1, sizeof(pthread_arg_t));
    targ->tid = 0;
    threadfun(targ);

    done_testing();
}

