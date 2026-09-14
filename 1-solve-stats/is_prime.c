#include <omp.h>
#include <stdio.h>

// return 0 when not prime, 1 when prime
int is_prime_single(int num) {
    if (num % 2 == 0 && num != 2) {
        return 0;
    }
    for (int i = 3; i * i <= num; i = i + 2) {
        if (num % i == 0) {
            return 0;
        }
    }
    return 1;
}

// return the number of primes in total
void is_prime(int limit) {
    double start_time = omp_get_wtime();
    int total_primes = 0;
#pragma omp parallel for reduction(+ : total_primes) schedule(runtime)
    for (int i = 2; i < limit; i++) {
        total_primes += is_prime_single(i);
    }

    double total_time = omp_get_wtime() - start_time;
    printf("Have total of %d primes in %d numbers using time of %.5f secs",
           total_primes,
           limit,
           total_time);
}

int main() {
    is_prime(200000);
}