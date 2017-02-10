/*  partr -- parallel tasks runtime

    Pool of synchronization trees, for synchronizing parfor-generated tasks.
    Synchronization and reduction are managed via two binary trees.
 */

#include <stdlib.h>
#include "partr.h"
#include "synctreepool.h"


/* arrival tree */
struct arriver_tag {
    int16_t index, next_avail;
    int16_t **tree;
};


/* reduction tree */
struct reducer_tag {
    int16_t index, next_avail;
    void ***tree;
};



/* pool of arrival trees */
static arriver_t *arriverpool;
static int16_t num_arrivers, num_arriver_tree_nodes, next_arriver;


/* pool of reduction trees */
static reducer_t *reducerpool;
static int16_t num_reducers, num_reducer_tree_nodes, next_reducer;


/*  synctreepool_init()
 */
void synctreepool_init()
{
    num_arriver_tree_nodes = (GRAIN_K * nthreads) - 1;
    num_reducer_tree_nodes = (2 * GRAIN_K * nthreads) - 1;

    /* num_arrivers = ((GRAIN_K * nthreads) ^ ARRIVERS_P) + 1 */
    num_arrivers = GRAIN_K * nthreads;
    for (int i = 1;  i < ARRIVERS_P;  ++i)
        num_arrivers = num_arrivers * num_arrivers;
    ++num_arrivers;

    num_reducers = num_arrivers * REDUCERS_FRAC;

    /* allocate */
    arriverpool = (arriver_t *)calloc(num_arrivers, sizeof (arriver_t));
    next_arriver = 0;
    for (int i = 0;  i < num_arrivers;  ++i) {
        arriverpool[i].index = i;
        arriverpool[i].next_avail = i + 1;
        posix_memalign((void **)&arriverpool[i].tree, 64,
                num_arriver_tree_nodes * sizeof (int16_t *));
        //arriverpool[i].tree =
        //        aligned_alloc(64, num_arriver_tree_nodes * sizeof (int16_t *));
        for (int j = 0;  j < num_arriver_tree_nodes;  ++j)
            posix_memalign((void **)&arriverpool[i].tree[j], 64, sizeof (int16_t));
            //arriverpool[i].tree[j] = aligned_alloc(64, sizeof (int16_t));
    }
    arriverpool[num_arrivers - 1].next_avail = -1;

    reducerpool = (reducer_t *)calloc(num_reducers, sizeof (reducer_t));
    next_reducer = 0;
    for (int i = 0;  i < num_reducers;  ++i) {
        reducerpool[i].index = i;
        reducerpool[i].next_avail = i + 1;
        posix_memalign((void **)&reducerpool[i].tree, 64,
                num_reducer_tree_nodes * sizeof (void **));
        //reducerpool[i].tree =
        //        aligned_alloc(64, num_reducer_tree_nodes * sizeof (void **));
        for (int j = 0;  j < num_reducer_tree_nodes;  ++j)
            posix_memalign((void **)&reducerpool[i].tree[j], 64, sizeof (void *));
            //reducerpool[i].tree[j] = aligned_alloc(64, sizeof (void *));
    }
    if (num_reducers > 0)
        reducerpool[num_reducers - 1].next_avail = -1;
    else
        next_reducer = -1;

    LOG_INFO(plog, "  %d arrivers and %d reducers allocated\n",
            num_arrivers, num_reducers);
}


/*  synctreepool_destroy()
 */
void synctreepool_destroy()
{
    for (int i = 0;  i < num_arrivers;  ++i) {
        for (int j = 0;  j < num_arriver_tree_nodes;  ++j)
            free(arriverpool[i].tree[j]);
        free(arriverpool[i].tree);
    }
    free(arriverpool);

    arriverpool = NULL;
    num_arrivers = 0;
    num_arriver_tree_nodes = 0;
    next_arriver = -1;

    for (int i = 0;  i < num_reducers;  ++i) {
        for (int j = 0;  j < num_reducer_tree_nodes;  ++j)
            free(reducerpool[i].tree[j]);
        free(reducerpool[i].tree);
    }
    free(reducerpool);

    reducerpool = NULL;
    num_reducers = 0;
    num_reducer_tree_nodes = 0;
    next_reducer = -1;
}


/*  arriver_alloc()
 */
arriver_t *arriver_alloc()
{
    int16_t candidate;
    arriver_t *arr;

    do {
        candidate = __atomic_load_n(&next_arriver, __ATOMIC_SEQ_CST);
        if (candidate == -1) {
            LOG_ERR(plog, "  <%d> arriver allocation failed\n", tid);
            return NULL;
        }
        arr = &arriverpool[candidate];
    } while (!__atomic_compare_exchange_n(&next_arriver,
                     &candidate, arr->next_avail,
                     0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
    return arr;
}


/*  arriver_free()
 */
void arriver_free(arriver_t *arr)
{
    for (int i = 0;  i < num_arriver_tree_nodes;  ++i)
        *arr->tree[i] = 0;

    __atomic_exchange(&next_arriver, &arr->index, &arr->next_avail,
            __ATOMIC_SEQ_CST);
}


/*  reducer_alloc()
 */
reducer_t *reducer_alloc()
{
    int16_t candidate;
    reducer_t *red;

    do {
        candidate = __atomic_load_n(&next_reducer, __ATOMIC_SEQ_CST);
        if (candidate == -1) {
            LOG_ERR(plog, "  <%d> reducer allocation failed\n", tid);
            return NULL;
        }
        red = &reducerpool[candidate];
    } while (!__atomic_compare_exchange_n(&next_reducer,
                     &candidate, red->next_avail,
                     0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
    return red;
}


/*  reducer_free()
 */
void reducer_free(reducer_t *red)
{
    for (int i = 0;  i < num_reducer_tree_nodes;  ++i)
        *red->tree[i] = 0;

    __atomic_exchange(&next_reducer, &red->index, &red->next_avail,
            __ATOMIC_SEQ_CST);
}


/*  last_arriver()
 */
int last_arriver(arriver_t *arr, int idx)
{
    int arrived, aidx = idx + (GRAIN_K * nthreads) - 1;

    while (aidx > 0) {
        --aidx;
        aidx >>= 1;
        arrived = __atomic_fetch_add(arr->tree[aidx], 1, __ATOMIC_SEQ_CST);
        if (!arrived) return 0;
    }

    return 1;
}


/*  reduce()
 */
void *reduce(arriver_t *arr, reducer_t *red, void *(*rf)(void *, void *),
        void *val, int idx)
{
    int arrived, aidx = idx + (GRAIN_K * nthreads) - 1, ridx = aidx, nidx;

    *red->tree[ridx] = val;
    while (aidx > 0) {
        --aidx;
        aidx >>= 1;
        arrived = __atomic_fetch_add(arr->tree[aidx], 1, __ATOMIC_SEQ_CST);
        if (!arrived) return NULL;

        /* neighbor has already arrived, get its value and reduce it */
        nidx = ridx & 0x1 ? ridx + 1 : ridx - 1;
        val = rf(val, *red->tree[nidx]);

        /* move up the tree */
        --ridx;
        ridx >>= 1;
        *red->tree[ridx] = val;
    }

    return val;
}

