/* Task 2: Tags & Wildcard Receives (SKELETON, fill in the TODOs)
 *
 * Rank 0 is a collector. Every other rank sends it exactly one message, but
 * a random delay means they do not arrive in rank order. Rank 0 must not
 * assume who is next: it posts a WILDCARD receive and discovers the true
 * sender afterward from the returned MPI_Status.
 *
 * Run with at least 4 ranks:
 *   mpicc -o collector collector_skeleton.c
 *   mpirun --oversubscribe -np 4 ./collector
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

    if (rank == 0) {
        int *heard_from = calloc(size, sizeof(int));
        int all_ok = 1;
        for (int i = 0; i < size - 1; i++) {
            int payload;
            MPI_Status status;
            int true_source, true_tag;

            /* TODO 1: post a receive that accepts a message from ANY source
             * and ANY tag into `payload` (we genuinely do not know who
             * arrives next), then fill in `true_source` and `true_tag` by
             * reading them back out of `status` (do not just assume they
             * match whatever wildcards you used to post the receive). */
            MPI_Recv(&payload, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            true_source = status.MPI_SOURCE;
            true_tag = status.MPI_TAG;

            printf(
                "Collector: received %d from rank %d (tag %d)\n", payload, true_source, true_tag);

            /* TODO 2: sanity-check this message:
             *   (a) have we already heard from true_source before? (use
             *       heard_from[] to detect a duplicate)
             *   (b) does payload == true_source * true_source, as every
             *       worker is supposed to send?
             * Print an error and set all_ok = 0 if either check fails,
             * otherwise mark heard_from[true_source] = 1. */
             if (true_source < 1 || true_source >= size) {
                fprintf(stderr, "Unexpected source %d\n", true_source);
                all_ok = 0;
            } else if (heard_from[true_source] || payload != true_source * true_source
                       || true_tag != true_source) {
                fprintf(stderr, "Invalid/duplicate message from rank %d\n", true_source);
                all_ok = 0;
            } else {
                heard_from[true_source] = 1;
            }
        }

        for (int r = 1; r < size; r++) {
            if (!heard_from[r]) { all_ok = 0; }
        }
        
        printf(all_ok ? "COLLECTOR: PASS\n" : "COLLECTOR: FAIL\n");
        free(heard_from);
    } else {
        /* Worker: wait a random amount of time before sending, so arrival
         * order at rank 0 actually varies from run to run. Nanosecond-
         * resolution time + pid, not time(NULL) alone, so that two runs
         * launched within the same second still get different delays. */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        unsigned int seed =
            (unsigned int)ts.tv_nsec ^ (unsigned int)getpid() ^ (unsigned int)(rank * 7919);
        usleep((rand_r(&seed) % 500) * 1000); /* 0-500 ms */

        int payload = rank * rank;
        MPI_Send(&payload, 1, MPI_INT, 0, rank, MPI_COMM_WORLD); /* tag = own rank */
    }

    MPI_Finalize();
    return 0;
}
