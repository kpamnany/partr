/*  partr -- parallel tasks runtime

    task definition
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <concurrent/concurrent.h>
#include <concurrent/shortname.h>


enum {
    PARTR_ONETASK,
    PARTR_NTASKS,
    PARTR_EXITTASK
};


typedef struct p1task_tag p1task_t;
typedef struct pntask_tag pntask_t;


typedef struct ptask_tag ptask_t;

struct ptask_tag {
    /* coroutine context and stack */
    struct concurrent_ctx *ctx;
    uint8_t               *stack;

    /* task entry point, argument, and result */
    void    *(*f)(void *, int64_t, int64_t);
    void    *arg, *ret;
    int64_t start, end;

    /* tasks to run on completion */
    ptask_t *cq, *cq_next;
    int8_t  cq_lock;

    /* clean up on completion */
    int8_t  detached;

    /* to manage task pools */
    int16_t pool, index, next_avail;

    /* for the multiqueue */
    int16_t prio;
};


#endif /* TASK_H */

