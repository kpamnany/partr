#include <stdio.h>
#include "tap.h"
#include "partr.h"

int arr[1024];

void *fill_arr(void *arg_, int64_t start, int64_t end)
{
    int64_t sum = 0;
    for (int64_t i = start;  i < end;  ++i) {
        arr[i] = i;
        sum += i;
    }

    return (void *)sum;
}

void *run(void *arg, int64_t start, int64_t end)
{
    int64_t sum;
    partr_t t;
    partr_parfor(&t, fill_arr, NULL, 1024, NULL);
    partr_sync((void *)&sum, t, 1);

    return (void *)sum;
}

int main(int argc, char **argv)
{
    int64_t sum;

    for (int i = 0;  i < 1024;  ++i)
        arr[i] = -1;

    partr_init();
    partr_start((void *)&sum, run, NULL, 0, 0);
    partr_shutdown();

    printf("sum: %lld\n", sum);

    int success = 1;
    for (int i = 0;  i < 1024;  ++i) {
        if (arr[i] != i) {
            success = 0;
            break;
        }
    }

    ok(success, "all elements filled");

    return 0;
}

