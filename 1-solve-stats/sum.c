#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#define ARR_LEN 100000000

void sum(int *in, int length, long long *out, double *time) {
    double start_time = omp_get_wtime();
#pragma omp parallel for
    for (int i = 0; i < length; i++) {
        *out += in[i];
    }

    *time = omp_get_wtime() - start_time;
}

int main() {
    int *a = malloc(sizeof(int) * ARR_LEN);
    for (int i = 0; i < ARR_LEN; i++) {
        a[i] = i;
    }
    long long *result = malloc(sizeof(long long));
    double *time = malloc(sizeof(time));
    sum(a, ARR_LEN, result, time);
    printf("The result is %lld with a time of %.3f", *result, *time);
}