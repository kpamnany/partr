/*  partr -- parallel tasks runtime

    task definition
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <concurrent/concurrent.h>
#include <concurrent/shortname.h>

#define TASK_READY      -1

/* a task is essentially a coroutine */
typedef struct ptask_tag {
    struct      concurrent_ctx *ctx;
    uint8_t     *stack;

    void        *(*f)(void *);
    void        *arg, *ret;

    /* which thread is running this task (-1 for none) */
    int16_t     running_tid;

    /* tasks to run on completion */
    char                cq_lock;
    struct ptask_tag    *cq_head, *cq_tail, *cq_next;

    /* for the multiqueue */
    int16_t prio;

    /* to manage task pools */
    int16_t pool, index, next_avail;
    char used;

} ptask_t;


#endif /* TASK_H */

