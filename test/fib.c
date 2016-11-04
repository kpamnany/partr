#include <stdio.h>

#include "tap.h"
#include "partr.h"

void *fib(void *arg_)
{
    int n = (int)arg_;
    if (n < 2)
        return n;

    partr tx, ty;
    tx = partr_spawn(fib, n-1);
    ty = partr_spawn(fib, n-2);
    int x, y;
    x = partr_sync(tx);
    y = partr_sync(ty);
    return x + y;
}

int main(int argc, char **argv)
{
    partr_init();

    int result;
    result = (int)fib(39);
    printf("%d\n", result);

    partr_shutdown();
    return 0;
}

