/*  partr -- parallel tasks runtime

    taskpools for fast task allocation/freeing
 */

#ifndef TASKPOOLS_H
#define TASKPOOLS_H

#include <stdint.h>
#include "task.h"


/* interface */
void taskpools_init();
void taskpools_destroy();
ptask_t *task_alloc();
void task_free(ptask_t *);


#endif /* TASKPOOLS_H */

