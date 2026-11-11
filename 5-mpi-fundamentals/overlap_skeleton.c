/* Task 3: Overlap Communication with Computation (SKELETON, fill in the TODOs)
 *
 * Every rank exchanges a block of data with its two ring neighbors (as it
 * might need to exchange boundary data in a domain-decomposed simulation),
 * and separately does a fixed amount of CPU-bound local computation. You
 * will measure the total time for two different strategies:
 *
 *   Strategy A (sequential): communicate fully, THEN compute.
 *   Strategy B (overlapped): START communicating, compute WHILE it is in
 *     flight, THEN wait for it to finish.
 *
 * Run with at least 4 ranks:
 *   mpicc -o overlap overlap_skeleton.c -lm
 *   mpirun --oversubscribe -np 4 ./overlap
 */
#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 2000000 /* doubles exchanged with EACH neighbor (~16 MB) */
#define TRIALS 8
#define COMPUTE_ITERS 300000 /* tune this if your machine is much faster/slower */

/* A fixed amount of CPU-bound work, independent of any communicated data.
 * Stands in for "useful local computation" a real program would do. */
double burn_cpu(int iters) {
    double x = 1.0000001;
    for (int i = 0; i < iters; i++) {
        x = sin(x) + cos(x) * 0.5;
    }
    return x;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 4) {
        if (rank == 0)
            fprintf(stderr, "Run with at least 4 ranks.\n");
        MPI_Finalize();
        return 1;
    }

    int right = (rank + 1) % size;
    int left = (rank - 1 + size) % size;

    double *send_buf = malloc(BLOCK_SIZE * sizeof(double));
    double *recv_from_left = malloc(BLOCK_SIZE * sizeof(double));
    double *recv_from_right = malloc(BLOCK_SIZE * sizeof(double));
    for (int i = 0; i < BLOCK_SIZE; i++)
        send_buf[i] = rank + i * 1e-9;

    double sequential_total = 0.0, overlap_total = 0.0, sink = 0.0;

    for (int t = 0; t < TRIALS; t++) {
        MPI_Barrier(MPI_COMM_WORLD); /* start each trial from the same line */

        /* --- Strategy A: sequential (communicate, THEN compute) --- */
        double t0 = MPI_Wtime();

        /* TODO 1: exchange `send_buf` with both neighbors using TWO calls
         * to MPI_Sendrecv (deadlock-safe):
         *   (a) send to `right` (tag 0) / receive from `left` (tag 0) into
         *       recv_from_left
         *   (b) send to `left` (tag 1) / receive from `right` (tag 1) into
         *       recv_from_right
         * Do this fully BEFORE burn_cpu below, so there is no overlap. */

        MPI_Sendrecv(send_buf,
                     BLOCK_SIZE,
                     MPI_DOUBLE,
                     right,
                     0,
                     recv_from_left,
                     BLOCK_SIZE,
                     MPI_DOUBLE,
                     left,
                     0,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
        MPI_Sendrecv(send_buf,
                     BLOCK_SIZE,
                     MPI_DOUBLE,
                     left,
                     1,
                     recv_from_right,
                     BLOCK_SIZE,
                     MPI_DOUBLE,
                     right,
                     1,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
        sink += burn_cpu(COMPUTE_ITERS);

        double t1 = MPI_Wtime();
        sequential_total += (t1 - t0);

        MPI_Barrier(MPI_COMM_WORLD);

        /* --- Strategy B: overlapped (start communication, compute while
         * it is in flight, THEN wait) --- */
        double t2 = MPI_Wtime();

        /* TODO 2: start the SAME four transfers as non-blocking operations
         * (MPI_Isend/MPI_Irecv into an MPI_Request[4], same tags as above),
         * call burn_cpu WHILE they are in flight, and only call
         * MPI_Waitall afterward. */

        MPI_Request requests[4];
        MPI_Irecv(recv_from_left, BLOCK_SIZE, MPI_DOUBLE, left, 0, MPI_COMM_WORLD, &requests[0]);
        MPI_Irecv(recv_from_right, BLOCK_SIZE, MPI_DOUBLE, right, 1, MPI_COMM_WORLD, &requests[1]);
        MPI_Isend(send_buf, BLOCK_SIZE, MPI_DOUBLE, right, 0, MPI_COMM_WORLD, &requests[2]);
        MPI_Isend(send_buf, BLOCK_SIZE, MPI_DOUBLE, left, 1, MPI_COMM_WORLD, &requests[3]);

        sink += burn_cpu(COMPUTE_ITERS);

        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);

        double t3 = MPI_Wtime();
        overlap_total += (t3 - t2);
    }

    if (rank == 0) {
        double seq_avg = sequential_total / TRIALS;
        double ovl_avg = overlap_total / TRIALS;
        printf("Sequential (no overlap): %.2f ms/trial\n", seq_avg * 1000.0);
        printf("Overlapped:              %.2f ms/trial\n", ovl_avg * 1000.0);
        printf("Savings: %.2f ms/trial (%.1f%%)\n",
               (seq_avg - ovl_avg) * 1000.0,
               100.0 * (seq_avg - ovl_avg) / seq_avg);
    }

    if (sink < 0.0)
        printf("unreachable\n"); /* keeps `sink` from being dead code */

    free(send_buf);
    free(recv_from_left);
    free(recv_from_right);
    MPI_Finalize();
    return 0;
}
