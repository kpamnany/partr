/*  partr -- parallel tasks runtime
  
    MultiQueues (http://arxiv.org/abs/1411.1209)
 */

#ifndef MULTIQ_H
#define MULTIQ_H

#include <stdint.h>
#include "task.h"


void multiq_init();
void multiq_destroy();
int multiq_insert(ptask_t *elem, int16_t priority);
ptask_t *multiq_deletemin();
int16_t multiq_minprio();


#endif /* MULTIQ_H */

