#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef N
#define N 100000000L
#endif

int main(void) {
    const double a = 1.1;
    double *x = malloc((size_t)N * sizeof(double));
    double *y = malloc((size_t)N * sizeof(double));
    if (!x || !y) {
        fprintf(stderr,
                "malloc failed (N=%ld, need ~%.1f GiB for x and y)\n",
                (long)N,
                (2.0 * (double)N * sizeof(double)) / (1024.0 * 1024.0 * 1024.0));
        return 1;
    }

    /* Serial init on the main thread (see Q3.9 first-touch / NUMA). */
    for (long i = 0; i < N; i++) {
        x[i] = 1.0;
        y[i] = 2.0;
    }

    printf("reduction  N=%ld  threads=%d\n", (long)N, omp_get_max_threads());

    double checksum = 0.0;
    double s_time = omp_get_wtime();

    /* TODO Q3.3: rewrite this kernel with OpenMP reduction.
     * Keep default(none) and schedule(static). */
    for (long i = 0; i < N; i++) {
        double value = a * x[i] + y[i];
        y[i] = value;
        checksum += value;
    }

    double t_time = omp_get_wtime() - s_time;
    printf("checksum=%.6f  time=%.6f s\n", checksum, t_time);

    free(x);
    free(y);
    return 0;
}
