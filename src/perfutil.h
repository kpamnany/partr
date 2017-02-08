/*  partr -- parallel tasks runtime
 */

#ifndef PERFUTIL_H
#define PERFUTIL_H

#include <stdint.h>

#define cpu_pause()  __asm__("pause;");

static inline uint64_t rdtscp()
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtscp"
                      : /* outputs */ "=a" (lo), "=d" (hi)
                      : /* no inputs */
                      : /* clobbers */ "%rcx");
    return ((uint64_t)hi << 32) + lo;
}


#endif /* PERFUTIL_H */

