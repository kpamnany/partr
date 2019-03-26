/*  partr -- parallel tasks runtime

    MultiQueues (http://arxiv.org/abs/1411.1209)
 */


#include <stdlib.h>
#include <pthread.h>
#include "partr.h"
#include "multiq.h"
#include "congrng.h"
#include "perfutil.h"


/* individual spin-lock synchronized task heap */
typedef struct taskheap_tag {
    char lock;
    ptask_t **tasks;
    int16_t ntasks, prio;
} taskheap_t;

/* heap 'n'ary */
static const int16_t heap_d = 8;

/* the multiqueue itself is 'p' task heaps */
static taskheap_t *heaps;
static int16_t heap_p;

/* for atomic snapshot */
static uint64_t snapshot_owner = -1;

/* unbias state for the RNG */
static uint64_t cong_unbias;

/* state for sleep checking */
static const int16_t not_sleeping = 0;
static const int16_t checking_for_sleeping = 1;
static const int16_t sleeping = 2;
static int16_t sleep_check_state = not_sleeping;


/*  multiq_init()
 */
void multiq_init()
{
    heap_p = MULTIQ_HEAP_C * nthreads;
    heaps = (taskheap_t *)calloc(heap_p, sizeof(taskheap_t));
    for (int16_t i = 0;  i < heap_p;  ++i) {
        __atomic_clear(&heaps[i].lock, __ATOMIC_RELAXED);
        heaps[i].tasks = (ptask_t **)
                calloc(MULTIQ_TASKS_PER_HEAP, sizeof(ptask_t *));
        heaps[i].ntasks = 0;
        heaps[i].prio = INT16_MAX;
    }
    unbias_cong(heap_p, &cong_unbias);
    LOG_INFO(plog, "  %d %d-ary heaps of %d tasks each\n",
             heap_p, heap_d, MULTIQ_TASKS_PER_HEAP);
}


/*  multiq_destroy()
 */
void multiq_destroy()
{
    for (int16_t i = 0;  i < heap_p;  ++i)
        free(heaps[i].tasks);
    free(heaps);
}


/*  sift_up()
 */
static void sift_up(taskheap_t *heap, int16_t idx)
{
    if (idx > 0) {
        int16_t parent = (idx-1)/heap_d;
        if (heap->tasks[idx]->prio <= heap->tasks[parent]->prio) {
            ptask_t *t = heap->tasks[parent];
            heap->tasks[parent] = heap->tasks[idx];
            heap->tasks[idx] = t;
            sift_up(heap, parent);
        }
    }
}


/*  sift_down()
 */
void sift_down(taskheap_t *heap, int16_t idx)
{
    if (idx < heap->ntasks) {
        for (int16_t child = heap_d*idx + 1;
                child < MULTIQ_TASKS_PER_HEAP && child <= heap_d*idx + heap_d;
                ++child) {
            if (heap->tasks[child]
                    &&  heap->tasks[child]->prio <= heap->tasks[idx]->prio) {
                ptask_t *t = heap->tasks[idx];
                heap->tasks[idx] = heap->tasks[child];
                heap->tasks[child] = t;
                sift_down(heap, child);
            }
        }
    }
}


/*  multiq_insert()
 */
int multiq_insert(ptask_t *task, int16_t priority)
{
    uint64_t rn;
    
    task->prio = priority;
    do {
        rn = cong(heap_p, cong_unbias, &rngseed);
    } while (__atomic_test_and_set(&heaps[rn].lock, __ATOMIC_ACQUIRE));

    if (heaps[rn].ntasks >= MULTIQ_TASKS_PER_HEAP) {
        LOG_ERR(plog, "  heap %llu is full\n", rn);
        __atomic_clear(&heaps[rn].lock, __ATOMIC_RELEASE);
        return -1;
    }

    heaps[rn].tasks[heaps[rn].ntasks++] = task;
    sift_up(&heaps[rn], heaps[rn].ntasks-1);
    __atomic_clear(&heaps[rn].lock, __ATOMIC_RELEASE);
    int16_t prio = __atomic_load_n(&heaps[rn].prio, __ATOMIC_SEQ_CST);
    if (task->prio < prio)
        __atomic_compare_exchange_n(&heaps[rn].prio, &prio, task->prio,
                                    0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);

    return 0;
}


/*  multiq_deletemin()
 */
ptask_t *multiq_deletemin()
{
    uint64_t rn1, rn2;
    int16_t i, prio1, prio2;
    ptask_t *task;

    for (i = 0;  i < heap_p;  ++i) {
        rn1 = cong(heap_p, cong_unbias, &rngseed);
        rn2 = cong(heap_p, cong_unbias, &rngseed);
        prio1 = __atomic_load_n(&heaps[rn1].prio, __ATOMIC_SEQ_CST);
        prio2 = __atomic_load_n(&heaps[rn2].prio, __ATOMIC_SEQ_CST);
        if (prio1 > prio2) {
            prio1 = prio2;
            rn1 = rn2;
        }
        else if (prio1 == prio2 && prio1 == INT16_MAX)
            continue;
        if (!__atomic_test_and_set(&heaps[rn1].lock, __ATOMIC_ACQUIRE)) {
            if (prio1 == heaps[rn1].prio)
                break;
            __atomic_clear(&heaps[rn1].lock, __ATOMIC_RELEASE);
        }
    }
    if (i == heap_p)
        return NULL;

    task = heaps[rn1].tasks[0];
    heaps[rn1].tasks[0] = heaps[rn1].tasks[--heaps[rn1].ntasks];
    heaps[rn1].tasks[heaps[rn1].ntasks] = NULL;
    prio1 = INT16_MAX;
    if (heaps[rn1].ntasks > 0) {
        sift_down(&heaps[rn1], 0);
        prio1 = heaps[rn1].tasks[0]->prio;
    }
    __atomic_store_n(&heaps[rn1].prio, prio1, __ATOMIC_SEQ_CST);
    __atomic_clear(&heaps[rn1].lock, __ATOMIC_RELEASE);

    return task;
}


/*  multiq_minprio()
 */
int16_t multiq_minprio()
{
    uint64_t rn1, rn2;
    int16_t prio1, prio2;

    rn1 = cong(heap_p, cong_unbias, &rngseed);
    rn2 = cong(heap_p, cong_unbias, &rngseed);
    prio1 = __atomic_load_n(&heaps[rn1].prio, __ATOMIC_SEQ_CST);
    prio2 = __atomic_load_n(&heaps[rn2].prio, __ATOMIC_SEQ_CST);
    if (prio2 < prio1)
        return prio2;
    return prio1;
}


/*  just_sleep()
 */
static void just_sleep(pthread_mutex_t *lock, pthread_cond_t *wakeup)
{
    pthread_mutex_lock(lock);
    if (__atomic_load_n(&sleep_check_state, __ATOMIC_SEQ_CST) == sleeping)
        pthread_cond_wait(wakeup, lock);
    else
        pthread_mutex_unlock(lock);
}


/*  snapshot_and_sleep()
 */
static void snapshot_and_sleep(pthread_mutex_t *lock, pthread_cond_t *wakeup)
{
    uint64_t snapshot_id = cong(UINT64_MAX, UINT64_MAX, &rngseed), previous = -1;
    if (!__atomic_compare_exchange_n(&snapshot_owner, &previous, snapshot_id, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        LOG_ERR(plog, "  snapshot has previous owner!\n");
        return;
    }

    int16_t i;
    for (i = 0;  i < heap_p;  ++i) {
        if (heaps[i].ntasks != 0)
            break;
    }
    if (i != heap_p) {
        LOG_INFO(plog, "  heap has tasks, snapshot aborted\n");
        return;
    }

    if (!__atomic_compare_exchange_n(&snapshot_owner, &snapshot_id, previous, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        LOG_INFO(plog, "  snapshot owner changed, snapshot aborted\n");
        return;
    }
    if (!__atomic_compare_exchange_n(&sleep_check_state, (int16_t *)&checking_for_sleeping,
                                     sleeping, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        LOG_ERR(plog, "  sleep aborted at snapshot end\n");
        return;
    }
    just_sleep(lock, wakeup);
}


/*  multiq_sleep_if_empty()
 */
void multiq_sleep_if_empty(pthread_mutex_t *lock, pthread_cond_t *wakeup)
{
    int16_t state;

sleep_start:
    state = __atomic_load_n(&sleep_check_state, __ATOMIC_SEQ_CST);
    if (state == checking_for_sleeping) {
        for (; ;) {
            cpu_pause();
            state = __atomic_load_n(&sleep_check_state, __ATOMIC_SEQ_CST);
            if (state == not_sleeping)
                break;
            else if (state == sleeping) {
                just_sleep(lock, wakeup);
                break;
            }
        }
    }
    else if (state == not_sleeping) {
        if (!__atomic_compare_exchange_n(&sleep_check_state, (int16_t *)&not_sleeping,
                                         checking_for_sleeping, 0,
                                         __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
            goto sleep_start;
        snapshot_and_sleep(lock, wakeup);
        if (!__atomic_compare_exchange_n(&sleep_check_state, (int16_t *)&sleeping,
                                         not_sleeping, 0,
                                         __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
            LOG_ERR(plog, "  sleep check state update failed\n");
    }
    else /* state == sleeping */
        just_sleep(lock, wakeup);
}

