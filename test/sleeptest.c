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

void *add(void *arg1, void *arg2)
{
    int64_t a1 = (int64_t)arg1;
    int64_t a2 = (int64_t)arg2;
    return (void *)(a1 + a2);
}

void *run(void *arg, int64_t start, int64_t end)
{
    int64_t sum;
    partr_t t;
    partr_parfor(&t, fill_arr, NULL, 1024, add);
    partr_sync((void *)&sum, t, 1);

    return (void *)sum;
}

void start_test()
{
    for (int i = 0;  i < 1024;  ++i)
        arr[i] = -1;

    int64_t par_sum;

    partr_start((void *)&par_sum, run, NULL, 0, 0);
    printf("sum: %lld\n", par_sum);

    int success = 1, sum = 0;
    for (int i = 0;  i < 1024;  ++i) {
        if (arr[i] != i) {
            success = 0;
            break;
        }
        sum = sum + arr[i];
    }

    ok(success, "all elements filled");
    ok(sum == par_sum, "%lld == %lld", sum, par_sum);

}

int main(int argc, char **argv)
{
    partr_init();

    start_test();
    diag("pausing for 5 seconds to let threads sleep\n");
    sleep(5);
    diag("re-running test\n");
    start_test();

    partr_shutdown();
    return 0;
}

