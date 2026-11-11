#include <float.h>
#include <omp.h>
#include <stdio.h>

void stats(const double *a, long n, double *sum_out, double *min_out) {
    double sum = 0.0;
    double min_value = DBL_MAX;

    omp_set_num_threads(omp_get_num_procs());
    double s_time, t_time;
    s_time = omp_get_wtime();

    printf("Using %d threads (padded, no false sharing)\n", omp_get_max_threads());

#pragma omp parallel for default(none) shared(a, n) reduction(+ : sum) reduction(min : min_value)
    for (long i = 0; i < n; i++) {
        sum += a[i];

        if (a[i] < min_value) {
            min_value = a[i];
        }
    }

    *sum_out = sum;
    *min_out = min_value;

    t_time = omp_get_wtime() - s_time;
    printf("Time: %lf secs\n", t_time);
}