#include <omp.h>
#include <stdio.h>
#define N_steps 4000000000LL
#define CBLK 16
int main() {
    double pi;
    double dx;
    double s_time, t_time;
    dx = 1.0 / N_steps;
    omp_set_num_threads(omp_get_num_procs());
    s_time = omp_get_wtime();
    int act_nthreads;
    /* Pad each partial sum to its own 128-byte block (covers Apple cache lines) */
    volatile double sum[128][CBLK] = {{0.0}};
    printf("Using %d threads (padded, no false sharing)\n", omp_get_max_threads());
#pragma omp parallel
    {
        long i;
        int id = omp_get_thread_num();
        int numthreads = omp_get_num_threads();
        if (id == 0)
            act_nthreads = numthreads;
        double x;
        for (i = id; i < N_steps; i += numthreads) {
            x = (i + 0.5) * dx;
            sum[id][0] += 4.0 / (1.0 + x * x);
        }
    }
    pi = 0.0;
    for (int i = 0; i < act_nthreads; i++)
        pi += sum[i][0];
    pi *= dx;
    t_time = omp_get_wtime() - s_time;
    printf("pi = %.15lf, %lld steps, %lf secs, %d threads\n", pi, N_steps, t_time, act_nthreads);
    return 0;
}
