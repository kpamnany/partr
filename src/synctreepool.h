/*  partr -- parallel tasks runtime

    Pool of synchronization trees, for synchronizing parfor-generated tasks.
 */

#ifndef SYNCTREEPOOL_H
#define SYNCTREEPOOL_H

#include <stdint.h>

typedef struct arriver_tag arriver_t;
typedef struct reducer_tag reducer_t;

/* interface */
void synctreepool_init();
void synctreepool_destroy();
arriver_t *arriver_alloc();
void arriver_free(arriver_t *);
reducer_t *reducer_alloc();
void reducer_free(reducer_t *);

int last_arriver(arriver_t *, int);
void *reduce(arriver_t *, reducer_t *, void *(*rf)(void *, void *), void *, int);


#endif /* SYNCTREEPOOL_H */

