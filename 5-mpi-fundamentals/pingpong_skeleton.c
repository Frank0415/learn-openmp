/* Task 1: Ping-Pong Benchmark (SKELETON, fill in the TODOs)
 *
 * Measure round-trip time between rank 0 and rank 1 for a range of message
 * sizes, then estimate alpha (latency) and beta (inverse bandwidth) from
 * your own measurements, using the cost model T(n) = alpha + n * beta.
 *
 * Run with exactly 2 ranks:
 *   mpicc -o pingpong pingpong_skeleton.c
 *   mpirun -np 2 ./pingpong
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_SIZES 7
#define TRIALS 50
#define WARMUP 5

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2) {
        if (rank == 0)
            fprintf(stderr, "Run with exactly 2 ranks.\n");
        MPI_Finalize();
        return 1;
    }

    /* Message sizes to test, in bytes. Doubling each time is deliberate:
     * it spreads points evenly on the log-log plot you saw in lecture. */
    long sizes[NUM_SIZES] = {8, 64, 512, 4096, 32768, 262144, 1048576};

    double one_way_us[NUM_SIZES]; /* fill this in below */

    for (int s = 0; s < NUM_SIZES; s++) {
        long n = sizes[s];
        char *buf = malloc(n);
        for (long i = 0; i < n; i++)
            buf[i] = (char)i;

        for (int t = 0; t < WARMUP; ++t) {
            if (rank == 0) {
                MPI_Send(buf, (int)n, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(buf, (int)n, MPI_BYTE, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                MPI_Recv(buf, (int)n, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buf, (int)n, MPI_BYTE, 0, 1, MPI_COMM_WORLD);
            }
        }
        MPI_Barrier(MPI_COMM_WORLD);

        double total_rtt = 0.0;
        for (int t = 0; t < TRIALS; t++) {
            if (rank == 0) {
                double start = MPI_Wtime();
                MPI_Send(buf, (int)n, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                MPI_Recv(buf, (int)n, MPI_BYTE, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                total_rtt += MPI_Wtime() - start;
            } else {
                MPI_Recv(buf, (int)n, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(buf, (int)n, MPI_BYTE, 0, 1, MPI_COMM_WORLD);
            }

            MPI_Barrier(MPI_COMM_WORLD);
        }

        if (rank == 0) {
            double avg_rtt_us = total_rtt / TRIALS;
            one_way_us[s] = avg_rtt_us / 2.0; /* one-way = half the round trip */
            printf("n = %8ld bytes   one-way time = %8.3f us\n", n, one_way_us[s]);
        }

        free(buf);
    }

    if (rank == 0) {
        /* TODO 3: estimate alpha and beta from your measurements.
         *
         * Use the smallest and largest message sizes you measured as two
         * points on the line T(n) = alpha + n*beta, and solve for alpha and
         * beta the way you would for any two-point line fit:
         *
         *   beta  = (T(n_large) - T(n_small)) / (n_large - n_small)
         *   alpha = T(n_small) - n_small * beta
         *
         * Print alpha (in us) and beta (in us/byte), then also print
         * 1/beta converted to GB/s, and the crossover point n* = alpha/beta
         * in bytes. Compare n* to the range of sizes you actually tested,
         * and to the Week 8 lecture table, which network tier does your
         * machine look most like?
         */
        double beta =
            (one_way_us[NUM_SIZES - 1] - one_way_us[0]) / (double)(sizes[NUM_SIZES - 1] - sizes[0]);
        double alpha = one_way_us[0] - sizes[0] * beta;
        printf("alpha = %.6f us\nbeta = %.9f us/byte\n", alpha, beta);
    }

    MPI_Finalize();
    return 0;
}
