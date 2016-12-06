#include <stdio.h>
#include "tap.h"
#include "partr.h"

int arr[1024];

void *fill_arr(void *arg_, int64_t start, int64_t end)
{
    for (int64_t i = start;  i < end;  ++i)
        arr[i] = i;

    return NULL;
}

void *run(void *arg, int64_t start, int64_t end)
{
    partr_t t;
    partr_parfor(&t, fill_arr, NULL, 1024, NULL);
    sleep(1);

    return 0;
}

int main(int argc, char **argv)
{
    void *ret;

    for (int i = 0;  i < 1024;  ++i)
        arr[i] = -1;

    partr_init();
    partr_start(&ret, run, NULL, 0, 0);
    partr_shutdown();

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

