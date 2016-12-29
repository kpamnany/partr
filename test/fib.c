#include <stdio.h>

#include "partr.h"

void *fib(void *arg_, int64_t start, int64_t end)
{
    partr_t tx;
    int64_t x, y, n = (int64_t)arg_;
    if (n < 2)
        return (void *)n;

    partr_spawn(&tx, fib, (void *)n-1, 0, 0, 0);
    y = (int64_t)fib((void *)n-2, 0, 0);
    partr_sync((void *)&x, tx, 1);

    return (void *)x + y;
}

void *serial_fib(void *arg_)
{
    int64_t x, y, n = (int64_t)arg_;
    if (n < 2)
        return (void *)n;
    x = (int64_t)serial_fib((void *)n-1);
    y = (int64_t)serial_fib((void *)n-2);
    return (void *)x + y;
}

void *run(void *arg, int64_t start, int64_t end)
{
    int64_t v = 10, result, sresult;
    result = (int64_t)fib((void *)v, 0, 0);
    sresult = (int64_t)serial_fib((void *)v);
    printf("fib(%lld)=%lld\nserial_fib(%lld)=%lld\n", v, result, v, sresult);

    return 0;
}

int main(int argc, char **argv)
{
    void *ret;
    partr_init();
    partr_start(&ret, run, NULL, 0, 0);
    partr_shutdown();
    return 0;
}

