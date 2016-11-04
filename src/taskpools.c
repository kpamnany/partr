/*  partr -- parallel tasks runtime

    taskpools for fast allocation/freeing
 */

#include <stdlib.h>
#include "partr.h"
#include "taskpools.h"


/* task pool for quick allocation/freeing of tasks */
typedef struct ptaskpool_tag {
    int16_t num_tasks, next_avail;
    ptask_t *tasks;
} ptaskpool_t;


/* a task pool for each thread */
static ptaskpool_t *ptaskpools;


/*  taskpools_init()
 */
void taskpools_init()
{
    ptaskpools = (ptaskpool_t *)calloc(nthreads, sizeof(ptaskpool_t));
    for (int16_t i = 0;  i < nthreads;  ++i) {
        ptaskpools[i].num_tasks = TASKS_PER_POOL;
        ptaskpools[i].next_avail = 0;
        ptaskpools[i].tasks = (ptask_t *)
                calloc(TASKS_PER_POOL, sizeof(ptask_t));
        for (int16_t j = 0;  j < TASKS_PER_POOL;  ++j) {
            ptaskpools[i].tasks[j].ctx =
                    calloc(ctx_sizeof(), sizeof(uint8_t));
            ptaskpools[i].tasks[j].stack =
                    calloc(TASK_STACK_SIZE, sizeof(uint8_t));
            ptaskpools[i].tasks[j].running_tid = TASK_READY;
            __atomic_clear(&ptaskpools[i].tasks[j].cq_lock, __ATOMIC_RELAXED);
            ptaskpools[i].tasks[j].cq_head = NULL;
            ptaskpools[i].tasks[j].cq_tail = NULL;
            ptaskpools[i].tasks[j].cq_next = NULL;
            ptaskpools[i].tasks[j].pool = i;
            ptaskpools[i].tasks[j].index = j;
            ptaskpools[i].tasks[j].next_avail = j + 1;
        }
        ptaskpools[i].tasks[TASKS_PER_POOL-1].next_avail = -1;
    }
    LOG_INFO(plog, "  %d tasks allocated per pool\n", TASKS_PER_POOL);
}


/*  taskpools_destroy()
 */
void taskpools_destroy()
{
    for (int16_t i = 0;  i < nthreads;  ++i) {
        for (int16_t j = 0;  j < TASKS_PER_POOL;  ++j) {
            free(ptaskpools[i].tasks[j].stack);
            free(ptaskpools[i].tasks[j].ctx);
        }
        free(ptaskpools[i].tasks);
    }
    free(ptaskpools);
}


/*  task_alloc()
 */
ptask_t *task_alloc()
{
    int16_t candidate;
    ptask_t *task;
    ptaskpool_t *pool = &ptaskpools[tid];

    do {
        candidate = __atomic_load_n(&pool->next_avail, __ATOMIC_SEQ_CST);
        if (candidate == -1) {
            LOG_ERR(plog, "  <%d> task allocation failed\n", tid);
            return NULL;
        }
        task = &pool->tasks[candidate];
    } while (!__atomic_compare_exchange_n(&pool->next_avail,
                     &candidate, task->next_avail,
                     0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
    return task;
}


/*  task_free()
 */
void task_free(ptask_t *task)
{
    ptaskpool_t *pool = &ptaskpools[task->pool];

    __atomic_exchange(&pool->next_avail, &task->index, &task->next_avail,
                      __ATOMIC_SEQ_CST);
}

