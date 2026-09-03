#include <omp.h>
#include <stdio.h>
#define N_steps 4000000000LL
int main() {
    long i;
    double pi;
    double sum = 0.0;
    double x, dx;
    double s_time, t_time;
    dx = 1.0 / N_steps;
    s_time = omp_get_wtime();
    for (i = 0; i < N_steps; i++) {
        x = (i + 0.5) * dx;
        sum += 4.0 / (1.0 + x * x);
    }
    pi = sum * dx;
    t_time = omp_get_wtime() - s_time;
    printf("pi = %.15lf, %lld steps, %lf secs\n", pi, N_steps, t_time);
    return 0;
}
