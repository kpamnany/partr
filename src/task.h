/*  partr -- parallel tasks runtime

    task definition
 */

#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <concurrent/concurrent.h>
#include <concurrent/shortname.h>


/* task settings */
#define TASK_TERMINATE          0x01
    /* terminate thread */
#define TASK_IS_DETACHED        0x02
    /* clean up the task on completion */
#define TASK_IS_STICKY          0x04
    /* task is sticky to the thread that first runs it */

typedef struct arriver_tag arriver_t;
typedef struct reducer_tag reducer_t;

typedef struct ptask_tag ptask_t;


/* a task */
struct ptask_tag {
    /* to link this task into queues */
    ptask_t *next;

    /* coroutine context and stack */
    struct concurrent_ctx *ctx;
    uint8_t               *stack;

    /* task entry point, arguments, result, reduction function */
    void    *(*f)(void *, int64_t, int64_t);
    void    *arg, *result;
    int64_t start, end;

    /* ----- IA-64 cache line boundary ----- */

    /* reduction function, for parfors */
    void    *(*rf)(void *, void *);

    /* parent (first) task of a parfor set */
    ptask_t *parent;

    /* to synchronize/reduce grains of a parfor */
    arriver_t *arr;
    reducer_t *red;

    /* parfor reduction result */
    void *red_result;

    /* completion queue and lock */
    ptask_t *cq;
    int8_t  cq_lock;

    /* task settings */
    int8_t  settings;

    /* tid of the thread to which this task is sticky */
    int16_t sticky_tid;

    /* the index of this task in the set of grains of a parfor */
    int16_t grain_num;

    /* for the multiqueue */
    int16_t prio;

    /* to manage task pools */
    int16_t pool, index, next_avail;

    /* padding to cache line boundary */
    int8_t cl2_padding[2];

    /* ----- IA-64 cache line boundary ----- */
};


#endif /* TASK_H */

