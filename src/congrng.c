/*  partr -- parallel tasks runtime
  
    Simple congruential random number generator (from VAX). No modulo bias.
 */

#include "congrng.h"
#include "perfutil.h"


/*  seed_cong() -- each thread needs its own seed!
 */
void seed_cong(uint64_t *seed)
{
    *seed = rdtscp();
}


/*  unbias_cong() -- sets up state to avoid modulo bias for the given max.
 */
void unbias_cong(uint64_t max, uint64_t *unbias)
{
    *unbias = UINT64_MAX - ((UINT64_MAX % max)+1);
}


/*  cong() -- linear congruential generator (was system RNG on VAXen).
 *      Loop to avoid modulo bias.
 */
uint64_t cong(uint64_t max, uint64_t unbias, uint64_t *seed)
{
    while ((*seed = 69069 * (*seed) + 362437) > unbias)
        ;
    return *seed % max;
}

