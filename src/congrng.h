/*  partr -- parallel tasks runtime
  
    Simple random number generator (linear congruential). No modulo bias.
 */

#ifndef CONGRNG_H
#define CONGRNG_H

#include <stdint.h>


void seed_cong(uint64_t *seed);
void unbias_cong(uint64_t max, uint64_t *unbias);
uint64_t cong(uint64_t max, uint64_t unbias, uint64_t *seed);


#endif /* CONGRNG_H */

