#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "tap.h"
#include "perfutil.h"
#include "congrng.h"
#include "partr.h"
#include "taskpools.h"
#include "multiq.h"

#include <unistd.h>

log_t plog;
int16_t nthreads;
__thread int16_t tid;
__thread uint64_t rngseed;

#define NTASKS_PER_POOL 512
static ptask_t *tasks[NTASKS_PER_POOL];

static int *success;

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


typedef struct lthread_arg_tag {
    int16_t tid;
} lthread_arg_t;

/* used for reducing across threads */
static int N = 0;


static void *threadfun(void *targ)
{
    int mysense = 1; /* for the barrier */

    ptask_t *task;
    int16_t curr_prio, ooo;

    lthread_arg_t *arg = (lthread_arg_t *)targ;
    tid = arg->tid;
    free(targ);
    seed_cong(&rngseed);

    if (tid == 0) {
        taskpools_init();
        multiq_init();

        success = (int *)calloc(nthreads, sizeof(int));

        /* single-thread alloc and insert */
        const int16_t tprios[] = { 1, 0, 3, 2 };
        int16_t t = 0;
        for (int16_t j = 0;  j < 4;  ++j) {
            success[tid] = 1;
            for (int16_t i = 0;  i < NTASKS_PER_POOL/4;  ++i) {
                tasks[t] = task_alloc();
                if (multiq_insert(tasks[t], tprios[j]) != 0)
                    success[tid] = 0;
                ++t;
            }
            ok(success[tid], "insert with priority %d", tprios[j]);
        }

        /* single-thread deletemin */
        success[tid] = 1;
        curr_prio = ooo = 0;
        for (int16_t i = 0;  i < NTASKS_PER_POOL;  ++i) {
            task = multiq_deletemin();
            if (task == NULL) {
                success[tid] = 0;
                break;
            }
            if (task->prio > curr_prio)
                curr_prio = task->prio;
            else if (task->prio < curr_prio) {
                diag("(tid %d) curr_prio: %d, task->prio: %d\n", tid, curr_prio, task->prio);
                ++ooo;
                curr_prio = task->prio;
            }
        }
        ok(success[tid], "deletemin (%d out-of-order)", ooo);
    }

    BARRIER();

    int each = NTASKS_PER_POOL/nthreads, start = tid * each, end = start + each;
    if (tid == nthreads-1) end = NTASKS_PER_POOL;

    /* parallel insert tests */
    success[tid] = 1;
    for (int16_t i = start;  i < end;  ++i) {
        if (multiq_insert(tasks[i], tid) != 0)
            success[tid] = 0;
    }

    BARRIER();

    if (tid == 0) {
        for (int16_t i = 1;  i < nthreads;  ++i)
            if (!success[i])
                success[0] = 0;
        ok(success[0], "parallel insertion, %d tasks\n", NTASKS_PER_POOL);
    }

    BARRIER();

    /* parallel deletemin tests */
    curr_prio = ooo = 0;
    int ndeq = 0;
    for (int16_t i = 0;  i < NTASKS_PER_POOL/nthreads;  ++i) {
        task = multiq_deletemin();
        if (task == NULL) {
            diag("(tid %d) !task\n", tid);
            continue;
        }
        ++ndeq;
        if (task->prio > curr_prio)
            curr_prio = task->prio;
        else if (task->prio < curr_prio) {
            diag("(tid %d) curr_prio: %d, task->prio: %d\n", tid, curr_prio, task->prio);
            ++ooo;
            curr_prio = task->prio;
        }
    }
    __atomic_add_fetch(&N, ndeq, __ATOMIC_SEQ_CST);

    BARRIER();

    if (tid == 0) {
        ok(N == NTASKS_PER_POOL, "parallel deletemin %d tasks (%d out-of-order)", N, ooo);

        for (int16_t i = 0;  i < NTASKS_PER_POOL;  ++i)
            task_free(tasks[i]);

        free(success);
        multiq_destroy();
        taskpools_destroy();
    }

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

    tid = 0;

    pthread_t pthread_id;
    pthread_attr_t pthread_attr;

    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);

    for (int16_t i = 1;  i < nthreads;  ++i) {
        lthread_arg_t *targ = (lthread_arg_t *)calloc(1, sizeof(lthread_arg_t));
        targ->tid = i;
        pthread_create(&pthread_id, &pthread_attr, threadfun, targ);
    }
    pthread_attr_destroy(&pthread_attr);

    /* thread 0 enters the thread function too */
    lthread_arg_t *targ = (lthread_arg_t *)calloc(1, sizeof(lthread_arg_t));
    targ->tid = 0;
    threadfun(targ);

    done_testing();
}

