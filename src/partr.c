/*  partr -- parallel tasks runtime

    spawn/sync/parfor
 */

#include "partr.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <pthread.h>
#include <sched.h>
#include <hwloc.h>

#include "perfutil.h"
#include "congrng.h"
#include "taskpools.h"
#include "multiq.h"


/* used for logging by the entire runtime */
log_t plog;

/* number of threads created */
int16_t nthreads;

/* thread-local 0-based identifier */
__thread int16_t tid;

/* task currently being executed */
__thread ptask_t *curr_task;

/* RNG seed */
__thread uint64_t rngseed;

/* forward declare thread function */
static void *partr_thread(void *arg_);

/* internally used to indicate a yield occurred in the runtime itself */
static const int64_t yield_from_sync = 1;

/* a parfor range is split into at most K*nthreads tasks */
static const int16_t K = 4;

/* initialization thread barrier */
static int volatile barcnt;
static int volatile barsense = 1;

#define BARRIER_INIT()          barcnt=nthreads
#define BARRIER_THREAD_DECL     int mysense = 1
#define BARRIER() do {                                                  \
    mysense = !mysense;                                                 \
    if (!__atomic_sub_fetch(&barcnt, 1, __ATOMIC_SEQ_CST)) {            \
        barcnt = nthreads;                                              \
        barsense = mysense;                                             \
    } else while (barsense != mysense);                                 \
} while(0)


/* thread function argument */
typedef struct lthread_arg_tag {
    int16_t             tid;
    int8_t              exclusive;
    hwloc_topology_t    topology;
    hwloc_cpuset_t      cpuset;
} lthread_arg_t;


/*  log_init() -- set up runtime logging
 */
static void log_init()
{
    int level;
    char *cp;

    cp = getenv(LOG_LEVEL_NAME);
    if (!cp)
        cp = DEFAULT_LOG_LEVEL;
    if (strncasecmp(cp, "debug", 5) == 0)
        level = LOG_LEVEL_DEBUG;
    else if (strncasecmp(cp, "info", 4) == 0)
        level = LOG_LEVEL_INFO;
    else if (strncasecmp(cp, "err", 3) == 0)
        level = LOG_LEVEL_ERR;
    else if (strncasecmp(cp, "critical", 8) == 0)
        level = LOG_LEVEL_CRITICAL;
    else /* if (strncasecmp(cp, "warn", 4) == 0) */
        level = LOG_LEVEL_WARN;

    LOG_SETUP(plog, level, stdout);
    LOG_INFO(plog, "partr threading\n");
}


/*  show_affinity()
 */
#ifdef __linux__
static void show_affinity()
{
    int i;
    cpu_set_t cset;
    char buf[2048], num[16];

    if (plog.level > LOG_LEVEL_DEBUG) return;

    pthread_t pthread_id = pthread_self();

    CPU_ZERO(&cset);
    pthread_getaffinity_np(pthread_id, sizeof(cset), &cset);
    buf[0] = '\0';
    for (i = 0;  i < CPU_SETSIZE;  ++i) {
        if (CPU_ISSET(i, &cset)) {
            snprintf(num, 15, "%d ", i);
            strcat(buf, num);
        }
    }
    LOG_DEBUG(plog, "    <%d> bound to %d CPU(s): %s\n",
              tid, CPU_COUNT(&cset), buf);
}
#else
static void show_affinity()
{
}
#endif


/*  partr_init() -- initialization entry point
 */
void partr_init()
{
    log_init();

    char *cp;

    /* get requested # threads */
    nthreads = DEFAULT_NUM_THREADS;
    cp = getenv(NUM_THREADS_NAME);
    if (cp)
        nthreads = strtol(cp, NULL, 10);
    LOG_INFO(plog, "  %d threads requested\n", nthreads);

    /* check if we have exclusive use of the machine */
    int exclusive = DEFAULT_MACHINE_EXCLUSIVE;
    cp = getenv(MACHINE_EXCLUSIVE_NAME);
    if (cp)
        exclusive = strtol(cp, NULL, 10);

    /* check machine topology */
    hwloc_topology_t topology;
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);
    int core_depth = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_CORE);
    unsigned ncores = hwloc_get_nbobjs_by_depth(topology, core_depth);
    LOG_INFO(plog, "  %d cores detected\n", ncores);
    int pu_depth = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_PU);
    unsigned npus = hwloc_get_nbobjs_by_depth(topology, pu_depth);
    LOG_INFO(plog, "  %d PUs detected\n", npus);

    /* some sanity checks */
    if (nthreads > npus) {
        LOG_WARN(plog, "  won't over-subscribe; adjusting number of threads"
                 " to %d\n", npus);
        nthreads = npus;
    }
    if (nthreads < 1) {
        LOG_INFO(plog, "  setting number of threads to number of cores (%d)\n",
                 ncores);
        nthreads = ncores;
    }
    int depth;
    if (nthreads <= ncores) {
        LOG_INFO(plog, "  1 thread per core\n");
        depth = core_depth;
    }
    else {
        LOG_INFO(plog, "  >1 thread per core\n");
        depth = pu_depth;
    }

    /* set affinity if we have exclusive use of the machine */
    hwloc_obj_t obj;
    hwloc_cpuset_t cpuset;
    if (exclusive) {
        LOG_INFO(plog, "  exclusive machine use\n");

        /* rebind this thread to the first core/PU */
        obj = hwloc_get_obj_by_depth(topology, depth, 0);
        assert(obj != NULL);
        cpuset = hwloc_bitmap_dup(obj->cpuset);
        /* hwloc_bitmap_singlify(cpuset); */
        hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD);
        hwloc_bitmap_free(cpuset);
    }
    else
        LOG_INFO(plog, "  non-exclusive machine use\n");

    tid = 0;
    seed_cong(&rngseed);
    show_affinity();

    /* initialize task pools */
    taskpools_init();

    /* initialize task multiqueue */
    multiq_init();

    /* initialize libconcurrent */
    concurrent_init();

    /* start threads */
    BARRIER_THREAD_DECL;
    BARRIER_INIT();

    pthread_t pthread_id;
    pthread_attr_t pthread_attr;

    pthread_attr_init(&pthread_attr);
    pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);

    for (int16_t i = 1;  i < nthreads;  ++i) {
        lthread_arg_t *targ = (lthread_arg_t *)calloc(1, sizeof(lthread_arg_t));
        targ->tid = i;
        targ->exclusive = exclusive;

        if (exclusive) {
            /* tell the thread which core to bind to */
            obj = hwloc_get_obj_by_depth(topology, depth, i);
            cpuset = hwloc_bitmap_dup(obj->cpuset);
            targ->topology = topology;
            targ->cpuset = cpuset;
        }
        pthread_create(&pthread_id, &pthread_attr, partr_thread, targ);
    }
    pthread_attr_destroy(&pthread_attr);

    /* wait for all threads to start up and bind to their CPUs */
    BARRIER();
    hwloc_topology_destroy(topology);
}


/*  partr_shutdown() -- shutdown all threads and clean up
 */
void partr_shutdown()
{
    /* TODO: create and add 'nthreads' shutdown tasks */

    /* TODO: wait for all threads to shut down */

    /* shut down the tasking library */
    concurrent_fin();

    /* destroy the task queues */
    multiq_destroy();

    /* destroy the task pools and free all tasks */
    taskpools_destroy();
}


/*  partr_coro() -- coroutine entry point
 */
static void partr_coro(struct concurrent_ctx *ctx)
{
    ptask_t *task = ctx_get_user_ptr(ctx);
    task->ret = task->f(task->arg, task->start, task->end);
}


/*  setup_task() -- allocate and initialize a task
 */
static ptask_t *setup_task(void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t start, int64_t end)
{
    ptask_t *task = task_alloc();
    if (task == NULL)
        return NULL;

    ctx_construct(task->ctx, task->stack, TASK_STACK_SIZE, partr_coro, task);
    task->f = f;
    task->arg = arg;
    task->start = start;
    task->end = end;
    task->detached = 0;

    return task;
}


/*  release_task() -- destroy the coroutine context and free the task
 */
static void *release_task(ptask_t *task)
{
    void *ret = task->ret;
    ctx_destruct(task->ctx);
    task->ret = task->arg = task->f = NULL;
    task->start = task->end = 0;
    task_free(task);
    return ret;
}


/* run_next() -- get the next available task and run it
 */
static int run_next()
{
    ptask_t *task = multiq_deletemin();
    LOG_DEBUG(plog, "  thread %d resuming task %p\n", tid, task);
    curr_task = task;
    int64_t y = (int64_t)resume(task->ctx);
    curr_task = NULL;
    if (!ctx_is_done(task->ctx)) {
        if (y != yield_from_sync) {
            LOG_DEBUG(plog, "  thread %d had task %p yield\n", tid, task);
            multiq_insert(task, task->prio);
        }
        return 0;
    }
    LOG_DEBUG(plog, "  thread %d completed task %p\n", tid, task);

    /* the task completed; as detached tasks cannot be synced, clean
       those up here 
     */
    if (task->detached) {
        release_task(task);
        return 0;
    }

    /* add all the tasks in this one's completion queue to the ready queue */
    while (__atomic_test_and_set(&task->cq_lock, __ATOMIC_ACQUIRE))
        cpu_pause();
    ptask_t *cqtask, *cqnext;
    cqtask = task->cq;
    task->cq = NULL;
    while (cqtask) {
        cqnext = cqtask->cq_next;
        cqtask->cq_next = NULL;
        LOG_DEBUG(plog, "  thread %d adding task %p's CQ: %p\n",
                    tid, task, cqtask);
        multiq_insert(cqtask, cqtask->prio);
        cqtask = cqnext;
    }
    __atomic_clear(&task->cq_lock, __ATOMIC_RELEASE);

    return 0;
}


/*  partr_start() -- the runtime entry point

    To be called from thread 0, before creating any tasks. Wraps into
    a task and invokes `f(arg)`; tasks should only be spawned/synced
    from within tasks.
 */
int partr_start(void **ret, void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t start, int64_t end)
{
    ptask_t *task = setup_task(f, arg, start, end);
    if (task == NULL)
        return -1;

    LOG_DEBUG(plog, "  thread %d invoking start task %p\n", tid, task);
    curr_task = task;
    int64_t y = (int64_t)resume(task->ctx);
    curr_task = NULL;

    if (!ctx_is_done(task->ctx)) {
        LOG_DEBUG(plog, "  thread %d had start task %p yield\n", tid, task);
        if (y != yield_from_sync)
            multiq_insert(task, tid);
        while (run_next() == 0)
            ;
    }

    *ret = release_task(task);

    LOG_INFO(plog, "  thread %d released start task %p\n", tid, task);
    return 0;
}


/*  partr_thread() -- the thread function

    Loops, getting tasks from the multiqueue and executing them.
 */
static void *partr_thread(void *arg_)
{
    BARRIER_THREAD_DECL;
    lthread_arg_t *arg = (lthread_arg_t *)arg_;

    tid = arg->tid;
    seed_cong(&rngseed);

    /* set affinity if requested */
    if (arg->exclusive) {
        hwloc_set_cpubind(arg->topology, arg->cpuset, HWLOC_CPUBIND_THREAD);
        hwloc_bitmap_free(arg->cpuset);
    }
    show_affinity();

    BARRIER();

    /* free the thread function argument */
    free(arg);

    /* get the highest priority task and run it */
    while (run_next() == 0)
        ;

    LOG_INFO(plog, "  thread %d exiting\n", tid);
    return NULL;
}


/*  partr_spawn() -- create a task for `f(arg)` and enqueue it for execution

    Implicitly asserts that `f(arg)` can run concurrently with everything
    else that's currently running. If `detach` is set, the spawned task
    will not be returned (and cannot be synced).
 */
int partr_spawn(partr_t *t, void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t start, int64_t end, int8_t detach)
{
    ptask_t *task = setup_task(f, arg, start, end);
    if (task == NULL)
        return -1;
    task->detached = detach;

    if (multiq_insert(task, tid) != 0) {
        release_task(task);
        return -2;
    }

    *t = detach ? NULL : (partr_t)task;

    LOG_DEBUG(plog, "  thread %d task %p spawned task %p\n", tid, curr_task, task);
    return 0;
}


/*  partr_sync() -- get the return value of task `t`

    Returns only when task `t` has completed.
 */
int partr_sync(void **r, partr_t t, int done_with_task)
{
    ptask_t *task = (ptask_t *)t;

    /* if the target task has not finished, add the current task to its
       completion queue; the thread that runs the target task will add
       this task back to the ready queue
     */
    if (!ctx_is_done(task->ctx)) {
        curr_task->cq_next = NULL;
        while (__atomic_test_and_set(&task->cq_lock, __ATOMIC_ACQUIRE))
            cpu_pause();

        /* ensure the task didn't finish before we got the lock */
        if (!ctx_is_done(task->ctx)) {
            LOG_DEBUG(plog, "  thread %d task %p sync on task %p\n",
                          tid, curr_task, task);

            /* add the current task to the CQ */
            if (task->cq == NULL)
                task->cq = curr_task;
            else {
                ptask_t *pt = task->cq;
                while (pt->cq_next)
                    pt = pt->cq_next;
                pt->cq_next = curr_task;
            }

            /* unlock the CQ and yield the current task */
            __atomic_clear(&task->cq_lock, __ATOMIC_RELEASE);
            yield_value(curr_task->ctx, (void *)yield_from_sync);
        }

        /* the task finished before we could add to its CQ */
        else {
            __atomic_clear(&task->cq_lock, __ATOMIC_RELEASE);
            LOG_DEBUG(plog, "  thread %d task %p sync on task %p success\n",
                        tid, curr_task, task);
        }
    }

    *r = task->ret;

    if (done_with_task)
        release_task(task);

    return 0;
}


/*  partr_parfor() -- spawn multiple tasks for a parallel loop

    Spawn tasks that invoke `f(arg, start, end)` such that the sum of `end-start`
    for all tasks is `count`.

    TODO: sync
    TODO: reduction
 */
int partr_parfor(partr_t *t, void *(*f)(void *, int64_t, int64_t),
        void *arg, int64_t count, void *(*rf)(void *))
{
    int64_t n = K*nthreads;
    lldiv_t each = lldiv(count, n);

    int64_t start = 0, end;
    for (int64_t i = 0;  i < n;  ++i) {
        end = start + each.quot + (i < each.rem ? 1 : 0);
        ptask_t *task = setup_task(f, arg, start, end);
        if (task == NULL) {
            LOG_CRITICAL(plog, "  thread %d parfor task setup failed!\n", tid);
            return -1;
        }

        if (multiq_insert(task, tid) != 0) {
            release_task(task);
            LOG_CRITICAL(plog, "  thread %d parfor multiq insertion failed!\n", tid);
            return -2;
        }
        start = end;
    }

    LOG_DEBUG(plog, "  thread %d task %p parfor spawned %lld tasks\n",
            tid, curr_task, n);
    return 0;
}

