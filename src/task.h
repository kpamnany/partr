/*  partr -- parallel tasks runtime

    task definition
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <concurrent/concurrent.h>
#include <concurrent/shortname.h>


typedef struct ptask_tag {
    /* coroutine context and stack */
    struct concurrent_ctx *ctx;
    uint8_t               *stack;

    /* task entry point, argument, and result */
    void *(*f)(void *);
    void *arg, *ret;

    /* tasks to run on completion */
    struct ptask_tag *cq, *cq_next;
    char             cq_lock;

    /* clean up on completion */
    bool detached;

    /* to manage task pools */
    int16_t pool, index, next_avail;

    /* for the multiqueue */
    int16_t prio;
} ptask_t;


#endif /* TASK_H */

