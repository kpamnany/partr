#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>
#include <immintrin.h>
#include <partr.h>

#if defined(__i386__)
static inline uint64_t rdtsc(void)
{
    uint64_t x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
#elif defined(__x86_64__)
static inline uint64_t rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo) | (((uint64_t)hi) << 32);
}
#elif defined(_COMPILER_MICROSOFT_)
#include <intrin.h>
static inline uint64_t rdtsc(void)
{
    return __rdtsc();
}
#endif

void l3d_partr(int nx, int ny, int nz, float *u1, float *u2);
void l3d_orig(int nx, int ny, int nz, float *u1, float *u2);

double cpughz()
{
    uint64_t t0 = rdtsc();
    sleep(1);
    uint64_t onesec = rdtsc() - t0;
    return onesec*1.0/1e9;
}

int main(int argc, char **argv)
{
    int nx, ny, nz, iters, i, j, k, ind, p_ind, verify, nthreads;
    float *u1, *u1_orig, *u2, *u2_orig, *foo,
        error_tol = 0.00001;
    double ghz;

    if (argc != 6) {
        fprintf(stderr, "Usage:\n"
            "  l3d_partr <nx> <ny> <nz> <#iters> <verify?>\n");
        exit(-1);
    }

    nx = strtol(argv[1], NULL, 10);
    ny = strtol(argv[2], NULL, 10);
    nz = strtol(argv[3], NULL, 10);

    iters = strtol(argv[4], NULL, 10);

    verify = strtol(argv[5], NULL, 10);

    ghz = cpughz();
    nthreads = 4;
    printf("machine speed is %g GHz, using %d threads\n", ghz, nthreads);

    printf("laplace3d: %d iterations on %dx%dx%d grid, "
           "verification is %s\n",
           iters, nx, ny, nz, verify ? "on" : "off");

    u1 = (float *)_mm_malloc(sizeof (float) * (nx * ny * nz), 64);
    u2 = (float *)_mm_malloc(sizeof (float) * (nx * ny * nz), 64);
    u1_orig = (float *)_mm_malloc(sizeof (float) * nx * ny * nz, 64);
    u2_orig = (float *)_mm_malloc(sizeof (float) * nx * ny * nz, 64);

    // initialize
    for (k = 0;  k < nz;  ++k) {
        for (j = 0;  j < ny;  ++j) {
            for (i = 0;  i < nx;  ++i) {
                ind = i + j*nx + k*nx*ny;
                p_ind = i + j*nx + k*nx*ny;

                if (i == 0  ||  i == nx - 1
                        ||  j == 0  ||  j == ny - 1
                        ||  k == 0  ||  k == nz - 1) {
                    // Dirichlet b.c.'s
                    u1[p_ind] = u1_orig[ind] = u2[p_ind] = 1.0f;
                }
                else {
                    u1[p_ind] = u1_orig[ind] = u2[p_ind] = 0.0f;
                }
            }
        }
    }

    partr_init();

    // run optimized version
    uint64_t t0 = rdtsc();
    for (i = 0;  i < iters;  ++i) {
        l3d_partr(nx, ny, nz, u1, u2);
        foo = u1; u1 = u2; u2 = foo;
    }
    uint64_t gold = rdtsc() - t0;
    double elapsed = gold / (ghz * 1e9);

    double grid_size = nx * ny * nz;
    double gflops = grid_size * iters * 6.0 / 1e9;
    double gflops_sec = gflops / elapsed;

    double traffic = grid_size * iters * 4 * 2.0 / 1e9;
    double bw_realized = traffic / elapsed;

    printf("laplace3d completed in %.4lf seconds\n", elapsed);
    printf("GFLOPs/sec: %.1f\n", gflops_sec);
    printf("BW realized: %.1f\n", bw_realized);

    if (verify) {
        // run serial version for verification
        uint64_t st0 = rdtsc();
        for (i = 0;  i < iters;  ++i) {
            l3d_orig(nx, ny, nz, u1_orig, u2_orig);
            foo = u1_orig; u1_orig = u2_orig; u2_orig = foo;
        }
        uint64_t ser = rdtsc() - st0;
        elapsed = ser / (ghz * 1e9);
        gflops_sec = gflops / elapsed;
        bw_realized = traffic / elapsed;

        printf("laplace3d_orig completed in %.2lf seconds\n", elapsed);
        printf("GFLOPs/sec: %.1f\n", gflops_sec);
        printf("BW realized: %.1f\n", bw_realized);

        // verify
        for (k = 0;  k < nz;  ++k) {
            for (j = 0;  j < ny;  ++j) {
                for (i = 0;  i < nx;  ++i) {
                    ind = i + j*nx + k*nx*ny;
                    p_ind = i + j*nx + k*nx*ny;

                    if (fabs(u1[p_ind] - u1_orig[ind]) > error_tol) {
                        printf("ERROR %f - %f [%d, %d, %d]\n",
                               u1[p_ind], u1_orig[ind], i, j, k);
                        goto done;
                    }
                }
            }
        }
        printf("verified, no error\n");
    }

    partr_shutdown();

    done:
    _mm_free(u1);
    _mm_free(u2);
    _mm_free(u1_orig);
    _mm_free(u2_orig);

    return 0;
}

typedef struct task_arg_tag {
    int nx, ny, nz;
    float *u1, *u2;
} task_arg_t;

void *l3d_partr_iter(void *arg, int64_t start, int64_t end)
{
    int i, j, k, ind;
    const float sixth = 1.0f/6.0f;
    task_arg_t *ta = (task_arg_t *)arg;
    int nx = ta->nx;
    int ny = ta->ny;
    int nz = ta->nz;
    float *u1 = ta->u1;
    float *u2 = ta->u2;

    for (k = start;  k < end;  ++k) {
        for (j = 0;  j < ny;  ++j) {
            for (i = 0;  i < nx;  ++i) {
                ind = i + j*nx + k*nx*ny;

                if (i == 0  ||  i == nx - 1
                        ||  j == 0  ||  j == ny - 1
                        ||  k == 0  ||  k == nz - 1) {
                    u2[ind] = u1[ind];          // Dirichlet b.c.'s
                }
                else {
                    u2[ind] = ( u1[ind-1    ] + u1[ind+1    ]
                              + u1[ind-nx   ] + u1[ind+nx   ]
                              + u1[ind-nx*ny] + u1[ind+nx*ny] ) * sixth;
                }
            }
        }
    }
    return NULL;
}

void *l3d_partr_run(void *arg, int64_t start, int64_t end)
{
    partr_t t;
    partr_parfor(&t, l3d_partr_iter, arg, end - start, NULL);
    partr_sync(NULL, t, 1);
    return NULL;
}

void l3d_partr(int nx, int ny, int nz, float *u1, float *u2)
{
    task_arg_t task_arg;
    task_arg.nx = nx;
    task_arg.ny = ny;
    task_arg.nz = nz;
    task_arg.u1 = u1;
    task_arg.u2 = u2;
    partr_start(NULL, l3d_partr_run, (void *)&task_arg, 0, nz);
}

void l3d_orig(int nx, int ny, int nz, float *u1, float *u2)
{
    int i, j, k, ind;
    const float sixth = 1.0f/6.0f;

    for (k = 0;  k < nz;  ++k) {
        for (j = 0;  j < ny;  ++j) {
            for (i = 0;  i < nx;  ++i) {
                ind = i + j*nx + k*nx*ny;

                if (i == 0  ||  i == nx - 1
                        ||  j == 0  ||  j == ny - 1
                        ||  k == 0  ||  k == nz - 1) {
                    u2[ind] = u1[ind];          // Dirichlet b.c.'s
                }
                else {
                    u2[ind] = ( u1[ind-1    ] + u1[ind+1    ]
                              + u1[ind-nx   ] + u1[ind+nx   ]
                              + u1[ind-nx*ny] + u1[ind+nx*ny] ) * sixth;
                }
            }
        }
    }
}
