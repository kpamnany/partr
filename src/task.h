/*  partr -- parallel tasks runtime

    task definition
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <concurrent/concurrent.h>
#include <concurrent/shortname.h>


typedef struct arriver_tag arriver_t;
typedef struct reducer_tag reducer_t;

typedef struct ptask_tag ptask_t;

struct ptask_tag {
    /* coroutine context and stack */
    struct concurrent_ctx *ctx;
    uint8_t               *stack;

    /* task entry point, arguments, result, reduction function */
    void    *(*f)(void *, int64_t, int64_t);
    void    *arg, *result;
    int64_t start, end;
    void    *(*rf)(void *, void *);

    /* parent (first) task of a parfor set */
    ptask_t *parent;

    /* the index of this task in the set of grains of a parfor */
    int16_t grain_num;

    /* to synchronize/reduce grains of a parfor */
    arriver_t *arr;
    reducer_t *red;

    /* parfor reduction result */
    void *red_result;

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

